<?php

/**
 * @file
 * Image preparation helpers for the AI Alt module.
 */

/**
 * Convert an SVG image to PNG for downstream processing.
 */
function _ai_alt_convert_svg_to_png($svg_uri) {
  $scheme = file_uri_scheme($svg_uri);
  if (!$scheme || !file_stream_wrapper_valid_scheme($scheme) || !in_array($scheme, array_keys(file_get_stream_wrappers(STREAM_WRAPPERS_LOCAL)), TRUE)) {
    watchdog('ai_alt', 'SVG conversion requires a local file URI: @uri', ['@uri' => $svg_uri]);
    return FALSE;
  }

  $svg_real_path = backdrop_realpath($svg_uri);
  if (!$svg_real_path || !is_readable($svg_real_path)) {
    watchdog('ai_alt', 'Could not resolve real path or file does not exist: @uri', ['@uri' => $svg_uri]);
    return FALSE;
  }

  $output_uri = preg_replace('/\.svg$/i', '.png', $svg_uri);
  $output_real_path = preg_replace('/\.svg$/i', '.png', $svg_real_path);

  if (!function_exists('proc_open')) {
    watchdog('ai_alt', 'proc_open() is not available; cannot convert SVG to PNG.');
    return FALSE;
  }

  $convert_path = '';
  foreach (explode(PATH_SEPARATOR, (string) getenv('PATH')) as $path) {
    $candidate = rtrim($path, DIRECTORY_SEPARATOR) . DIRECTORY_SEPARATOR . 'convert';
    if (is_executable($candidate)) {
      $convert_path = $candidate;
      break;
    }
  }
  if ($convert_path === '') {
    watchdog('ai_alt', 'ImageMagick `convert` command not found.');
    return FALSE;
  }

  // Refuse conversion if ImageMagick's policy does not restrict SVG delegates,
  // which could allow untrusted SVG content to fetch external resources or
  // invoke dangerous delegates (e.g. Ghostscript, HTTPS, MVG).
  $policy_descriptors = [
    0 => ['pipe', 'r'],
    1 => ['pipe', 'w'],
    2 => ['pipe', 'w'],
  ];
  $policy_process = proc_open([$convert_path, '-list', 'policy'], $policy_descriptors, $policy_pipes);
  $policy_output = '';
  if (is_resource($policy_process)) {
    fclose($policy_pipes[0]);
    $policy_output = stream_get_contents($policy_pipes[1]);
    stream_get_contents($policy_pipes[2]);
    fclose($policy_pipes[1]);
    fclose($policy_pipes[2]);
    proc_close($policy_process);
  }
  $svg_blocked = $policy_output && (
    strpos($policy_output, 'pattern: SVG') !== FALSE ||
    strpos($policy_output, 'pattern: HTTPS') !== FALSE ||
    strpos($policy_output, 'pattern: HTTP') !== FALSE
  );
  if (!$svg_blocked) {
    watchdog('ai_alt', 'SVG conversion skipped: ImageMagick policy does not restrict external resource access. Configure a policy.xml to block HTTP/HTTPS/MVG delegates before enabling SVG conversion.');
    return FALSE;
  }

  // Strip external href/xlink:href and script elements from the SVG before
  // passing it to ImageMagick as a defence-in-depth measure.
  $svg_content = file_get_contents($svg_real_path);
  if ($svg_content === FALSE) {
    watchdog('ai_alt', 'Could not read SVG file for sanitization: @path', ['@path' => $svg_real_path]);
    return FALSE;
  }
  $svg_content = preg_replace('/<script\b[^>]*>.*?<\/script>/is', '', $svg_content);
  $svg_content = preg_replace_callback('/\b(xlink:href|href)\s*=\s*(["\'])(.*?)\2/is', function ($matches) {
    return preg_match('/^(?:https?|file|ftp|data):/i', trim($matches[3])) ? '' : $matches[0];
  }, $svg_content);
  $temporary_directory = dirname($svg_real_path);
  $sanitized_path = tempnam($temporary_directory, 'ai-alt-svg-');
  $temporary_output = tempnam($temporary_directory, 'ai-alt-png-');
  if (!$sanitized_path || !$temporary_output) {
    if ($sanitized_path) {
      @unlink($sanitized_path);
    }
    if ($temporary_output) {
      @unlink($temporary_output);
    }
    watchdog('ai_alt', 'Could not create temporary files for SVG conversion.');
    return FALSE;
  }
  if (file_put_contents($sanitized_path, $svg_content) === FALSE) {
    watchdog('ai_alt', 'Could not write sanitized SVG: @path', ['@path' => $sanitized_path]);
    @unlink($sanitized_path);
    @unlink($temporary_output);
    return FALSE;
  }

  $descriptors = [
    0 => ['pipe', 'r'],
    1 => ['pipe', 'w'],
    2 => ['pipe', 'w'],
  ];
  $process = proc_open([$convert_path, $sanitized_path, 'PNG32:' . $temporary_output], $descriptors, $pipes);
  if (!is_resource($process)) {
    @unlink($sanitized_path);
    @unlink($temporary_output);
    watchdog('ai_alt', 'Could not start ImageMagick for SVG conversion.');
    return FALSE;
  }

  fclose($pipes[0]);
  stream_set_blocking($pipes[1], FALSE);
  stream_set_blocking($pipes[2], FALSE);
  $started = microtime(TRUE);
  $timed_out = FALSE;
  do {
    $status = proc_get_status($process);
    if (!$status['running']) {
      break;
    }
    if (microtime(TRUE) - $started > 30) {
      $timed_out = TRUE;
      proc_terminate($process, 9);
      break;
    }
    usleep(100000);
  } while (TRUE);

  $stderr = stream_get_contents($pipes[2]);
  fclose($pipes[1]);
  fclose($pipes[2]);
  $exit_code = proc_close($process);
  if ($exit_code === -1 && isset($status['exitcode']) && $status['exitcode'] >= 0) {
    $exit_code = $status['exitcode'];
  }
  @unlink($sanitized_path);

  if ($timed_out || $exit_code !== 0 || !file_exists($temporary_output) || filesize($temporary_output) === 0 || !image_get_info($temporary_output)) {
    @unlink($temporary_output);
    watchdog('ai_alt', 'Failed to convert SVG to PNG: @uri (@error)', [
      '@uri' => $output_uri,
      '@error' => $timed_out ? 'conversion timed out' : trim($stderr),
    ]);
    return FALSE;
  }

  if (!rename($temporary_output, $output_real_path)) {
    @unlink($temporary_output);
    watchdog('ai_alt', 'Could not publish converted SVG PNG: @uri', ['@uri' => $output_uri]);
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
    $converted_uri = _ai_alt_convert_svg_to_png($file_uri);
    if ($converted_uri) {
      $file_uri = $converted_uri;
    }
  }

  if (!$apply_image_style) {
    return $file_uri;
  }

  $style_name = config_get('ai_alt.settings', 'image_style');
  if ($style_name === NULL) {
    $style_name = 'ai_image_alt_text';
  }
  if ($style_name === '') {
    return $file_uri;
  }
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
