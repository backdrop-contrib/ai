<?php

/**
 * @file
 * Admin forms for AI provider, default model, and logging configuration.
 */

/**
 * Providers tab form.
 */
function ai_providers_form($form, &$form_state) {
  $form = [];
  $form['#attached']['css'][] = backdrop_get_path('module', 'ai') . '/css/ai.admin-overview.css';
  backdrop_add_css(backdrop_get_path('module', 'ai') . '/css/ai.admin-overview.css');
  $form['#attributes']['class'][] = 'ai-settings-form';
  $form['#submit'][] = 'ai_providers_form_submit';

  $available_keys = function_exists('key_get_key_names_as_options') ? key_get_key_names_as_options() : [];
  $providers_root = config_get('ai.settings', 'providers') ?: [];
  $providers = ai_get_providers(TRUE);

  uasort($providers, function ($a, $b) {
    $label_a = isset($a['label']) ? (string) $a['label'] : '';
    $label_b = isset($b['label']) ? (string) $b['label'] : '';
    return strcasecmp($label_a, $label_b);
  });

  $form['providers'] = [
    '#type' => 'fieldset',
    '#attributes' => ['class' => ['ai-settings-section', 'ai-providers-section']],
    '#title' => t('AI Providers'),
    '#description' => t('Enable one or more providers and configure their credentials.'),
    '#tree' => TRUE,
  ];

  foreach ($providers as $provider_id => $provider_info) {
    $default_enabled = !empty($providers_root[$provider_id]['enabled']);
    $checkbox_key = 'enabled_providers_' . $provider_id;
    $settings_key = $provider_id . '_settings';
    $api_key_element_name = 'api_key_' . $provider_id;

    $form['providers'][$checkbox_key] = [
      '#type' => 'checkbox',
      '#title' => $provider_info['label'],
      '#description' => $provider_info['description'] . ai_provider_links_markup($provider_info),
      '#default_value' => $default_enabled ? 1 : 0,
      '#prefix' => '<div class="ai-provider-card">',
    ];

    $form['providers'][$settings_key] = [
      '#type' => 'container',
      '#attributes' => ['class' => ['ai-provider-card-settings']],
      '#suffix' => '</div>',
      '#states' => [
        'visible' => [
          ':input[name="providers[' . $checkbox_key . ']"]' => ['checked' => TRUE],
        ],
      ],
    ];

    $context = ['provider' => $provider_id, 'form' => &$form];
    $provider_settings =& $form['providers'][$settings_key];
    backdrop_alter('ai_provider_settings', $provider_settings, $context);

    if (!empty($provider_settings[$api_key_element_name]) && !empty($provider_settings[$api_key_element_name]['#description'])) {
      $provider_settings[$api_key_element_name]['#preserve_description'] = TRUE;
    }

    if (!empty($provider_info['key_url']) && empty($provider_settings[$api_key_element_name])) {
      $key_title = !empty($provider_info['key_label']) ? $provider_info['key_label'] : t('API Key');
      $provider_settings[$api_key_element_name] = [
        '#type' => 'key_select',
        '#title' => $key_title,
        '#default_value' => !empty($providers_root[$provider_id]['api_key']) ? $providers_root[$provider_id]['api_key'] : '',
        '#options' => $available_keys,
        '#key_filters' => ['type' => 'authentication'],
        '#description' => '',
        '#weight' => -5,
      ];
    }

    if (!empty($provider_info['key_url']) && !empty($provider_settings[$api_key_element_name]) && empty($provider_settings[$api_key_element_name]['#preserve_description'])) {
      $provider_settings[$api_key_element_name]['#description'] = t('Get your API key from <a href="@url" target="_blank">@url</a>', ['@url' => $provider_info['key_url']]);
      $provider_settings[$api_key_element_name]['#preserve_description'] = TRUE;
    }

    if (!empty($default_enabled)) {
      try {
        $api = ai_get_api($provider_id);
        $models = $api ? $api->getModels() : [];

        if (!empty($models)) {
          $provider_settings[$provider_id . '_models'] = [
            '#type' => 'markup',
            '#markup' => '<details class="provider-models"><summary><strong>' . check_plain($provider_info['label']) . ' - ' . t('@count models', ['@count' => count($models)]) . '</strong></summary><div style="max-height: 300px; overflow-y: auto; font-size: 0.9em; line-height: 1.4; padding:0.5em 0;">' . implode('<br/>', array_map('check_plain', array_values($models))) . '</div></details>',
            '#weight' => 10,
          ];
        }
      }
      catch (\Exception $e) {
        watchdog('ai', 'Failed to fetch models for provider @provider: @msg', ['@provider' => $provider_id, '@msg' => $e->getMessage()], WATCHDOG_WARNING);
        $provider_settings[$provider_id . '_models'] = [
          '#type' => 'markup',
          '#markup' => '<div class="error">' . t('Error fetching models for provider @provider: @message', ['@provider' => check_plain($provider_info['label']), '@message' => check_plain($e->getMessage())]) . '</div>',
          '#weight' => 10,
        ];
      }
    }
  }

  $form['actions'] = ['#type' => 'actions'];
  $form['actions']['submit'] = [
    '#type' => 'submit',
    '#value' => t('Save configuration'),
  ];

  return $form;
}

/**
 * Submit handler for ai_providers_form().
 */
function ai_providers_form_submit($form, &$form_state) {
  $config = config('ai.settings');
  $providers = ai_get_providers(TRUE);
  $providers_values = !empty($form_state['values']['providers']) && is_array($form_state['values']['providers']) ? $form_state['values']['providers'] : [];
  $existing_providers = $config->get('providers') ?: [];
  $providers_root = [];

  foreach ($existing_providers as $provider_id => $provider_config) {
    if (!isset($providers[$provider_id])) {
      $providers_root[$provider_id] = $provider_config;
    }
  }

  foreach ($providers as $provider_id => $provider_info) {
    $checkbox_key = 'enabled_providers_' . $provider_id;
    $settings_key = $provider_id . '_settings';
    $api_key_field = 'api_key_' . $provider_id;

    $providers_root[$provider_id] = [
      'enabled' => !empty($providers_values[$checkbox_key]) ? 1 : 0,
    ];

    if (!empty($providers_values[$settings_key]) && is_array($providers_values[$settings_key])) {
      foreach ($providers_values[$settings_key] as $key => $value) {
        if ($key === $api_key_field || $key === 'api_key') {
          $providers_root[$provider_id]['api_key'] = $value;
          continue;
        }
        $providers_root[$provider_id][$key] = $value;
      }
    }
  }

  $config->set('providers', $providers_root);
  $config->save();
  if (function_exists('ai_clear_models_cache')) {
    ai_clear_models_cache();
  }

  backdrop_set_message(t('AI provider settings saved.'));
  $form_state['rebuild'] = TRUE;
}

/**
 * Default models tab form.
 */
function ai_defaults_form($form, &$form_state) {
  $form = [];
  $form['#attached']['css'][] = backdrop_get_path('module', 'ai') . '/css/ai.admin-overview.css';
  backdrop_add_css(backdrop_get_path('module', 'ai') . '/css/ai.admin-overview.css');
  $form['#attributes']['class'][] = 'ai-settings-form';
  $form['#submit'][] = 'ai_defaults_form_submit';

  $chat_models = ai_get_all_chat_models();
  $image_models = ai_get_all_image_models();
  $vision_models = ai_get_all_vision_models();
  $embedding_models = ai_get_all_models_for_method('getEmbeddingModels');
  $moderation_models = ai_get_all_moderation_models();
  $tts_models = ai_get_all_tts_models();
  $stt_models = ai_get_all_stt_models();

  $none = ['' => t('- No models available -')];

  $form['default_chat_model'] = [
    '#type' => 'select',
    '#title' => t('Default chat model'),
    '#options' => !empty($chat_models) ? $chat_models : $none,
    '#default_value' => ai_default_model($chat_models ?: [], config_get('ai.settings', 'default_chat_model')),
  ];
  $form['default_text_model'] = [
    '#type' => 'select',
    '#title' => t('Default text model'),
    '#options' => !empty($chat_models) ? $chat_models : $none,
    '#default_value' => ai_default_model($chat_models ?: [], config_get('ai.settings', 'default_text_model')),
  ];
  $form['default_image_model'] = [
    '#type' => 'select',
    '#title' => t('Default image model'),
    '#options' => !empty($image_models) ? $image_models : $none,
    '#default_value' => ai_default_model($image_models ?: [], config_get('ai.settings', 'default_image_model')),
  ];
  $form['default_vision_model'] = [
    '#type' => 'select',
    '#title' => t('Default vision model'),
    '#options' => !empty($vision_models) ? $vision_models : $none,
    '#default_value' => ai_default_model($vision_models ?: [], config_get('ai.settings', 'default_vision_model')),
  ];
  $form['embedding_model'] = [
    '#type' => 'select',
    '#title' => t('Default embedding model'),
    '#options' => !empty($embedding_models) ? $embedding_models : $none,
    '#default_value' => ai_default_model($embedding_models ?: [], config_get('ai.settings', 'embedding_model'), ['text-embedding-3-small']),
  ];
  $form['default_moderation_model'] = [
    '#type' => 'select',
    '#title' => t('Default moderation model'),
    '#options' => !empty($moderation_models) ? $moderation_models : $none,
    '#default_value' => ai_default_model($moderation_models ?: [], config_get('ai.settings', 'default_moderation_model')),
  ];
  $form['default_tts_model'] = [
    '#type' => 'select',
    '#title' => t('Default text-to-speech model'),
    '#options' => !empty($tts_models) ? $tts_models : $none,
    '#default_value' => ai_default_model($tts_models ?: [], config_get('ai.settings', 'default_tts_model')),
  ];
  $form['default_stt_model'] = [
    '#type' => 'select',
    '#title' => t('Default speech-to-text model'),
    '#options' => !empty($stt_models) ? $stt_models : $none,
    '#default_value' => ai_default_model($stt_models ?: [], config_get('ai.settings', 'default_stt_model')),
  ];

  $form['actions'] = ['#type' => 'actions'];
  $form['actions']['submit'] = [
    '#type' => 'submit',
    '#value' => t('Save configuration'),
  ];

  return $form;
}

/**
 * Submit handler for ai_defaults_form().
 */
function ai_defaults_form_submit($form, &$form_state) {
  $config = config('ai.settings');
  $config->set('default_chat_model', $form_state['values']['default_chat_model']);
  $config->set('default_text_model', $form_state['values']['default_text_model']);
  $config->set('default_image_model', $form_state['values']['default_image_model']);
  $config->set('default_vision_model', $form_state['values']['default_vision_model']);
  $config->set('embedding_model', $form_state['values']['embedding_model']);
  $config->set('default_moderation_model', $form_state['values']['default_moderation_model']);
  $config->set('default_tts_model', $form_state['values']['default_tts_model']);
  $config->set('default_stt_model', $form_state['values']['default_stt_model']);
  $config->save();

  backdrop_set_message(t('Default model settings saved.'));
}

/**
 * API logging tab form.
 */
function ai_logging_form($form, &$form_state) {
  $form = [];
  $form['#attached']['css'][] = backdrop_get_path('module', 'ai') . '/css/ai.admin-overview.css';
  backdrop_add_css(backdrop_get_path('module', 'ai') . '/css/ai.admin-overview.css');
  $form['#attributes']['class'][] = 'ai-settings-form';
  $form['#submit'][] = 'ai_logging_form_submit';

  $form['ai_log_enabled'] = [
    '#type' => 'checkbox',
    '#title' => t('Enable API logging'),
    '#default_value' => config_get('ai.settings', 'ai_log_enabled') ?? FALSE,
    '#description' => t('If enabled, AI requests will be logged to the database.'),
  ];
  $form['ai_log_max_age'] = [
    '#type' => 'select',
    '#title' => t('Discard logs older than'),
    '#default_value' => config_get('ai.settings', 'ai_log_max_age') ?? 2592000,
    '#options' => [
      0 => t('Never'),
      3600 => t('1 hour'),
      86400 => t('1 day'),
      604800 => t('1 week'),
      2592000 => t('1 month'),
      15552000 => t('6 months'),
      31536000 => t('1 year'),
    ],
    '#states' => [
      'visible' => [
        ':input[name="ai_log_enabled"]' => ['checked' => TRUE],
      ],
    ],
  ];

  $form['actions'] = ['#type' => 'actions'];
  $form['actions']['submit'] = [
    '#type' => 'submit',
    '#value' => t('Save configuration'),
  ];

  return $form;
}

/**
 * Submit handler for ai_logging_form().
 */
function ai_logging_form_submit($form, &$form_state) {
  $config = config('ai.settings');
  $config->set('ai_log_enabled', (bool) $form_state['values']['ai_log_enabled']);
  $config->set('ai_log_max_age', (int) $form_state['values']['ai_log_max_age']);
  $config->save();

  backdrop_set_message(t('API logging settings saved.'));
}
