<?php


/**
 * @file
 * User tools for the AI Tools registry.
 */

function ai_tools_user_definitions() {
  return [
    'get_user_info' => [
      'definition' => [
        'type'     => 'function',
        'function' => [
          'name'        => 'get_user_info',
          'description' => 'Look up a Backdrop CMS user by their numeric user ID or username. Returns public profile info only — no passwords or private data.',
          'parameters'  => [
            'type'       => 'object',
            'properties' => [
              'uid' => [
                'type'        => 'integer',
                'description' => 'The numeric user ID.',
              ],
              'name' => [
                'type'        => 'string',
                'description' => 'The username (used if uid is not provided).',
              ],
            ],
          ],
        ],
      ],
      'execute'  => 'ai_tools_execute_get_user_info',
      'module'   => 'ai_tools',
      'category' => 'user',
    ],
    'get_user_nodes' => [
      'definition' => [
        'type'     => 'function',
        'function' => [
          'name'        => 'get_user_nodes',
          'description' => 'List published nodes authored by a specific user.',
          'parameters'  => [
            'type'       => 'object',
            'properties' => [
              'uid' => [
                'type'        => 'integer',
                'description' => 'The numeric user ID of the author.',
              ],
              'limit' => [
                'type'        => 'integer',
                'description' => 'Maximum results (default 10, max 25).',
              ],
            ],
            'required' => ['uid'],
          ],
        ],
      ],
      'execute'  => 'ai_tools_execute_get_user_nodes',
      'module'   => 'ai_tools',
      'category' => 'user',
    ],
  ];
}

function ai_tools_execute_get_user_info(array $args) {
  $account = NULL;

  if (!empty($args['uid'])) {
    $account = user_load((int) $args['uid']);
  }
  elseif (!empty($args['name'])) {
    $account = user_load_by_name($args['name']);
  }

  if (!$account) {
    return ['error' => 'User not found.'];
  }

  if (!user_access('access user profiles')) {
    return ['error' => 'Access denied for user profiles.'];
  }

  // Return only non-sensitive fields.
  return [
    'uid'     => (int) $account->uid,
    'name'    => $account->name,
    'status'  => $account->status ? 'active' : 'blocked',
    'created' => format_date($account->created, 'short'),
  ];
}

function ai_tools_execute_get_user_nodes(array $args) {
  $uid = (int) ($args['uid'] ?? 0);
  if (!$uid) {
    return ['error' => 'uid must be a positive integer.'];
  }
  $limit = min(max(1, (int) ($args['limit'] ?? 10)), 25);

  $rows = db_select('node', 'n')
    ->fields('n', ['nid', 'title', 'type', 'created'])
    ->addTag('node_access')
    ->condition('n.uid', $uid)
    ->condition('n.status', 1)
    ->orderBy('n.created', 'DESC')
    ->range(0, $limit)
    ->execute()
    ->fetchAll(PDO::FETCH_ASSOC);

  return ['uid' => $uid, 'results' => $rows, 'count' => count($rows)];
}
