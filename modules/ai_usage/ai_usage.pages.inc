<?php

/**
 * @file
 * Admin pages for AI Usage.
 */

/**
 * Page callback: AI Usage report.
 */
function ai_usage_report_page() {
  $filters = $_SESSION['ai_usage_filter'] ?? [];
  $query = db_select('ai_usage_records', 'u')
    ->extend('PagerDefault')
    ->extend('TableSort');
  $query->fields('u');
  $query->limit(50);
  $header = [
    ['data' => t('ID'), 'field' => 'id', 'sort' => 'desc'],
    ['data' => t('Time'), 'field' => 'timestamp'],
    ['data' => t('User')],
    ['data' => t('Module'), 'field' => 'module'],
    ['data' => t('Operation'), 'field' => 'operation'],
    ['data' => t('Provider / model')],
    ['data' => t('Tokens'), 'field' => 'total_tokens'],
    ['data' => t('Cost (USD)'), 'field' => 'cost_usd'],
    ['data' => t('Source'), 'field' => 'usage_source'],
    ['data' => t('Duration'), 'field' => 'duration_ms'],
    ['data' => t('Status'), 'field' => 'status'],
  ];
  $query->orderByHeader($header);
  ai_usage_apply_filters($query, $filters);

  $build['filter_form'] = backdrop_get_form('ai_usage_filter_form');
  $build['export'] = [
    '#markup' => l(t('Download CSV'), 'admin/reports/ai-usage/export', ['attributes' => ['class' => ['button']]]),
  ];
  $budget = ai_usage_budget_summary_build();
  if ($budget) {
    $build['budget'] = $budget;
  }
  $build['summary'] = ai_usage_summary_build($filters);
  $rows = [];
  foreach ($query->execute() as $record) {
    $account = $record->uid ? user_load($record->uid) : FALSE;
    $user_label = $account ? theme('username', ['account' => $account]) : t('Anonymous/system');
    $rows[] = [
      $record->id,
      format_date($record->timestamp, 'short'),
      $user_label,
      check_plain($record->module),
      check_plain($record->operation),
      check_plain($record->provider . ($record->model ? ' / ' . $record->model : '')) . (!empty($record->requested_model) ? '<br><small>' . t('requested @model', ['@model' => $record->requested_model]) . '</small>' : ''),
      number_format((int) $record->total_tokens),
      $record->cost_source !== 'unknown' ? '$' . number_format((float) $record->cost_usd, 8) : t('Unknown'),
      check_plain($record->usage_source),
      number_format((int) $record->duration_ms) . ' ms',
      $record->status ? t('Success') : t('Failure'),
    ];
  }

  $build['table'] = [
    '#theme' => 'table',
    '#header' => $header,
    '#rows' => $rows,
    '#empty' => t('No AI usage records found.'),
  ];
  $build['pager'] = ['#theme' => 'pager'];
  return $build;
}

/**
 * Form builder: AI Usage filters.
 */
function ai_usage_filter_form($form, &$form_state) {
  $filters = $_SESSION['ai_usage_filter'] ?? [];
  $form['filters'] = [
    '#type' => 'fieldset',
    '#title' => t('Filters'),
    '#collapsible' => TRUE,
    '#collapsed' => empty($filters),
  ];
  foreach (['module' => 'Module', 'operation' => 'Operation', 'provider' => 'Provider'] as $key => $label) {
    $options = db_query('SELECT DISTINCT ' . $key . ' FROM {ai_usage_records} WHERE ' . $key . ' <> :empty ORDER BY ' . $key . ' ASC', [':empty' => ''])->fetchAllKeyed(0, 0);
    $form['filters'][$key] = [
      '#type' => 'select',
      '#title' => t($label),
      '#options' => ['' => t('- Any -')] + $options,
      '#default_value' => $filters[$key] ?? '',
    ];
  }
  $form['filters']['usage_source'] = [
    '#type' => 'select',
    '#title' => t('Usage source'),
    '#options' => ['' => t('- Any -'), 'provider' => t('Provider'), 'estimated' => t('Estimated'), 'unknown' => t('Unknown')],
    '#default_value' => $filters['usage_source'] ?? '',
  ];
  $form['filters']['actions'] = ['#type' => 'actions'];
  $form['filters']['actions']['submit'] = [
    '#type' => 'submit',
    '#value' => t('Filter'),
  ];
  if (!empty($filters)) {
    $form['filters']['actions']['reset'] = [
      '#type' => 'submit',
      '#value' => t('Reset'),
    ];
  }
  return $form;
}

/**
 * Submit handler for ai_usage_filter_form().
 */
function ai_usage_filter_form_submit($form, &$form_state) {
  if ($form_state['values']['op'] === t('Reset')) {
    unset($_SESSION['ai_usage_filter']);
    return;
  }
  $_SESSION['ai_usage_filter'] = [
    'module' => $form_state['values']['module'],
    'operation' => $form_state['values']['operation'],
    'provider' => $form_state['values']['provider'],
    'usage_source' => $form_state['values']['usage_source'],
  ];
}

/**
 * Apply report filters to a database query.
 */
function ai_usage_apply_filters($query, array $filters) {
  foreach (['module', 'operation', 'provider', 'usage_source'] as $field) {
    if (isset($filters[$field]) && $filters[$field] !== '') {
      $query->condition('u.' . $field, $filters[$field]);
    }
  }
}

/**
 * Build the current filtered summary.
 */
function ai_usage_summary_build(array $filters) {
  $query = db_select('ai_usage_records', 'u');
  $query->addExpression('COUNT(u.id)', 'calls');
  $query->addExpression('SUM(u.total_tokens)', 'tokens');
  $query->addExpression('SUM(u.input_tokens)', 'input_tokens');
  $query->addExpression('SUM(u.output_tokens)', 'output_tokens');
  $query->addExpression('SUM(u.cost_usd)', 'cost_usd');
  $query->addExpression('SUM(u.duration_ms)', 'duration_ms');
  ai_usage_apply_filters($query, $filters);
  $summary = $query->execute()->fetchAssoc();
  $rows = [[
    ['data' => t('Calls'), 'header' => TRUE],
    number_format((int) ($summary['calls'] ?? 0)),
  ], [
    ['data' => t('Total tokens'), 'header' => TRUE],
    number_format((int) ($summary['tokens'] ?? 0)),
  ], [
    ['data' => t('Input / output'), 'header' => TRUE],
    number_format((int) ($summary['input_tokens'] ?? 0)) . ' / ' . number_format((int) ($summary['output_tokens'] ?? 0)),
  ], [
    ['data' => t('Estimated cost'), 'header' => TRUE],
    '$' . number_format((float) ($summary['cost_usd'] ?? 0), 8),
  ], [
    ['data' => t('Total provider time'), 'header' => TRUE],
    number_format((int) ($summary['duration_ms'] ?? 0)) . ' ms',
  ]];
  return [
    '#type' => 'fieldset',
    '#title' => t('Filtered summary'),
    '#collapsible' => TRUE,
    '#collapsed' => FALSE,
    'table' => ['#theme' => 'table', '#rows' => $rows],
  ];
}

/**
 * Build the current-user budget status shown above the report.
 */
function ai_usage_budget_summary_build() {
  $config = ai_usage_get_budget_config();
  if (!$config['enabled']) {
    return [];
  }
  $status = ai_usage_get_budget_status();
  if ($status['limits']['tokens'] <= 0 && $status['limits']['cost_usd'] <= 0) {
    return [];
  }

  $token_limit = $status['limits']['tokens'] > 0
    ? number_format($status['limits']['tokens'])
    : t('Unlimited');
  $cost_limit = $status['limits']['cost_usd'] > 0
    ? '$' . number_format($status['limits']['cost_usd'], 2)
    : t('Unlimited');
  $rows = [
    [
      ['data' => t('Period'), 'header' => TRUE],
      format_date($status['period_start'], 'short') . ' – ' . format_date($status['period_end'] - 1, 'short'),
    ],
    [
      ['data' => t('Tokens'), 'header' => TRUE],
      number_format($status['current']['tokens']) . ' / ' . $token_limit,
    ],
    [
      ['data' => t('Estimated cost'), 'header' => TRUE],
      '$' . number_format($status['current']['cost_usd'], 8) . ' / ' . $cost_limit,
    ],
    [
      ['data' => t('Usage'), 'header' => TRUE],
      number_format($status['percent'], 1) . '%',
    ],
  ];
  return [
    '#type' => 'fieldset',
    '#title' => t('Current @scope budget', ['@scope' => $status['scope']]),
    '#collapsible' => TRUE,
    '#collapsed' => FALSE,
    'table' => ['#theme' => 'table', '#rows' => $rows],
    'notice' => [
      '#markup' => $status['exceeded']
        ? '<p class="messages error">' . check_plain(t('The configured budget has been reached.')) . '</p>'
        : '',
    ],
  ];
}

/**
 * Page callback: list the synced and manual model prices in use.
 */
function ai_usage_pricing_page() {
  if (!db_table_exists('ai_model_catalog')) {
    return ['#markup' => '<p>' . t('The AI model catalog table is missing. Run database updates.') . '</p>'];
  }
  $filters = $_SESSION['ai_usage_pricing_filter'] ?? [];
  $synced_at = (int) state_get('ai_catalog_synced', 0);
  $build['status'] = [
    '#markup' => '<p>' . ($synced_at
      ? t('Model catalog synced from <a href="@url">models.dev</a>, with LiteLLM as a fallback, on @date. Manual prices are set on the <a href="@settings">AI Usage settings</a> page and override catalog prices. Rates are USD per million tokens.', ['@url' => 'https://models.dev', '@date' => format_date($synced_at, 'short'), '@settings' => url('admin/config/ai/ai-usage')])
      : t('The model catalog has not been synced yet. Sync it on the <a href="@catalog">model catalog settings</a> page, or add manual prices on the <a href="@settings">AI Usage settings</a> page.', ['@catalog' => url('admin/config/ai/settings/catalog'), '@settings' => url('admin/config/ai/ai-usage')])) . '</p>',
  ];
  $build['filter_form'] = backdrop_get_form('ai_usage_pricing_filter_form');

  $header = [
    ['data' => t('Provider'), 'field' => 'provider', 'sort' => 'asc'],
    ['data' => t('Model'), 'field' => 'model'],
    ['data' => t('Family'), 'field' => 'family'],
    ['data' => t('Input / 1M'), 'field' => 'input_usd'],
    ['data' => t('Output / 1M'), 'field' => 'output_usd'],
    ['data' => t('Cache read / 1M'), 'field' => 'cache_read_usd'],
    ['data' => t('Cache write / 1M'), 'field' => 'cache_write_usd'],
    ['data' => t('Tools'), 'field' => 'tool_call'],
    ['data' => t('Vision'), 'field' => 'vision'],
    ['data' => t('Reasoning'), 'field' => 'reasoning'],
    ['data' => t('Context'), 'field' => 'context_limit'],
    ['data' => t('Status')],
    ['data' => t('Source')],
  ];

  // Manual rows with no catalog price cannot join the paged query.
  $manual_only = [];
  foreach (config_get('ai_usage.settings', 'pricing') ?: [] as $key => $rates) {
    if (!is_array($rates)) {
      continue;
    }
    list($provider, $model) = strpos($key, '/') !== FALSE ? explode('/', $key, 2) : ['', $key];
    if (!ai_usage_pricing_row_matches($provider, $model, $filters)) {
      continue;
    }
    $exists = $provider !== '' && db_query('SELECT 1 FROM {ai_model_catalog} WHERE provider = :provider AND model = :model AND input_usd IS NOT NULL', [':provider' => $provider, ':model' => $model])->fetchField();
    if (!$exists) {
      $manual_only[] = [
        check_plain($provider !== '' ? ai_get_provider_label($provider) : t('Any provider')),
        check_plain($model),
        ai_usage_format_rate($rates['input'] ?? 0),
        ai_usage_format_rate($rates['output'] ?? 0),
        t('manual'),
      ];
    }
  }
  if ($manual_only) {
    $build['manual'] = [
      '#theme' => 'table',
      '#caption' => t('Manual prices without a synced price'),
      '#header' => [t('Provider'), t('Model'), t('Input / 1M'), t('Output / 1M'), t('Source')],
      '#rows' => $manual_only,
    ];
  }

  $query = db_select('ai_model_catalog', 'p')
    ->extend('PagerDefault')
    ->extend('TableSort');
  $query->fields('p');
  $query->isNotNull('p.input_usd');
  $query->limit(50);
  $query->orderByHeader($header);
  $query->orderBy('p.model');
  if (!empty($filters['provider'])) {
    $query->condition('p.provider', $filters['provider']);
  }
  if (isset($filters['model']) && $filters['model'] !== '') {
    $query->condition('p.model', '%' . db_like($filters['model']) . '%', 'LIKE');
  }
  if (!empty($filters['capability']) && in_array($filters['capability'], ['tool_call', 'vision', 'reasoning', 'structured_output'], TRUE)) {
    $query->condition('p.' . $filters['capability'], 1);
  }
  if (!empty($filters['hide_deprecated'])) {
    $query->condition('p.status', 'deprecated', '<>');
  }

  $flag = function ($value) {
    if ($value === NULL) {
      return '—';
    }
    return (int) $value ? t('Yes') : t('No');
  };
  $rows = [];
  foreach ($query->execute() as $record) {
    $pricing = ai_usage_get_pricing($record->provider, $record->model);
    $rows[] = [
      check_plain(ai_get_provider_label($record->provider)),
      check_plain($record->model),
      check_plain((string) $record->family),
      ai_usage_format_rate($pricing['input']),
      ai_usage_format_rate($pricing['output']),
      $record->cache_read_usd !== NULL ? ai_usage_format_rate($record->cache_read_usd) : '—',
      $record->cache_write_usd !== NULL ? ai_usage_format_rate($record->cache_write_usd) : '—',
      $flag($record->tool_call),
      $flag($record->vision),
      $flag($record->reasoning),
      $record->context_limit !== NULL ? number_format((int) $record->context_limit) : '—',
      check_plain((string) $record->status),
      $pricing['source'] === 'manual' ? t('manual override') : check_plain((string) $record->price_source),
    ];
  }
  $build['table'] = [
    '#theme' => 'table',
    '#header' => $header,
    '#rows' => $rows,
    '#empty' => t('No catalog model prices found.'),
  ];
  $build['pager'] = ['#theme' => 'pager'];
  return $build;
}

/**
 * Whether a manual price row passes the price list filters.
 */
function ai_usage_pricing_row_matches($provider, $model, array $filters) {
  if (!empty($filters['provider']) && $provider !== $filters['provider']) {
    return FALSE;
  }
  // Manual rows carry no capability data.
  if (!empty($filters['capability'])) {
    return FALSE;
  }
  return !isset($filters['model']) || $filters['model'] === '' || stripos($model, $filters['model']) !== FALSE;
}

/**
 * Format a per-token USD rate as a per-million-token price.
 */
function ai_usage_format_rate($rate) {
  return '$' . rtrim(rtrim(number_format((float) $rate * 1000000, 4), '0'), '.');
}

/**
 * Form builder: price list filters.
 */
function ai_usage_pricing_filter_form($form, &$form_state) {
  $filters = $_SESSION['ai_usage_pricing_filter'] ?? [];
  $providers = [];
  foreach (db_query('SELECT DISTINCT provider FROM {ai_model_catalog} WHERE input_usd IS NOT NULL ORDER BY provider ASC')->fetchCol() as $provider) {
    $providers[$provider] = ai_get_provider_label($provider);
  }
  $form['filters'] = [
    '#type' => 'fieldset',
    '#title' => t('Filters'),
    '#collapsible' => TRUE,
    '#collapsed' => empty($filters),
  ];
  $form['filters']['provider'] = [
    '#type' => 'select',
    '#title' => t('Provider'),
    '#options' => ['' => t('- Any -')] + $providers,
    '#default_value' => $filters['provider'] ?? '',
  ];
  $form['filters']['model'] = [
    '#type' => 'textfield',
    '#title' => t('Model contains'),
    '#default_value' => $filters['model'] ?? '',
    '#size' => 30,
  ];
  $form['filters']['capability'] = [
    '#type' => 'select',
    '#title' => t('Capability'),
    '#options' => [
      '' => t('- Any -'),
      'tool_call' => t('Tools'),
      'vision' => t('Vision'),
      'reasoning' => t('Reasoning'),
      'structured_output' => t('Structured output'),
    ],
    '#default_value' => $filters['capability'] ?? '',
  ];
  $form['filters']['hide_deprecated'] = [
    '#type' => 'checkbox',
    '#title' => t('Hide deprecated models'),
    '#default_value' => !empty($filters['hide_deprecated']),
  ];
  $form['filters']['actions'] = ['#type' => 'actions'];
  $form['filters']['actions']['submit'] = [
    '#type' => 'submit',
    '#value' => t('Filter'),
  ];
  if (!empty($filters)) {
    $form['filters']['actions']['reset'] = [
      '#type' => 'submit',
      '#value' => t('Reset'),
    ];
  }
  return $form;
}

/**
 * Submit handler for ai_usage_pricing_filter_form().
 */
function ai_usage_pricing_filter_form_submit($form, &$form_state) {
  if ($form_state['values']['op'] === t('Reset')) {
    unset($_SESSION['ai_usage_pricing_filter']);
    return;
  }
  $_SESSION['ai_usage_pricing_filter'] = array_filter([
    'provider' => (string) $form_state['values']['provider'],
    'model' => trim((string) $form_state['values']['model']),
    'capability' => (string) $form_state['values']['capability'],
    'hide_deprecated' => (int) !empty($form_state['values']['hide_deprecated']),
  ]);
}

/**
 * Page callback: export filtered AI Usage records as CSV.
 */
function ai_usage_export_page() {
  $filters = $_SESSION['ai_usage_filter'] ?? [];
  $query = db_select('ai_usage_records', 'u');
  $query->fields('u');
  $query->orderBy('u.timestamp', 'DESC');
  ai_usage_apply_filters($query, $filters);

  backdrop_add_http_header('Content-Type', 'text/csv; charset=utf-8');
  backdrop_add_http_header('Content-Disposition', 'attachment; filename="ai-usage.csv"');
  $handle = fopen('php://output', 'w');
  fputcsv($handle, ['id', 'timestamp', 'uid', 'module', 'operation', 'provider', 'model', 'requested_model', 'request_id', 'context_group', 'status', 'duration_ms', 'input_tokens', 'output_tokens', 'cached_tokens', 'cache_write_tokens', 'reasoning_tokens', 'total_tokens', 'usage_source', 'input_cost_usd', 'output_cost_usd', 'cost_usd', 'cost_source', 'error_message']);
  foreach ($query->execute() as $record) {
    fputcsv($handle, [
      $record->id,
      $record->timestamp,
      $record->uid,
      $record->module,
      $record->operation,
      $record->provider,
      $record->model,
      $record->requested_model,
      $record->request_id,
      $record->context_group,
      $record->status,
      $record->duration_ms,
      $record->input_tokens,
      $record->output_tokens,
      $record->cached_tokens,
      $record->cache_write_tokens,
      $record->reasoning_tokens,
      $record->total_tokens,
      $record->usage_source,
      $record->input_cost_usd,
      $record->output_cost_usd,
      $record->cost_usd,
      $record->cost_source,
      $record->error_message,
    ]);
  }
  fclose($handle);
  backdrop_exit();
}
