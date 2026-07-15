<?php

/**
 * @file
 * Administrative configuration forms for AI Moderation.
 */

/**
 * Settings form for moderation profiles, targets, and prefilter settings.
 */
function ai_moderation_settings_form($form, &$form_state) {
  ai_moderation_load_include('evaluate');
  $config = config('ai_moderation.settings');
  $form['#validate'][] = 'ai_moderation_settings_form_validate';
  $profiles = $config->get('profiles') ?: [];
  if (empty($profiles)) {
    $profiles = ['default' => _ai_moderation_profile('default', [])];
  }
  $models = ai_get_all_chat_models();
  $model_options = ['' => t('- Use default chat model -')] + $models;

  $form['profiles'] = [
    '#type' => 'fieldset',
    '#title' => t('Moderation profiles'),
    '#tree' => TRUE,
    '#description' => t('Profiles describe the community policy, score bands, and enforcement behavior. New profiles are shadow-mode by default. Editorial levels are Clear (0–9), Needs review (10–20), Smoldering (21–70), and Dumpster fire (71–100). These labels are separate from the configurable allow/review/block action thresholds.'),
  ];
  foreach ($profiles as $id => $profile) {
    $profiles[$id] = $profile = _ai_moderation_profile($id, $profile);
    $form['profiles'][$id] = [
      '#type' => 'fieldset',
      '#title' => check_plain($profile['label']) . ' (' . check_plain($id) . ')',
      '#collapsible' => TRUE,
      '#collapsed' => $id !== 'default',
    ];
    $form['profiles'][$id]['label'] = [
      '#type' => 'textfield',
      '#title' => t('Label'),
      '#default_value' => $profile['label'],
      '#required' => TRUE,
    ];
    $form['profiles'][$id]['prompt'] = [
      '#type' => 'textarea',
      '#title' => t('Community policy prompt'),
      '#description' => t('Describe what this community allows and what should be flagged. Submitted text is treated as untrusted data, not instructions.'),
      '#default_value' => $profile['prompt'],
      '#rows' => 5,
      '#required' => TRUE,
    ];
    $form['profiles'][$id]['model'] = [
      '#type' => 'select',
      '#title' => t('Model'),
      '#options' => $model_options,
      '#default_value' => $profile['model'],
      '#disabled' => empty($models),
      '#description' => empty($models) ? t('Enable a chat-capable AI provider before using moderation.') : t('Leave default to use the global moderation/default chat model.'),
    ];
    $form['profiles'][$id]['bands'] = [
      '#type' => 'container',
      '#attributes' => ['class' => ['container-inline']],
    ];
    $form['profiles'][$id]['bands']['band_allow_max'] = [
      '#type' => 'number',
      '#title' => t('Allow through score'),
      '#default_value' => $profile['band_allow_max'],
      '#min' => 0,
      '#max' => 100,
      '#required' => TRUE,
      '#description' => t('Moderation action threshold: scores through this value are allowed.'),
    ];
    $form['profiles'][$id]['bands']['band_review_max'] = [
      '#type' => 'number',
      '#title' => t('Review through score'),
      '#default_value' => $profile['band_review_max'],
      '#min' => 0,
      '#max' => 100,
      '#required' => TRUE,
      '#description' => t('Moderation action threshold: scores through this value are reviewed; higher scores are blocked when enforcement is enabled.'),
    ];
    $form['profiles'][$id]['on_failure'] = [
      '#type' => 'select',
      '#title' => t('On provider failure'),
      '#options' => ['review' => t('Queue for review'), 'allow' => t('Allow'), 'block' => t('Block')],
      '#default_value' => $profile['on_failure'],
      '#description' => t('Default review may briefly expose spam. Block can reject legitimate submissions during an outage.'),
    ];
    $form['profiles'][$id]['enforce'] = [
      '#type' => 'select',
      '#title' => t('Enforcement'),
      '#options' => ['log_only' => t('Log only (shadow mode)'), 'enforce' => t('Enforce block verdicts')],
      '#default_value' => $profile['enforce'],
    ];
    $form['profiles'][$id]['site_context'] = [
      '#type' => 'select',
      '#title' => t('Site context'),
      '#options' => ['off' => t('Off'), 'respect_config' => t('Respect site-context configuration')],
      '#default_value' => $profile['site_context'],
      '#description' => t('Off is recommended: a conversational persona can cause the classifier to refuse rather than classify harmful text.'),
    ];
  }
  $form['profiles']['add_profile'] = [
    '#type' => 'submit',
    '#value' => t('Add new profile'),
    '#submit' => ['ai_moderation_settings_form_show_profile_submit'],
    '#limit_validation_errors' => [],
  ];

  if (!empty($form_state['storage']['adding_profile'])) {
    $new_profile = [
      'label' => t('New moderation profile'),
      'prompt' => t('This is a community forum. Flag spam, scams, harassment, threats, explicit material, or content that is unsuitable for the community.'),
      'model' => '',
      'band_allow_max' => 30,
      'band_review_max' => 70,
      'on_failure' => 'review',
      'enforce' => 'log_only',
      'site_context' => 'off',
    ];
    $form['new_profile'] = [
      '#type' => 'fieldset',
      '#title' => t('New moderation profile'),
      '#tree' => TRUE,
    ];
    $form['new_profile']['machine_name'] = [
      '#type' => 'textfield',
      '#title' => t('Machine name'),
      '#description' => t('Use lowercase letters, numbers, and underscores.'),
      '#maxlength' => 64,
      '#required' => TRUE,
    ];
    $form['new_profile']['label'] = [
      '#type' => 'textfield',
      '#title' => t('Label'),
      '#default_value' => $new_profile['label'],
      '#required' => TRUE,
    ];
    $form['new_profile']['prompt'] = [
      '#type' => 'textarea',
      '#title' => t('Community policy prompt'),
      '#default_value' => $new_profile['prompt'],
      '#rows' => 5,
      '#required' => TRUE,
    ];
    $form['new_profile']['model'] = [
      '#type' => 'select',
      '#title' => t('Model'),
      '#options' => $model_options,
      '#default_value' => $new_profile['model'],
      '#disabled' => empty($models),
    ];
    $form['new_profile']['band_allow_max'] = [
      '#type' => 'number',
      '#title' => t('Allow through score'),
      '#default_value' => $new_profile['band_allow_max'],
      '#min' => 0,
      '#max' => 100,
      '#required' => TRUE,
    ];
    $form['new_profile']['band_review_max'] = [
      '#type' => 'number',
      '#title' => t('Review through score'),
      '#default_value' => $new_profile['band_review_max'],
      '#min' => 0,
      '#max' => 100,
      '#required' => TRUE,
    ];
    $form['new_profile']['on_failure'] = [
      '#type' => 'select',
      '#title' => t('On provider failure'),
      '#options' => ['review' => t('Queue for review'), 'allow' => t('Allow'), 'block' => t('Block')],
      '#default_value' => $new_profile['on_failure'],
    ];
    $form['new_profile']['enforce'] = [
      '#type' => 'select',
      '#title' => t('Enforcement'),
      '#options' => ['log_only' => t('Log only (shadow mode)'), 'enforce' => t('Enforce block verdicts')],
      '#default_value' => $new_profile['enforce'],
    ];
    $form['new_profile']['site_context'] = [
      '#type' => 'select',
      '#title' => t('Site context'),
      '#options' => ['off' => t('Off'), 'respect_config' => t('Respect site-context configuration')],
      '#default_value' => $new_profile['site_context'],
    ];
    $form['new_profile']['actions'] = ['#type' => 'actions'];
    $form['new_profile']['actions']['save'] = [
      '#type' => 'submit',
      '#value' => t('Save profile'),
      '#submit' => ['ai_moderation_settings_form_save_profile_submit'],
      '#validate' => ['ai_moderation_settings_form_save_profile_validate'],
      '#limit_validation_errors' => [['new_profile']],
    ];
    $form['new_profile']['actions']['cancel'] = [
      '#type' => 'submit',
      '#value' => t('Cancel'),
      '#submit' => ['ai_moderation_settings_form_cancel_profile_submit'],
      '#limit_validation_errors' => [],
    ];
  }

  $form['targets'] = [
    '#type' => 'fieldset',
    '#title' => t('Content targets'),
    '#tree' => TRUE,
    '#description' => t('Choose a profile for each entity bundle. Only selected targets run synchronously on save.'),
  ];
  $target_settings = $config->get('targets') ?: [];
  $profile_options = ['' => t('- Do not moderate -')];
  foreach ($profiles as $id => $profile) {
    $profile_options[$id] = $profile['label'];
  }
  foreach (ai_moderation_get_entity_info() as $entity_type => $definition) {
    if (empty($definition['bundles'])) {
      continue;
    }
    $form['targets'][$entity_type] = [
      '#type' => 'fieldset',
      '#title' => check_plain($definition['label']),
      '#collapsible' => TRUE,
      '#collapsed' => TRUE,
    ];
    foreach ($definition['bundles'] as $bundle => $label) {
      $form['targets'][$entity_type][$bundle] = [
        '#type' => 'select',
        '#title' => check_plain($label),
        '#options' => $profile_options,
        '#default_value' => $target_settings[$entity_type][$bundle] ?? '',
      ];
    }
  }

  $form['prefilter'] = [
    '#type' => 'fieldset',
    '#title' => t('Optional provider moderation prefilter'),
    '#tree' => TRUE,
    '#collapsible' => TRUE,
    '#collapsed' => TRUE,
    '#description' => t('This optional provider moderation prefilter can only escalate high-risk submissions. It never allows a submission by itself.'),
  ];
  $prefilter = $config->get('prefilter') ?: [];
  $moderation_models = ai_get_all_moderation_models();
  $form['prefilter']['enabled'] = [
    '#type' => 'checkbox',
    '#title' => t('Enable prefilter'),
    '#default_value' => !empty($prefilter['enabled']),
    '#disabled' => empty($moderation_models),
    '#description' => empty($moderation_models) ? t('Requires an enabled provider that advertises moderation support.') : t('Unavailable or failed prefilter calls fall back to chat evaluation.'),
  ];
  $form['prefilter']['model'] = [
    '#type' => 'select',
    '#title' => t('Moderation model'),
    '#options' => ['' => t('- Automatic -')] + $moderation_models,
    '#default_value' => $prefilter['model'] ?? '',
    '#disabled' => empty($moderation_models),
  ];
  $form['prefilter']['escalate_score'] = [
    '#type' => 'number',
    '#title' => t('Escalate score'),
    '#default_value' => $prefilter['escalate_score'] ?? 95,
    '#min' => 0,
    '#max' => 100,
    '#required' => TRUE,
  ];

  $form['advanced'] = [
    '#type' => 'fieldset',
    '#title' => t('Advanced'),
    '#tree' => TRUE,
    '#collapsible' => TRUE,
    '#collapsed' => TRUE,
  ];
  $form['advanced']['default_model'] = [
    '#type' => 'select',
    '#title' => t('Default model'),
    '#options' => $model_options,
    '#default_value' => $config->get('default_model') ?: '',
    '#disabled' => empty($models),
  ];
  $form['advanced']['log_all'] = [
    '#type' => 'checkbox',
    '#title' => t('Store allow verdicts too'),
    '#default_value' => $config->get('log_all') !== FALSE,
  ];

  $form['preview'] = [
    '#type' => 'fieldset',
    '#title' => t('Test a profile'),
    '#tree' => TRUE,
    '#collapsible' => TRUE,
    '#collapsed' => TRUE,
    '#description' => t('Tests saved profile settings. This is not a provider endpoint explorer.'),
  ];
  $form['preview']['profile'] = [
    '#type' => 'select',
    '#title' => t('Profile'),
    '#options' => array_slice($profile_options, 1, NULL, TRUE),
    '#default_value' => 'default',
  ];
  $form['preview']['text'] = [
    '#type' => 'textarea',
    '#title' => t('Sample text'),
    '#rows' => 4,
  ];
  $form['preview']['test'] = [
    '#type' => 'submit',
    '#value' => t('Test profile'),
    '#submit' => ['ai_moderation_settings_form_test_submit'],
    '#limit_validation_errors' => [['preview']],
  ];
  if (!empty($form_state['storage']['preview_result'])) {
    $result = $form_state['storage']['preview_result'];
    $form['preview']['result'] = [
      '#markup' => '<pre>' . check_plain(json_encode($result, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES)) . '</pre>',
    ];
  }

  $form['actions'] = ['#type' => 'actions'];
  $form['actions']['submit'] = [
    '#type' => 'submit',
    '#value' => t('Save configuration'),
    '#submit' => ['ai_moderation_settings_form_submit'],
  ];
  return $form;
}

/**
 * Validate profile score bands and profile machine name.
 */
function ai_moderation_settings_form_validate($form, &$form_state) {
  foreach (($form_state['values']['profiles'] ?? []) as $id => $profile) {
    if (!is_array($profile)) {
      continue;
    }
    $allow = (int) ($profile['bands']['band_allow_max'] ?? 0);
    $review = (int) ($profile['bands']['band_review_max'] ?? 0);
    if ($allow > $review) {
      form_set_error('profiles][' . $id . '][bands][band_review_max', t('Review score must be greater than or equal to allow score.'));
    }
  }
}

/**
 * Persist settings in one config object.
 */
function ai_moderation_settings_form_submit($form, &$form_state) {
  $values = $form_state['values'];
  $profiles = [];
  foreach ($values['profiles'] as $id => $profile) {
    if (in_array($id, ['add_profile', 'new_profile_id'], TRUE) || !is_array($profile)) {
      continue;
    }
    $profiles[$id] = [
      'label' => trim($profile['label']),
      'prompt' => trim($profile['prompt']),
      'model' => $profile['model'],
      'band_allow_max' => (int) $profile['bands']['band_allow_max'],
      'band_review_max' => (int) $profile['bands']['band_review_max'],
      'on_failure' => $profile['on_failure'],
      'enforce' => $profile['enforce'],
      'site_context' => $profile['site_context'],
    ];
  }
  $targets = [];
  foreach (($values['targets'] ?? []) as $entity_type => $bundles) {
    foreach ($bundles as $bundle => $profile_id) {
      if ($profile_id !== '') {
        $targets[$entity_type][$bundle] = $profile_id;
      }
    }
  }
  $config = config('ai_moderation.settings');
  $config->set('profiles', $profiles);
  $config->set('targets', $targets);
  $config->set('prefilter', [
    'enabled' => !empty($values['prefilter']['enabled']),
    'model' => $values['prefilter']['model'],
    'escalate_score' => (int) $values['prefilter']['escalate_score'],
  ]);
  $config->set('default_model', $values['advanced']['default_model']);
  $config->set('temperature', 0);
  $config->set('log_all', !empty($values['advanced']['log_all']));
  $config->save();
  backdrop_set_message(t('AI Moderation configuration saved.'));
}

/**
 * Reveal the dedicated new-profile form.
 */
function ai_moderation_settings_form_show_profile_submit($form, &$form_state) {
  $form_state['storage']['adding_profile'] = TRUE;
  $form_state['rebuild'] = TRUE;
}

/**
 * Save one new profile from its dedicated form.
 */
function ai_moderation_settings_form_save_profile_submit($form, &$form_state) {
  $values = $form_state['values']['new_profile'];
  $new = trim($values['machine_name'] ?? '');
  $config = config('ai_moderation.settings');
  $profiles = $config->get('profiles') ?: [];
  if (empty($profiles)) {
    $profiles = ['default' => _ai_moderation_default_profile()];
  }
  $profiles[$new] = [
    'label' => trim($values['label']),
    'prompt' => trim($values['prompt']),
    'model' => $values['model'],
    'band_allow_max' => (int) $values['band_allow_max'],
    'band_review_max' => (int) $values['band_review_max'],
    'on_failure' => $values['on_failure'],
    'enforce' => $values['enforce'],
    'site_context' => $values['site_context'],
  ];
  $config->set('profiles', $profiles)->save();
  backdrop_set_message(t('Profile @profile was saved.', ['@profile' => $new]));
  $form_state['storage']['adding_profile'] = FALSE;
  $form_state['rebuild'] = TRUE;
}

/**
 * Validate the dedicated new-profile form before its submit handler runs.
 */
function ai_moderation_settings_form_save_profile_validate($form, &$form_state) {
  $values = $form_state['values']['new_profile'] ?? [];
  $new = trim($values['machine_name'] ?? '');
  if ($new === '' || in_array($new, ['add_profile', 'new_profile_id'], TRUE) || !preg_match('/^[a-z][a-z0-9_]*$/', $new)) {
    form_set_error('new_profile][machine_name', t('Profile machine name must start with a letter and contain only lowercase letters, numbers, and underscores.'));
  }
  $profiles = config('ai_moderation.settings')->get('profiles') ?: [];
  if (isset($profiles[$new])) {
    form_set_error('new_profile][machine_name', t('That profile already exists.'));
  }
  if ((int) ($values['band_allow_max'] ?? 0) > (int) ($values['band_review_max'] ?? 0)) {
    form_set_error('new_profile][band_review_max', t('Review score must be greater than or equal to allow score.'));
  }
}

/**
 * Close the new-profile form without saving it.
 */
function ai_moderation_settings_form_cancel_profile_submit($form, &$form_state) {
  $form_state['storage']['adding_profile'] = FALSE;
  $form_state['rebuild'] = TRUE;
}

/**
 * Evaluate sample text through an existing saved profile.
 */
function ai_moderation_settings_form_test_submit($form, &$form_state) {
  $text = trim($form_state['values']['preview']['text'] ?? '');
  if ($text === '') {
    form_set_error('preview][text', t('Enter sample text to test this profile.'));
    return;
  }
  $form_state['storage']['preview_result'] = ai_moderation_evaluate($text, [
    'profile' => $form_state['values']['preview']['profile'],
  ]);
  $form_state['rebuild'] = TRUE;
}
