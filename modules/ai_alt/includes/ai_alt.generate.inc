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
  static $apis = [];

  $configured_model = _ai_alt_resolve_model();
  if ($configured_model === '') {
    throw new Exception(t('No vision-capable AI model is configured.'));
  }
  if (isset($apis[$configured_model])) {
    return $apis[$configured_model];
  }

  $api = ai_get_api_for_model($configured_model);

  if (!$api) {
    throw new Exception(t('No API provider is configured for model @model.', ['@model' => $configured_model]));
  }

  $apis[$configured_model] = $api;
  return $apis[$configured_model];
}

/**
 * Resolve a configured or capability-aware fallback vision model.
 */
function _ai_alt_resolve_model() {
  static $model;
  if ($model !== NULL) {
    return $model;
  }

  $configured = trim((string) config_get('ai_alt.settings', 'model'));
  if ($configured !== '') {
    return $model = $configured;
  }

  $preferred = trim((string) config_get('ai.settings', 'default_vision_model'));
  $grouped = function_exists('ai_get_all_vision_models') ? ai_get_all_vision_models() : [];
  if (function_exists('ai_default_model')) {
    $model = ai_default_model($grouped, $preferred);
  }
  elseif ($preferred !== '') {
    $model = $preferred;
  }
  else {
    $model = '';
    foreach ($grouped as $provider_models) {
      if (is_array($provider_models) && !empty($provider_models)) {
        $model = (string) key($provider_models);
        break;
      }
    }
  }

  return $model;
}

/**
 * Describe an image from a final AI-consumable URL.
 */
function _ai_alt_describe_image_url($image_url) {
  $api = _ai_alt_get_api();
  return ai_alt_normalize_generated_text($api->describeImage($image_url, TRUE, _ai_alt_resolve_model()));
}

/**
 * Normalize model output before placing it in an alt field.
 */
function ai_alt_normalize_generated_text($text) {
  $text = trim((string) $text);
  $text = preg_replace('/^\s*(alt text|alt|caption)\s*:\s*/i', '', $text);
  $text = trim($text, " \t\n\r\0\x0B\"'");
  $text = trim(preg_replace('/\s+/', ' ', $text));
  return function_exists('truncate_utf8') ? truncate_utf8($text, 255, TRUE, TRUE) : substr($text, 0, 255);
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
  if (empty($file) || empty($file->uri) || empty($file->filemime) || strpos($file->filemime, 'image/') !== 0) {
    return FALSE;
  }

  try {
    $alt_text = _ai_alt_generate_alt_text_from_uri($file->uri, $apply_image_style);
    return !empty($alt_text) ? $alt_text : FALSE;
  }
  catch (\Throwable $e) {
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
