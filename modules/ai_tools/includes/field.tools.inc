<?php


/**
 * @file
 * Field tools for the AI Tools registry.
 */

function ai_tools_field_definitions() {
  return [
    'get_entity_field_information' => [
      'definition' => [
        'type'     => 'function',
        'function' => [
          'name'        => 'get_entity_field_information',
          'description' => 'List all fields attached to a content type with their machine names, labels, types, and cardinality.',
          'parameters'  => [
            'type'       => 'object',
            'properties' => [
              'content_type' => [
                'type'        => 'string',
                'description' => 'Machine name of the content type.',
              ],
            ],
            'required' => ['content_type'],
          ],
        ],
      ],
      'execute'   => 'ai_tools_execute_get_entity_field_information',
      'module'    => 'ai_tools',
      'category'  => 'field',
      'operation' => 'read',
    ],
    'get_field_values_and_context' => [
      'definition' => [
        'type'     => 'function',
        'function' => [
          'name'        => 'get_field_values_and_context',
          'description' => 'Get the field values of a specific node along with content type and field label context.',
          'parameters'  => [
            'type'       => 'object',
            'properties' => [
              'node_id' => [
                'type'        => 'integer',
                'description' => 'The numeric node ID (nid).',
              ],
              'fields' => [
                'type'        => 'array',
                'items'       => ['type' => 'string'],
                'description' => 'Optional list of field machine names to return. Returns all fields if omitted.',
              ],
            ],
            'required' => ['node_id'],
          ],
        ],
      ],
      'execute'   => 'ai_tools_execute_get_field_values_and_context',
      'module'    => 'ai_tools',
      'category'  => 'field',
      'operation' => 'read',
    ],
    'list_field_types' => [
      'definition' => [
        'type'     => 'function',
        'function' => [
          'name'        => 'list_field_types',
          'description' => 'List all available field types registered on this Backdrop site.',
          'parameters'  => [
            'type'       => 'object',
            'properties' => new stdClass(),
            'required'   => [],
          ],
        ],
      ],
      'execute'   => 'ai_tools_execute_list_field_types',
      'module'    => 'ai_tools',
      'category'  => 'field',
      'operation' => 'read',
    ],
    'get_field_storage' => [
      'definition' => [
        'type'     => 'function',
        'function' => [
          'name'        => 'get_field_storage',
          'description' => 'Get the storage definition for a field — its type, cardinality, and which bundles use it.',
          'parameters'  => [
            'type'       => 'object',
            'properties' => [
              'field_name' => [
                'type'        => 'string',
                'description' => 'Machine name of the field (e.g. "field_tags").',
              ],
            ],
            'required' => ['field_name'],
          ],
        ],
      ],
      'execute'    => 'ai_tools_execute_get_field_storage',
      'module'     => 'ai_tools',
      'category'   => 'field',
      'operation'  => 'read',
      'permission' => 'administer fields',
    ],
    'get_field_instance' => [
      'definition' => [
        'type'     => 'function',
        'function' => [
          'name'        => 'get_field_instance',
          'description' => 'Get the instance configuration for a field on a specific content type — label, required status, widget type, and description.',
          'parameters'  => [
            'type'       => 'object',
            'properties' => [
              'field_name' => [
                'type'        => 'string',
                'description' => 'Machine name of the field.',
              ],
              'content_type' => [
                'type'        => 'string',
                'description' => 'Machine name of the content type.',
              ],
            ],
            'required' => ['field_name', 'content_type'],
          ],
        ],
      ],
      'execute'    => 'ai_tools_execute_get_field_instance',
      'module'     => 'ai_tools',
      'category'   => 'field',
      'operation'  => 'read',
      'permission' => 'administer fields',
    ],
    'create_field_storage' => [
      'definition' => [
        'type'     => 'function',
        'function' => [
          'name'        => 'create_field_storage',
          'description' => 'Create a new field storage definition, or return an existing compatible storage definition for reuse. Must be done before attaching the field to a content type with create_field_instance. Pass label/source_label when creating a field from a human label so the machine name is derived deterministically.',
          'parameters'  => [
            'type'       => 'object',
            'properties' => [
              'field_name' => [
                'type'        => 'string',
                'description' => 'Optional machine name for the field — must start with "field_" (e.g. "field_summary"). If label/source_label is supplied, the tool derives the machine name from that label.',
              ],
              'label' => [
                'type'        => 'string',
                'description' => 'Human-readable source label for the field. Used to derive the machine name, e.g. "Whoa!" becomes "field_whoa".',
              ],
              'source_label' => [
                'type'        => 'string',
                'description' => 'Alias for label. Human-readable source label used to derive the field machine name.',
              ],
              'field_type' => [
                'type'        => 'string',
                'description' => 'Field type machine name. Use list_field_types to see available types.',
              ],
              'cardinality' => [
                'type'        => 'integer',
                'description' => 'Number of values allowed: 1 = single value, -1 = unlimited. Default 1.',
              ],
              'vocabulary' => [
                'type'        => 'string',
                'description' => 'For taxonomy_term_reference fields, or entityreference fields targeting taxonomy terms, the vocabulary machine name to allow, such as "tags" or "languages".',
              ],
              'parent' => [
                'type'        => 'integer',
                'description' => 'For taxonomy_term_reference fields, optional parent term ID constraint. Default 0.',
              ],
              'target_type' => [
                'type'        => 'string',
                'description' => 'For entityreference fields, target entity type machine name, such as "taxonomy_term", "node", or "user". Use "taxonomy_term" for an entity reference to taxonomy terms.',
              ],
              'target_bundle' => [
                'type'        => 'string',
                'description' => 'For entityreference fields, one allowed target bundle. For taxonomy_term targets this is the vocabulary machine name.',
              ],
              'target_bundles' => [
                'type'        => 'array',
                'items'       => ['type' => 'string'],
                'description' => 'For entityreference fields, allowed target bundles. For taxonomy_term targets these are vocabulary machine names.',
              ],
            ],
            'required' => ['field_type'],
          ],
        ],
      ],
      'execute'     => 'ai_tools_execute_create_field_storage',
      'module'      => 'ai_tools',
      'category'    => 'field',
      'operation'   => 'write',
      'destructive' => FALSE,
      'permission'  => 'administer fields',
    ],
    'create_field_instance' => [
      'definition' => [
        'type'     => 'function',
        'function' => [
          'name'        => 'create_field_instance',
          'description' => 'Attach an existing field storage to a content type. The field storage must already exist — use create_field_storage first if needed. If the content type was just created in the same request, first call get_content_type_info and only proceed after it verifies the bundle exists.',
          'parameters'  => [
            'type'       => 'object',
            'properties' => [
              'field_name' => [
                'type'        => 'string',
                'description' => 'Machine name of the field storage to attach.',
              ],
              'content_type' => [
                'type'        => 'string',
                'description' => 'Machine name of the content type.',
              ],
              'label' => [
                'type'        => 'string',
                'description' => 'Human-readable label for this field on the content type.',
              ],
              'required' => [
                'type'        => 'boolean',
                'description' => 'Whether the field is required. Default false.',
              ],
              'description' => [
                'type'        => 'string',
                'description' => 'Help text shown below the field on the edit form.',
              ],
            ],
            'required' => ['field_name', 'content_type', 'label'],
          ],
        ],
      ],
      'execute'     => 'ai_tools_execute_create_field_instance',
      'module'      => 'ai_tools',
      'category'    => 'field',
      'operation'   => 'write',
      'destructive' => FALSE,
      'permission'  => 'administer fields',
    ],
    'edit_field_instance' => [
      'definition' => [
        'type'     => 'function',
        'function' => [
          'name'        => 'edit_field_instance',
          'description' => 'Edit an existing field instance on a content type — change its label, required status, or description.',
          'parameters'  => [
            'type'       => 'object',
            'properties' => [
              'field_name' => [
                'type'        => 'string',
                'description' => 'Machine name of the field.',
              ],
              'content_type' => [
                'type'        => 'string',
                'description' => 'Machine name of the content type.',
              ],
              'label' => [
                'type'        => 'string',
                'description' => 'New human-readable label.',
              ],
              'required' => [
                'type'        => 'boolean',
                'description' => 'Whether the field is required.',
              ],
              'description' => [
                'type'        => 'string',
                'description' => 'New help text.',
              ],
            ],
            'required' => ['field_name', 'content_type'],
          ],
        ],
      ],
      'execute'     => 'ai_tools_execute_edit_field_instance',
      'module'      => 'ai_tools',
      'category'    => 'field',
      'operation'   => 'write',
      'destructive' => FALSE,
      'permission'  => 'administer fields',
    ],
  ];
}

// -----------------------------------------------------------------------
// Executors
// -----------------------------------------------------------------------

function ai_tools_execute_get_entity_field_information(array $args) {
  $type_name = trim($args['content_type'] ?? '');
  if ($type_name === '') {
    return ['error' => 'content_type is required.'];
  }
  if (!node_type_load($type_name)) {
    return ['error' => "Content type '$type_name' not found."];
  }
  $instances = field_info_instances('node', $type_name);
  $fields    = [];
  foreach ($instances as $field_name => $instance) {
    $field_info = field_info_field($field_name);
    $fields[]   = [
      'machine_name' => $field_name,
      'label'        => $instance['label'],
      'type'         => $field_info['type'] ?? 'unknown',
      'module'       => $field_info['module'] ?? '',
      'required'     => !empty($instance['required']),
      'cardinality'  => $field_info['cardinality'] ?? 1,
      'widget_type'  => $instance['widget']['type'] ?? '',
    ];
  }
  return ['content_type' => $type_name, 'fields' => $fields, 'count' => count($fields)];
}

function ai_tools_execute_get_field_values_and_context(array $args) {
  $nid = (int) ($args['node_id'] ?? 0);
  if (!$nid) {
    return ['error' => 'node_id must be a positive integer.'];
  }
  $node = node_load($nid);
  if (!$node) {
    return ['error' => "No node found with nid $nid."];
  }
  $filter_fields = !empty($args['fields']) ? array_flip($args['fields']) : NULL;
  $instances     = field_info_instances('node', $node->type);
  $result        = [
    'nid'    => (int) $node->nid,
    'title'  => $node->title,
    'type'   => $node->type,
    'status' => $node->status ? 'published' : 'unpublished',
    'fields' => [],
  ];
  foreach ($instances as $field_name => $instance) {
    if ($filter_fields !== NULL && !isset($filter_fields[$field_name])) {
      continue;
    }
    $items                          = field_get_items('node', $node, $field_name);
    $field_info                     = field_info_field($field_name);
    $result['fields'][$field_name]  = [
      'label'  => $instance['label'],
      'type'   => $field_info['type'] ?? 'unknown',
      'values' => $items ?: [],
    ];
  }
  return $result;
}

function ai_tools_execute_list_field_types(array $args) {
  $types   = field_info_field_types();
  $widgets = field_info_widget_types();

  $result = [];
  foreach ($types as $type => $info) {
    // Use the field type's own declared default settings so contrib types are
    // covered automatically without a hardcoded hints table.
    $default_settings = !empty($info['settings']) && is_array($info['settings']) ? $info['settings'] : new stdClass();

    $entry = [
      'type'             => $type,
      'label'            => isset($info['label']) ? (string) $info['label'] : $type,
      'module'           => $info['module'] ?? '',
      'default_settings' => $default_settings,
    ];

    $default_widget = isset($info['default_widget']) ? $info['default_widget'] : '';
    if ($default_widget && isset($widgets[$default_widget])) {
      $entry['default_widget']       = $default_widget;
      $entry['default_widget_label'] = (string) $widgets[$default_widget]['label'];
    }

    $result[] = $entry;
  }
  usort($result, function ($a, $b) { return strcmp($a['type'], $b['type']); });
  return ['field_types' => $result, 'count' => count($result)];
}

function ai_tools_execute_get_field_storage(array $args) {
  $field_name = trim($args['field_name'] ?? '');
  if ($field_name === '') {
    return ['error' => 'field_name is required.'];
  }
  $field = field_info_field($field_name);
  if (!$field) {
    return ['error' => "No field storage found for '$field_name'."];
  }
  $used_on = [];
  foreach ($field['bundles'] ?? [] as $entity_type => $bundle_list) {
    foreach ($bundle_list as $bundle) {
      $used_on[] = ['entity_type' => $entity_type, 'bundle' => $bundle];
    }
  }
  return [
    'field_name'   => $field['field_name'],
    'type'         => $field['type'],
    'module'       => $field['module'],
    'cardinality'  => (int) $field['cardinality'],
    'translatable' => (bool) $field['translatable'],
    'settings'     => $field['settings'] ?? [],
    'used_on'      => $used_on,
  ];
}

function ai_tools_execute_get_field_instance(array $args) {
  $field_name   = trim($args['field_name'] ?? '');
  $content_type = trim($args['content_type'] ?? '');
  if ($field_name === '' || $content_type === '') {
    return ['error' => 'field_name and content_type are required.'];
  }
  $instance = field_info_instance('node', $field_name, $content_type);
  if (!$instance) {
    return ['error' => "No instance of '$field_name' found on '$content_type'."];
  }
  return [
    'field_name'   => $field_name,
    'content_type' => $content_type,
    'label'        => $instance['label'],
    'required'     => (bool) $instance['required'],
    'description'  => $instance['description'] ?? '',
    'widget_type'  => $instance['widget']['type'] ?? '',
  ];
}

function ai_tools_field_machine_name_from_label($label) {
  $machine = preg_replace('/[^a-z0-9]+/', '_', strtolower(trim($label)));
  $machine = trim($machine, '_');
  if ($machine === '') {
    return '';
  }
  return 'field_' . substr($machine, 0, 26);
}

function ai_tools_field_target_bundles($settings) {
  if (!empty($settings['handler_settings']['target_bundles']) && is_array($settings['handler_settings']['target_bundles'])) {
    return array_values($settings['handler_settings']['target_bundles']);
  }
  return [];
}

function ai_tools_field_existing_storage_matches_request($existing, $requested, $cardinality_specified) {
  if (($existing['type'] ?? '') !== ($requested['type'] ?? '')) {
    return "Existing field storage '{$existing['field_name']}' has type '{$existing['type']}', not '{$requested['type']}'.";
  }
  if ($cardinality_specified && (int) ($existing['cardinality'] ?? 1) !== (int) ($requested['cardinality'] ?? 1)) {
    return "Existing field storage '{$existing['field_name']}' has cardinality '{$existing['cardinality']}', not '{$requested['cardinality']}'.";
  }
  if (($requested['type'] ?? '') === 'taxonomy_term_reference') {
    $existing_allowed = $existing['settings']['allowed_values'][0] ?? [];
    $requested_allowed = $requested['settings']['allowed_values'][0] ?? [];
    if (($existing_allowed['vocabulary'] ?? '') !== ($requested_allowed['vocabulary'] ?? '')) {
      return "Existing field storage '{$existing['field_name']}' references vocabulary '{$existing_allowed['vocabulary']}', not '{$requested_allowed['vocabulary']}'.";
    }
    if ((int) ($existing_allowed['parent'] ?? 0) !== (int) ($requested_allowed['parent'] ?? 0)) {
      return "Existing field storage '{$existing['field_name']}' has taxonomy parent '{$existing_allowed['parent']}', not '{$requested_allowed['parent']}'.";
    }
  }
  if (($requested['type'] ?? '') === 'entityreference') {
    $existing_settings = $existing['settings'] ?? [];
    $requested_settings = $requested['settings'] ?? [];
    if (($existing_settings['target_type'] ?? '') !== ($requested_settings['target_type'] ?? '')) {
      return "Existing field storage '{$existing['field_name']}' targets entity type '{$existing_settings['target_type']}', not '{$requested_settings['target_type']}'.";
    }
    $requested_bundles = ai_tools_field_target_bundles($requested_settings);
    if (!empty($requested_bundles)) {
      $existing_bundles = ai_tools_field_target_bundles($existing_settings);
      sort($requested_bundles);
      sort($existing_bundles);
      if ($existing_bundles !== $requested_bundles) {
        return "Existing field storage '{$existing['field_name']}' has target bundles '" . implode(', ', $existing_bundles) . "', not '" . implode(', ', $requested_bundles) . "'.";
      }
    }
  }
  return TRUE;
}

function ai_tools_execute_create_field_storage(array $args) {
  $input_field_name = trim($args['field_name'] ?? '');
  $source_label = trim($args['label'] ?? ($args['source_label'] ?? ''));
  $field_name = $input_field_name;
  if ($source_label !== '') {
    $field_name = ai_tools_field_machine_name_from_label($source_label);
  }
  $field_type = trim($args['field_type'] ?? '');
  if ($field_name === '' || $field_type === '') {
    return ['error' => 'field_type and either field_name or label/source_label are required.'];
  }
  if (strpos($field_name, 'field_') !== 0) {
    return ['error' => 'field_name must start with "field_".'];
  }
  if (!isset(field_info_field_types()[$field_type])) {
    return ['error' => "Field type '$field_type' not found. Use list_field_types to see available types."];
  }
  $cardinality_specified = isset($args['cardinality']);
  $cardinality = isset($args['cardinality']) ? (int) $args['cardinality'] : 1;
  $vocabulary = '';
  $field = [
    'field_name' => $field_name,
    'type' => $field_type,
    'cardinality' => $cardinality,
  ];
  if ($field_type === 'taxonomy_term_reference') {
    if (!module_exists('taxonomy')) {
      return ['error' => 'The taxonomy module must be enabled to create taxonomy_term_reference fields.'];
    }
    $vocabulary = trim($args['vocabulary'] ?? '');
    if ($vocabulary === '') {
      return ['error' => 'vocabulary is required when creating taxonomy_term_reference field storage.'];
    }
    if (!taxonomy_vocabulary_load($vocabulary)) {
      return ['error' => "Vocabulary '$vocabulary' not found."];
    }
    $field['settings'] = [
      'allowed_values' => [
        [
          'vocabulary' => $vocabulary,
          'parent' => isset($args['parent']) ? (int) $args['parent'] : 0,
        ],
      ],
    ];
  }
  elseif ($field_type === 'entityreference') {
    $vocabulary = trim($args['vocabulary'] ?? '');
    $target_type = trim($args['target_type'] ?? '');
    $target_bundle = trim($args['target_bundle'] ?? '');
    $target_bundles = [];

    if ($target_type === '' && $vocabulary !== '') {
      $target_type = 'taxonomy_term';
    }
    if ($target_type === 'taxonomy') {
      $target_type = 'taxonomy_term';
    }
    if ($target_bundle === '' && $vocabulary !== '') {
      $target_bundle = $vocabulary;
    }
    if (!empty($args['target_bundles']) && is_array($args['target_bundles'])) {
      foreach ($args['target_bundles'] as $bundle) {
        $bundle = trim($bundle);
        if ($bundle !== '') {
          $target_bundles[$bundle] = $bundle;
        }
      }
    }
    if ($target_bundle !== '') {
      $target_bundles[$target_bundle] = $target_bundle;
    }
    if ($target_type === '') {
      return ['error' => 'target_type is required when creating entityreference field storage. Use target_type "taxonomy_term" for an entity reference to a taxonomy vocabulary.'];
    }
    if ($target_type === 'taxonomy_term') {
      if (!module_exists('taxonomy')) {
        return ['error' => 'The taxonomy module must be enabled to create entityreference fields that target taxonomy terms.'];
      }
      foreach (array_keys($target_bundles) as $bundle) {
        if (!taxonomy_vocabulary_load($bundle)) {
          return ['error' => "Vocabulary '$bundle' not found."];
        }
      }
    }
    $field['settings'] = [
      'target_type' => $target_type,
      'handler' => 'base',
      'handler_settings' => [
        'target_bundles' => $target_bundles,
        'sort' => [
          'type' => 'none',
        ],
        'behaviors' => [
          'views' => [
            'status' => 0,
          ],
        ],
      ],
    ];
  }
  if ($existing = field_info_field($field_name)) {
    $match = ai_tools_field_existing_storage_matches_request($existing, $field, $cardinality_specified);
    if ($match !== TRUE) {
      return ['error' => $match, 'field_name' => $field_name, 'status' => 'incompatible_existing_storage'];
    }
    $result = [
      'status' => 'existing',
      'reusable' => TRUE,
      'field_name' => $field_name,
      'field_type' => $field_type,
      'cardinality' => (int) ($existing['cardinality'] ?? $cardinality),
      'message' => "Compatible field storage '$field_name' already exists and can be attached to another content type.",
    ];
    if ($source_label !== '') {
      $result['source_label'] = $source_label;
    }
    if ($input_field_name !== '' && $input_field_name !== $field_name) {
      $result['input_field_name'] = $input_field_name;
    }
    if (!empty($vocabulary)) {
      $result['vocabulary'] = $vocabulary;
    }
    if (!empty($target_type)) {
      $result['target_type'] = $target_type;
    }
    if (!empty($target_bundles)) {
      $result['target_bundles'] = array_values($target_bundles);
    }
    return $result;
  }
  try {
    field_create_field($field);
    $result = ['status' => 'created', 'field_name' => $field_name, 'field_type' => $field_type, 'cardinality' => $cardinality];
    if ($source_label !== '') {
      $result['source_label'] = $source_label;
    }
    if ($input_field_name !== '' && $input_field_name !== $field_name) {
      $result['input_field_name'] = $input_field_name;
      $result['message'] = "Derived field machine name '$field_name' from label '$source_label'.";
    }
    if (!empty($vocabulary)) {
      $result['vocabulary'] = $vocabulary;
    }
    if (!empty($target_type)) {
      $result['target_type'] = $target_type;
    }
    if (!empty($target_bundles)) {
      $result['target_bundles'] = array_values($target_bundles);
    }
    return $result;
  }
  catch (\Exception $e) {
    return ['error' => $e->getMessage()];
  }
}

function ai_tools_execute_create_field_instance(array $args) {
  $field_name   = trim($args['field_name'] ?? '');
  $content_type = trim($args['content_type'] ?? '');
  $label        = trim($args['label'] ?? '');
  if ($field_name === '' || $content_type === '' || $label === '') {
    return ['error' => 'field_name, content_type, and label are required.'];
  }
  if (!field_info_field($field_name)) {
    return ['error' => "Field storage '$field_name' does not exist. Use create_field_storage first."];
  }
  node_type_cache_reset();
  field_info_cache_clear();
  entity_info_cache_clear();
  $type = node_type_load($content_type);
  if (!$type) {
    return [
      'error' => "Content type '$content_type' not found. If you just created it, first verify it with get_content_type_info, then retry create_field_instance.",
      'requires_phase' => 'create_content_type',
    ];
  }
  if (field_info_instance('node', $field_name, $content_type)) {
    return ['error' => "Field '$field_name' is already attached to '$content_type'."];
  }
  try {
    field_create_instance([
      'field_name'  => $field_name,
      'entity_type' => 'node',
      'bundle'      => $content_type,
      'label'       => $label,
      'required'    => !empty($args['required']),
      'description' => $args['description'] ?? '',
    ]);
    return ['status' => 'created', 'field_name' => $field_name, 'content_type' => $content_type, 'label' => $label];
  }
  catch (\Exception $e) {
    return ['error' => $e->getMessage()];
  }
}

function ai_tools_execute_edit_field_instance(array $args) {
  $field_name   = trim($args['field_name'] ?? '');
  $content_type = trim($args['content_type'] ?? '');
  if ($field_name === '' || $content_type === '') {
    return ['error' => 'field_name and content_type are required.'];
  }
  $instance = field_info_instance('node', $field_name, $content_type);
  if (!$instance) {
    return ['error' => "No instance of '$field_name' found on '$content_type'."];
  }
  $changed = FALSE;
  if (!empty($args['label'])) {
    $instance['label'] = $args['label'];
    $changed = TRUE;
  }
  if (isset($args['required'])) {
    $instance['required'] = (bool) $args['required'];
    $changed = TRUE;
  }
  if (isset($args['description'])) {
    $instance['description'] = $args['description'];
    $changed = TRUE;
  }
  if (!$changed) {
    return ['status' => 'no_changes', 'field_name' => $field_name, 'content_type' => $content_type];
  }
  try {
    field_update_instance($instance);
    return ['status' => 'updated', 'field_name' => $field_name, 'content_type' => $content_type];
  }
  catch (\Exception $e) {
    return ['error' => $e->getMessage()];
  }
}
