<?php

/**
 * @file
 * Admin pages for AI logs.
 */

/**
 * Page callback: AI Logs list.
 */
function ai_log_list_page() {
  $header = [
    ['data' => t('ID'), 'field' => 'id', 'sort' => 'desc'],
    ['data' => t('Timestamp'), 'field' => 'timestamp'],
    ['data' => t('Module'), 'field' => 'module'],
    ['data' => t('Operation'), 'field' => 'operation'],
    ['data' => t('Model'), 'field' => 'model'],
    ['data' => t('Provider'), 'field' => 'provider'],
    ['data' => t('Status'), 'field' => 'status'],
    ['data' => t('Duration'), 'field' => 'duration'],
    ['data' => t('Operations')],
  ];

  // Add filters
  $build['filter_form'] = backdrop_get_form('ai_log_filter_form');

  $query = db_select('ai_log', 'l')
    ->extend('PagerDefault')
    ->extend('TableSort');
  $query->fields('l');
  $query->limit(50);
  $query->orderByHeader($header);

  // Apply filters
  if (isset($_SESSION['ai_log_filter'])) {
    foreach ($_SESSION['ai_log_filter'] as $field => $value) {
      if ($value !== '') {
        $query->condition($field, $value);
      }
    }
  }

  $result = $query->execute();

  $rows = [];
  foreach ($result as $log) {
    $rows[] = [
      $log->id,
      format_date($log->timestamp, 'short'),
      check_plain($log->module),
      check_plain($log->operation),
      check_plain($log->model),
      check_plain($log->provider),
      $log->status ? t('Success') : t('Failure'),
      number_format($log->duration, 2) . 's',
      l(t('View'), 'admin/reports/ai/' . $log->id),
    ];
  }

  $build['table'] = [
    '#theme' => 'table',
    '#header' => $header,
    '#rows' => $rows,
    '#empty' => t('No AI logs found.'),
  ];
  $build['pager'] = [
    '#theme' => 'pager',
  ];

  return $build;
}

/**
 * Form builder: AI Logs filter form.
 */
function ai_log_filter_form($form, &$form_state) {
  $form['filters'] = [
    '#type' => 'fieldset',
    '#title' => t('Filters'),
    '#collapsible' => TRUE,
    '#collapsed' => empty($_SESSION['ai_log_filter']),
  ];

  $options_module = db_query("SELECT DISTINCT module FROM {ai_log} ORDER BY module ASC")->fetchAllKeyed(0, 0);
  $form['filters']['module'] = [
    '#type' => 'select',
    '#title' => t('Module'),
    '#options' => ['' => t('- Any -')] + $options_module,
    '#default_value' => $_SESSION['ai_log_filter']['module'] ?? '',
  ];

  $options_op = db_query("SELECT DISTINCT operation FROM {ai_log} ORDER BY operation ASC")->fetchAllKeyed(0, 0);
  $form['filters']['operation'] = [
    '#type' => 'select',
    '#title' => t('Operation'),
    '#options' => ['' => t('- Any -')] + $options_op,
    '#default_value' => $_SESSION['ai_log_filter']['operation'] ?? '',
  ];

  $options_provider = db_query("SELECT DISTINCT provider FROM {ai_log} ORDER BY provider ASC")->fetchAllKeyed(0, 0);
  $form['filters']['provider'] = [
    '#type' => 'select',
    '#title' => t('Provider'),
    '#options' => ['' => t('- Any -')] + $options_provider,
    '#default_value' => $_SESSION['ai_log_filter']['provider'] ?? '',
  ];

  $form['filters']['status'] = [
    '#type' => 'select',
    '#title' => t('Status'),
    '#options' => [
      '' => t('- Any -'),
      1 => t('Success'),
      0 => t('Failure'),
    ],
    '#default_value' => $_SESSION['ai_log_filter']['status'] ?? '',
  ];

  $form['filters']['actions'] = ['#type' => 'actions'];
  $form['filters']['actions']['submit'] = [
    '#type' => 'submit',
    '#value' => t('Filter'),
  ];
  if (!empty($_SESSION['ai_log_filter'])) {
    $form['filters']['actions']['reset'] = [
      '#type' => 'submit',
      '#value' => t('Reset'),
    ];
  }

  return $form;
}

/**
 * Submit handler: AI Logs filter form.
 */
function ai_log_filter_form_submit($form, &$form_state) {
  if ($form_state['values']['op'] == t('Reset')) {
    unset($_SESSION['ai_log_filter']);
  }
  else {
    $_SESSION['ai_log_filter'] = [
      'module' => $form_state['values']['module'],
      'operation' => $form_state['values']['operation'],
      'provider' => $form_state['values']['provider'],
      'status' => $form_state['values']['status'],
    ];
  }
}

/**
 * Page callback: View AI Log entry.
 */
function ai_log_view_page($log) {
  if (!$log) {
    return MENU_NOT_FOUND;
  }
  // If only the ID was passed (fallback for old menu cache)
  if (!is_object($log)) {
    $log = ai_log_load($log);
    if (!$log) {
      return MENU_NOT_FOUND;
    }
  }
  backdrop_set_title(t('AI Log entry #@id', ['@id' => $log->id]));

  $rows = [
    [['data' => t('ID'), 'header' => TRUE], $log->id],
    [['data' => t('Timestamp'), 'header' => TRUE], format_date($log->timestamp, 'long')],
    [['data' => t('User'), 'header' => TRUE], theme('username', ['account' => user_load($log->uid)])],
    [['data' => t('Module'), 'header' => TRUE], check_plain($log->module)],
    [['data' => t('Operation'), 'header' => TRUE], check_plain($log->operation)],
    [['data' => t('Model'), 'header' => TRUE], check_plain($log->model)],
    [['data' => t('Provider'), 'header' => TRUE], check_plain($log->provider)],
    [['data' => t('Status'), 'header' => TRUE], $log->status ? t('Success') : t('Failure')],
    [['data' => t('Duration'), 'header' => TRUE], number_format($log->duration, 4) . 's'],
  ];

  if (!empty($log->error_message)) {
    $rows[] = [['data' => t('Error Message'), 'header' => TRUE], check_plain($log->error_message)];
  }

  $build['details'] = [
    '#theme' => 'table',
    '#rows' => $rows,
  ];

  $build['request'] = [
    '#type' => 'fieldset',
    '#title' => t('Request Data'),
    '#collapsible' => TRUE,
    '#collapsed' => FALSE,
  ];
  $request_data = $log->request_data;
  if ($request_json = json_decode($request_data)) {
    $request_data = json_encode($request_json, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES);
  }

  $build['request']['data'] = [
    '#markup' => '<pre>' . check_plain($request_data) . '</pre>',
  ];

  $build['response'] = [
    '#type' => 'fieldset',
    '#title' => t('Response Data'),
    '#collapsible' => TRUE,
    '#collapsed' => FALSE,
  ];

  $response_data = $log->response_data;
  if ($response_json = json_decode($response_data)) {
    $response_data = json_encode($response_json, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES);
  }

  $build['response']['data'] = [
    '#markup' => '<pre>' . check_plain($response_data) . '</pre>',
  ];

  return $build;
}
