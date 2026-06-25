<?php

/**
 * @file
 * AI generation helpers for the AI Alt module.
 */

/**
 * Get an AI API instance.
 *
 * @return AIApi
 *   The AI API instance.
 */
function _ai_alt_get_api() {
  static $api;

  if ($api) {
    return $api;
  }

  $configured_model = config_get('ai_alt.settings', 'model');
  $api = ai_get_api_for_model($configured_model);

  if (!$api) {
    throw new Exception(t('No API provider is configured for model @model.', ['@model' => $configured_model]));
  }

  return $api;
}

/**
 * Describe an image from a final AI-consumable URL.
 */
function _ai_alt_describe_image_url($image_url) {
  $api = _ai_alt_get_api();
  return $api->describeImage($image_url);
}

/**
 * Generate alt text from a local image URI.
 */
function _ai_alt_generate_alt_text_from_uri($file_uri, $apply_image_style = TRUE) {
  module_load_include('inc', 'ai_alt', 'includes/ai_alt.image');
  $prepared_uri = _ai_alt_prepare_image_uri($file_uri, $apply_image_style);
  return _ai_alt_describe_image_url(file_create_url($prepared_uri));
}

/**
 * Generate alt text from a file entity.
 */
function _ai_alt_generate_from_file($file, $apply_image_style = TRUE) {
  try {
    $alt_text = _ai_alt_generate_alt_text_from_uri($file->uri, $apply_image_style);
    return !empty($alt_text) ? $alt_text : FALSE;
  }
  catch (\Exception $e) {
    watchdog('ai_alt', 'Failed to generate alt text for file @fid: @error', [
      '@fid' => isset($file->fid) ? $file->fid : 'new',
      '@error' => $e->getMessage(),
    ], WATCHDOG_ERROR);
    return FALSE;
  }
}

/**
 * Generate alt text from a file ID.
 */
function _ai_alt_generate_alt_text_from_fid($fid, $apply_image_style = TRUE) {
  $file = file_load($fid);
  if (!$file) {
    return FALSE;
  }

  return _ai_alt_generate_from_file($file, $apply_image_style);
}
