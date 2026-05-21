<?php


/**
 * @file
 * Content type tools for the AI Tools registry.
 */

function ai_tools_content_type_definitions() {
  return [
    'list_content_types' => [
      'definition' => [
        'type'     => 'function',
        'function' => [
          'name'        => 'list_content_types',
          'description' => 'List all content types on this Backdrop site with their machine names, labels, add-content descriptions, and form help text.',
          'parameters'  => [
            'type'       => 'object',
            'properties' => new stdClass(),
            'required'   => [],
          ],
        ],
      ],
      'execute'   => 'ai_tools_execute_list_content_types',
      'module'    => 'ai_tools',
      'category'  => 'content_type',
      'operation' => 'read',
    ],
    'get_content_type_info' => [
      'definition' => [
        'type'     => 'function',
        'function' => [
          'name'        => 'get_content_type_info',
          'description' => 'Get detailed information about a Backdrop content type including its fields, labels, add-content description, form help text, and settings.',
          'parameters'  => [
            'type'       => 'object',
            'properties' => [
              'content_type' => [
                'type'        => 'string',
                'description' => 'Machine name of the content type (e.g. "post", "page").',
              ],
            ],
            'required' => ['content_type'],
          ],
        ],
      ],
      'execute'   => 'ai_tools_execute_get_content_type_info',
      'module'    => 'ai_tools',
      'category'  => 'content_type',
      'operation' => 'read',
    ],
    'create_content_type' => [
      'definition' => [
        'type'     => 'function',
        'function' => [
          'name'        => 'create_content_type',
          'description' => 'Create a new Backdrop content type. If the user also asks for fields, do this as phase 1: call create_content_type, then get_content_type_info to verify the bundle exists, then create field storage/instances.',
          'parameters'  => [
            'type'       => 'object',
            'properties' => [
              'type' => [
                'type'        => 'string',
                'description' => 'Machine name (lowercase letters, numbers, underscores only; max 32 chars).',
              ],
              'name' => [
                'type'        => 'string',
                'description' => 'Human-readable label.',
              ],
              'description' => [
                'type'        => 'string',
                'description' => 'Optional add-content page description. This is what the "Provide description" checkbox controls in the content type form.',
              ],
              'help' => [
                'type'        => 'string',
                'description' => 'Optional explanation or submission guidelines shown at the top of the create/edit form for this content type.',
              ],
              'title_label' => [
                'type'        => 'string',
                'description' => 'Label for the title field. Defaults to "Title".',
              ],
            ],
            'required' => ['type', 'name'],
          ],
        ],
      ],
      'execute'     => 'ai_tools_execute_create_content_type',
      'module'      => 'ai_tools',
      'category'    => 'content_type',
      'operation'   => 'write',
      'destructive' => FALSE,
      'permission'  => 'administer content types',
    ],
    'edit_content_type' => [
      'definition' => [
        'type'     => 'function',
        'function' => [
          'name'        => 'edit_content_type',
          'description' => 'Edit an existing Backdrop content type — change its label, add-content description, form help text, or title field label.',
          'parameters'  => [
            'type'       => 'object',
            'properties' => [
              'type' => [
                'type'        => 'string',
                'description' => 'Machine name of the content type to edit.',
              ],
              'name' => [
                'type'        => 'string',
                'description' => 'New human-readable label.',
              ],
              'description' => [
                'type'        => 'string',
                'description' => 'New add-content page description. This is what the "Provide description" checkbox controls in the content type form.',
              ],
              'help' => [
                'type'        => 'string',
                'description' => 'New explanation or submission guidelines shown at the top of the create/edit form for this content type.',
              ],
              'title_label' => [
                'type'        => 'string',
                'description' => 'New label for the title field.',
              ],
            ],
            'required' => ['type'],
          ],
        ],
      ],
      'execute'     => 'ai_tools_execute_edit_content_type',
      'module'      => 'ai_tools',
      'category'    => 'content_type',
      'operation'   => 'write',
      'destructive' => FALSE,
      'permission'  => 'administer content types',
    ],
  ];
}

// -----------------------------------------------------------------------
// Executors
// -----------------------------------------------------------------------

function ai_tools_execute_list_content_types(array $args) {
  $types = node_type_get_types();
  $result = [];
  foreach ($types as $type) {
    $result[] = [
      'machine_name' => $type->type,
      'label'        => $type->name,
      'description'  => $type->description,
      'help'         => $type->help,
      'title_label'  => $type->title_label,
    ];
  }
  usort($result, function ($a, $b) { return strcmp($a['machine_name'], $b['machine_name']); });
  return ['content_types' => $result, 'count' => count($result)];
}

function ai_tools_execute_get_content_type_info(array $args) {
  $type_name = trim($args['content_type'] ?? '');
  if ($type_name === '') {
    return ['error' => 'content_type is required.'];
  }
  $type = node_type_load($type_name);
  if (!$type) {
    return ['error' => "Content type '$type_name' not found."];
  }
  $fields = field_info_instances('node', $type_name);
  $field_summary = [];
  foreach ($fields as $field_name => $instance) {
    $field_info = field_info_field($field_name);
    $field_summary[] = [
      'machine_name' => $field_name,
      'label'        => $instance['label'],
      'type'         => $field_info['type'] ?? 'unknown',
      'required'     => !empty($instance['required']),
      'cardinality'  => $field_info['cardinality'] ?? 1,
    ];
  }
  return [
    'machine_name' => $type->type,
    'label'        => $type->name,
    'description'  => $type->description,
    'help'         => $type->help,
    'title_label'  => $type->title_label,
    'field_count'  => count($field_summary),
    'fields'       => $field_summary,
  ];
}

function ai_tools_execute_create_content_type(array $args) {
  $type_name = trim($args['type'] ?? '');
  $label     = trim($args['name'] ?? '');
  if ($type_name === '' || $label === '') {
    return ['error' => 'type and name are required.'];
  }
  if (!preg_match('/^[a-z0-9_]+$/', $type_name)) {
    return ['error' => 'type must be lowercase letters, numbers, and underscores only.'];
  }
  if (strlen($type_name) > 32) {
    return ['error' => 'type must be 32 characters or fewer.'];
  }
  if (node_type_load($type_name)) {
    return ['error' => "Content type '$type_name' already exists."];
  }
  $type              = new stdClass();
  $type->type        = $type_name;
  $type->name        = $label;
  $type->description = $args['description'] ?? '';
  $type->help        = isset($args['help']) ? $args['help'] : '';
  $type->title_label = $args['title_label'] ?? 'Title';
  $type->base        = 'node_content';
  $type->custom      = 1;
  $type->is_new      = TRUE;
  $type->modified    = 1;
  $type->locked      = 0;
  node_type_save($type);
  node_type_cache_reset();
  field_info_cache_clear();
  entity_info_cache_clear();
  node_add_body_field($type);
  node_type_cache_reset();
  field_info_cache_clear();
  entity_info_cache_clear();

  $created = node_type_load($type_name);
  if (!$created) {
    return ['error' => "Content type '$type_name' was saved but could not be reloaded. Do not create fields yet."];
  }

  return [
    'status' => 'created',
    'type' => $type_name,
    'label' => $label,
    'description' => $created->description,
    'help' => $created->help,
    'verified' => TRUE,
    'next_step' => 'Content type exists. Now create field storage and create field instances for this content type.',
  ];
}

function ai_tools_execute_edit_content_type(array $args) {
  $type_name = trim($args['type'] ?? '');
  if ($type_name === '') {
    return ['error' => 'type is required.'];
  }
  $type = node_type_load($type_name);
  if (!$type) {
    return ['error' => "Content type '$type_name' not found."];
  }
  $changed = FALSE;
  if (!empty($args['name'])) {
    $type->name = $args['name'];
    $changed = TRUE;
  }
  if (isset($args['description'])) {
    $type->description = $args['description'];
    $changed = TRUE;
  }
  if (isset($args['help'])) {
    $type->help = $args['help'];
    $changed = TRUE;
  }
  if (!empty($args['title_label'])) {
    $type->title_label = $args['title_label'];
    $changed = TRUE;
  }
  if (!$changed) {
    return ['status' => 'no_changes', 'type' => $type_name];
  }
  node_type_save($type);
  return [
    'status' => 'updated',
    'type' => $type_name,
    'label' => $type->name,
    'description' => $type->description,
    'help' => $type->help,
  ];
}
