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
  $filters = _ai_moderation_normalize_queue_filters($_GET);
  if ($reported_only) {
    $query->condition('verdict', 'allow', '<>');
  }
  if ($filters['verdict'] !== '') {
    $query->condition('verdict', $filters['verdict']);
  }
  if ($filters['score_min'] !== '') {
    $query->condition('score', $filters['score_min'], '>=');
  }
  if ($filters['score_max'] !== '') {
    $query->condition('score', $filters['score_max'], '<=');
  }
  $records = [];
  $entity_ids = [];
  foreach ($query->execute() as $record) {
    $records[] = $record;
    $entity_ids[$record->entity_type][(string) $record->entity_id] = $record->entity_id;
  }
  $loaded_entities = [];
  foreach ($entity_ids as $entity_type => $ids) {
    if (!empty(entity_get_info($entity_type))) {
      $loaded_entities[$entity_type] = entity_load($entity_type, array_values($ids));
    }
  }
  $rows = [];
  foreach ($records as $record) {
    $label = check_plain($record->entity_type . ':' . $record->entity_bundle . '/' . $record->entity_id);
    $entity = $loaded_entities[$record->entity_type][$record->entity_id] ?? NULL;
    if (!empty($entity)) {
      $uri = entity_uri($record->entity_type, $entity);
      if (!empty($uri['path'])) {
        $label = l($label, $uri['path']);
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
  $ranges = ai_moderation_level_ranges();
  $build['level_legend'] = [
    '#markup' => '<div class="ai-moderation-level-legend"><strong>' . t('Level guide') . '</strong><span class="ai-moderation-level-legend__item">' . $ranges['clear'] . ': ' . t('Clear') . '</span><span class="ai-moderation-level-legend__item">' . $ranges['review'] . ': ' . t('Needs review') . '</span><span class="ai-moderation-level-legend__item">' . $ranges['smoldering'] . ': ' . t('Smoldering') . '</span><span class="ai-moderation-level-legend__item">' . $ranges['dumpster_fire'] . ': ' . t('Dumpster fire') . '</span><p>' . t('These are editorial severity labels. Profile allow/review thresholds separately control the moderation action.') . '</p></div>',
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
  $filters = _ai_moderation_normalize_queue_filters($_GET);
  $has_filters = $filters['verdict'] !== '' || $filters['score_min'] !== '' || $filters['score_max'] !== '';
  $form['#queue_path'] = current_path();
  $form['filters'] = ['#type' => 'fieldset', '#title' => t('Filter'), '#tree' => TRUE, '#collapsible' => TRUE, '#collapsed' => !$has_filters];
  $form['filters']['verdict'] = [
    '#type' => 'select',
    '#title' => t('Verdict'),
    '#options' => ['' => t('- All -'), 'allow' => t('Allow'), 'review' => t('Review'), 'block' => t('Block'), 'error' => t('Error')],
    '#default_value' => $filters['verdict'],
  ];
  $form['filters']['score_min'] = ['#type' => 'number', '#title' => t('Minimum score'), '#min' => 0, '#max' => 100, '#default_value' => $filters['score_min']];
  $form['filters']['score_max'] = ['#type' => 'number', '#title' => t('Maximum score'), '#min' => 0, '#max' => 100, '#default_value' => $filters['score_max']];
  $form['filters']['actions'] = ['#type' => 'actions'];
  $form['filters']['actions']['submit'] = ['#type' => 'submit', '#value' => t('Filter')];
  $form['filters']['actions']['reset'] = ['#type' => 'submit', '#value' => t('Reset'), '#submit' => ['ai_moderation_queue_filter_form_reset'], '#limit_validation_errors' => []];
  return $form;
}

/**
 * Submit queue filters as query parameters.
 */
function ai_moderation_queue_filter_form_submit($form, &$form_state) {
  $values = (array) ($form_state['values']['filters'] ?? []);
  $query = array_filter(_ai_moderation_normalize_queue_filters($values), function ($value) {
    return $value !== '';
  });
  $form_state['redirect'] = [$form['#queue_path'], ['query' => $query]];
}

/**
 * Normalize supported queue filters from request or form input.
 */
function _ai_moderation_normalize_queue_filters(array $input) {
  $filters = ['verdict' => '', 'score_min' => '', 'score_max' => ''];
  if (isset($input['verdict']) && is_string($input['verdict']) && in_array($input['verdict'], ['allow', 'review', 'block', 'error'], TRUE)) {
    $filters['verdict'] = $input['verdict'];
  }
  foreach (['score_min', 'score_max'] as $key) {
    $value = $input[$key] ?? '';
    if (is_int($value) && $value >= 0 && $value <= 100) {
      $filters[$key] = $value;
    }
    elseif (is_string($value) && preg_match('/^(?:0|[1-9][0-9]?|100)$/D', $value)) {
      $filters[$key] = (int) $value;
    }
  }
  return $filters;
}

/**
 * Reset queue filters.
 */
function ai_moderation_queue_filter_form_reset($form, &$form_state) {
  $form_state['redirect'] = $form['#queue_path'];
}

/**
 * Show a verdict without retaining submitted text in the verdict record.
 *
 * Best-effort reconstruct content from the live or versioned entity for review,
 * and identify current content that does not match the evaluated content.
 */
function ai_moderation_queue_view_page($vid) {
  backdrop_add_css(backdrop_get_path('module', 'ai_moderation') . '/css/ai_moderation.queue.css');
  if (!db_table_exists('ai_moderation_verdicts')) {
    return ['#markup' => '<p>' . t('Moderation verdict storage is not installed yet. Run database updates.') . '</p>'];
  }
  $record = db_select('ai_moderation_verdicts', 'am')->fields('am')->condition('vid', (int) $vid)->execute()->fetchObject();
  if (!$record) {
    return backdrop_not_found();
  }
  backdrop_set_title(t('Moderation verdict @id', ['@id' => $record->vid]));
  $categories = _ai_moderation_format_categories(json_decode($record->categories, TRUE));
  $entity_text = '';
  $entity_text_label = '';
  $entity_title = '';
  $entity_info = entity_get_info($record->entity_type);
  if (!empty($entity_info)) {
    $loaded_revision = FALSE;
    $revision_key = $entity_info['entity keys']['revision'] ?? '';
    $attempted_revision = !empty($record->entity_revision) && $revision_key !== '';
    if ($attempted_revision) {
      $entities = entity_load($record->entity_type, [$record->entity_id], [$revision_key => $record->entity_revision]);
      $loaded_revision = !empty($entities);
    }
    else {
      $entities = entity_load($record->entity_type, [$record->entity_id]);
    }
    if ($attempted_revision && empty($entities)) {
      $entities = entity_load($record->entity_type, [$record->entity_id]);
    }
    if (!empty($entities)) {
      $entity = reset($entities);
      $entity_title = !empty($entity->title) ? $entity->title : (!empty($entity->subject) ? $entity->subject : '');
      $definitions = ai_moderation_get_entity_info();
      $extract = $definitions[$record->entity_type]['extract'] ?? '';
      if (is_callable($extract)) {
        $entity_text = call_user_func($extract, $entity);
        if (!empty($record->content_hash) && hash('sha256', $entity_text) !== $record->content_hash) {
          $entity_text_label = t('Current content (does not match evaluated content)');
        }
        else {
          $entity_text_label = $loaded_revision ? t('Evaluated content') : t('Current content');
        }
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
    [t('Categories'), check_plain($categories)],
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
      '#markup' => '<h3>' . check_plain($entity_text_label) . '</h3><div class="ai-moderation-reported-content__text"><pre>' . check_plain($entity_text) . '</pre></div>',
    ];
  }
  return $build;
}

/**
 * Format stored category lists and provider category maps for display.
 */
function _ai_moderation_format_categories($categories) {
  if (!is_array($categories) || empty($categories)) {
    return '';
  }
  $labels = [];
  foreach ($categories as $category => $value) {
    if (is_int($category)) {
      if (!is_string($value) || trim($value) === '') {
        continue;
      }
      $labels[] = $value;
    }
    else {
      if (!is_string($category) || $category === '' || !is_bool($value)) {
        continue;
      }
      if ($value) {
        $labels[] = $category;
      }
    }
  }
  return implode(', ', $labels);
}

/**
 * Return the editorial level for a stored moderation verdict.
 *
 * This is presentation only. The stored verdict/action remains authoritative
 * for enforcement, while the level borrows the internal Dumpster Fire tone.
 */
function _ai_moderation_level_markup($record) {
  $thresholds = ai_moderation_level_thresholds();
  $key = 'needs-review';
  $label = t('Needs review');
  $score = $record->score === NULL ? NULL : (int) $record->score;
  if ($score === NULL) {
    $key = 'error';
    $label = t('Error');
  }
  elseif ($score > $thresholds['smoldering']) {
    $key = 'dumpster-fire';
    $label = t('Dumpster fire');
  }
  elseif ($score !== NULL && $score <= $thresholds['clear']) {
    $key = 'clear';
    $label = t('Clear');
  }
  elseif ($score !== NULL && $score <= $thresholds['review']) {
    $key = 'needs-review';
    $label = t('Needs review');
  }
  else {
    $key = 'smoldering';
    $label = t('Smoldering');
  }
  return '<span class="ai-moderation-level ai-moderation-level--' . $key . '"><span class="ai-moderation-level__icon" aria-hidden="true"></span><span class="ai-moderation-level__label">' . check_plain($label) . '</span></span>';
}
