<?php

/**
 * @file
 * Bee commands for the AI Upgrade module.
 */

/**
 * Implements hook_bee_command().
 */
function ai_upgrade_bee_command() {
  $items = [];

  $items['ai-upgrade'] = [
    'description' => bt('Enable AI replacement modules and migrate from legacy OpenAI modules.'),
    'bootstrap' => BEE_BOOTSTRAP_FULL,
    'arguments' => [
      'modules' => bt('Optional comma-separated list of ai_* modules to upgrade. Defaults to all actionable candidates.'),
    ],
    'optional_arguments' => ['modules'],
    'callback' => 'bee_ai_upgrade',
  ];

  return $items;
}

/**
 * Bee callback: run AI upgrade flow.
 */
function bee_ai_upgrade($arguments = [], $options = []) {
  unset($options);

  $candidates = ai_upgrade_get_candidates();
  if (empty($candidates)) {
    bee_message(bt('No AI upgrade candidates detected.'), 'warning');
    return;
  }

  if (!empty($arguments['modules'])) {
    $modules = array_filter(array_map('trim', explode(',', $arguments['modules'])));
  }
  else {
    $modules = array_keys($candidates);
  }

  $missing = ai_upgrade_find_missing_modules($modules);
  if (!empty($missing)) {
    bee_message(bt('AI upgrade aborted. Replacement modules are missing on disk:'), 'error');
    foreach (explode("\n", trim(ai_upgrade_format_missing_modules($missing))) as $line) {
      if ($line !== '') {
        bee_message($line, 'error');
      }
    }
    return;
  }

  try {
    $results = ai_upgrade_execute($modules);
  }
  catch (RuntimeException $e) {
    bee_message(bt('AI upgrade aborted.'), 'error');
    foreach (explode("\n", trim($e->getMessage())) as $line) {
      if ($line !== '') {
        bee_message($line, 'error');
      }
    }
    return;
  }
  foreach ($results as $result) {
    bee_message(bt('@label: migrated @count config object(s); disabled legacy: @legacy; tables: @tables', [
      '@label' => $result['label'],
      '@count' => $result['migrated'],
      '@legacy' => !empty($result['disabled']) ? implode(', ', $result['disabled']) : bt('none'),
      '@tables' => ai_upgrade_table_results_summary($result['tables']),
    ]));
  }
}
