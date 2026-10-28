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
        . t('The following replacement modules are not present on disk. The upgrade skips them, and any module sharing their legacy modules, until they are installed:')
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
      '@module' => !empty($candidate['deprecated']) ? t('none') : $candidate['module'],
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
 * Confirm downloading missing AI replacement projects.
 */
function ai_upgrade_install_missing_form($form, &$form_state) {
  $candidates = ai_upgrade_get_candidates();
  $missing = ai_upgrade_find_missing_modules(array_keys($candidates));
  if (empty($missing)) {
    backdrop_set_message(t('No missing AI replacement modules were found.'), 'warning');
    $form_state['redirect'] = 'admin/config/ai/upgrade';
    return [];
  }

  // Show exactly which release, and how stable it is, before downloading.
  $items = [];
  $unstable = FALSE;
  foreach (array_keys(ai_upgrade_get_missing_projects(array_keys($candidates))) as $project_name) {
    $release = ai_upgrade_get_project_release($project_name);
    if (!$release) {
      $items[] = t('@project: no published release, install it manually', ['@project' => $project_name]);
      continue;
    }
    $items[] = t('@project @version (@stability)', [
      '@project' => $project_name,
      '@version' => $release['version'],
      '@stability' => ai_upgrade_release_stability($release),
    ]);
    if (!empty($release['version_extra'])) {
      $unstable = TRUE;
    }
  }
  $description = theme('item_list', ['items' => $items]);
  if ($unstable) {
    $description .= '<p><strong>' . t('Some of these are not stable releases. Alpha, beta, release candidate and development versions may change or have bugs; test on a copy of the site first.') . '</strong></p>';
  }
  $description .= '<p>' . t('This downloads the newest release of each missing project from backdropcms.org into the modules directory, along with any modules they depend on that are missing. Nothing is enabled until you run the upgrade.') . '</p>';

  return confirm_form(
    $form,
    t('Download the missing AI replacement projects?'),
    'admin/config/ai/upgrade',
    $description,
    t('Install missing modules'),
    t('Cancel')
  );
}

/**
 * Submit handler for ai_upgrade_install_missing_form().
 */
function ai_upgrade_install_missing_form_submit($form, &$form_state) {
  $candidates = ai_upgrade_get_candidates();
  $result = ai_upgrade_download_missing_projects(array_keys($candidates));

  foreach ($result['errors'] as $error) {
    backdrop_set_message($error, 'error');
  }
  if (!empty($result['downloaded'])) {
    backdrop_set_message(t('Downloaded missing AI projects: @projects.', [
      '@projects' => implode(', ', $result['downloaded']),
    ]));
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

  try {
    $plan = ai_upgrade_plan($selected);
  }
  catch (\Throwable $e) {
    form_set_error('modules', t('AI upgrade aborted.') . '<br><pre>' . check_plain($e->getMessage()) . '</pre>');
    return;
  }

  $form_state['redirect'] = 'admin/config/ai/upgrade';
  if (empty($plan['run'])) {
    ai_upgrade_batch_finished(TRUE, $plan['skipped'], []);
    return;
  }

  // Run one module per batch request so a large upgrade cannot time out.
  $operations = [];
  foreach ($plan['run'] as $module) {
    $operations[] = ['ai_upgrade_batch_module', [$module]];
  }
  batch_set([
    'title' => t('Running AI upgrade'),
    'operations' => $operations,
    'finished' => 'ai_upgrade_batch_finished',
    'file' => backdrop_get_path('module', 'ai_upgrade') . '/ai_upgrade.pages.inc',
  ]);
  // Seed the results so skipped modules are reported alongside the rest.
  $batch = &batch_get();
  $batch['sets'][count($batch['sets']) - 1]['results'] = $plan['skipped'];
}

/**
 * Batch operation: upgrade one AI module.
 */
function ai_upgrade_batch_module($module, &$context) {
  module_load_install('ai_upgrade');
  $info = ai_upgrade_module_info($module);
  $label = !empty($info['label']) ? $info['label'] : $module;
  try {
    $result = ai_upgrade_execute_module($module);
  }
  catch (\Throwable $e) {
    $result = [
      'module' => $module,
      'label' => $label,
      'skipped' => t('Upgrade failed: @message', ['@message' => $e->getMessage()]),
    ];
  }
  if ($result) {
    $context['results'][$module] = $result;
  }
  $context['message'] = t('Upgraded @label.', ['@label' => $label]);
}

/**
 * Batch finished callback: report each module's upgrade result.
 */
function ai_upgrade_batch_finished($success, $results, $operations) {
  module_load_install('ai_upgrade');
  if (!$success) {
    backdrop_set_message(t('The AI upgrade did not finish. Rerun it to complete the remaining modules.'), 'error');
  }
  foreach ($results as $result) {
    if (!empty($result['skipped'])) {
      backdrop_set_message(t('@label: skipped.', ['@label' => $result['label']]) . '<pre>' . check_plain($result['skipped']) . '</pre>', 'warning');
      continue;
    }
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
