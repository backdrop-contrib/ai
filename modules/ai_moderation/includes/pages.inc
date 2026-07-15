<?php

/**
 * @file
 * Review queue pages for AI Moderation.
 */

/**
 * List moderation verdicts with status and score filters.
 */
function ai_moderation_queue_page($reported_only = FALSE) {
  backdrop_add_css(backdrop_get_path('module', 'ai_moderation') . '/css/ai_moderation.queue.css');
  if (!db_table_exists('ai_moderation_verdicts')) {
    return ['#markup' => '<p>' . t('Moderation verdict storage is not installed yet. Run database updates.') . '</p>'];
  }
  $header = [
    ['data' => t('Content'), 'field' => 'entity_type'],
    ['data' => t('Profile'), 'field' => 'profile'],
    ['data' => t('Score'), 'field' => 'score'],
    ['data' => t('Level')],
    ['data' => t('Verdict'), 'field' => 'verdict'],
    ['data' => t('Action'), 'field' => 'action'],
    ['data' => t('Source'), 'field' => 'source'],
    ['data' => t('Created'), 'field' => 'created', 'sort' => 'desc'],
    ['data' => t('Operations')],
  ];
  $query = db_select('ai_moderation_verdicts', 'am')->extend('PagerDefault')->extend('TableSort');
  $query->fields('am')->limit(50)->orderByHeader($header);
  if ($reported_only) {
    $query->condition('verdict', ['allow'], '<>');
  }
  if (!empty($_GET['verdict'])) {
    $query->condition('verdict', $_GET['verdict']);
  }
  if (isset($_GET['score_min']) && $_GET['score_min'] !== '') {
    $query->condition('score', (int) $_GET['score_min'], '>=');
  }
  if (isset($_GET['score_max']) && $_GET['score_max'] !== '') {
    $query->condition('score', (int) $_GET['score_max'], '<=');
  }
  $rows = [];
  foreach ($query->execute() as $record) {
    $label = check_plain($record->entity_type . ':' . $record->entity_bundle . '/' . $record->entity_id);
    $entity_info = entity_get_info($record->entity_type);
    if (!empty($entity_info)) {
      $entity = entity_load($record->entity_type, [$record->entity_id]);
      if (!empty($entity)) {
        $entity = reset($entity);
        $uri = entity_uri($record->entity_type, $entity);
        if (!empty($uri['path'])) {
          $label = l($label, $uri['path']);
        }
      }
    }
    $rows[] = [
      $label,
      check_plain($record->profile),
      $record->score === NULL ? t('Error') : (int) $record->score,
      _ai_moderation_level_markup($record),
      check_plain($record->verdict),
      check_plain($record->action),
      check_plain($record->source),
      format_date($record->created, 'short'),
      l(t('View'), 'admin/config/ai/ai-moderation/queue/' . $record->vid),
    ];
  }
  $build['filters'] = backdrop_get_form('ai_moderation_queue_filter_form');
  $build['level_legend'] = [
    '#markup' => '<div class="ai-moderation-level-legend"><strong>' . t('Level guide') . '</strong><span class="ai-moderation-level-legend__item">0–9: ' . t('Clear') . '</span><span class="ai-moderation-level-legend__item">10–20: ' . t('Needs review') . '</span><span class="ai-moderation-level-legend__item">21–70: ' . t('Smoldering') . '</span><span class="ai-moderation-level-legend__item">71–100: ' . t('Dumpster fire') . '</span><p>' . t('These are editorial severity labels. Profile allow/review thresholds separately control the moderation action.') . '</p></div>',
    '#weight' => -5,
  ];
  $build['table'] = [
    '#theme' => 'table',
    '#header' => $header,
    '#rows' => $rows,
    '#empty' => t('No moderation verdicts match these filters.'),
  ];
  $build['pager'] = ['#theme' => 'pager'];
  return $build;
}

/**
 * Filter form for the review queue.
 */
function ai_moderation_queue_filter_form($form, &$form_state) {
  $form['filters'] = ['#type' => 'fieldset', '#title' => t('Filter'), '#tree' => TRUE, '#collapsible' => TRUE, '#collapsed' => empty($_GET)];
  $form['filters']['verdict'] = [
    '#type' => 'select',
    '#title' => t('Verdict'),
    '#options' => ['' => t('- All -'), 'allow' => t('Allow'), 'review' => t('Review'), 'block' => t('Block'), 'error' => t('Error')],
    '#default_value' => $_GET['verdict'] ?? '',
  ];
  $form['filters']['score_min'] = ['#type' => 'number', '#title' => t('Minimum score'), '#min' => 0, '#max' => 100, '#default_value' => $_GET['score_min'] ?? ''];
  $form['filters']['score_max'] = ['#type' => 'number', '#title' => t('Maximum score'), '#min' => 0, '#max' => 100, '#default_value' => $_GET['score_max'] ?? ''];
  $form['filters']['actions'] = ['#type' => 'actions'];
  $form['filters']['actions']['submit'] = ['#type' => 'submit', '#value' => t('Filter')];
  $form['filters']['actions']['reset'] = ['#type' => 'submit', '#value' => t('Reset'), '#submit' => ['ai_moderation_queue_filter_form_reset'], '#limit_validation_errors' => []];
  return $form;
}

/**
 * Submit queue filters as query parameters.
 */
function ai_moderation_queue_filter_form_submit($form, &$form_state) {
  $query = array_filter($form_state['values']['filters'], function ($value) {
    return $value !== '' && !is_array($value);
  });
  $form_state['redirect'] = ['admin/config/ai/ai-moderation/queue', ['query' => $query]];
}

/**
 * Reset queue filters.
 */
function ai_moderation_queue_filter_form_reset($form, &$form_state) {
  $form_state['redirect'] = 'admin/config/ai/ai-moderation/queue';
}

/**
 * Show one moderation verdict without exposing submitted text.
 */
function ai_moderation_queue_view_page($vid) {
  backdrop_add_css(backdrop_get_path('module', 'ai_moderation') . '/css/ai_moderation.queue.css');
  $record = db_select('ai_moderation_verdicts', 'am')->fields('am')->condition('vid', (int) $vid)->execute()->fetchObject();
  if (!$record) {
    return backdrop_not_found();
  }
  backdrop_set_title(t('Moderation verdict @id', ['@id' => $record->vid]));
  $categories = json_decode($record->categories, TRUE);
  $entity_text = '';
  $entity_title = '';
  $entity_info = entity_get_info($record->entity_type);
  if (!empty($entity_info)) {
    $entities = entity_load($record->entity_type, [$record->entity_id]);
    if (!empty($entities)) {
      $entity = reset($entities);
      $entity_title = !empty($entity->title) ? $entity->title : (!empty($entity->subject) ? $entity->subject : '');
      $definitions = ai_moderation_get_entity_info();
      $extract = $definitions[$record->entity_type]['extract'] ?? '';
      if (is_callable($extract)) {
        $entity_text = call_user_func($extract, $entity);
      }
    }
  }
  $rows = [
    [t('Content'), check_plain($record->entity_type . ':' . $record->entity_bundle . '/' . $record->entity_id)],
    [t('Profile'), check_plain($record->profile)],
    [t('Score'), $record->score === NULL ? t('Error') : (int) $record->score],
    [t('Level'), _ai_moderation_level_markup($record)],
    [t('Verdict'), check_plain($record->verdict)],
    [t('Action'), check_plain($record->action)],
    [t('Categories'), check_plain(implode(', ', is_array($categories) ? $categories : []))],
    [t('Reason'), nl2br(check_plain($record->reason))],
    [t('Model'), check_plain($record->model)],
    [t('Prefilter'), $record->prefilter_flagged === NULL ? t('Not run') : ($record->prefilter_flagged ? t('Flagged') : t('Clear'))],
    [t('Source'), check_plain($record->source)],
    [t('Created'), format_date($record->created, 'long')],
  ];
  $build = [
    'table' => ['#theme' => 'table', '#rows' => $rows],
    'back' => ['#markup' => '<p>' . l(t('Back to review queue'), 'admin/config/ai/ai-moderation/queue') . '</p>'],
  ];
  if ($entity_title !== '' || $entity_text !== '') {
    $build['content'] = [
      '#type' => 'fieldset',
      '#title' => t('Reported content'),
      '#weight' => -10,
    ];
    if ($entity_title !== '') {
      $build['content']['title'] = [
        '#markup' => '<h2 class="ai-moderation-reported-content__title">' . check_plain($entity_title) . '</h2>',
      ];
    }
    $build['content']['text'] = [
      '#markup' => '<div class="ai-moderation-reported-content__text"><pre>' . check_plain($entity_text) . '</pre></div>',
    ];
  }
  return $build;
}

/**
 * Return the editorial level for a stored moderation verdict.
 *
 * This is presentation only. The stored verdict/action remains authoritative
 * for enforcement, while the level borrows the internal Dumpster Fire tone.
 */
function _ai_moderation_level_markup($record) {
  $key = 'needs-review';
  $label = t('Needs review');
  $score = $record->score === NULL ? NULL : (int) $record->score;
  if ($score !== NULL && $score > 70) {
    $key = 'dumpster-fire';
    $label = t('Dumpster fire');
  }
  elseif ($score !== NULL && $score <= 9) {
    $key = 'clear';
    $label = t('Clear');
  }
  elseif ($score !== NULL && $score <= 20) {
    $key = 'needs-review';
    $label = t('Needs review');
  }
  else {
    $key = 'smoldering';
    $label = t('Smoldering');
  }
  return '<span class="ai-moderation-level ai-moderation-level--' . $key . '"><span class="ai-moderation-level__icon" aria-hidden="true"></span><span class="ai-moderation-level__label">' . check_plain($label) . '</span></span>';
}
