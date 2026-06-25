<?php

/**
 * @file
 * Filename helpers for the AI Alt module.
 */

/**
 * Normalize an AI response into a safe filename stem.
 */
function _ai_alt_build_filename_from_label($label, $extension = '') {
  $label = strtolower(trim((string) $label));
  $label = preg_replace('/[^a-z0-9\-_ ]+/', '', $label);
  $label = preg_replace('/\s+/', '-', $label);
  $label = system_transliterate_machine_name($label, [
    'replace' => '-',
    'maxlength' => 96,
  ]);
  $parts = array_values(array_filter(explode('-', $label), 'strlen'));
  $parts = array_slice($parts, 0, 6);
  $filename = trim(implode('-', $parts), '-');

  if ($filename === '') {
    $filename = 'image';
  }

  if (!empty($extension)) {
    return $filename . '.' . strtolower($extension);
  }

  return $filename;
}

/**
 * Generate a short filename from image content.
 *
 * @return string|false
 *   The new filename or FALSE.
 */
function _ai_alt_generate_short_filename_for_file($file) {
  if (empty($file) || empty($file->uri)) {
    return FALSE;
  }

  $model = config_get('ai_alt.settings', 'model');
  if (empty($model)) {
    return FALSE;
  }

  $image_url = ai_alt_get_file_image_url($file);
  $prompt = 'Write a short filename for this image. Return 3 to 6 lowercase words separated by hyphens. No extension. No quotes. No punctuation. Be descriptive but concise.';
  $messages = [
    [
      'role' => 'user',
      'content' => [
        ['type' => 'text', 'text' => $prompt],
        ['type' => 'image_url', 'image_url' => ['url' => $image_url, 'detaifal' => 'high']],
      ],
    ],
  ];

  try {
    $label = ai_chat($model, $messages, 0.2, 80, FALSE, [
      'operation' => 'ai_alt_generate_short_filename',
      'fid' => $file->fid,
    ]);
  }
  catch (\Exception $e) {
    watchdog('ai_alt', 'Failed to generate filename for file @fid: @error', [
      '@fid' => $file->fid,
      '@error' => $e->getMessage(),
    ], WATCHDOG_ERROR);
    return FALSE;
  }

  if (empty($label)) {
    return FALSE;
  }

  $extension = pathinfo($file->filename, PATHINFO_EXTENSION);
  return ai_alt_build_filename_from_label($label, $extension);
}

/**
 * Move a file object to a new filename in the same directory.
 */
function _ai_alt_move_file_to_filename($file, $new_filename) {
  if (empty($file) || empty($file->uri) || empty($new_filename)) {
    return FALSE;
  }

  $old_uri = $file->uri;
  $current_filename = backdrop_basename($file->uri);
  if ($current_filename === $new_filename) {
    return FALSE;
  }

  $destination = file_stream_wrapper_uri_normalize(backdrop_dirname($file->uri) . '/' . $new_filename);
  $moved_file = file_move($file, $destination, FILE_EXISTS_RENAME);
  if (!$moved_file) {
    return FALSE;
  }

  ai_alt_rewrite_referenced_file_urls($moved_file, $old_uri, $moved_file->uri);
  ai_alt_create_file_redirect($old_uri, $moved_file->uri);

  return $moved_file;
}

/**
 * Check whether a file URI uses a local stream wrapper.
 */
function _ai_alt_file_uses_local_stream_wrapper($uri) {
  $scheme = file_uri_scheme($uri);
  if (!$scheme) {
    return FALSE;
  }

  $local_wrappers = file_get_stream_wrappers(STREAM_WRAPPERS_LOCAL);
  return !empty($local_wrappers[$scheme]);
}
