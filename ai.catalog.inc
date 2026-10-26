<?php

/**
 * @file
 * Model catalog sync from public model metadata feeds.
 *
 * models.dev is the primary source. The LiteLLM price map fills fields and
 * models that models.dev lacks. Neither is authoritative: a manual capability
 * override and the provider's own API always win over catalog hints.
 */

/**
 * Primary catalog feed, keyed by catalog provider ID.
 */
define('AI_CATALOG_MODELS_DEV_URL', 'https://models.dev/api.json');

/**
 * Fallback catalog feed, keyed by model with a litellm_provider field.
 */
define('AI_CATALOG_LITELLM_URL', 'https://raw.githubusercontent.com/BerriAI/litellm/main/model_prices_and_context_window.json');

/**
 * Maps AI provider IDs to each feed's provider IDs.
 *
 * Providers without a hosted catalog entry (local runtimes, proxies,
 * character- or second-billed services) are deliberately absent.
 */
function ai_catalog_feed_providers() {
  return [
    'anthropic' => ['models_dev' => 'anthropic', 'litellm' => ['anthropic']],
    'aws_bedrock' => ['models_dev' => 'amazon-bedrock', 'litellm' => ['bedrock', 'bedrock_converse']],
    'azure' => ['models_dev' => 'azure', 'litellm' => ['azure']],
    'deepseek' => ['models_dev' => 'deepseek', 'litellm' => ['deepseek']],
    'google_gemini' => ['models_dev' => 'google', 'litellm' => ['gemini']],
    'groq' => ['models_dev' => 'groq', 'litellm' => ['groq']],
    'huggingface' => ['models_dev' => 'huggingface', 'litellm' => []],
    'mistral' => ['models_dev' => 'mistral', 'litellm' => ['mistral']],
    'moonshot' => ['models_dev' => 'moonshotai', 'litellm' => ['moonshot']],
    'openai' => ['models_dev' => 'openai', 'litellm' => ['openai']],
    'openrouter' => ['models_dev' => 'openrouter', 'litellm' => []],
    'perplexity' => ['models_dev' => 'perplexity', 'litellm' => ['perplexity']],
  ];
}

/**
 * Download both feeds and replace the catalog for enabled providers.
 *
 * models.dev is required: without it the catalog would lose the family,
 * status, and capability fields it supplies, so the previous catalog is kept.
 * A LiteLLM failure only skips the fallback.
 *
 * @return int|false
 *   Number of catalog entries, or FALSE when the sync failed.
 */
function ai_catalog_sync() {
  state_set('ai_catalog_sync_attempted', REQUEST_TIME);
  $provider_ids = array_keys(ai_get_providers());

  $catalog = ai_catalog_fetch(AI_CATALOG_MODELS_DEV_URL);
  if ($catalog === NULL) {
    return FALSE;
  }
  $entries = ai_catalog_parse_models_dev($catalog, $provider_ids);
  unset($catalog);
  $litellm = ai_catalog_fetch(AI_CATALOG_LITELLM_URL);
  if ($litellm !== NULL) {
    $entries = ai_catalog_merge($entries, ai_catalog_parse_litellm($litellm, $provider_ids));
  }
  unset($litellm);

  if (!$entries) {
    watchdog('ai', 'Model catalog sync found no entries for the enabled providers; the previous catalog was kept.', [], WATCHDOG_WARNING);
    return FALSE;
  }

  $transaction = db_transaction();
  try {
    db_delete('ai_model_catalog')->execute();
    foreach (array_chunk($entries, 200, TRUE) as $chunk) {
      $insert = db_insert('ai_model_catalog')->fields(array_keys(ai_catalog_empty_entry()) + ['provider' => 'provider', 'model' => 'model']);
      foreach ($chunk as $key => $entry) {
        list($provider, $model) = explode('/', $key, 2);
        $insert->values(['provider' => $provider, 'model' => $model] + $entry);
      }
      $insert->execute();
    }
  }
  catch (Exception $e) {
    $transaction->rollback();
    watchdog('ai', 'Model catalog sync could not store entries: @message', ['@message' => $e->getMessage()], WATCHDOG_WARNING);
    return FALSE;
  }
  unset($transaction);

  state_set('ai_catalog_synced', REQUEST_TIME);
  backdrop_static_reset('ai_model_catalog_provider');
  return count($entries);
}

/**
 * Fetch and decode one JSON feed.
 *
 * @return array|null
 *   Decoded feed, or NULL on failure.
 */
function ai_catalog_fetch($url) {
  $response = backdrop_http_request($url, ['timeout' => 30]);
  if (!empty($response->error) || (int) $response->code !== 200) {
    watchdog('ai', 'Model catalog feed @url failed: @error', ['@url' => $url, '@error' => !empty($response->error) ? $response->error : 'HTTP ' . $response->code], WATCHDOG_WARNING);
    return NULL;
  }
  $data = json_decode($response->data, TRUE);
  if (!is_array($data)) {
    watchdog('ai', 'Model catalog feed @url is not valid JSON.', ['@url' => $url], WATCHDOG_WARNING);
    return NULL;
  }
  return $data;
}

/**
 * Catalog entry fields, all unknown.
 *
 * NULL means the feed did not say. Prices are USD per token.
 */
function ai_catalog_empty_entry() {
  return [
    'input_usd' => NULL,
    'output_usd' => NULL,
    'cache_read_usd' => NULL,
    'cache_write_usd' => NULL,
    'tool_call' => NULL,
    'vision' => NULL,
    'reasoning' => NULL,
    'structured_output' => NULL,
    'temperature' => NULL,
    'context_limit' => NULL,
    'output_limit' => NULL,
    'family' => '',
    'status' => '',
    'release_date' => '',
    'price_source' => '',
  ];
}

/**
 * Extract catalog entries from a models.dev feed.
 *
 * @param array $catalog
 *   Decoded models.dev api.json, keyed by catalog provider ID.
 * @param array $provider_ids
 *   AI provider IDs to keep entries for.
 *
 * @return array
 *   Entries keyed by "provider/model".
 */
function ai_catalog_parse_models_dev(array $catalog, array $provider_ids) {
  $entries = [];
  $map = array_intersect_key(ai_catalog_feed_providers(), array_flip($provider_ids));
  foreach ($map as $provider_id => $feeds) {
    $models = $catalog[$feeds['models_dev']]['models'] ?? NULL;
    if (!is_array($models)) {
      continue;
    }
    foreach ($models as $model_id => $model) {
      $model_id = (string) $model_id;
      if (!is_array($model) || $model_id === '' || strlen($model_id) > 255) {
        continue;
      }
      $entry = ai_catalog_empty_entry();
      $cost = is_array($model['cost'] ?? NULL) ? $model['cost'] : [];
      // models.dev quotes USD per million tokens.
      if (ai_catalog_rate($cost['input'] ?? NULL) !== NULL) {
        $entry['input_usd'] = ai_catalog_rate($cost['input']) / 1000000;
        $entry['output_usd'] = (ai_catalog_rate($cost['output'] ?? NULL) ?? 0) / 1000000;
        $cache_read = ai_catalog_rate($cost['cache_read'] ?? NULL);
        $entry['cache_read_usd'] = $cache_read !== NULL ? $cache_read / 1000000 : NULL;
        $cache_write = ai_catalog_rate($cost['cache_write'] ?? NULL);
        $entry['cache_write_usd'] = $cache_write !== NULL ? $cache_write / 1000000 : NULL;
        $entry['price_source'] = 'models.dev';
      }
      $entry['tool_call'] = ai_catalog_flag($model['tool_call'] ?? NULL);
      $inputs = $model['modalities']['input'] ?? NULL;
      $entry['vision'] = is_array($inputs) ? (int) in_array('image', $inputs, TRUE) : NULL;
      $entry['reasoning'] = ai_catalog_flag($model['reasoning'] ?? NULL);
      $entry['structured_output'] = ai_catalog_flag($model['structured_output'] ?? NULL);
      $entry['temperature'] = ai_catalog_flag($model['temperature'] ?? NULL);
      $entry['context_limit'] = ai_catalog_limit($model['limit']['context'] ?? NULL);
      $entry['output_limit'] = ai_catalog_limit($model['limit']['output'] ?? NULL);
      $entry['family'] = substr((string) ($model['family'] ?? ''), 0, 64);
      $entry['status'] = substr((string) ($model['status'] ?? ''), 0, 16);
      $entry['release_date'] = substr((string) ($model['release_date'] ?? ''), 0, 10);
      $entries[$provider_id . '/' . $model_id] = $entry;
    }
  }
  return $entries;
}

/**
 * Extract catalog entries from the LiteLLM price map.
 *
 * @param array $map
 *   Decoded model_prices_and_context_window.json.
 * @param array $provider_ids
 *   AI provider IDs to keep entries for.
 *
 * @return array
 *   Entries keyed by "provider/model".
 */
function ai_catalog_parse_litellm(array $map, array $provider_ids) {
  $lookup = [];
  foreach (array_intersect_key(ai_catalog_feed_providers(), array_flip($provider_ids)) as $provider_id => $feeds) {
    foreach ($feeds['litellm'] as $litellm_provider) {
      $lookup[$litellm_provider] = $provider_id;
    }
  }

  $entries = [];
  $today = date('Y-m-d', REQUEST_TIME);
  foreach ($map as $key => $model) {
    $litellm_provider = is_array($model) ? (string) ($model['litellm_provider'] ?? '') : '';
    if (!isset($lookup[$litellm_provider])) {
      continue;
    }
    $provider_id = $lookup[$litellm_provider];
    $model_id = (string) $key;
    if (strpos($model_id, $litellm_provider . '/') === 0) {
      $model_id = substr($model_id, strlen($litellm_provider) + 1);
    }
    // Bedrock keys with a path carry region or size prefixes, not model IDs.
    if ($model_id === '' || strlen($model_id) > 255 || ($provider_id === 'aws_bedrock' && strpos($model_id, '/') !== FALSE)) {
      continue;
    }

    $entry = ai_catalog_empty_entry();
    if (ai_catalog_rate($model['input_cost_per_token'] ?? NULL) !== NULL) {
      $entry['input_usd'] = ai_catalog_rate($model['input_cost_per_token']);
      $entry['output_usd'] = ai_catalog_rate($model['output_cost_per_token'] ?? NULL) ?? 0;
      $entry['cache_read_usd'] = ai_catalog_rate($model['cache_read_input_token_cost'] ?? NULL);
      $entry['cache_write_usd'] = ai_catalog_rate($model['cache_creation_input_token_cost'] ?? NULL);
      $entry['price_source'] = 'litellm';
    }
    $entry['tool_call'] = ai_catalog_flag($model['supports_function_calling'] ?? NULL);
    $entry['vision'] = ai_catalog_flag($model['supports_vision'] ?? NULL);
    $entry['reasoning'] = ai_catalog_flag($model['supports_reasoning'] ?? NULL);
    $entry['structured_output'] = ai_catalog_flag($model['supports_response_schema'] ?? NULL);
    $entry['context_limit'] = ai_catalog_limit($model['max_input_tokens'] ?? NULL);
    $entry['output_limit'] = ai_catalog_limit($model['max_output_tokens'] ?? NULL);
    $deprecation = (string) ($model['deprecation_date'] ?? '');
    if (preg_match('/^\d{4}-\d{2}-\d{2}$/', $deprecation) && $deprecation <= $today) {
      $entry['status'] = 'deprecated';
    }
    $entries[$provider_id . '/' . $model_id] = $entry;
  }
  return $entries;
}

/**
 * Fill unknown fields of primary entries from fallback entries.
 *
 * Prices are filled as a set, so input, output, and cache rates always come
 * from the same feed.
 */
function ai_catalog_merge(array $primary, array $fallback) {
  foreach ($fallback as $key => $entry) {
    if (!isset($primary[$key])) {
      $primary[$key] = $entry;
      continue;
    }
    $merged = $primary[$key];
    if ($merged['input_usd'] === NULL && $entry['input_usd'] !== NULL) {
      foreach (['input_usd', 'output_usd', 'cache_read_usd', 'cache_write_usd', 'price_source'] as $field) {
        $merged[$field] = $entry[$field];
      }
    }
    foreach ($entry as $field => $value) {
      if (in_array($field, ['input_usd', 'output_usd', 'cache_read_usd', 'cache_write_usd', 'price_source'], TRUE)) {
        continue;
      }
      if (($merged[$field] === NULL || $merged[$field] === '') && $value !== NULL && $value !== '') {
        $merged[$field] = $value;
      }
    }
    $primary[$key] = $merged;
  }
  return $primary;
}

/**
 * Normalize a non-negative numeric rate, or NULL.
 */
function ai_catalog_rate($value) {
  return is_numeric($value) && $value >= 0 ? (float) $value : NULL;
}

/**
 * Normalize a boolean feed flag to 1/0, or NULL when absent.
 */
function ai_catalog_flag($value) {
  return is_bool($value) ? (int) $value : NULL;
}

/**
 * Normalize a positive token limit, or NULL.
 */
function ai_catalog_limit($value) {
  return is_numeric($value) && $value > 0 ? (int) min($value, 2147483647) : NULL;
}

/**
 * Form builder: model catalog settings.
 */
function ai_catalog_settings_form($form, &$form_state) {
  $synced_at = (int) state_get('ai_catalog_synced', 0);
  $count = db_table_exists('ai_model_catalog') ? (int) db_query('SELECT COUNT(*) FROM {ai_model_catalog}')->fetchField() : 0;
  $form['status'] = [
    '#markup' => '<p>' . ($synced_at
      ? format_plural($count, '1 model in the catalog, synced @date.', '@count models in the catalog, synced @date.', ['@date' => format_date($synced_at, 'short')])
      : t('The model catalog has not been synced yet.')) . '</p>',
  ];
  $form['catalog_sync'] = [
    '#type' => 'checkbox',
    '#title' => t('Sync the model catalog daily'),
    '#default_value' => config_get('ai.settings', 'catalog_sync') !== FALSE,
    '#description' => t('Cron downloads the public <a href="@models_dev">models.dev</a> catalog, with the <a href="@litellm">LiteLLM price map</a> as a fallback, for the enabled providers. The catalog supplies model prices, fills capability gaps the provider API leaves, omits temperature for models that reject it, caps output tokens at the model limit, and labels model lists. Manual capability overrides and provider-reported capabilities always win.', [
      '@models_dev' => 'https://models.dev',
      '@litellm' => 'https://github.com/BerriAI/litellm',
    ]),
  ];
  $form['actions'] = ['#type' => 'actions'];
  $form['actions']['submit'] = [
    '#type' => 'submit',
    '#value' => t('Save configuration'),
  ];
  $form['actions']['sync'] = [
    '#type' => 'submit',
    '#value' => t('Sync now'),
    '#submit' => ['ai_catalog_settings_form_sync_submit'],
  ];
  return $form;
}

/**
 * Submit handler for ai_catalog_settings_form().
 */
function ai_catalog_settings_form_submit($form, &$form_state) {
  config_set('ai.settings', 'catalog_sync', !empty($form_state['values']['catalog_sync']));
  backdrop_set_message(t('Model catalog settings saved.'));
}

/**
 * Submit handler for the "Sync now" button.
 */
function ai_catalog_settings_form_sync_submit($form, &$form_state) {
  $count = ai_catalog_sync();
  if ($count === FALSE) {
    backdrop_set_message(t('The model catalog could not be synced. The previous catalog was kept; see the site log for details.'), 'error');
  }
  else {
    backdrop_set_message(format_plural($count, 'Synced 1 catalog entry.', 'Synced @count catalog entries.'));
  }
}
