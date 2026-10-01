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
      check_plain($record->provider . ($record->model ? ' / ' . $record->model : '')),
      number_format((int) $record->total_tokens),
      $record->cost_source === 'manual' ? '$' . number_format((float) $record->cost_usd, 8) : t('Unknown'),
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
  fputcsv($handle, ['id', 'timestamp', 'uid', 'module', 'operation', 'provider', 'model', 'request_id', 'context_group', 'status', 'duration_ms', 'input_tokens', 'output_tokens', 'cached_tokens', 'reasoning_tokens', 'total_tokens', 'usage_source', 'input_cost_usd', 'output_cost_usd', 'cost_usd', 'cost_source', 'error_message']);
  foreach ($query->execute() as $record) {
    fputcsv($handle, [
      $record->id,
      $record->timestamp,
      $record->uid,
      $record->module,
      $record->operation,
      $record->provider,
      $record->model,
      $record->request_id,
      $record->context_group,
      $record->status,
      $record->duration_ms,
      $record->input_tokens,
      $record->output_tokens,
      $record->cached_tokens,
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
