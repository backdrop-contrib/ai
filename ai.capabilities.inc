<?php

/**
 * @file
 * Model capability admin: what each provider reports, and per-site overrides.
 *
 * Providers disagree about capability metadata -- some return a typed catalog,
 * some compatible gateways return an untyped catalog. This UI shows the
 * auto-detected state for every enabled provider and allows manual assignments.
 */

/**
 * Title callback for the per-provider capability form.
 */
function ai_capabilities_page_title($provider_id) {
  return t('@provider model capabilities', ['@provider' => ai_get_provider_label($provider_id)]);
}

/**
 * Page callback: capability overview for all providers.
 */
function ai_capabilities_overview_page() {
  backdrop_add_css(backdrop_get_path('module', 'ai') . '/css/ai.admin-overview.css');

  $providers = ai_get_providers(TRUE);
  $providers_root = config_get('ai.settings', 'providers') ?: [];
  $capability_labels = ai_capability_labels();

  $build = [];
  $build['intro'] = [
    '#type' => 'markup',
    '#markup' => '<p>' . t('Each cell shows how many models are offered for that capability. <em>Auto</em> uses API metadata where available and provider fallback rules otherwise. Use Configure to assign models manually when detection is missing or incorrect. Overrides take precedence in capability-specific selectors, including the <a href="@defaults">default model</a> selectors.', [
      '@defaults' => url('admin/config/ai/settings/defaults'),
    ]) . '</p>',
  ];

  $header = [t('Provider'), t('Models')];
  foreach ($capability_labels as $label) {
    $header[] = ['data' => $label, 'class' => ['ai-capability-col']];
  }
  $header[] = t('Operations');

  $rows = [];
  $any_override = FALSE;

  foreach ($providers as $provider_id => $provider_info) {
    $enabled = !empty($providers_root[$provider_id]['enabled']);
    if (!$enabled) {
      continue;
    }

    $models = ai_get_provider_models($provider_id);
    $auto = ai_get_provider_auto_capability_models($provider_id);
    $overrides = ai_get_provider_capability_overrides($provider_id);

    $row = [
      ['data' => check_plain($provider_info['label']), 'class' => ['ai-capability-provider']],
      count($models),
    ];

    foreach (array_keys($capability_labels) as $capability) {
      if (isset($overrides[$capability])) {
        $any_override = TRUE;
        $auto_count = isset($auto[$capability]) ? count($auto[$capability]) : 0;
        $cell = '<span class="ai-capability-status ai-capability-status--overridden">'
          . t('@count overridden', ['@count' => $overrides[$capability]])
          . '</span><br /><small>' . t('auto: @count', ['@count' => $auto_count]) . '</small>';
      }
      else {
        $auto_count = isset($auto[$capability]) ? count($auto[$capability]) : 0;
        $cell = '<span class="ai-capability-status ai-capability-status--auto">'
          . t('@count auto', ['@count' => $auto_count]) . '</span>';
      }
      $row[] = ['data' => $cell, 'class' => ['ai-capability-cell']];
    }

    $row[] = l(t('Configure'), 'admin/config/ai/settings/capabilities/' . $provider_id);
    $rows[] = $row;
  }

  if (empty($rows)) {
    $build['empty'] = [
      '#type' => 'markup',
      '#markup' => '<p>' . t('No providers are enabled yet. Enable one on the <a href="@url">AI settings page</a>.', [
        '@url' => url('admin/config/ai/settings'),
      ]) . '</p>',
    ];
    return $build;
  }

  if ($any_override) {
    $build['notice'] = [
      '#type' => 'markup',
      '#markup' => '<div class="messages warning">' . t('Some capabilities are overridden on this site. Models excluded by an override do not appear in any model selector, even if the provider still offers them.') . '</div>',
    ];
  }

  $build['table'] = [
    '#type' => 'markup',
    '#markup' => theme('table', [
      'header' => $header,
      'rows' => $rows,
      'attributes' => ['class' => ['ai-capability-overview']],
    ]),
  ];

  return $build;
}

/**
 * Form builder: capability matrix for one provider.
 */
function ai_capabilities_form($form, &$form_state, $provider_id) {
  $providers = ai_get_providers(TRUE);
  if (!isset($providers[$provider_id])) {
    backdrop_not_found();
    backdrop_exit();
  }

  $form['#attached']['css'][] = backdrop_get_path('module', 'ai') . '/css/ai.admin-overview.css';
  $form['#provider_id'] = $provider_id;

  $providers_root = config_get('ai.settings', 'providers') ?: [];
  $enabled = !empty($providers_root[$provider_id]['enabled']);
  $defaults = isset($providers_root[$provider_id]['manual_capability_models']) && is_array($providers_root[$provider_id]['manual_capability_models'])
    ? $providers_root[$provider_id]['manual_capability_models']
    : [];

  if (!$enabled) {
    $form['disabled'] = [
      '#type' => 'markup',
      '#markup' => '<div class="messages warning">' . t('@provider is not enabled. Enable it on the <a href="@url">AI settings page</a> to fetch its models.', [
        '@provider' => $providers[$provider_id]['label'],
        '@url' => url('admin/config/ai/settings'),
      ]) . '</div>',
    ];
  }

  $models = $enabled ? ai_get_provider_models($provider_id) : [];
  $auto = $enabled ? ai_get_provider_auto_capability_models($provider_id) : [];

  $summary = ai_capability_override_summary($provider_id);
  if ($summary !== '') {
    $form['override_notice'] = [
      '#type' => 'markup',
      '#markup' => '<div class="messages warning">' . t('Overrides are active for: @summary. Everything else falls back to what @provider reports.', [
        '@summary' => $summary,
        '@provider' => $providers[$provider_id]['label'],
      ]) . '</div>',
    ];
  }

  $matrix_options = [
    'title' => t('@provider models', ['@provider' => $providers[$provider_id]['label']]),
    'description' => t('Tick <em>Override</em> in a column to choose which models are offered. Select none to disable that capability. Untick Override to restore automatic detection. Assignments correct model classification; they do not add operations the provider adapter cannot execute. Audio input is separate from TTS and STT. Automatic selections from @provider are shown greyed out.', [
      '@provider' => $providers[$provider_id]['label'],
    ]),
    'auto' => $auto,
    'override_parents' => ['capability_overrides'],
    'empty_message' => t('No models were returned by @provider. Check its configuration on the <a href="@url">AI settings page</a>.', [
      '@provider' => $providers[$provider_id]['label'],
      '@url' => url('admin/config/ai/settings'),
    ]),
  ];
  // Keep one consistent matrix for every provider. The matrix JavaScript uses
  // delegated events, rather than one #states watcher per cell, so large
  // catalogs do not lock the browser while the form initializes.
  $form['capabilities'] = ai_capability_matrix_element($models, $defaults, ['manual_capability_models'], $matrix_options);

  $form['actions'] = ['#type' => 'actions'];
  $form['actions']['submit'] = [
    '#type' => 'submit',
    '#value' => t('Save capabilities'),
  ];
  $form['actions']['back'] = [
    '#type' => 'markup',
    '#markup' => ' ' . l(t('Back to overview'), 'admin/config/ai/settings/capabilities'),
  ];
  // Large catalogs can exceed PHP's input limit. Reject truncated submissions
  // instead of interpreting missing checkboxes as deliberate exclusions.
  $form['capabilities_complete'] = [
    '#type' => 'hidden',
    '#value' => 'complete',
    '#weight' => 1000,
  ];

  return $form;
}

/**
 * Validate the complete submission, while permitting zero-model overrides.
 */
function ai_capabilities_form_validate($form, &$form_state) {
  if (($form_state['input']['capabilities_complete'] ?? '') !== 'complete') {
    form_set_error('', t('The model selections were incomplete and have not been saved. Increase the PHP max_input_vars limit for large model catalogs, then try again.'));
  }
}

/**
 * Submit handler for ai_capabilities_form().
 */
function ai_capabilities_form_submit($form, &$form_state) {
  $provider_id = $form['#provider_id'];
  $overrides = !empty($form_state['values']['capability_overrides']) ? $form_state['values']['capability_overrides'] : [];
  $selected = !empty($form_state['values']['manual_capability_models']) ? $form_state['values']['manual_capability_models'] : [];

  // Only columns whose Override box is ticked are stored; everything else is
  // removed so the provider's own detection applies again.
  $manual = [];
  foreach ($overrides as $capability => $on) {
    if (empty($on)) {
      continue;
    }
    $models = isset($selected[$capability]) ? ai_parse_provider_model_list($selected[$capability]) : [];
    $manual[ai_normalize_capability_name($capability)] = $models;
  }

  $config = config('ai.settings');
  $providers_root = $config->get('providers') ?: [];
  if (!isset($providers_root[$provider_id]) || !is_array($providers_root[$provider_id])) {
    $providers_root[$provider_id] = [];
  }

  if (empty($manual)) {
    unset($providers_root[$provider_id]['manual_capability_models']);
  }
  else {
    $providers_root[$provider_id]['manual_capability_models'] = $manual;
  }

  $config->set('providers', $providers_root);
  $config->save();

  if (function_exists('ai_clear_models_cache')) {
    ai_clear_models_cache();
  }

  if (empty($manual)) {
    backdrop_set_message(t('Capability overrides cleared for @provider; automatic detection is used again.', [
      '@provider' => ai_get_provider_label($provider_id),
    ]));
  }
  else {
    backdrop_set_message(t('Capability overrides saved for @provider: @summary.', [
      '@provider' => ai_get_provider_label($provider_id),
      '@summary' => ai_capability_override_summary($provider_id),
    ]));
  }

  $form_state['redirect'] = 'admin/config/ai/settings/capabilities';
}
