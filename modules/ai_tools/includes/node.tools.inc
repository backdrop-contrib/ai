<?php


/**
 * @file
 * Node-related tools for the AI Tools registry.
 */

/**
 * Returns tool definitions for node operations.
 */
function ai_tools_node_definitions() {
  return [
    'get_node_info' => [
      'definition' => [
        'type'     => 'function',
        'function' => [
          'name'        => 'get_node_info',
          'description' => 'Retrieve title, content type, and publication status of a Backdrop node by its numeric ID.',
          'parameters'  => [
            'type'       => 'object',
            'properties' => [
              'node_id' => [
                'type'        => 'integer',
                'description' => 'The numeric node ID (nid).',
              ],
            ],
            'required' => ['node_id'],
          ],
        ],
      ],
      'execute'  => 'ai_tools_execute_get_node_info',
      'operation' => 'read',
      'module'   => 'ai_tools',
      'category' => 'node',
    ],
    'search_nodes' => [
      'definition' => [
        'type'     => 'function',
        'function' => [
          'name'        => 'search_nodes',
          'description' => 'Search nodes by title or body text. Returns matching node IDs, titles, types, and published status. Includes unpublished nodes the current user is allowed to see.',
          'parameters'  => [
            'type'       => 'object',
            'properties' => [
              'query' => [
                'type'        => 'string',
                'description' => 'Text to search for in node titles and body.',
              ],
              'content_type' => [
                'type'        => 'string',
                'description' => 'Optional machine name of content type to restrict results (e.g. "post", "page").',
              ],
              'limit' => [
                'type'        => 'integer',
                'description' => 'Maximum number of results to return (default 5, max 20).',
              ],
              'include_unpublished' => [
                'type'        => 'boolean',
                'description' => 'Include unpublished nodes the user may see (default true). Set false for published only.',
              ],
            ],
            'required' => ['query'],
          ],
        ],
      ],
      'execute'  => 'ai_tools_execute_search_nodes',
      'operation' => 'read',
      'module'   => 'ai_tools',
      'category' => 'node',
    ],
    'get_nodes_by_type' => [
      'definition' => [
        'type'     => 'function',
        'function' => [
          'name'        => 'get_nodes_by_type',
          'description' => 'List published nodes of a specific content type, ordered by most recently changed.',
          'parameters'  => [
            'type'       => 'object',
            'properties' => [
              'content_type' => [
                'type'        => 'string',
                'description' => 'Machine name of the content type (e.g. "post", "page").',
              ],
              'limit' => [
                'type'        => 'integer',
                'description' => 'Maximum results (default 10, max 25).',
              ],
            ],
            'required' => ['content_type'],
          ],
        ],
      ],
      'execute'  => 'ai_tools_execute_get_nodes_by_type',
      'operation' => 'read',
      'module'   => 'ai_tools',
      'category' => 'node',
    ],
    'get_recent_nodes' => [
      'definition' => [
        'type'     => 'function',
        'function' => [
          'name'        => 'get_recent_nodes',
          'description' => 'Return the most recently published nodes on the site.',
          'parameters'  => [
            'type'       => 'object',
            'properties' => [
              'limit' => [
                'type'        => 'integer',
                'description' => 'Number of nodes to return (default 5, max 20).',
              ],
              'content_type' => [
                'type'        => 'string',
                'description' => 'Optional content type filter.',
              ],
            ],
            'required' => [],
          ],
        ],
      ],
      'execute'  => 'ai_tools_execute_get_recent_nodes',
      'operation' => 'read',
      'module'   => 'ai_tools',
      'category' => 'node',
    ],
    'create_node' => [
      'definition' => [
        'type'     => 'function',
        'function' => [
          'name'        => 'create_node',
          'description' => 'Create a new node of an existing content type. The node is saved unpublished unless published is true and the user may publish. Call search_nodes first to avoid duplicates. Only fills title and body; use get_content_type_info to check the type exists.',
          'parameters'  => [
            'type'       => 'object',
            'properties' => [
              'content_type' => [
                'type'        => 'string',
                'description' => 'Machine name of the content type (e.g. "post", "page").',
              ],
              'title' => [
                'type'        => 'string',
                'description' => 'Node title.',
              ],
              'body' => [
                'type'        => 'string',
                'description' => 'Node body as HTML or plain text. Omit for an empty body.',
              ],
              'published' => [
                'type'        => 'boolean',
                'description' => 'Publish immediately. Defaults to false (unpublished draft).',
              ],
            ],
            'required' => ['content_type', 'title'],
          ],
        ],
      ],
      'execute'     => 'ai_tools_execute_create_node',
      'operation'   => 'write',
      'destructive' => FALSE,
      'module'      => 'ai_tools',
      'category'    => 'node',
    ],
  ];
}

// -----------------------------------------------------------------------
// Executors
// -----------------------------------------------------------------------

function ai_tools_execute_get_node_info(array $args) {
  $nid = (int) ($args['node_id'] ?? 0);
  if (!$nid) {
    return ['error' => 'node_id must be a positive integer.'];
  }
  $node = node_load($nid);
  if (!$node) {
    return ['error' => "No node found with nid $nid."];
  }
  if (!node_access('view', $node)) {
    return ['error' => "Access denied for node $nid."];
  }
  return [
    'nid'    => (int) $node->nid,
    'title'  => $node->title,
    'type'   => $node->type,
    'status' => $node->status ? 'published' : 'unpublished',
  ];
}

function ai_tools_execute_search_nodes(array $args) {
  $query_text = trim($args['query'] ?? '');
  if ($query_text === '') {
    return ['error' => 'query must not be empty.'];
  }
  $limit = min(max(1, (int) ($args['limit'] ?? 5)), 20);

  $q = db_select('node', 'n')
    ->fields('n', ['nid', 'title', 'type', 'status'])
    ->distinct()
    ->addTag('node_access')
    ->orderBy('n.changed', 'DESC')
    ->range(0, $limit);

  // Unpublished nodes only appear to users who could open them anyway.
  global $user;
  $include_unpublished = !isset($args['include_unpublished']) || !empty($args['include_unpublished']);
  if (!$include_unpublished) {
    $q->condition('n.status', 1);
  }
  elseif (!user_access('bypass node access') && !user_access('administer nodes')) {
    if (user_access('view own unpublished content') && $user->uid) {
      $q->condition(db_or()->condition('n.status', 1)->condition('n.uid', $user->uid));
    }
    else {
      $q->condition('n.status', 1);
    }
  }

  $search_or = db_or()
    ->condition('n.title', '%' . db_like($query_text) . '%', 'LIKE');

  if (db_table_exists('field_data_body')) {
    $q->leftJoin('field_data_body', 'b', 'b.entity_type = :node_type AND b.entity_id = n.nid', [':node_type' => 'node']);
    $search_or->condition('b.body_value', '%' . db_like($query_text) . '%', 'LIKE');
  }

  $q->condition($search_or);

  if (!empty($args['content_type'])) {
    $q->condition('n.type', $args['content_type']);
  }

  $rows = $q->execute()->fetchAll(PDO::FETCH_ASSOC);
  return ['results' => $rows, 'count' => count($rows)];
}

function ai_tools_execute_get_nodes_by_type(array $args) {
  $type = trim($args['content_type'] ?? '');
  if ($type === '') {
    return ['error' => 'content_type is required.'];
  }
  $limit = min(max(1, (int) ($args['limit'] ?? 10)), 25);

  $rows = db_select('node', 'n')
    ->fields('n', ['nid', 'title', 'status', 'changed'])
    ->addTag('node_access')
    ->condition('n.type', $type)
    ->condition('n.status', 1)
    ->orderBy('n.changed', 'DESC')
    ->range(0, $limit)
    ->execute()
    ->fetchAll(PDO::FETCH_ASSOC);

  return ['type' => $type, 'results' => $rows, 'count' => count($rows)];
}

function ai_tools_execute_get_recent_nodes(array $args) {
  $limit = min(max(1, (int) ($args['limit'] ?? 5)), 20);

  $q = db_select('node', 'n')
    ->fields('n', ['nid', 'title', 'type', 'changed'])
    ->addTag('node_access')
    ->condition('n.status', 1)
    ->orderBy('n.created', 'DESC')
    ->range(0, $limit);

  if (!empty($args['content_type'])) {
    $q->condition('n.type', $args['content_type']);
  }

  $rows = $q->execute()->fetchAll(PDO::FETCH_ASSOC);
  return ['results' => $rows, 'count' => count($rows)];
}

function ai_tools_execute_create_node(array $args) {
  global $user;

  $type = trim($args['content_type'] ?? '');
  $title = trim($args['title'] ?? '');
  if ($type === '' || $title === '') {
    return ['error' => 'content_type and title are required.'];
  }
  if (!node_type_get_type($type)) {
    return ['error' => "Content type '$type' not found."];
  }
  if (!node_access('create', $type)) {
    return ['error' => "Access denied: you may not create '$type' content."];
  }

  $node = entity_create('node', [
    'type' => $type,
    'title' => $title,
    'uid' => $user->uid,
    'name' => $user->name,
    'language' => LANGUAGE_NONE,
    'status' => 0,
  ]);

  // Publishing needs the same permission the node form requires.
  if (!empty($args['published'])) {
    if (!user_access('administer nodes')) {
      return ['error' => 'Access denied: you may not publish nodes. Omit published to save a draft.'];
    }
    $node->status = 1;
  }

  $body = isset($args['body']) ? (string) $args['body'] : '';
  if ($body !== '') {
    if (!field_info_instance('node', 'body', $type)) {
      return ['error' => "Content type '$type' has no body field; omit body."];
    }
    $format = filter_default_format($user);
    $node->body[LANGUAGE_NONE][0] = ['value' => $body, 'format' => $format];
  }

  node_save($node);

  return [
    'status' => 'created',
    'nid' => (int) $node->nid,
    'title' => $node->title,
    'type' => $node->type,
    'published' => (bool) $node->status,
  ];
}
