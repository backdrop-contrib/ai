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

  // Hand files over by URI so the bytes are read through the stream wrapper.
  // Their URLs can be unreadable over HTTP, e.g. private files behind access
  // checks or objects in a non-public S3 bucket.
  $scheme = file_uri_scheme($prepared_uri);
  if ($scheme && file_stream_wrapper_valid_scheme($scheme) && is_readable($prepared_uri)) {
    return _ai_alt_describe_image_url($prepared_uri);
  }

  return _ai_alt_describe_image_url(file_create_url($prepared_uri));
}

/**
 * Generate alt text from a file entity.
 */
function _ai_alt_generate_from_file($file, $apply_image_style = TRUE) {
  _ai_alt_generation_error(NULL, TRUE);

  if (empty($file) || empty($file->uri) || empty($file->filemime) || strpos($file->filemime, 'image/') !== 0) {
    _ai_alt_generation_error(t('The file is not an image.'));
    return FALSE;
  }

  $fid = isset($file->fid) ? $file->fid : 'new';

  if (!file_exists($file->uri)) {
    $reason = t('The image file is missing from storage (@uri). Restore the file or delete the file record.', ['@uri' => $file->uri]);
    _ai_alt_generation_error($reason);
    watchdog('ai_alt', 'Skipped alt text generation for file @fid: @reason', [
      '@fid' => $fid,
      '@reason' => $reason,
    ], WATCHDOG_WARNING);
    return FALSE;
  }

  try {
    $alt_text = _ai_alt_generate_alt_text_from_uri($file->uri, $apply_image_style);
    if (empty($alt_text)) {
      _ai_alt_generation_error(t('The AI model returned an empty description.'));
      return FALSE;
    }
    return $alt_text;
  }
  catch (\Throwable $e) {
    $reason = _ai_alt_explain_generation_error($e);
    _ai_alt_generation_error($reason);
    watchdog('ai_alt', 'Failed to generate alt text for file @fid: @reason Provider response: @error', [
      '@fid' => $fid,
      '@reason' => $reason,
      '@error' => $e->getMessage(),
    ], WATCHDOG_ERROR);
    return FALSE;
  }
}

/**
 * Store or read why the most recent alt text generation failed.
 *
 * @param string|null $reason
 *   A translated, user-facing reason to record, or NULL to only read.
 * @param bool $reset
 *   TRUE to clear the stored reason.
 *
 * @return string
 *   The stored reason, or an empty string when the last attempt succeeded.
 */
function _ai_alt_generation_error($reason = NULL, $reset = FALSE) {
  $stored = &backdrop_static(__FUNCTION__, '');
  if ($reset) {
    $stored = '';
  }
  if ($reason !== NULL) {
    $stored = $reason;
  }
  return $stored;
}

/**
 * Translate a provider exception into a reason a site editor can act on.
 */
function _ai_alt_explain_generation_error(\Throwable $e) {
  $message = $e->getMessage();
  $lc = strtolower($message);

  if (strpos($lc, 'error while downloading file') !== FALSE) {
    $code = preg_match('/status code:\s*(\d+)/i', $message, $m) ? $m[1] : '';
    return t('The AI provider could not download the image (HTTP @code). The site could not read the file to send it directly, and its URL is not publicly reachable.', ['@code' => $code ?: '?']);
  }
  if (strpos($lc, 'expected a valid url') !== FALSE) {
    return t('The site could not read the image file, so there was no valid image to send.');
  }
  if ($e instanceof AIRateLimitException || strpos($lc, 'rate limit') !== FALSE || strpos($lc, '(429)') !== FALSE) {
    return t("The AI provider's rate limit was reached. Wait a few minutes and try again.");
  }
  if ($e instanceof AIUsageLimitException) {
    return t('The site AI usage limit was reached: @message', ['@message' => $message]);
  }
  if (strpos($lc, '(401)') !== FALSE || strpos($lc, 'invalid_api_key') !== FALSE || strpos($lc, 'incorrect api key') !== FALSE) {
    return t('The AI provider rejected the API key. Check the key in the AI provider settings.');
  }
  if (strpos($lc, 'insufficient_quota') !== FALSE || strpos($lc, 'exceeded your current quota') !== FALSE) {
    return t('The AI provider account is out of credit or quota.');
  }
  if ($e instanceof AIConfigurationException || strpos($lc, 'no vision-capable') !== FALSE || strpos($lc, 'no api provider') !== FALSE) {
    return t('AI alt text is not configured: @message', ['@message' => $message]);
  }
  if ($e instanceof AIUnsupportedCapabilityException) {
    return t('The configured model cannot read images. Choose a vision-capable model in the AI alt text settings.');
  }
  if (strpos($lc, 'timed out') !== FALSE || strpos($lc, 'curl error 28') !== FALSE || strpos($lc, 'timeout') !== FALSE) {
    return t('The request to the AI provider timed out. Try again.');
  }
  if (strpos($lc, 'content_policy') !== FALSE || strpos($lc, 'safety system') !== FALSE) {
    return t("The AI provider declined to describe this image under its content policy.");
  }
  if (strpos($lc, 'image_parse_error') !== FALSE || strpos($lc, 'invalid image') !== FALSE || strpos($lc, 'unsupported image') !== FALSE || strpos($lc, 'too large') !== FALSE) {
    return t('The AI provider could not process this image. It may be corrupt, too large, or in an unsupported format.');
  }
  if (preg_match('/\((5\d\d)\)/', $message, $m)) {
    return t('The AI provider had a server error (HTTP @code). Try again later.', ['@code' => $m[1]]);
  }

  return t('The AI provider returned an error: @message', ['@message' => truncate_utf8(preg_replace('/\s+/', ' ', strip_tags($message)), 200, TRUE, TRUE)]);
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
