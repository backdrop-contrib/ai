<?php

/**
 * @file
 * Image preparation helpers for the AI Alt module.
 */

/**
 * Convert an SVG image to PNG for downstream processing.
 */
function _ai_alt_convert_svg_to_png($svg_uri) {
  if (file_stream_wrapper_valid_scheme(file_uri_scheme($svg_uri))) {
    $svg_real_path = backdrop_realpath($svg_uri);
  }
  else {
    $svg_real_path = $svg_uri;
  }

  if (!$svg_real_path || (!file_exists($svg_real_path) && file_uri_scheme($svg_uri) !== 's3')) {
    watchdog('ai_alt', 'Could not resolve real path or file does not exist: @uri', ['@uri' => $svg_uri]);
    return FALSE;
  }

  $output_uri = preg_replace('/\.svg$/i', '.png', $svg_real_path);

  if (!shell_exec('which convert')) {
    watchdog('ai_alt', 'ImageMagick `convert` command not found.');
    return FALSE;
  }

  $command = escapeshellcmd('convert ' . escapeshellarg($svg_real_path) . ' PNG32:' . escapeshellarg($output_uri));
  watchdog('ai_alt', 'Running command: @command', ['@command' => $command]);
  shell_exec($command);

  if (!file_exists($output_uri)) {
    watchdog('ai_alt', 'Failed to convert SVG to PNG: @uri', ['@uri' => $output_uri]);
    return FALSE;
  }

  watchdog('ai_alt', 'Successfully converted SVG to PNG: @uri', ['@uri' => $output_uri]);
  return $output_uri;
}

/**
 * Resolve the best local image URI for AI processing.
 */
function _ai_alt_prepare_image_uri($file_uri, $apply_image_style = TRUE) {
  $extension = strtolower(pathinfo($file_uri, PATHINFO_EXTENSION));

  if ($extension === 'svg') {
    $converted_uri = ai_alt_convert_svg_to_png($file_uri);
    if ($converted_uri) {
      $file_uri = $converted_uri;
    }
  }

  if (!$apply_image_style) {
    return $file_uri;
  }

  $style_name = config_get('ai_alt.settings', 'image_style') ?: 'ai_image_alt_text';
  $styled_uri = image_style_path($style_name, $file_uri);
  $style = image_style_load($style_name);
  if ($style && !file_exists($styled_uri)) {
    image_style_create_derivative($style, $file_uri, $styled_uri);
  }

  if (file_exists($styled_uri)) {
    return $styled_uri;
  }

  return $file_uri;
}

/**
 * Build a file URL for an image file, using derivative if configured.
 */
function _ai_alt_get_file_image_url($file) {
  $file_uri = _ai_alt_prepare_image_uri($file->uri, TRUE);
  return file_create_url($file_uri);
}
