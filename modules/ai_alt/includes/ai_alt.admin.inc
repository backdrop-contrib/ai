<?php

/**
 * @file
 * Admin form helpers for the AI Alt module.
 */

/**
 * Build the configuration form for AI Alt Text.
 *
 * @return array
 *   The configuration form.
 */
function ai_alt_settings_form(array $form, array &$form_state) {
  $config = config('ai_alt.settings');

  $form = [];
  $form['#config'] = 'ai_alt.settings';

  $prompt = $config->get('prompt');

  $bulk_link = l(t('Bulk generate alt text'), 'admin/config/ai/ai-alt-bulk');
  $form['bulk_link'] = [
    '#type' => 'item',
    '#markup' => t('You can also !link for existing images.', ['!link' => $bulk_link]),
    '#weight' => -10,
  ];

  $form['prompt'] = [
    '#type' => 'textarea',
    '#title' => t('AI Alt text generation prompt'),
    '#default_value' => $prompt,
    '#description' => t('Prompt used for generating the alt text.'),
    '#required' => TRUE,
    '#rows' => 15,
    '#cols' => 80,
    '#attributes' => [
      'style' => 'white-space: pre-wrap;',
    ],
  ];

  $image_styles = image_styles();
  $image_styles_options = [];

  foreach ($image_styles as $name => $image_style) {
    $image_styles_options[$name] = $image_style['label'];
  }

  $form['image_style'] = [
    '#title' => t('Image style'),
    '#type' => 'select',
    '#options' => $image_styles_options,
    '#default_value' => $config->get('image_style') ?? 'ai_image_alt_text',
    '#empty_option' => t('Original (NOT RECOMMENDED)'),
    '#description' => t('Image style to use before sending the image to reduce resolution and reformat as PNG.'),
  ];

  $models = ai_get_all_vision_models();

  $flat_models = [];
  foreach ($models as $g_label => $g_items) {
    if (is_array($g_items)) {
      foreach ($g_items as $k => $v) {
        $flat_models[$k] = $v;
      }
    }
    else {
      $flat_models[$g_label] = $g_items;
    }
  }

  $current_model = $config->get('model');
  if ($current_model && !isset($flat_models[$current_model])) {
    $all_models = ai_get_all_models();
    $flat_all_models = [];
    foreach ($all_models as $g_label => $g_items) {
      if (is_array($g_items)) {
        foreach ($g_items as $k => $v) {
          $flat_all_models[$k] = $v;
        }
      }
      else {
        $flat_all_models[$g_label] = $g_items;
      }
    }

    if (isset($flat_all_models[$current_model])) {
      $models[t('Current selection')][$current_model] = $flat_all_models[$current_model] . ' ' . t('(currently selected, non-vision)');
      $flat_models[$current_model] = $flat_all_models[$current_model];
    }
    else {
      $parts = explode('/', $current_model);
      $provider_id = $parts[0] ?? '';
      $providers_root = config_get('ai.settings', 'providers') ?: [];
      if (!empty($providers_root[$provider_id]['enabled'])) {
        $provider_label = ai_get_provider_label($provider_id);
        $models[$provider_label][$current_model] = $current_model . ' ' . t('(currently selected)');
        $flat_models[$current_model] = $current_model;
      }
    }
  }

  if (empty($models)) {
    $link = l(t('configure a provider'), 'admin/config/ai/settings');
    backdrop_set_message(t('No suitable models are available from your configured providers. Please !link in AI settings.', ['!link' => $link]), 'warning');
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
    '#default_value' => $config->get('model') ?: $default_model,
    '#required' => TRUE,
    '#description' => t('AI model to use for generating the alt text. If the desired model is not listed, ensure its provider is enabled in AI settings.'),
  ];

  $form['autogenerate'] = [
    '#title' => t('Autogenerate on upload'),
    '#type' => 'checkbox',
    '#default_value' => $config->get('autogenerate') ?? FALSE,
    '#description' => t('Automatically generate alt text when uploading an image.'),
  ];

  $form['rename_filenames'] = [
    '#title' => t('Rename uploaded image filenames'),
    '#type' => 'checkbox',
    '#default_value' => $config->get('rename_filenames') ?? FALSE,
    '#description' => t('Rename new image files from a short AI-generated label, not the full alt text.'),
  ];

  $form['hide_button'] = [
    '#title' => t('Hide button'),
    '#type' => 'checkbox',
    '#default_value' => $config->get('hide_button') ?? FALSE,
    '#description' => t('Hide the button to generate alt text manually.'),
    '#states' => [
      'enabled' => [
        ':input[name="autogenerate"]' => ['checked' => TRUE],
      ],
    ],
  ];

  $form['#validate'][] = 'ai_alt_settings_form_validate';

  return system_settings_form($form);
}

/**
 * Validate the AI Alt settings form.
 */
function ai_alt_settings_form_validate($form, array &$form_state) {
  $autogenerate = $form_state['values']['autogenerate'] ?? FALSE;
  $hide_button = $form_state['values']['hide_button'] ?? FALSE;

  if (!$autogenerate && $hide_button) {
    form_set_error('hide_button', t('The "Hide button" option cannot be enabled unless "Autogenerate on upload" is checked.'));
  }
}
