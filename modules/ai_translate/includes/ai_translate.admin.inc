<?php

/**
 * @file
 * Admin settings form for the AI Translate module.
 */

/**
 * Build the configuration form for AI Translate.
 */
function ai_translate_settings_form(array $form, array &$form_state) {
  $config = config('ai_translate.settings');

  $form = [];
  $form['#config'] = 'ai_translate.settings';

  $languages = language_list(TRUE);
  if (count($languages) < 2) {
    $link = l(t('Regional and language settings'), 'admin/config/regional/language');
    backdrop_set_message(t('AI Translate needs at least two enabled languages. Configure them under !link first.', ['!link' => $link]), 'warning');
  }

  $models = ai_get_all_chat_models();
  $current_model = $config->get('model');

  $flat_models = [];
  foreach ($models as $group_label => $group_items) {
    if (is_array($group_items)) {
      foreach ($group_items as $key => $value) {
        $flat_models[$key] = $value;
      }
    }
    else {
      $flat_models[$group_label] = $group_items;
    }
  }

  if (empty($models)) {
    $link = l(t('configure a provider'), 'admin/config/ai/settings');
    backdrop_set_message(t('No suitable chat models are available from your configured providers. Please !link in AI settings.', ['!link' => $link]), 'warning');
  }

  $default_model = config_get('ai.settings', 'default_text_model') ?: '';
  if (!empty($flat_models) && !isset($flat_models[$default_model])) {
    reset($flat_models);
    $default_model = key($flat_models);
  }

  $form['model'] = [
    '#type' => 'select',
    '#title' => t('Model to use'),
    '#options' => $models,
    '#default_value' => isset($flat_models[$current_model]) ? $current_model : $default_model,
    '#required' => TRUE,
    '#description' => t('AI model used to translate node titles and fields. If the desired model is not listed, ensure its provider is enabled in AI settings.'),
  ];

  $form['temperature'] = [
    '#type' => 'textfield',
    '#title' => t('Temperature'),
    '#default_value' => $config->get('temperature') ?? 0.3,
    '#size' => 5,
    '#description' => t('Lower values (e.g. 0.3) produce more literal, consistent translations. Higher values allow more paraphrasing.'),
  ];

  $form['max_tokens'] = [
    '#type' => 'number',
    '#title' => t('Maximum tokens per field'),
    '#default_value' => $config->get('max_tokens') ?? 2048,
    '#min' => 1,
    '#description' => t('Upper bound on the length of a single translated field value.'),
  ];

  $form['prompt'] = [
    '#type' => 'textarea',
    '#title' => t('Default translation prompt'),
    '#default_value' => $config->get('prompt'),
    '#description' => t('Sent as the system prompt for every translated field unless a per-language override below is set. Use the placeholders %source and %target for the source and target language names.', [
      '%source' => '{source_language}',
      '%target' => '{target_language}',
    ]),
    '#required' => TRUE,
    '#rows' => 8,
  ];

  $form['language_prompts'] = [
    '#type' => 'fieldset',
    '#title' => t('Per-language prompt overrides'),
    '#collapsible' => TRUE,
    '#collapsed' => TRUE,
    '#tree' => TRUE,
    '#description' => t('Optional. Leave blank to use the default prompt above for a given language.'),
  ];

  $saved_language_prompts = $config->get('language_prompts') ?: [];
  foreach ($languages as $langcode => $language) {
    $form['language_prompts'][$langcode] = [
      '#type' => 'textarea',
      '#title' => $language->name,
      '#default_value' => $saved_language_prompts[$langcode] ?? '',
      '#rows' => 4,
    ];
  }

  $form['reference_depth'] = [
    '#type' => 'number',
    '#title' => t('Entity reference follow depth'),
    '#default_value' => $config->get('reference_depth') ?? 0,
    '#min' => 0,
    '#max' => 5,
    '#description' => t('How many levels of entity-reference fields to follow and translate along with the content. 0 (default) translates only the content itself and leaves referenced entities pointing at their original, untranslated version. Referenced content the current user cannot view is always skipped, regardless of this setting.'),
  ];

  $form['publish_behavior'] = [
    '#type' => 'radios',
    '#title' => t('Publish status of AI-generated translations'),
    '#options' => [
      'draft' => t('Always save as unpublished, for editorial review before going live.'),
      'preserve' => t('Match the publish status of the source content.'),
    ],
    '#default_value' => $config->get('publish_behavior') ?: 'draft',
  ];

  return system_settings_form($form);
}
