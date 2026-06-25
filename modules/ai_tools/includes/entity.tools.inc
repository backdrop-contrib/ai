<?php


/**
 * @file
 * Entity introspection tools for the AI Tools registry.
 */

function ai_tools_entity_definitions() {
  return [
    'list_entity_types' => [
      'definition' => [
        'type'     => 'function',
        'function' => [
          'name'        => 'list_entity_types',
          'description' => 'List all entity types registered on this Backdrop site (e.g. node, user, taxonomy_term, file).',
          'parameters'  => [
            'type'       => 'object',
            'properties' => new stdClass(),
            'required'   => [],
          ],
        ],
      ],
      'execute'  => 'ai_tools_execute_list_entity_types',
      'module'   => 'ai_tools',
      'category' => 'entity',
      'permission' => 'access content overview',
    ],
    'list_bundles' => [
      'definition' => [
        'type'     => 'function',
        'function' => [
          'name'        => 'list_bundles',
          'description' => 'List all bundles (sub-types) for a given entity type. For "node" this returns content types; for "taxonomy_term" it returns vocabularies.',
          'parameters'  => [
            'type'       => 'object',
            'properties' => [
              'entity_type' => [
                'type'        => 'string',
                'description' => 'Entity type machine name (e.g. "node", "taxonomy_term", "user", "file").',
              ],
            ],
            'required' => ['entity_type'],
          ],
        ],
      ],
      'execute'  => 'ai_tools_execute_list_bundles',
      'module'   => 'ai_tools',
      'category' => 'entity',
      'permission' => 'access content overview',
    ],
    'list_content_entities' => [
      'definition' => [
        'type'     => 'function',
        'function' => [
          'name'        => 'list_content_entities',
          'description' => 'List content entities (nodes) matching optional filters. Returns IDs and titles.',
          'parameters'  => [
            'type'       => 'object',
            'properties' => [
              'content_type' => [
                'type'        => 'string',
                'description' => 'Filter by content type machine name.',
              ],
              'status' => [
                'type'        => 'integer',
                'description' => 'Publication status: 1 for published (default), 0 for unpublished.',
              ],
              'limit' => [
                'type'        => 'integer',
                'description' => 'Maximum results (default 10, max 50).',
              ],
            ],
            'required' => [],
          ],
        ],
      ],
      'execute'  => 'ai_tools_execute_list_content_entities',
      'module'   => 'ai_tools',
      'category' => 'entity',
      'permission' => 'access content overview',
    ],
  ];
}

// -----------------------------------------------------------------------
// Executors
// -----------------------------------------------------------------------

function ai_tools_execute_list_entity_types(array $args) {
  $entity_info = entity_get_info();
  $result = [];
  foreach ($entity_info as $type => $info) {
    $result[] = [
      'type'         => $type,
      'label'        => isset($info['label']) ? (string) $info['label'] : $type,
      'module'       => $info['module'] ?? '',
      'bundle_count' => !empty($info['bundles']) ? count($info['bundles']) : 0,
    ];
  }
  usort($result, function ($a, $b) { return strcmp($a['type'], $b['type']); });
  return ['entity_types' => $result, 'count' => count($result)];
}

function ai_tools_execute_list_bundles(array $args) {
  $entity_type = trim($args['entity_type'] ?? '');
  if ($entity_type === '') {
    return ['error' => 'entity_type is required.'];
  }

  $entity_info = entity_get_info($entity_type);
  if (!$entity_info) {
    return ['error' => "Entity type '$entity_type' not found."];
  }

  $bundles = [];
  foreach ($entity_info['bundles'] ?? [] as $bundle_name => $bundle_info) {
    $bundles[] = [
      'machine_name' => $bundle_name,
      'label'        => isset($bundle_info['label']) ? (string) $bundle_info['label'] : $bundle_name,
    ];
  }

  return ['entity_type' => $entity_type, 'bundles' => $bundles, 'count' => count($bundles)];
}

function ai_tools_execute_list_content_entities(array $args) {
  $limit  = min(max(1, (int) ($args['limit'] ?? 10)), 50);
  $status = isset($args['status']) ? (int) $args['status'] : 1;

  $q = db_select('node', 'n')
    ->fields('n', ['nid', 'title', 'type', 'created', 'changed'])
    ->addTag('node_access')
    ->condition('n.status', $status)
    ->orderBy('n.changed', 'DESC')
    ->range(0, $limit);

  if (!empty($args['content_type'])) {
    $q->condition('n.type', $args['content_type']);
  }

  $rows = $q->execute()->fetchAll(PDO::FETCH_ASSOC);
  return ['results' => $rows, 'count' => count($rows)];
}
