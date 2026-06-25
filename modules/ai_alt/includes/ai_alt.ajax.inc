<?php

/**
 * @file
 * AJAX handlers for the AI Alt module.
 */

/**
 * Build a standard AJAX alert response.
 */
function _ai_alt_ajax_alert($message) {
  return [
    '#type' => 'ajax',
    '#commands' => [
      ajax_command_alert($message),
    ],
  ];
}

/**
 * AJAX callback to generate alt text in the CKEditor modal.
 */
function _ai_alt_generate_alt_text_for_ckeditor($form, array &$form_state) {
  $fid = $form['image']['fid']['#value']['fid'] ?? NULL;

  if (!$fid) {
    watchdog('ai_alt', 'No file ID (fid) found in the form.', [], WATCHDOG_WARNING);
    return _ai_alt_ajax_alert(t('Please upload or select an image before generating alt text.'));
  }

  $file = file_load($fid);
  if (!$file) {
    watchdog('ai_alt', 'Failed to load file with fid: @fid', ['@fid' => $fid], WATCHDOG_WARNING);
    return _ai_alt_ajax_alert(t('Failed to load the image file. Please try again.'));
  }

  try {
    $generated_alt_text = _ai_alt_generate_from_file($file, FALSE);

    if (!empty($generated_alt_text)) {
      $form['alt']['#value'] = $generated_alt_text;
      return [
        '#type' => 'ajax',
        '#commands' => [
          ajax_command_replace('#edit-alt-wrapper', backdrop_render($form['alt'])),
        ],
      ];
    }

    backdrop_set_message(t('Failed to generate alt text. Please try again.'), 'error');
  }
  catch (\Exception $e) {
    watchdog('ai_alt', 'Error generating alt text: @error', ['@error' => $e->getMessage()], WATCHDOG_ERROR);
    return _ai_alt_ajax_alert(t('An error occurred while generating alt text.'));
  }

  return _ai_alt_ajax_alert(t('Failed to generate alt text. Please try again.'));
}

/**
 * Generate alt text for CKEditor autogeneration requests.
 */
function _ai_alt_generate_alt_text_for_ckeditor_autogenerate() {
  $fid = $_POST['fid'] ?? NULL;
  $src = $_POST['src'] ?? NULL;

  if (!$fid && !$src) {
    watchdog('ai_alt', 'No file ID (fid) or source (src) provided.', [], WATCHDOG_WARNING);
    backdrop_json_output(['status' => 'error', 'message' => 'No image provided.']);
    return;
  }

  try {
    if ($fid) {
      $generated_alt_text = _ai_alt_generate_alt_text_from_fid($fid, TRUE);
      if (!$generated_alt_text) {
        backdrop_json_output(['status' => 'error', 'message' => 'Failed to load image file.']);
        return;
      }
    }
    else {
      // Preserve existing behavior for library-selected images supplied as src.
      $generated_alt_text = _ai_alt_describe_image_url(file_create_url($src));
    }

    if (!empty($generated_alt_text)) {
      backdrop_json_output(['status' => 'success', 'alt_text' => $generated_alt_text]);
      return;
    }

    backdrop_json_output(['status' => 'error', 'message' => 'Failed to generate alt text.']);
  }
  catch (\Exception $e) {
    watchdog('ai_alt', 'Error generating alt text: @error', ['@error' => $e->getMessage()], WATCHDOG_ERROR);
    backdrop_json_output(['status' => 'error', 'message' => 'An error occurred while generating alt text.']);
  }
}

/**
 * AJAX callback for the manual Generate Alt Text button.
 */
function _ai_alt_generate_alt_text_ajax($form, array &$form_state) {
  $triggering_element = $form_state['triggering_element'];

  $fid = isset($triggering_element['#attributes']['data-file-id']) ? $triggering_element['#attributes']['data-file-id'] : NULL;
  $field_name = isset($triggering_element['#attributes']['data-field-name']) ? $triggering_element['#attributes']['data-field-name'] : NULL;
  $delta = isset($triggering_element['#attributes']['data-delta']) ? $triggering_element['#attributes']['data-delta'] : NULL;
  $field_lang = isset($triggering_element['#attributes']['data-field-lang']) ? $triggering_element['#attributes']['data-field-lang'] : LANGUAGE_NONE;

  if (!$field_name || $delta === NULL) {
    backdrop_set_message(t('Unable to determine field context.'), 'error');
    return _ai_alt_ajax_alert(t('An error occurred while generating alt text. Please try again.'));
  }

  $file = file_load($fid);
  if (!$file) {
    backdrop_set_message(t('File not found for the given file ID.'), 'error');
    return _ai_alt_ajax_alert(t('File not found for the given file ID.'));
  }

  try {
    $generated_alt_text = _ai_alt_generate_from_file($file, TRUE);
  }
  catch (\Exception $e) {
    watchdog('ai_alt', 'Error generating alt text: @error', ['@error' => $e->getMessage()], WATCHDOG_ERROR);
    backdrop_set_message(t('An error occurred while generating alt text.'), 'error');
    return _ai_alt_ajax_alert(t('An error occurred while generating alt text. Please try again.'));
  }

  if (!$generated_alt_text) {
    backdrop_set_message(t('Failed to generate alt text. Please try again.'), 'error');
    return _ai_alt_ajax_alert(t('An error occurred while generating alt text. Please try again.'));
  }

  $model = config_get('ai_alt.settings', 'model') ?: 'unknown';
  watchdog('ai_alt', 'Generated alt text for fid @fid using model @model.', ['@fid' => $fid, '@model' => $model]);

  $selector = "input[name='{$field_name}[{$field_lang}][{$delta}][alt]']";
  return [
    '#type' => 'ajax',
    '#commands' => [
      ajax_command_invoke($selector, 'val', [$generated_alt_text]),
    ],
  ];
}

/**
 * Handle AJAX requests to generate alt text.
 */
function _ai_alt_generate_alt_text_callback() {
  $fid = $_POST['fid'] ?? NULL;
  $field_name = $_POST['field_name'] ?? NULL;
  $delta = $_POST['delta'] ?? NULL;

  if (!$fid || !$field_name || $delta === NULL) {
    watchdog('ai_alt', 'Missing parameters in AJAX request.');
    return ['status' => 'error', 'message' => 'Invalid request.'];
  }

  $file = file_load($fid);
  if (!$file) {
    watchdog('ai_alt', 'File not found for fid: @fid', ['@fid' => $fid]);
    return ['status' => 'error', 'message' => 'File not found.'];
  }

  try {
    $generated_alt_text = _ai_alt_generate_from_file($file, FALSE);
  }
  catch (\Exception $e) {
    watchdog('ai_alt', 'Error generating alt text: @error', ['@error' => $e->getMessage()], WATCHDOG_ERROR);
    return ['status' => 'error', 'message' => 'An error occurred while generating alt text.'];
  }

  if (!$generated_alt_text) {
    return ['status' => 'error', 'message' => 'Alt text generation failed.'];
  }

  return [
    'status' => 'success',
    'alt_text' => $generated_alt_text,
    'wrapper_id' => "ai-alt-field-wrapper-{$field_name}-{$delta}",
  ];
}
