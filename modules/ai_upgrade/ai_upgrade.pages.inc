<?php

/**
 * @file
 * Admin UI for upgrading from legacy openai_* modules to ai_* modules.
 */

/**
 * Build AI upgrade form.
 */
function ai_upgrade_form($form, &$form_state) {
  $candidates = ai_upgrade_get_candidates();

  $form['intro'] = [
    '#markup' => '<p>' . t('Select AI replacement modules to enable. Each selected module will migrate matching legacy configuration and disable its openai_* counterpart.') . '</p>',
  ];

  if (empty($candidates)) {
    $form['empty'] = [
      '#markup' => '<p>' . t('No upgrade candidates detected. Either legacy OpenAI modules are already disabled, or their AI replacements are already enabled.') . '</p>',
    ];
    return $form;
  }

  $missing = ai_upgrade_find_missing_modules(array_keys($candidates));
  if (!empty($missing)) {
    $install_link = '';
    if (module_exists('installer')) {
      $install_link = ' ' . l(t('Install missing modules'), 'admin/config/ai/upgrade/install-missing');
    }
    $form['missing'] = [
      '#markup' => '<div class="messages warning"><strong>'
        . t('The following replacement modules are not present on disk. Install them before running the upgrade:')
        . '</strong><pre>' . check_plain(ai_upgrade_format_missing_modules($missing)) . '</pre>'
        . $install_link . '</div>',
    ];
  }

  $options = [];
  foreach ($candidates as $module => $candidate) {
    $status = $candidate['replacement_enabled'] ? t('enabled') : t('disabled');
    $legacy = !empty($candidate['legacy_enabled']) ? implode(', ', $candidate['legacy_enabled']) : t('none enabled');
    $on_disk = isset($missing[$module]) ? t('NOT ON DISK') : t('available');
    $options[$module] = t('@label (@module) | AI module: @status | legacy: @legacy | files: @on_disk', [
      '@label' => $candidate['label'],
      '@module' => $module,
      '@status' => $status,
      '@legacy' => $legacy,
      '@on_disk' => $on_disk,
    ]);
  }

  $form['modules'] = [
    '#type' => 'checkboxes',
    '#title' => t('Upgrade candidates'),
    '#options' => $options,
    '#default_value' => array_combine(array_keys($options), array_keys($options)),
  ];

  $form['actions'] = ['#type' => 'actions'];
  $form['actions']['submit'] = [
    '#type' => 'submit',
    '#value' => t('Run AI upgrade'),
  ];

  return $form;
}

/**
 * Confirm queuing missing AI replacement projects in Backdrop's installer.
 */
function ai_upgrade_install_missing_form($form, &$form_state) {
  $candidates = ai_upgrade_get_candidates();
  $missing = ai_upgrade_find_missing_modules(array_keys($candidates));
  if (empty($missing)) {
    backdrop_set_message(t('No missing AI replacement modules were found.'), 'warning');
    $form_state['redirect'] = 'admin/config/ai/upgrade';
    return [];
  }

  return confirm_form(
    $form,
    t('Install the missing AI replacement projects?'),
    'admin/config/ai/upgrade',
    t('This queues the missing projects in Backdrop\'s installer. You can review the selected releases before installation.'),
    t('Install missing modules'),
    t('Cancel')
  );
}

/**
 * Submit handler for ai_upgrade_install_missing_form().
 */
function ai_upgrade_install_missing_form_submit($form, &$form_state) {
  $candidates = ai_upgrade_get_candidates();
  $result = ai_upgrade_queue_missing_projects(array_keys($candidates));

  foreach ($result['errors'] as $error) {
    backdrop_set_message($error, 'error');
  }
  if (!empty($result['queued'])) {
    backdrop_set_message(t('Missing AI projects queued for installation: @projects.', [
      '@projects' => implode(', ', $result['queued']),
    ]));
    $form_state['redirect'] = 'admin/installer/install/select_versions';
    return;
  }

  $form_state['redirect'] = 'admin/config/ai/upgrade';
}

/**
 * Submit handler for AI upgrade form.
 */
function ai_upgrade_form_submit($form, &$form_state) {
  $selected = array_values(array_filter($form_state['values']['modules']));
  if (empty($selected)) {
    form_set_error('modules', t('Select at least one module to upgrade.'));
    return;
  }

  $missing = ai_upgrade_find_missing_modules($selected);
  if (!empty($missing)) {
    form_set_error('modules', t('AI upgrade aborted. Replacement modules are missing on disk:') . '<br><pre>' . check_plain(ai_upgrade_format_missing_modules($missing)) . '</pre>');
    return;
  }

  try {
    $results = ai_upgrade_execute($selected);
  }
  catch (\Throwable $e) {
    form_set_error('modules', t('AI upgrade aborted.') . '<br><pre>' . check_plain($e->getMessage()) . '</pre>');
    return;
  }
  foreach ($results as $result) {
    $message = t('@label: migrated @count config object(s). Disabled legacy modules: @legacy.', [
      '@label' => $result['label'],
      '@count' => $result['migrated'],
      '@legacy' => !empty($result['disabled']) ? implode(', ', $result['disabled']) : t('none'),
    ]);
    $message .= ' ' . t('Tables: @tables. Role permission grants: @perms. Block placements swapped: @blocks.', [
      '@tables' => ai_upgrade_table_results_summary($result['tables']),
      '@perms' => $result['permissions'],
      '@blocks' => $result['blocks'],
    ]);
    backdrop_set_message($message);
  }
}
