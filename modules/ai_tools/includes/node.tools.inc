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
      'module'   => 'ai_tools',
      'category' => 'node',
    ],
    'search_nodes' => [
      'definition' => [
        'type'     => 'function',
        'function' => [
          'name'        => 'search_nodes',
          'description' => 'Search published nodes by title or body text. Returns matching node IDs, titles, and types.',
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
            ],
            'required' => ['query'],
          ],
        ],
      ],
      'execute'  => 'ai_tools_execute_search_nodes',
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
      'module'   => 'ai_tools',
      'category' => 'node',
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
    ->addTag('node_access')
    ->condition('n.status', 1)
    ->condition('n.title', '%' . db_like($query_text) . '%', 'LIKE')
    ->orderBy('n.changed', 'DESC')
    ->range(0, $limit);

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
