<?php

/**
 * @file
 * Core translation logic for the AI Translate module.
 *
 * Translates a node into a target language by creating a new node in
 * Backdrop's node translation set (core `translation` module), with field
 * values translated by an AI provider through the `ai` module's chat API.
 */

if (!function_exists('_ai_translate_get_model')) {
  /**
   * Resolves the configured or default AI model for translation.
   *
   * @return string|false
   *   The model identifier (e.g. 'openai/gpt-4o-mini'), or FALSE if none available.
   */
  function _ai_translate_get_model() {
    $config = config('ai_translate.settings');
    $model = $config->get('model');
    if (empty($model) && function_exists('ai_default_model')) {
      $model = ai_default_model('chat');
    }
    return !empty($model) ? $model : FALSE;
  }
}

/**
 * Field types this module knows how to translate.
 *
 * Kept as a fixed allowlist rather than an admin setting for v1 — expanding
 * it means adding a case to _ai_translate_field_delta(), not just flipping a
 * checkbox, since each type stores translatable text in different columns.
 */
function _ai_translate_supported_field_types() {
  return [
    'text',
    'text_long',
    'text_with_summary',
    'link_field',
    'image',
    'file',
    'entityreference',
    'paragraphs',
  ];
}

/**
 * Translate one node into $target_langcode, creating a new translation-set
 * member node.
 *
 * @param object $node
 *   The fully loaded source node.
 * @param string $target_langcode
 *   Target language code. Must be a configured, enabled language.
 * @param array $context
 *   Internal recursion state. Callers should omit this — it is built fresh
 *   on the top-level call and threaded through entity-reference recursion.
 *
 * @return array
 *   ['status' => bool, 'node' => object|NULL, 'message' => string]
 */
function ai_translate_node($node, $target_langcode, array $context = []) {
  $result = ['status' => FALSE, 'node' => NULL, 'message' => ''];

  if (empty($node) || empty($node->nid)) {
    $result['message'] = t('No content was provided to translate.');
    return $result;
  }

  // Access check before doing anything else — the exact gap SA-CONTRIB-2026-
  // 120/121 shipped without. A user must be able to view the source content
  // to have it translated on their behalf.
  if (!node_access('view', $node)) {
    $result['message'] = t('You do not have access to view this content.');
    return $result;
  }

  if (!module_exists('translation') || !translation_supported_type($node->type)) {
    $result['message'] = t('The %type content type does not have translation enabled.', ['@type' => $node->type, '%type' => $node->type]);
    return $result;
  }

  $languages = language_list();
  if (!isset($languages[$target_langcode])) {
    $result['message'] = t('@lang is not a configured site language.', ['@lang' => $target_langcode]);
    return $result;
  }

  $source_langcode = !empty($node->langcode) ? $node->langcode : LANGUAGE_NONE;
  if ($source_langcode === $target_langcode) {
    $result['message'] = t('The content is already in the target language.');
    return $result;
  }

  $tnid = !empty($node->tnid) ? $node->tnid : $node->nid;
  $existing_translations = translation_node_get_translations($tnid);
  if (isset($existing_translations[$target_langcode])) {
    // Already translated — reuse rather than duplicate. Relevant mainly
    // when this node was reached by following an entity-reference field on
    // an earlier translation run.
    $existing_node = node_load($existing_translations[$target_langcode]->nid);
    if (!$existing_node) {
      watchdog('ai_translate', 'Translation set @tnid contains an inaccessible or missing @lang node @nid.', [
        '@tnid' => $tnid,
        '@lang' => $target_langcode,
        '@nid' => $existing_translations[$target_langcode]->nid,
      ], WATCHDOG_WARNING);
      $result['message'] = t('The existing @lang translation could not be loaded.', ['@lang' => $target_langcode]);
      return $result;
    }
    $result['status'] = TRUE;
    $result['node'] = $existing_node;
    $result['message'] = t('A translation already exists; reused it.');
    return $result;
  }

  $config = config('ai_translate.settings');
  $model = _ai_translate_get_model();
  if (empty($model)) {
    $result['message'] = t('No AI model is configured or available for AI Translate.');
    watchdog('ai_translate', 'Translation aborted for node @nid: no model configured. Set one at admin/config/ai/ai-translate.', ['@nid' => $node->nid], WATCHDOG_ERROR);
    return $result;
  }

  // Seed recursion state on the top-level call.
  $context += [
    'visited' => [],
    'depth' => 0,
    'max_depth' => (int) ($config->get('reference_depth') ?? 0),
    'config' => $config,
    'model' => $model,
  ];
  $context['visited'][$node->nid] = TRUE;

  $translated = clone $node;
  // Node declares several of these properties. Unsetting declared properties
  // leaves them undefined on PHP 8 and causes backdrop_write_record() warnings.
  $translated->nid = NULL;
  $translated->vid = NULL;
  $translated->vuuid = NULL;
  $translated->created = REQUEST_TIME;
  $translated->changed = REQUEST_TIME;
  $translated->translate = 0;
  unset($translated->path, $translated->log);
  $translated->is_new = TRUE;
  $translated->langcode = $target_langcode;
  $translated->translation_source = $node;
  $translated->tnid = 0;

  $publish_behavior = $config->get('publish_behavior') ?: 'draft';
  $translated->status = ($publish_behavior === 'preserve') ? $node->status : 0;

  $translated->title = _ai_translate_text($node->title, $source_langcode, $target_langcode, $context);

  $instances = field_info_instances('node', $node->type);
  $supported_types = _ai_translate_supported_field_types();

  foreach ($instances as $field_name => $instance) {
    $field = field_info_field($field_name);
    if (empty($field) || !in_array($field['type'], $supported_types, TRUE)) {
      continue;
    }

    $value_langcode = isset($node->{$field_name}[$source_langcode]) ? $source_langcode : LANGUAGE_NONE;
    if (empty($node->{$field_name}[$value_langcode])) {
      continue;
    }

    // A field stored under the source node's own langcode is language-aware
    // and its translated values belong under the target node's langcode. A
    // field stored under LANGUAGE_NONE is language-neutral and stays that
    // way — writing translated text back under $source_langcode here would
    // silently leave it keyed to the wrong (old) language on the new node.
    // LANGUAGE_NONE (und) is language-neutral, not the source language of a
    // language-aware field. Keep neutral fields under und on the translation.
    $output_langcode = ($source_langcode !== LANGUAGE_NONE && $value_langcode === $source_langcode) ? $target_langcode : LANGUAGE_NONE;

    if ($field['type'] === 'paragraphs') {
      // Paragraphs cloning attaches new items directly to the host entity via
      // setHostEntity(). Clear the source references first and let that API
      // rebuild the field with newly cloned, translated items.
      unset($translated->{$field_name});
      foreach ($node->{$field_name}[$value_langcode] as $item) {
        _ai_translate_paragraph_delta($item, $field_name, $source_langcode, $target_langcode, $translated, $context, $output_langcode);
      }
      continue;
    }

    $translated_items = [];
    foreach ($node->{$field_name}[$value_langcode] as $delta => $item) {
      $translated_items[$delta] = _ai_translate_field_delta($item, $field['type'], $field, $source_langcode, $target_langcode, $context);
    }
    unset($translated->{$field_name});
    $translated->{$field_name}[$output_langcode] = $translated_items;
  }

  // Allow other modules to alter the translated node prior to saving.
  backdrop_alter('ai_translate_node', $translated, $node, $target_langcode, $context);

  node_save($translated);

  watchdog('ai_translate', 'Created @target translation of node @nid as new node @new_nid.', [
    '@target' => $target_langcode,
    '@nid' => $node->nid,
    '@new_nid' => $translated->nid,
  ], WATCHDOG_INFO);

  $result['status'] = TRUE;
  $result['node'] = $translated;
  return $result;
}

/**
 * Clone and translate one Paragraphs item for the translated host entity.
 *
 * Paragraphs are reusable entities rather than inline field values. The
 * translated node therefore needs its own paragraph items; otherwise the
 * source node's paragraphs would remain shared and untranslated.
 */
function _ai_translate_paragraph_delta(array $item, $field_name, $source_langcode, $target_langcode, $host_entity, array &$context, $host_langcode) {
  if (!module_exists('paragraphs') || !function_exists('paragraphs_field_get_entity')) {
    return $item;
  }

  $source_paragraph = paragraphs_field_get_entity($item);
  if (!$source_paragraph || empty($source_paragraph->bundle)) {
    return $item;
  }

  $translated_paragraph = clone $source_paragraph;
  $translated_paragraph->item_id = NULL;
  $translated_paragraph->revision_id = NULL;
  $translated_paragraph->is_new = TRUE;
  $translated_paragraph->field_name = $field_name;
  $host_type = ($host_entity instanceof Node) ? 'node' : 'paragraphs_item';
  // The default TRUE flag attaches the new item to its host, matching the
  // Paragraphs module's node-clone integration.
  $translated_paragraph->setHostEntity($host_type, $host_entity, $host_langcode);

  $instances = field_info_instances('paragraphs_item', $source_paragraph->bundle);
  foreach ($instances as $nested_field_name => $instance) {
    $nested_field = field_info_field($nested_field_name);
    if (empty($nested_field) || !isset($source_paragraph->{$nested_field_name})) {
      continue;
    }

    $nested_langcode = isset($source_paragraph->{$nested_field_name}[$source_langcode]) ? $source_langcode : LANGUAGE_NONE;
    if (empty($source_paragraph->{$nested_field_name}[$nested_langcode])) {
      continue;
    }
    $output_langcode = ($source_langcode !== LANGUAGE_NONE && $nested_langcode === $source_langcode) ? $target_langcode : LANGUAGE_NONE;
    if ($nested_field['type'] === 'paragraphs') {
      unset($translated_paragraph->{$nested_field_name});
      foreach ($source_paragraph->{$nested_field_name}[$nested_langcode] as $nested_item) {
        _ai_translate_paragraph_delta($nested_item, $nested_field_name, $source_langcode, $target_langcode, $translated_paragraph, $context, $output_langcode);
      }
    }
    else {
      $nested_items = [];
      foreach ($source_paragraph->{$nested_field_name}[$nested_langcode] as $nested_delta => $nested_item) {
        $nested_items[$nested_delta] = _ai_translate_field_delta($nested_item, $nested_field['type'], $nested_field, $source_langcode, $target_langcode, $context);
      }
      unset($translated_paragraph->{$nested_field_name});
      $translated_paragraph->{$nested_field_name}[$output_langcode] = $nested_items;
    }
  }

  return $translated_paragraph;
}

/**
 * Translate the columns of a single field delta, dispatched by field type.
 */
function _ai_translate_field_delta(array $item, $field_type, array $field, $source_langcode, $target_langcode, array &$context) {
  switch ($field_type) {
    case 'text':
    case 'text_long':
      if (!empty($item['value'])) {
        $item['value'] = _ai_translate_text($item['value'], $source_langcode, $target_langcode, $context);
      }
      return $item;

    case 'text_with_summary':
      if (!empty($item['value'])) {
        $item['value'] = _ai_translate_text($item['value'], $source_langcode, $target_langcode, $context);
      }
      if (!empty($item['summary'])) {
        $item['summary'] = _ai_translate_text($item['summary'], $source_langcode, $target_langcode, $context);
      }
      return $item;

    case 'link_field':
      if (!empty($item['title'])) {
        $item['title'] = _ai_translate_text($item['title'], $source_langcode, $target_langcode, $context);
      }
      return $item;

    case 'image':
      if (!empty($item['alt'])) {
        $item['alt'] = _ai_translate_text($item['alt'], $source_langcode, $target_langcode, $context);
      }
      if (!empty($item['title'])) {
        $item['title'] = _ai_translate_text($item['title'], $source_langcode, $target_langcode, $context);
      }
      return $item;

    case 'file':
      if (!empty($item['description'])) {
        $item['description'] = _ai_translate_text($item['description'], $source_langcode, $target_langcode, $context);
      }
      return $item;

    case 'entityreference':
      return _ai_translate_follow_reference($item, $field, $target_langcode, $context);

    default:
      return $item;
  }
}

/**
 * Follow an entity-reference field delta, translating the target if access
 * and depth allow it. Skips silently (never fatal) when access is denied,
 * depth is exceeded, or the target type isn't a node — the reference is
 * left pointing at the original (source-language) entity in that case.
 */
function _ai_translate_follow_reference(array $item, array $field, $target_langcode, array &$context) {
  if (empty($item['target_id'])) {
    return $item;
  }

  $max_depth = $context['max_depth'] ?? 0;
  if ($context['depth'] >= $max_depth) {
    return $item;
  }

  $target_type = $field['settings']['target_type'] ?? 'node';
  if ($target_type !== 'node') {
    // Only node translation sets are supported by this module.
    watchdog('ai_translate', 'Skipped following @type reference (target_id @id): only node references can be followed.', [
      '@type' => $target_type,
      '@id' => $item['target_id'],
    ], WATCHDOG_DEBUG);
    return $item;
  }

  $ref_nid = (int) $item['target_id'];
  if (!empty($context['visited'][$ref_nid])) {
    // Cycle guard.
    return $item;
  }

  $ref_node = node_load($ref_nid);
  if (empty($ref_node)) {
    return $item;
  }

  if (!node_access('view', $ref_node)) {
    watchdog('ai_translate', 'Skipped following reference to node @nid: current user lacks view access.', ['@nid' => $ref_nid], WATCHDOG_DEBUG);
    return $item;
  }

  if (!translation_supported_type($ref_node->type)) {
    return $item;
  }

  $context['visited'][$ref_nid] = TRUE;
  $context['depth']++;

  $sub_result = ai_translate_node($ref_node, $target_langcode, $context);

  $context['depth']--;

  if (!empty($sub_result['status']) && !empty($sub_result['node']->nid)) {
    $item['target_id'] = $sub_result['node']->nid;
  }

  return $item;
}

/**
 * Translate a single string via the configured AI model. Returns the
 * original text unchanged on any failure so content is never lost, and
 * records the failure on $context for the caller to surface.
 */
function _ai_translate_text($text, $source_langcode, $target_langcode, array &$context) {
  $text = trim((string) $text);
  if ($text === '') {
    return $text;
  }

  /** @var Config $config */
  $config = $context['config'];
  $model = !empty($context['model']) ? $context['model'] : _ai_translate_get_model();
  if (empty($model)) {
    watchdog('ai_translate', 'Translation call aborted: no AI model available.', [], WATCHDOG_ERROR);
    $context['errors'][] = 'no model available';
    return $text;
  }
  $temperature = (float) ($config->get('temperature') ?? 0.3);
  $max_tokens = (int) ($config->get('max_tokens') ?? 2048);

  $prompt = _ai_translate_build_prompt($source_langcode, $target_langcode, $config);
  $messages = [
    ['role' => 'system', 'content' => $prompt],
    ['role' => 'user', 'content' => $text],
  ];

  try {
    $context_extra = [
      'operation' => 'ai_translate',
      'source_langcode' => $source_langcode,
      'target_langcode' => $target_langcode,
    ];
    $response = ai_chat($model, $messages, $temperature, $max_tokens, FALSE, $context_extra);
  }
  catch (\Exception $e) {
    watchdog('ai_translate', 'Translation call failed for @source to @target: @message', [
      '@source' => $source_langcode,
      '@target' => $target_langcode,
      '@message' => $e->getMessage(),
    ], WATCHDOG_ERROR);
    $context['errors'][] = $e->getMessage();
    return $text;
  }

  $translated_text = '';
  if (is_string($response) && trim($response) !== '') {
    $translated_text = trim($response);
  }
  elseif (is_array($response) && !empty($response['choices'][0]['message']['content'])) {
    $translated_text = trim($response['choices'][0]['message']['content']);
  }

  if ($translated_text !== '') {
    // Allow modules to alter or post-process translated text.
    backdrop_alter('ai_translate_text', $translated_text, $text, $source_langcode, $target_langcode, $context);
    return $translated_text;
  }

  watchdog('ai_translate', 'Translation call for @source to @target returned no usable content.', [
    '@source' => $source_langcode,
    '@target' => $target_langcode,
  ], WATCHDOG_WARNING);
  $context['errors'][] = 'empty response';
  return $text;
}

/**
 * Build the system prompt for one translation call, applying a per-language
 * override when configured, falling back to the site-wide default prompt.
 */
function _ai_translate_build_prompt($source_langcode, $target_langcode, $config) {
  $language_prompts = $config->get('language_prompts') ?: [];
  $prompt = !empty($language_prompts[$target_langcode]) ? $language_prompts[$target_langcode] : $config->get('prompt');

  $languages = language_list();
  $source_name = isset($languages[$source_langcode]) ? $languages[$source_langcode]->name : $source_langcode;
  $target_name = isset($languages[$target_langcode]) ? $languages[$target_langcode]->name : $target_langcode;

  return strtr($prompt, [
    '{source_language}' => $source_name,
    '{target_language}' => $target_name,
  ]);
}
