<?php

/**
 * @file
 * Form and widget integration helpers for the AI Alt module.
 */

/**
 * Process callback to inject alt-text generation into managed file fields.
 */
function _ai_alt_image_field_process($element, array &$form_state, $form) {
  $delta = 0;
  $field_name = 'unknown_field';

  if (empty($element['#value']) || !isset($element['#value']['alt'])) {
    return $element;
  }

  if (!empty($element['#parents'])) {
    $last_parent = end($element['#parents']);
    if (is_numeric($last_parent)) {
      $delta = (int) $last_parent;
    }
    reset($element['#parents']);
  }

  if (!empty($element['#field_name'])) {
    $field_name = $element['#field_name'];
  }

  if (!empty($element['#id'])) {
    $element['#id'] .= '--delta-' . $delta;
  }

  $current_alt = trim($element['#value']['alt']);
  $hide_button = config_get('ai_alt.settings', 'hide_button');
  $autogenerate = config_get('ai_alt.settings', 'autogenerate');

  if (empty($hide_button) && !isset($element['ai_alt_text_generation'])) {
    $element['ai_alt_text_generation'] = [
      '#type' => 'button',
      '#value' => t('Generate Alt Text'),
      '#name' => 'ai_alt_text_generation' . $field_name . '_' . $delta,
      '#ajax' => [
        'callback' => 'ai_alt_generate_alt_text_ajax',
        'wrapper' => $element['#id'],
      ],
      '#attributes' => [
        'class' => ['ai-alt-text-generation', 'button--primary'],
        'data-file-id' => isset($element['#value']['fid']) ? $element['#value']['fid'] : NULL,
        'data-field-name' => $field_name,
        'data-delta' => $delta,
      ],
      '#weight' => 1,
    ];
  }

  $has_global_alt = FALSE;
  if (module_exists('image_entity_alt') && !empty($element['#value']['fid'])) {
    $alt_field = function_exists('image_entity_alt_field_name') ? image_entity_alt_field_name() : 'field_alt';
    $file = file_load((int) $element['#value']['fid']);
    if ($file && !empty($file->{$alt_field}[LANGUAGE_NONE][0]['value'])) {
      $has_global_alt = TRUE;
    }
  }

  if (!empty($autogenerate) && $current_alt === '' && !$has_global_alt) {
    $unique_key = $field_name . ':' . $delta;
    $element['#attached']['js'][] = [
      'data' => [
        'aiAlt' => [
          $unique_key => [
            'fid' => isset($element['#value']['fid']) ? $element['#value']['fid'] : NULL,
            'field_name' => $field_name,
            'delta' => $delta,
            'target_id' => $element['#id'],
          ],
        ],
      ],
      'type' => 'setting',
    ];
    $element['#attached']['js'][] = backdrop_get_path('module', 'ai_alt') . '/js/ai_alt_autogenerate.js';
  }

  return $element;
}

/**
 * Alter the CKEditor image form to support AI alt generation.
 */
function _ai_alt_form_alter(array &$form, array &$form_state, $form_id) {
  if ($form_id != 'filter_format_editor_image_form') {
    return;
  }

  if (isset($form['image'])) {
    $form['image']['fid']['#weight'] = 0;
    $form['image']['src']['#weight'] = 1;
  }
  if (isset($form['alt'])) {
    $form['alt']['#weight'] = 2;
  }
  if (isset($form['size'])) {
    $form['size']['#weight'] = 4;
    $form['size']['width']['#weight'] = 5;
    $form['size']['height']['#weight'] = 6;
  }
  if (isset($form['align'])) {
    $form['align']['#weight'] = 7;
  }
  if (isset($form['caption'])) {
    $form['caption']['#weight'] = 8;
  }
  if (isset($form['actions'])) {
    $form['actions']['submit']['#weight'] = 8;
  }

  if (!isset($form['alt'])) {
    return;
  }

  $form['alt']['#prefix'] = '<div id="edit-alt-wrapper">';
  $form['alt']['#suffix'] = '</div>';

  $hide_button = config_get('ai_alt.settings', 'hide_button');
  $autogenerate = config_get('ai_alt.settings', 'autogenerate');

  if (empty($hide_button)) {
    $form['generate_with_ai'] = [
      '#type' => 'button',
      '#value' => t('Generate Alt Text'),
      '#ajax' => [
        'callback' => 'ai_alt_generate_alt_text_for_ckeditor',
        'wrapper' => 'edit-alt-wrapper',
      ],
      '#attributes' => [
        'class' => ['ckeditor-ai-generate-button'],
      ],
      '#weight' => 3,
    ];
  }

  if (empty($autogenerate)) {
    return;
  }

  $uploaded_fid = $form_state['values']['fid'] ?? NULL;

  if ($uploaded_fid) {
    $form['#attached']['js'][] = [
      'data' => [
        'aiAlt' => [
          'fid' => $uploaded_fid,
          'wrapper_id' => 'edit-alt-wrapper',
        ],
      ],
      'type' => 'setting',
    ];
  }
  else {
    watchdog('ai_alt', 'No uploaded image detected, waiting for selection.', []);
    $form['#attached']['js'][] = [
      'data' => [
        'aiAlt' => [
          'wrapper_id' => 'edit-alt-wrapper',
        ],
      ],
      'type' => 'setting',
    ];
  }

  $form['#attached']['js'][] = backdrop_get_path('module', 'ai_alt') . '/js/ai_alt_autogenerate_ckeditor.js';
}

/**
 * Attach AI generation controls to image file forms.
 */
function _ai_alt_attach_to_file_form(array &$form, array &$form_state) {
  if (!module_exists('image_entity_alt')) {
    return;
  }

  $autogenerate = config_get('ai_alt.settings', 'autogenerate');
  $hide_button = config_get('ai_alt.settings', 'hide_button');

  $file = NULL;
  if (isset($form_state['file'])) {
    $file = $form_state['file'];
  }
  elseif (isset($form_state['storage']['upload'])) {
    $file = file_load($form_state['storage']['upload']);
  }

  if (!$file || strpos($file->filemime, 'image/') !== 0) {
    return;
  }

  $alt_field = image_entity_alt_field_name();
  if (!isset($form[$alt_field])) {
    return;
  }

  $lang = isset($form[$alt_field]['#language']) ? $form[$alt_field]['#language'] : LANGUAGE_NONE;
  if (!isset($form[$alt_field][$lang][0]['value'])) {
    return;
  }

  $wrapper_id = 'ai-alt-file-wrapper';
  $form[$alt_field][$lang][0]['value']['#prefix'] = '<div id="' . $wrapper_id . '">';
  $form[$alt_field][$lang][0]['value']['#suffix'] = '</div>';

  $form[$alt_field][$lang][0]['generate_button'] = [
    '#type' => 'button',
    '#value' => t('AI Generate Alt Text'),
    '#ajax' => [
      'callback' => 'ai_alt_file_form_ajax_callback',
      'wrapper' => $wrapper_id,
    ],
    '#weight' => 10,
    '#attributes' => [
      'class' => ['ai-alt-file-generate-button'],
    ],
  ];

  if ($hide_button && $autogenerate) {
    $form[$alt_field][$lang][0]['generate_button']['#attributes']['style'] = 'display: none;';
  }

  $form[$alt_field][$lang][0]['generate_button']['#limit_validation_errors'] = [];
  $form['#attached']['js'][] = [
    'data' => [
      'aiAltFile' => [
        'autogenerate' => (bool) $autogenerate,
      ],
    ],
    'type' => 'setting',
  ];
  $form['#attached']['js'][] = backdrop_get_path('module', 'ai_alt') . '/js/ai_alt_file_form.js';
}

/**
 * AJAX callback for file form alt text generation.
 */
function _ai_alt_file_form_ajax_callback($form, array &$form_state) {
  $alt_field = image_entity_alt_field_name();
  $lang = isset($form[$alt_field]['#language']) ? $form[$alt_field]['#language'] : LANGUAGE_NONE;

  $fid = NULL;
  if (isset($form_state['file']->fid)) {
    $fid = $form_state['file']->fid;
  }
  elseif (isset($form_state['storage']['upload'])) {
    $fid = $form_state['storage']['upload'];
  }

  if ($fid) {
    $file = file_load($fid);
    if ($file) {
      $alt_text = ai_alt_generate_from_file($file);
      if ($alt_text) {
        $form[$alt_field][$lang][0]['value']['#value'] = $alt_text;
      }
    }
  }

  return $form[$alt_field][$lang][0]['value'];
}
