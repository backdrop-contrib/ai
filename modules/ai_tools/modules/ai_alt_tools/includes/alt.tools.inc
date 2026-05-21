<?php

/**
 * @file
 * Alt text tools for the AI Tools registry.
 */

/**
 * Tool definitions.
 */
function ai_alt_tools_definitions() {
  return [
    'find_images_missing_alt' => [
      'definition' => [
        'type' => 'function',
        'function' => [
          'name' => 'find_images_missing_alt',
          'description' => 'Find image files and inline images that are missing alt text. Use this to build a work list before generating alt text.',
          'parameters' => [
            'type' => 'object',
            'properties' => [
              'limit' => [
                'type' => 'integer',
                'description' => 'Maximum results to return. Default 20, max 100.',
              ],
              'bundle' => [
                'type' => 'string',
                'description' => 'Optional node content type machine name to restrict inline image scanning.',
              ],
              'include_inline' => [
                'type' => 'boolean',
                'description' => 'Also scan text fields for inline images missing alt text.',
              ],
            ],
            'required' => [],
          ],
        ],
      ],
      'execute' => 'ai_alt_tools_execute_find_images_missing_alt',
      'module' => 'ai_alt_tools',
      'category' => 'media',
    ],
    'get_image_details' => [
      'definition' => [
        'type' => 'function',
        'function' => [
          'name' => 'get_image_details',
          'description' => 'Get file metadata, current alt text, and usage context for one image.',
          'parameters' => [
            'type' => 'object',
            'properties' => [
              'fid' => [
                'type' => 'integer',
                'description' => 'File ID.',
              ],
            ],
            'required' => ['fid'],
          ],
        ],
      ],
      'execute' => 'ai_alt_tools_execute_get_image_details',
      'module' => 'ai_alt_tools',
      'category' => 'media',
    ],
    'generate_alt_for_image' => [
      'definition' => [
        'type' => 'function',
        'function' => [
          'name' => 'generate_alt_for_image',
          'description' => 'Generate concise alt text for an image file and save it to the file entity.',
          'parameters' => [
            'type' => 'object',
            'properties' => [
              'fid' => [
                'type' => 'integer',
                'description' => 'File ID.',
              ],
              'context' => [
                'type' => 'string',
                'description' => 'Optional usage context to improve the generated alt text.',
              ],
              'force' => [
                'type' => 'boolean',
                'description' => 'Overwrite existing alt text if true.',
              ],
            ],
            'required' => ['fid'],
          ],
        ],
      ],
      'execute' => 'ai_alt_tools_execute_generate_alt_for_image',
      'module' => 'ai_alt_tools',
      'category' => 'media',
    ],
    'seed_alt_for_image_from_usage' => [
      'definition' => [
        'type' => 'function',
        'function' => [
          'name' => 'seed_alt_for_image_from_usage',
          'description' => 'Seed global alt text for an image file from existing alt text found in image field usage or inline image markup.',
          'parameters' => [
            'type' => 'object',
            'properties' => [
              'fid' => [
                'type' => 'integer',
                'description' => 'File ID.',
              ],
              'force' => [
                'type' => 'boolean',
                'description' => 'Overwrite existing global alt text if true.',
              ],
            ],
            'required' => ['fid'],
          ],
        ],
      ],
      'execute' => 'ai_alt_tools_execute_seed_alt_for_image_from_usage',
      'module' => 'ai_alt_tools',
      'category' => 'media',
    ],
    'copy_usage_alt_to_global' => [
      'definition' => [
        'type' => 'function',
        'function' => [
          'name' => 'copy_usage_alt_to_global',
          'description' => 'Copy existing local usage alt text into the global alt text field. No image analysis. No paraphrase.',
          'parameters' => [
            'type' => 'object',
            'properties' => [
              'fid' => [
                'type' => 'integer',
                'description' => 'File ID.',
              ],
              'force' => [
                'type' => 'boolean',
                'description' => 'Overwrite existing global alt text if true.',
              ],
            ],
            'required' => ['fid'],
          ],
        ],
      ],
      'execute' => 'ai_alt_tools_execute_seed_alt_for_image_from_usage',
      'module' => 'ai_alt_tools',
      'category' => 'media',
    ],
    'set_alt_for_image' => [
      'definition' => [
        'type' => 'function',
        'function' => [
          'name' => 'set_alt_for_image',
          'description' => 'Set the alt text on a file entity directly without calling AI.',
          'parameters' => [
            'type' => 'object',
            'properties' => [
              'fid' => [
                'type' => 'integer',
                'description' => 'File ID.',
              ],
              'alt_text' => [
                'type' => 'string',
                'description' => 'Alt text to save.',
              ],
            ],
            'required' => ['fid', 'alt_text'],
          ],
        ],
      ],
      'execute' => 'ai_alt_tools_execute_set_alt_for_image',
      'module' => 'ai_alt_tools',
      'category' => 'media',
    ],
    'rename_file_for_image' => [
      'definition' => [
        'type' => 'function',
        'function' => [
          'name' => 'rename_file_for_image',
          'description' => 'Generate a short AI label for an image file and rename the stored file to match it, then rewrite exact stored image URLs to the new URL.',
          'parameters' => [
            'type' => 'object',
            'properties' => [
              'fid' => [
                'type' => 'integer',
                'description' => 'File ID.',
              ],
            ],
            'required' => ['fid'],
          ],
        ],
      ],
      'execute' => 'ai_alt_tools_execute_rename_file_for_image',
      'module' => 'ai_alt_tools',
      'category' => 'media',
    ],
  ];
}

/**
 * Find image files and inline images that are missing alt text.
 */
function ai_alt_tools_execute_find_images_missing_alt(array $args) {
  $limit = min(max(1, (int) (!empty($args['limit']) ? $args['limit'] : 20)), 100);
  $include_inline = !empty($args['include_inline']);
  $bundle = !empty($args['bundle']) ? trim((string) $args['bundle']) : '';

  $alt_field = ai_alt_tools_alt_field_name();
  $alt_table = ai_alt_tools_alt_table($alt_field);
  if (!$alt_table) {
    return ['error' => 'No file alt field table found. Check image_entity_alt settings.'];
  }

  $images = [];
  $total_missing = 0;

  $file_query = db_select('file_managed', 'f')
    ->fields('f', ['fid', 'filename', 'uri', 'filemime'])
    ->range(0, $limit * 2);
  $file_query->condition('f.filemime', 'image/%', 'LIKE');
  $file_query->leftJoin($alt_table, 'a', 'a.entity_id = f.fid AND a.entity_type = :entity_type AND a.deleted = 0 AND a.language = :langcode AND a.delta = 0', [
    ':entity_type' => 'file',
    ':langcode' => LANGUAGE_NONE,
  ]);
  $file_query->fields('a', [$alt_field . '_value']);
  $file_query->where('(a.' . $alt_field . '_value IS NULL OR a.' . $alt_field . '_value = :empty)', [':empty' => '']);
  $file_rows = $file_query->execute()->fetchAll(PDO::FETCH_ASSOC);
  $total_missing += count($file_rows);

  foreach ($file_rows as $row) {
    if (count($images) >= $limit) {
      break;
    }
    $file = file_load((int) $row['fid']);
    if (!$file) {
      continue;
    }
    if (!ai_alt_tools_file_has_allowed_usage($file)) {
      continue;
    }
    $images[] = ai_alt_tools_build_file_image_row($file);
  }

  if ($include_inline && count($images) < $limit) {
    list($inline_rows, $inline_total) = ai_alt_tools_find_inline_missing_alt($bundle, $limit - count($images));
    $total_missing += $inline_total;
    $images = array_merge($images, $inline_rows);
  }

  return [
    'total_missing' => $total_missing,
    'returned' => count($images),
    'images' => $images,
  ];
}

/**
 * Get details for one image file.
 */
function ai_alt_tools_execute_get_image_details(array $args) {
  $fid = (int) (!empty($args['fid']) ? $args['fid'] : 0);
  if (!$fid) {
    return ['error' => 'fid must be a positive integer.'];
  }

  $file = file_load($fid);
  if (!$file) {
    return ['error' => "No file found with fid $fid."];
  }
  if (strpos((string) $file->filemime, 'image/') !== 0) {
    return ['error' => "File $fid is not an image."];
  }

  $details = ai_alt_tools_build_file_image_row($file);
  $details['dimensions'] = image_get_info($file->uri);
  $details['usages'] = ai_alt_tools_get_file_usage_contexts($file, 20);
  $details['inline_usages'] = ai_alt_tools_find_inline_usage_contexts($fid, 20);

  return $details;
}

/**
 * Generate alt text for a file and save it.
 */
function ai_alt_tools_execute_generate_alt_for_image(array $args) {
  $fid = (int) (!empty($args['fid']) ? $args['fid'] : 0);
  if (!$fid) {
    return ['error' => 'fid must be a positive integer.'];
  }

  $file = file_load($fid);
  if (!$file) {
    return ['error' => "No file found with fid $fid."];
  }
  if (strpos((string) $file->filemime, 'image/') !== 0) {
    return ['error' => "File $fid is not an image."];
  }

  $current_alt = ai_alt_tools_get_file_alt_text($file);
  $force = !empty($args['force']);
  if (!$force && trim($current_alt) !== '') {
    return [
      'fid' => $fid,
      'alt_text' => $current_alt,
      'saved' => FALSE,
      'skipped' => TRUE,
      'reason' => 'Alt text already exists.',
    ];
  }

  $model = ai_alt_tools_default_chat_model();
  if ($model === '') {
    return ['error' => 'No chat model available.'];
  }

  $context = !empty($args['context']) ? trim((string) $args['context']) : '';
  $prompt = 'Write concise, accurate alt text for this image.';
  if ($context !== '') {
    $prompt .= "\nContext: " . $context;
  }
  $prompt .= "\nReturn alt text only. No quotes. No prefix.";

  $messages = [
    [
      'role' => 'system',
      'content' => 'You write accessible alt text for web images. Keep it concise and factual.',
    ],
    [
      'role' => 'user',
      'content' => [
        [
          'type' => 'text',
          'text' => $prompt,
        ],
        [
          'type' => 'image_url',
          'image_url' => [
            'url' => file_create_url($file->uri),
          ],
        ],
      ],
    ],
  ];

  try {
    $alt_text = ai_chat($model, $messages, 0.2, 120, FALSE, [
      'operation' => 'ai_alt_tools_generate_alt',
    ]);
  }
  catch (\Exception $e) {
    return ['error' => $e->getMessage()];
  }

  $alt_text = ai_alt_tools_normalize_alt_text($alt_text);
  if ($alt_text === '') {
    return ['error' => 'Model returned empty alt text.'];
  }

  ai_alt_tools_set_file_alt_text($file, $alt_text);

  return [
    'fid' => $fid,
    'filename' => $file->filename,
    'alt_text' => $alt_text,
    'saved' => TRUE,
  ];
}

/**
 * Seed global alt text from existing usage alt text.
 */
function ai_alt_tools_execute_seed_alt_for_image_from_usage(array $args) {
  $fid = (int) (!empty($args['fid']) ? $args['fid'] : 0);
  if (!$fid) {
    return ['error' => 'fid must be a positive integer.'];
  }

  $file = file_load($fid);
  if (!$file) {
    return ['error' => "No file found with fid $fid."];
  }
  if (strpos((string) $file->filemime, 'image/') !== 0) {
    return ['error' => "File $fid is not an image."];
  }
  if (!ai_alt_tools_file_has_allowed_usage($file)) {
    return [
      'fid' => $fid,
      'saved' => FALSE,
      'skipped' => TRUE,
      'reason' => 'File is used only on excluded content types.',
    ];
  }

  $current_alt = ai_alt_tools_get_file_alt_text($file);
  $force = !empty($args['force']);
  if (!$force && trim($current_alt) !== '') {
    return [
      'fid' => $fid,
      'alt_text' => $current_alt,
      'saved' => FALSE,
      'skipped' => TRUE,
      'reason' => 'Alt text already exists.',
    ];
  }

  $seed = ai_alt_tools_find_alt_from_usage($file);
  if ($seed === FALSE) {
    return [
      'fid' => $fid,
      'saved' => FALSE,
      'skipped' => TRUE,
      'reason' => 'No alt text found in file usage.',
    ];
  }

  $alt_text = ai_alt_tools_normalize_alt_text($seed['alt_text']);
  if ($alt_text === '') {
    return [
      'fid' => $fid,
      'saved' => FALSE,
      'skipped' => TRUE,
      'reason' => 'Found alt text was empty after normalization.',
    ];
  }

  ai_alt_tools_set_file_alt_text($file, $alt_text);

  return [
    'fid' => $fid,
    'filename' => $file->filename,
    'alt_text' => $alt_text,
    'saved' => TRUE,
    'source' => $seed['source'],
  ];
}

/**
 * Set alt text directly.
 */
function ai_alt_tools_execute_set_alt_for_image(array $args) {
  $fid = (int) (!empty($args['fid']) ? $args['fid'] : 0);
  $alt_text = isset($args['alt_text']) ? trim((string) $args['alt_text']) : '';
  if (!$fid) {
    return ['error' => 'fid must be a positive integer.'];
  }
  if ($alt_text === '') {
    return ['error' => 'alt_text must not be empty.'];
  }

  $file = file_load($fid);
  if (!$file) {
    return ['error' => "No file found with fid $fid."];
  }

  ai_alt_tools_set_file_alt_text($file, $alt_text);

  return [
    'fid' => $fid,
    'filename' => $file->filename,
    'alt_text' => $alt_text,
    'saved' => TRUE,
  ];
}

/**
 * Rename an image file from a short AI label.
 */
function ai_alt_tools_execute_rename_file_for_image(array $args) {
  $fid = (int) (!empty($args['fid']) ? $args['fid'] : 0);
  if (!$fid) {
    return ['error' => 'fid must be a positive integer.'];
  }

  $file = file_load($fid);
  if (!$file) {
    return ['error' => "No file found with fid $fid."];
  }
  if (strpos((string) $file->filemime, 'image/') !== 0) {
    return ['error' => "File $fid is not an image."];
  }

  module_load_include('module', 'ai_alt');
  if (!function_exists('ai_alt_generate_short_filename_for_file') || !function_exists('ai_alt_move_file_to_filename')) {
    return ['error' => 'Rename helper unavailable.'];
  }

  if (!function_exists('ai_alt_file_uses_local_stream_wrapper') || !ai_alt_file_uses_local_stream_wrapper($file->uri)) {
    return [
      'fid' => $fid,
      'saved' => FALSE,
      'skipped' => TRUE,
      'reason' => 'File storage is not local; rename skipped to avoid breaking remote URLs.',
    ];
  }

  $new_filename = ai_alt_generate_short_filename_for_file($file);
  if ($new_filename === FALSE || $new_filename === '') {
    return [
      'fid' => $fid,
      'saved' => FALSE,
      'skipped' => TRUE,
      'reason' => 'Could not generate a filename label.',
    ];
  }

  if (backdrop_basename($file->uri) === $new_filename) {
    return [
      'fid' => $fid,
      'filename' => $file->filename,
      'saved' => FALSE,
      'skipped' => TRUE,
      'reason' => 'Filename already matches generated label.',
    ];
  }

  $moved_file = ai_alt_move_file_to_filename($file, $new_filename);
  if (!$moved_file) {
    return [
      'fid' => $fid,
      'saved' => FALSE,
      'skipped' => FALSE,
      'error' => 'Failed to move file to new filename.',
    ];
  }

  return [
    'fid' => $fid,
    'filename' => $moved_file->filename,
    'uri' => $moved_file->uri,
    'saved' => TRUE,
  ];
}

/**
 * Find alt text for a file from referencing entities.
 *
 * Returns ['alt_text' => string, 'source' => [...]] or FALSE.
 */
function ai_alt_tools_find_alt_from_usage($file) {
  $usages = file_usage_list($file);
  if (empty($usages)) {
    return FALSE;
  }

  $rows = [];
  foreach ($usages as $module => $types) {
    foreach ($types as $type => $ids) {
      foreach ($ids as $id => $count) {
        $rows[] = [
          'module' => $module,
          'type' => $type,
          'id' => (int) $id,
        ];
      }
    }
  }

  usort($rows, function ($a, $b) {
    return strcmp($a['module'] . ':' . $a['type'] . ':' . $a['id'], $b['module'] . ':' . $b['type'] . ':' . $b['id']);
  });

  foreach ($rows as $usage) {
    if (!ai_alt_tools_can_load_entity_type($usage['type'])) {
      continue;
    }

    $entities = entity_load($usage['type'], [$usage['id']]);
    if (empty($entities[$usage['id']])) {
      continue;
    }

    $alt = ai_alt_tools_scan_entity_for_file_alt($usage['type'], $entities[$usage['id']], $file);
    if ($alt !== FALSE) {
      return [
        'alt_text' => $alt,
        'source' => $usage,
      ];
    }
  }

  return FALSE;
}

/**
 * Check whether an entity type can be loaded with entity_load().
 */
function ai_alt_tools_can_load_entity_type($entity_type) {
  $info = entity_get_info($entity_type);
  return !empty($info);
}

/**
 * Recursively scan an entity for alt text for one file.
 */
function ai_alt_tools_scan_entity_for_file_alt($entity_type, $entity, $file) {
  $entity_info = entity_get_info($entity_type);
  if (empty($entity_info)) {
    return FALSE;
  }

  $bundle_key = !empty($entity_info['entity keys']['bundle']) ? $entity_info['entity keys']['bundle'] : NULL;
  $bundle = $bundle_key && isset($entity->{$bundle_key}) ? $entity->{$bundle_key} : $entity_type;
  $instances = field_info_instances($entity_type, $bundle);
  $lang = LANGUAGE_NONE;

  foreach ($instances as $field_name => $instance) {
    $field_info = field_info_field($field_name);
    if (empty($field_info['type']) || empty($entity->{$field_name}[$lang])) {
      continue;
    }

    switch ($field_info['type']) {
      case 'image':
        foreach ($entity->{$field_name}[$lang] as $item) {
          if (!empty($item['fid']) && (int) $item['fid'] === (int) $file->fid && !empty($item['alt'])) {
            $alt = trim($item['alt']);
            if ($alt !== '') {
              return $alt;
            }
          }
        }
        break;

      case 'text':
      case 'text_long':
      case 'text_with_summary':
        foreach ($entity->{$field_name}[$lang] as $item) {
          if (empty($item['value'])) {
            continue;
          }
          $alt = ai_alt_tools_find_alt_in_html($item['value'], $file);
          if ($alt !== FALSE) {
            return $alt;
          }
        }
        break;

      case 'paragraphs':
        foreach ($entity->{$field_name}[$lang] as $item) {
          if (empty($item['value'])) {
            continue;
          }
          $children = entity_load('paragraphs_item', [$item['value']]);
          if (empty($children[$item['value']])) {
            continue;
          }
          $alt = ai_alt_tools_scan_entity_for_file_alt('paragraphs_item', $children[$item['value']], $file);
          if ($alt !== FALSE) {
            return $alt;
          }
        }
        break;
    }
  }

  return FALSE;
}

/**
 * Find alt text for a file within HTML.
 */
function ai_alt_tools_find_alt_in_html($html, $file) {
  $img_tags = ai_alt_tools_parse_img_tags($html);

  foreach ($img_tags as $info) {
    $matched = FALSE;

    if (!empty($info['fid']) && (int) $info['fid'] === (int) $file->fid) {
      $matched = TRUE;
    }
    elseif (!empty($info['src'])) {
      $resolved_fid = ai_alt_tools_src_to_fid($info['src']);
      if ($resolved_fid && (int) $resolved_fid === (int) $file->fid) {
        $matched = TRUE;
      }
    }

    if ($matched && !empty($info['alt'])) {
      return $info['alt'];
    }
  }

  return FALSE;
}

/**
 * Parse inline img tags from HTML.
 */
function ai_alt_tools_parse_img_tags($html) {
  $found = [];

  if (!preg_match_all('/<img[^>]+>/is', (string) $html, $matches)) {
    return $found;
  }

  foreach ($matches[0] as $tag) {
    $info = [];

    if (preg_match('/data-file-id\s*=\s*["\']?(\d+)["\']?/i', $tag, $m)) {
      $info['fid'] = (int) $m[1];
    }

    if (preg_match('/\balt\s*=\s*(["\'])(.*?)\1/is', $tag, $m)) {
      $alt = trim(html_entity_decode($m[2], ENT_QUOTES, 'UTF-8'));
      if ($alt !== '') {
        $info['alt'] = $alt;
      }
    }

    if (preg_match('/\bsrc\s*=\s*(["\'])(.*?)\1/is', $tag, $m)) {
      $info['src'] = $m[2];
    }

    $found[] = $info;
  }

  return $found;
}

/**
 * Resolve an image src URL to a Backdrop fid.
 */
function ai_alt_tools_src_to_fid($src) {
  if (empty($src)) {
    return FALSE;
  }

  $src = preg_replace('/[?#].*$/', '', $src);
  global $base_url;
  if (strpos($src, $base_url) === 0) {
    $src = substr($src, strlen($base_url));
  }
  $src = ltrim($src, '/');

  $public_path = rtrim(config_get('system.core', 'file_public_path') ?: 'files', '/');
  $private_path = rtrim(config_get('system.core', 'file_private_path') ?: '', '/');

  if ($public_path && strpos($src, $public_path . '/') === 0) {
    $file_path = substr($src, strlen($public_path) + 1);
    if (preg_match('#^styles/[^/]+/public/(.+)$#', $file_path, $m)) {
      $file_path = $m[1];
    }
    $uri = 'public://' . $file_path;
  }
  elseif ($private_path && strpos($src, $private_path . '/') === 0) {
    $file_path = substr($src, strlen($private_path) + 1);
    if (preg_match('#^styles/[^/]+/private/(.+)$#', $file_path, $m)) {
      $file_path = $m[1];
    }
    $uri = 'private://' . $file_path;
  }
  else {
    return FALSE;
  }

  return db_select('file_managed', 'fm')
    ->fields('fm', ['fid'])
    ->condition('uri', $uri)
    ->execute()
    ->fetchField();
}

/**
 * Build a result row for a file image.
 */
function ai_alt_tools_build_file_image_row($file) {
  return [
    'source' => 'file',
    'fid' => (int) $file->fid,
    'filename' => $file->filename,
    'url' => file_create_url($file->uri),
    'global_alt' => ai_alt_tools_get_file_alt_text($file),
    'usages' => ai_alt_tools_get_file_usage_contexts($file, 10),
  ];
}

/**
 * Return node bundles excluded from alt workflows.
 *
 * Uses AI Privacy config when available.
 */
function ai_alt_tools_excluded_node_bundles() {
  if (!module_exists('ai_privacy')) {
    return [];
  }

  $denied = config_get('ai_privacy.settings', 'denied_entity_bundles');
  if (empty($denied) || !is_array($denied)) {
    return [];
  }

  $bundles = isset($denied['node']) && is_array($denied['node']) ? $denied['node'] : [];
  return array_values(array_filter(array_map('trim', $bundles)));
}

/**
 * Return TRUE if file has at least one allowed node usage, or no node usage.
 */
function ai_alt_tools_file_has_allowed_usage($file) {
  $excluded_fids = ai_alt_tools_excluded_file_ids();
  if (empty($excluded_fids)) {
    return TRUE;
  }

  return !in_array((int) $file->fid, $excluded_fids, TRUE);
}

/**
 * Return file IDs that are used only by excluded node bundles.
 */
function ai_alt_tools_excluded_file_ids() {
  static $cached = NULL;
  if ($cached !== NULL) {
    return $cached;
  }

  $excluded = ai_alt_tools_excluded_node_bundles();
  if (empty($excluded)) {
    $cached = [];
    return $cached;
  }

  $denied_query = db_select('file_usage', 'fu');
  $denied_query->fields('fu', ['fid']);
  $denied_query->distinct();
  $denied_query->condition('fu.type', 'node');
  $denied_query->join('node', 'n', 'n.nid = fu.id');
  $denied_query->condition('n.type', $excluded, 'IN');
  $denied = $denied_query->execute()->fetchCol();

  if (empty($denied)) {
    $cached = [];
    return $cached;
  }

  $allowed_query = db_select('file_usage', 'fu');
  $allowed_query->fields('fu', ['fid']);
  $allowed_query->distinct();
  $allowed_query->condition('fu.type', 'node');
  $allowed_query->join('node', 'n', 'n.nid = fu.id');
  $allowed_query->condition('n.type', $excluded, 'NOT IN');
  $allowed = $allowed_query->execute()->fetchCol();

  $cached = array_values(array_diff($denied, $allowed));
  return $cached;
}

/**
 * Return configured file alt field name.
 */
function ai_alt_tools_alt_field_name() {
  if (function_exists('image_entity_alt_field_name')) {
    return image_entity_alt_field_name();
  }
  return 'field_alt';
}

/**
 * Return SQL table name for configured file alt field.
 */
function ai_alt_tools_alt_table($alt_field) {
  $field = field_info_field($alt_field);
  if (empty($field) || empty($field['field_name'])) {
    return '';
  }
  return _field_sql_storage_tablename($field);
}

/**
 * Resolve available chat model from AI settings.
 */
function ai_alt_tools_default_chat_model() {
  $configured = trim((string) config_get('ai.settings', 'default_text_model'));
  $models_grouped = function_exists('ai_get_all_chat_models') ? ai_get_all_chat_models() : [];

  $flat = [];
  foreach ($models_grouped as $group) {
    if (!is_array($group)) {
      continue;
    }
    foreach ($group as $model_id => $label) {
      $flat[$model_id] = $label;
    }
  }

  if ($configured !== '' && (empty($flat) || isset($flat[$configured]))) {
    return $configured;
  }

  if (!empty($flat)) {
    reset($flat);
    return key($flat);
  }

  return config_get('ai.settings', 'default_text_model') ?: 'gpt-4o-mini';
}

/**
 * Load current file alt value.
 */
function ai_alt_tools_get_file_alt_text($file) {
  $alt_field = ai_alt_tools_alt_field_name();
  $alt_table = ai_alt_tools_alt_table($alt_field);
  if (!$alt_table) {
    return '';
  }

  $value = db_select($alt_table, 'a')
    ->fields('a', [$alt_field . '_value'])
    ->condition('a.entity_type', 'file')
    ->condition('a.entity_id', $file->fid)
    ->condition('a.deleted', 0)
    ->condition('a.language', LANGUAGE_NONE)
    ->condition('a.delta', 0)
    ->range(0, 1)
    ->execute()
    ->fetchField();

  return is_string($value) ? $value : '';
}

/**
 * Save alt text on file entity.
 */
function ai_alt_tools_set_file_alt_text($file, $alt_text) {
  $alt_field = ai_alt_tools_alt_field_name();
  $langcode = LANGUAGE_NONE;
  $file->{$alt_field} = [
    $langcode => [
      [
        'value' => $alt_text,
      ],
    ],
  ];
  file_save($file);
}

/**
 * Normalise AI output.
 */
function ai_alt_tools_normalize_alt_text($text) {
  $text = trim((string) $text);
  $text = preg_replace('/^\s*(alt text|alt|caption)\s*:\s*/i', '', $text);
  $text = trim($text, " \t\n\r\0\x0B\"'");
  return trim(preg_replace('/\s+/', ' ', $text));
}

/**
 * Return file usage contexts.
 */
function ai_alt_tools_get_file_usage_contexts($file, $limit = 10) {
  $usage = file_usage_list($file);
  $rows = [];

  foreach ($usage as $module => $types) {
    foreach ($types as $type => $ids) {
      foreach ($ids as $id => $count) {
        $row = [
          'module' => $module,
          'type' => $type,
          'id' => (int) $id,
          'count' => (int) $count,
        ];
        if ($type === 'node') {
          $node = node_load((int) $id);
          if ($node) {
            $row['entity_type'] = 'node';
            $row['title'] = $node->title;
            $row['bundle'] = $node->type;
          }
        }
        $rows[] = $row;
      }
    }
  }

  usort($rows, function ($a, $b) {
    return strcmp($a['module'] . ':' . $a['type'] . ':' . $a['id'], $b['module'] . ':' . $b['type'] . ':' . $b['id']);
  });

  return array_slice($rows, 0, $limit);
}

/**
 * Find inline images missing alt text.
 */
function ai_alt_tools_find_inline_missing_alt($bundle = '', $limit = 20) {
  $results = [];
  $total = 0;
  $bundles = [];

  if ($bundle !== '') {
    $bundles = [$bundle];
  }
  else {
    $bundles = array_keys(node_type_get_types());
  }

  foreach ($bundles as $bundle_name) {
    $instances = field_info_instances('node', $bundle_name);
    if (empty($instances)) {
      continue;
    }

    $text_fields = [];
    foreach ($instances as $field_name => $instance) {
      $field = field_info_field($field_name);
      if (empty($field['type']) || !in_array($field['type'], ['text', 'text_long', 'text_with_summary'], TRUE)) {
        continue;
      }
      $text_fields[] = $field_name;
    }

    if (empty($text_fields)) {
      continue;
    }

    $nids = db_select('node', 'n')
      ->fields('n', ['nid'])
      ->condition('n.type', $bundle_name)
      ->condition('n.status', 1)
      ->orderBy('n.changed', 'DESC')
      ->range(0, 100)
      ->execute()
      ->fetchCol();

    if (empty($nids)) {
      continue;
    }

    $nodes = node_load_multiple($nids);
    foreach ($nodes as $node) {
      foreach ($text_fields as $field_name) {
        $items = field_get_items('node', $node, $field_name);
        if (empty($items)) {
          continue;
        }
        foreach ($items as $delta => $item) {
        foreach (ai_alt_tools_extract_inline_images((string) ($item['value'] ?? '')) as $match) {
          if (!empty($match['alt'])) {
            continue;
          }
          $total++;
          $results[] = [
            'source' => 'inline',
            'entity_type' => 'node',
            'entity_id' => (int) $node->nid,
              'bundle' => $node->type,
              'title' => $node->title,
              'field' => $field_name,
              'delta' => (int) $delta,
              'src' => $match['src'],
              'fid' => !empty($match['fid']) ? (int) $match['fid'] : NULL,
            ];
            if (count($results) >= $limit) {
              return [$results, $total];
            }
          }
        }
      }
    }
  }

  return [$results, $total];
}

/**
 * Find inline image usage contexts for one fid.
 */
function ai_alt_tools_find_inline_usage_contexts($fid, $limit = 20) {
  $matches = [];
  $nids = db_query("SELECT DISTINCT n.nid FROM {node} n WHERE n.status = 1 ORDER BY n.changed DESC LIMIT 100")->fetchCol();
  if (empty($nids)) {
    return $matches;
  }

  $nodes = node_load_multiple($nids);
  foreach ($nodes as $node) {
    $instances = field_info_instances('node', $node->type);
    if (empty($instances)) {
      continue;
    }
    foreach ($instances as $field_name => $instance) {
      $field = field_info_field($field_name);
      if (empty($field['type']) || !in_array($field['type'], ['text', 'text_long', 'text_with_summary'], TRUE)) {
        continue;
      }
      $items = field_get_items('node', $node, $field_name);
      if (empty($items)) {
        continue;
      }
      foreach ($items as $delta => $item) {
        foreach (ai_alt_tools_extract_inline_images((string) ($item['value'] ?? '')) as $match) {
          if (empty($match['fid']) || (int) $match['fid'] !== (int) $fid) {
            continue;
          }
          $matches[] = [
            'entity_type' => 'node',
            'entity_id' => (int) $node->nid,
            'bundle' => $node->type,
            'title' => $node->title,
            'field' => $field_name,
            'delta' => (int) $delta,
            'src' => $match['src'],
          ];
          if (count($matches) >= $limit) {
            return $matches;
          }
        }
      }
    }
  }

  return $matches;
}

/**
 * Extract inline image tags from HTML.
 */
function ai_alt_tools_extract_inline_images($html) {
  $results = [];
  $html = trim((string) $html);
  if ($html === '' || stripos($html, '<img') === FALSE) {
    return $results;
  }

  $previous = libxml_use_internal_errors(TRUE);
  $dom = new DOMDocument();
  $dom->loadHTML('<?xml encoding="utf-8" ?>' . $html);
  libxml_clear_errors();
  libxml_use_internal_errors($previous);

  $images = $dom->getElementsByTagName('img');
  foreach ($images as $img) {
    $src = trim((string) $img->getAttribute('src'));
    $alt = trim((string) $img->getAttribute('alt'));
    $results[] = [
      'src' => $src,
      'alt' => $alt,
      'fid' => ai_alt_tools_resolve_fid_from_src($src),
    ];
  }

  return $results;
}

/**
 * Best-effort resolution from img src to fid.
 */
function ai_alt_tools_resolve_fid_from_src($src) {
  $src = trim((string) $src);
  if ($src === '' || stripos($src, 'data:image/') === 0) {
    return NULL;
  }

  $path = parse_url($src, PHP_URL_PATH);
  $basename = !empty($path) ? basename($path) : basename($src);
  if ($basename === '') {
    return NULL;
  }

  $files = db_select('file_managed', 'f')
    ->fields('f', ['fid', 'uri', 'filename'])
    ->condition('f.filemime', 'image/%', 'LIKE')
    ->condition('f.filename', $basename)
    ->range(0, 50)
    ->execute()
    ->fetchAll(PDO::FETCH_ASSOC);

  foreach ($files as $file) {
    $url = file_create_url($file['uri']);
    if ($url === $src || $file['filename'] === $basename) {
      return (int) $file['fid'];
    }
  }

  return NULL;
}

