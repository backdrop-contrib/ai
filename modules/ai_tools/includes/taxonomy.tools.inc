<?php


/**
 * @file
 * Taxonomy tools for the AI Tools registry.
 */

function ai_tools_taxonomy_definitions() {
  return [
    'get_vocabularies' => [
      'definition' => [
        'type'     => 'function',
        'function' => [
          'name'        => 'get_vocabularies',
          'description' => 'List all taxonomy vocabularies on this site with their machine names and labels.',
          'parameters'  => [
            'type'       => 'object',
            'properties' => new stdClass(),
            'required'   => [],
          ],
        ],
      ],
      'execute'  => 'ai_tools_execute_get_vocabularies',
      'module'   => 'ai_tools',
      'category' => 'taxonomy',
    ],
    'get_taxonomy_terms' => [
      'definition' => [
        'type'     => 'function',
        'function' => [
          'name'        => 'get_taxonomy_terms',
          'description' => 'List terms in a taxonomy vocabulary by its machine name.',
          'parameters'  => [
            'type'       => 'object',
            'properties' => [
              'vocabulary' => [
                'type'        => 'string',
                'description' => 'Machine name of the vocabulary (use get_vocabularies to find it).',
              ],
              'limit' => [
                'type'        => 'integer',
                'description' => 'Maximum number of terms to return (default 20, max 100).',
              ],
            ],
            'required' => ['vocabulary'],
          ],
        ],
      ],
      'execute'  => 'ai_tools_execute_get_taxonomy_terms',
      'module'   => 'ai_tools',
      'category' => 'taxonomy',
    ],
    'get_term_info' => [
      'definition' => [
        'type'     => 'function',
        'function' => [
          'name'        => 'get_term_info',
          'description' => 'Get details about a specific taxonomy term by its term ID (tid).',
          'parameters'  => [
            'type'       => 'object',
            'properties' => [
              'tid' => [
                'type'        => 'integer',
                'description' => 'The numeric taxonomy term ID.',
              ],
            ],
            'required' => ['tid'],
          ],
        ],
      ],
      'execute'   => 'ai_tools_execute_get_term_info',
      'module'    => 'ai_tools',
      'category'  => 'taxonomy',
      'operation' => 'read',
    ],
    'modify_taxonomy_term' => [
      'definition' => [
        'type'     => 'function',
        'function' => [
          'name'        => 'modify_taxonomy_term',
          'description' => 'Create or edit a taxonomy term. Omit tid to create a new term; provide tid to edit an existing one. Always call get_taxonomy_terms first to check for duplicates before creating.',
          'parameters'  => [
            'type'       => 'object',
            'properties' => [
              'name' => [
                'type'        => 'string',
                'description' => 'Term name.',
              ],
              'vocabulary' => [
                'type'        => 'string',
                'description' => 'Machine name of the vocabulary. Required when creating a new term.',
              ],
              'tid' => [
                'type'        => 'integer',
                'description' => 'Term ID — provide to edit an existing term. Omit to create a new one.',
              ],
              'description' => [
                'type'        => 'string',
                'description' => 'Term description.',
              ],
              'parent' => [
                'type'        => 'integer',
                'description' => 'Parent term ID for hierarchical vocabularies. 0 or omit for top-level.',
              ],
              'weight' => [
                'type'        => 'integer',
                'description' => 'Sort weight. Lower values sort first.',
              ],
            ],
            'required' => ['name'],
          ],
        ],
      ],
      'execute'     => 'ai_tools_execute_modify_taxonomy_term',
      'module'      => 'ai_tools',
      'category'    => 'taxonomy',
      'operation'   => 'write',
      'destructive' => FALSE,
      'permission'  => 'administer taxonomy',
    ],
    'modify_vocabulary' => [
      'definition' => [
        'type'     => 'function',
        'function' => [
          'name'        => 'modify_vocabulary',
          'description' => 'Create or edit a taxonomy vocabulary. If a vocabulary with the given machine_name exists it is updated; otherwise a new one is created.',
          'parameters'  => [
            'type'       => 'object',
            'properties' => [
              'machine_name' => [
                'type'        => 'string',
                'description' => 'Machine name of the vocabulary (lowercase letters, numbers, underscores).',
              ],
              'name' => [
                'type'        => 'string',
                'description' => 'Human-readable vocabulary label.',
              ],
              'description' => [
                'type'        => 'string',
                'description' => 'Vocabulary description.',
              ],
            ],
            'required' => ['machine_name', 'name'],
          ],
        ],
      ],
      'execute'     => 'ai_tools_execute_modify_vocabulary',
      'module'      => 'ai_tools',
      'category'    => 'taxonomy',
      'operation'   => 'write',
      'destructive' => FALSE,
      'permission'  => 'administer taxonomy',
    ],
  ];
}

function ai_tools_execute_get_vocabularies(array $args) {
  $vocabs = taxonomy_get_vocabularies();
  $result = [];
  foreach ($vocabs as $vocab) {
    $result[] = [
      'machine_name' => $vocab->machine_name,
      'label'        => $vocab->name,
      'description'  => $vocab->description,
    ];
  }
  return ['vocabularies' => $result, 'count' => count($result)];
}

function ai_tools_execute_get_taxonomy_terms(array $args) {
  $vocab_name = trim($args['vocabulary'] ?? '');
  if ($vocab_name === '') {
    return ['error' => 'vocabulary machine name is required.'];
  }
  $limit = min(max(1, (int) ($args['limit'] ?? 20)), 100);

  $vocab = taxonomy_vocabulary_load($vocab_name);
  if (!$vocab) {
    return ['error' => "Vocabulary '$vocab_name' not found."];
  }

  $terms = db_select('taxonomy_term_data', 't')
    ->fields('t', ['tid', 'name', 'description', 'weight'])
    ->condition('t.vocabulary', $vocab_name)
    ->orderBy('t.weight')
    ->orderBy('t.name')
    ->range(0, $limit)
    ->execute()
    ->fetchAll(PDO::FETCH_ASSOC);

  return ['vocabulary' => $vocab_name, 'terms' => $terms, 'count' => count($terms)];
}

function ai_tools_execute_get_term_info(array $args) {
  $tid = (int) ($args['tid'] ?? 0);
  if (!$tid) {
    return ['error' => 'tid must be a positive integer.'];
  }
  $term = taxonomy_term_load($tid);
  if (!$term) {
    return ['error' => "No term found with tid $tid."];
  }
  return [
    'tid'         => (int) $term->tid,
    'name'        => $term->name,
    'vocabulary'  => $term->vocabulary,
    'description' => $term->description,
    'weight'      => (int) $term->weight,
  ];
}

function ai_tools_execute_modify_taxonomy_term(array $args) {
  if (!user_access('administer taxonomy')) {
    return ['error' => 'Access denied: You do not have permission to administer taxonomy.'];
  }

  $name = trim($args['name'] ?? '');
  if ($name === '') {
    return ['error' => 'name is required.'];
  }

  $tid = !empty($args['tid']) ? (int) $args['tid'] : 0;

  if ($tid) {
    $term = taxonomy_term_load($tid);
    if (!$term) {
      return ['error' => "No term found with tid $tid."];
    }
  }
  else {
    $vocab_name = trim($args['vocabulary'] ?? '');
    if ($vocab_name === '') {
      return ['error' => 'vocabulary is required when creating a new term.'];
    }
    if (!taxonomy_vocabulary_load($vocab_name)) {
      return ['error' => "Vocabulary '$vocab_name' not found."];
    }
    $existing_tid = db_query(
      "SELECT tid FROM {taxonomy_term_data} WHERE vocabulary = :vocabulary AND LOWER(name) = LOWER(:name) LIMIT 1",
      [
        ':vocabulary' => $vocab_name,
        ':name' => $name,
      ]
    )->fetchField();
    if (!empty($existing_tid)) {
      $existing = taxonomy_term_load($existing_tid);
      if ($existing) {
        return [
          'status' => 'exists',
          'tid' => (int) $existing->tid,
          'name' => $existing->name,
          'vocabulary' => $existing->vocabulary,
          'exists' => TRUE,
        ];
      }
    }
    $term = entity_create('taxonomy_term', [
      'vocabulary' => $vocab_name,
    ]);
  }

  $term->name = $name;
  if (isset($args['description'])) {
    $term->description = $args['description'];
  }
  if (isset($args['weight'])) {
    $term->weight = (int) $args['weight'];
  }
  if (isset($args['parent'])) {
    $term->parent = [(int) $args['parent']];
  }

  taxonomy_term_save($term);
  $saved = taxonomy_term_load($term->tid);
  if (!$saved) {
    return ['error' => 'Term save completed but could not reload the saved term.'];
  }
  backdrop_static_reset('taxonomy_get_tree');
  backdrop_static_reset('taxonomy_get_tree:parents');
  backdrop_static_reset('taxonomy_get_tree:terms');
  backdrop_static_reset('taxonomy_term_load_parents');
  backdrop_static_reset('taxonomy_term_load_parents_all');
  backdrop_static_reset('taxonomy_term_load_children');

  $current_terms = ai_tools_execute_get_taxonomy_terms([
    'vocabulary' => $saved->vocabulary,
    'limit' => 100,
  ]);

  return [
    'status'        => $tid ? 'updated' : 'created',
    'tid'           => (int) $saved->tid,
    'name'          => $saved->name,
    'vocabulary'    => $saved->vocabulary,
    'exists'        => TRUE,
    'current_count' => !empty($current_terms['count']) ? (int) $current_terms['count'] : 0,
    'current_terms' => !empty($current_terms['terms']) ? $current_terms['terms'] : [],
    'message'       => sprintf(
      'Saved term "%s" in "%s". Current terms: %s.',
      $saved->name,
      $saved->vocabulary,
      !empty($current_terms['terms']) ? implode(', ', array_map(function ($term) { return $term['name']; }, $current_terms['terms'])) : 'none'
    ),
  ];
}

function ai_tools_execute_modify_vocabulary(array $args) {
  if (!user_access('administer taxonomy')) {
    return ['error' => 'Access denied: You do not have permission to administer taxonomy.'];
  }

  $machine_name = trim($args['machine_name'] ?? '');
  $name         = trim($args['name'] ?? '');
  if ($machine_name === '' || $name === '') {
    return ['error' => 'machine_name and name are required.'];
  }
  if (!preg_match('/^[a-z0-9_]+$/', $machine_name)) {
    return ['error' => 'machine_name must be lowercase letters, numbers, and underscores only.'];
  }

  $vocab = taxonomy_vocabulary_load($machine_name);
  if ($vocab) {
    $action = 'updated';
  }
  else {
    $vocab               = new TaxonomyVocabulary();
    $vocab->machine_name = $machine_name;
    $action              = 'created';
  }

  $vocab->name = $name;
  if (isset($args['description'])) {
    $vocab->description = $args['description'];
  }

  taxonomy_vocabulary_save($vocab);
  $saved = taxonomy_vocabulary_load($machine_name);
  $current_terms = ai_tools_execute_get_taxonomy_terms([
    'vocabulary' => $machine_name,
    'limit' => 100,
  ]);

  return [
    'status' => $action,
    'machine_name' => $machine_name,
    'label' => $name,
    'exists' => !empty($saved),
    'current_count' => !empty($current_terms['count']) ? (int) $current_terms['count'] : 0,
    'current_terms' => !empty($current_terms['terms']) ? $current_terms['terms'] : [],
    'message' => $action === 'created'
      ? sprintf('Created vocabulary "%s".', $name)
      : sprintf('Updated existing vocabulary "%s".', $name),
  ];
}
