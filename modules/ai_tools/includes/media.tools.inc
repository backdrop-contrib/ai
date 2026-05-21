<?php


/**
 * @file
 * Tool definitions for media and vision operations.
 */

/**
 * Returns media-related tool definitions.
 */
function ai_tools_media_definitions() {
  return [
    'ai_image_to_text' => [
      'definition' => [
        'type' => 'function',
        'function' => [
          'name' => 'ai_image_to_text',
          'description' => 'Extract text or information from an image (OCR/Vision).',
          'parameters' => [
            'type' => 'object',
            'properties' => [
              'image_fid' => [
                'type' => 'integer',
                'description' => 'The file ID (fid) of the image to process.',
              ],
              'prompt' => [
                'type' => 'string',
                'description' => 'Optional instructions on what to extract (e.g. "extract all names", "read the table").',
              ],
              'model' => [
                'type' => 'string',
                'description' => 'Optional vision-capable model ID. Defaults to gpt-4o.',
              ],
            ],
            'required' => ['image_fid'],
          ],
        ],
      ],
      'execute' => 'ai_tools_execute_image_to_text',
      'module' => 'ai_tools',
      'category' => 'media',
      'operation' => 'read',
    ],
  ];
}

/**
 * Tool execution callback: Image to Text.
 */
function ai_tools_execute_image_to_text(array $args) {
  $fid = (int) ($args['image_fid'] ?? 0);
  if ($fid <= 0) {
    return ['error' => 'image_fid must be a positive integer.'];
  }
  $prompt = !empty($args['prompt']) ? $args['prompt'] : 'Extract all text from the image.';
  // Default to a known vision model if none provided.
  $model = !empty($args['model']) ? $args['model'] : (config_get('ai.settings', 'default_vision_model') ?: config_get('ai.settings', 'default_text_model') ?: 'gpt-4o');

  $file = file_load($fid);
  if (!$file || empty($file->uri)) {
    return ['error' => "File not found: $fid"];
  }
  if (empty($file->filemime) || strpos($file->filemime, 'image/') !== 0) {
    return ['error' => "File is not an image: $fid"];
  }
  if (function_exists('file_access') && !file_access('view', $file)) {
    return ['error' => "Access denied for file: $fid"];
  }

  $path = backdrop_realpath($file->uri);
  if (!$path || !is_file($path)) {
    return ['error' => "File path not found for fid $fid"];
  }
  $max_bytes = 20 * 1024 * 1024;
  $size = filesize($path);
  if ($size === FALSE || $size > $max_bytes) {
    return ['error' => "Image is too large for OCR: $fid"];
  }

  $data = base64_encode(file_get_contents($path));
  if (!$data) {
    return ['error' => "Could not read file data for fid $fid"];
  }

  $mime = !empty($file->filemime) ? $file->filemime : 'image/jpeg';
  $url = 'data:' . $mime . ';base64,' . $data;

  $messages = [
    [
      'role' => 'user',
      'content' => [
        ['type' => 'text', 'text' => $prompt],
        ['type' => 'image_url', 'image_url' => ['url' => $url]],
      ],
    ],
  ];

  try {
    $response = ai_chat($model, $messages);
    if (empty($response)) {
      return ['error' => 'AI returned an empty response.'];
    }
    return ['text' => $response];
  }
  catch (\Exception $e) {
    return ['error' => $e->getMessage()];
  }
}
