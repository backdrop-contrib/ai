<?php

/**
 * @file
 * Tool definitions and executors for ai_translate agent tools.
 */

/**
 * Returns tool definitions for hook_ai_tools().
 */
function ai_translate_tools_definitions() {
  $tools = [];

  $tools['translate_node'] = [
    'definition' => [
      'type' => 'function',
      'function' => [
        'name' => 'translate_node',
        'description' => 'Translates a published node into a configured target site language using AI, creating a new node linked into the translation set. Call list_supported_languages first to verify the target language code.',
        'parameters' => [
          'type' => 'object',
          'properties' => [
            'nid' => ['type' => 'integer', 'description' => 'The source node ID to translate.'],
            'target_langcode' => ['type' => 'string', 'description' => 'The target language code (e.g. es, fr, de).'],
          ],
          'required' => ['nid', 'target_langcode'],
          'additionalProperties' => FALSE,
        ],
      ],
    ],
    'execute' => 'ai_translate_tool_translate_node',
    'operation' => 'write',
    'permission' => 'use ai translate',
    'module' => 'ai_translate',
    'category' => 'translation',
  ];

  $tools['list_supported_languages'] = [
    'definition' => [
      'type' => 'function',
      'function' => [
        'name' => 'list_supported_languages',
        'description' => 'Lists configured and enabled languages on the site available for AI translation targets, including the site default language.',
        'parameters' => [
          'type' => 'object',
          'properties' => new \stdClass(),
          'additionalProperties' => FALSE,
        ],
      ],
    ],
    'execute' => 'ai_translate_tool_list_supported_languages',
    'operation' => 'read',
    'permission' => 'access content',
    'module' => 'ai_translate',
    'category' => 'translation',
  ];

  $tools['list_untranslated_nodes'] = [
    'definition' => [
      'type' => 'function',
      'function' => [
        'name' => 'list_untranslated_nodes',
        'description' => 'Finds published nodes of translation-enabled content types that do not yet have a translation in the specified target language.',
        'parameters' => [
          'type' => 'object',
          'properties' => [
            'target_langcode' => ['type' => 'string', 'description' => 'Target language code to check for missing translations.'],
            'bundle' => ['type' => 'string', 'description' => 'Optional content type machine name (e.g. article, post) to restrict results.'],
            'limit' => ['type' => 'integer', 'description' => 'Maximum number of nodes to return (default 20, max 100).'],
          ],
          'required' => ['target_langcode'],
          'additionalProperties' => FALSE,
        ],
      ],
    ],
    'execute' => 'ai_translate_tool_list_untranslated_nodes',
    'operation' => 'read',
    'permission' => 'access content',
    'module' => 'ai_translate',
    'category' => 'translation',
  ];

  return $tools;
}

/**
 * Tool execution callback: translate_node.
 */
function ai_translate_tool_translate_node(array $parameters) {
  $nid = isset($parameters['nid']) ? (int) $parameters['nid'] : 0;
  $target_langcode = isset($parameters['target_langcode']) ? trim((string) $parameters['target_langcode']) : '';

  if ($nid <= 0) {
    return ['error' => 'A valid positive integer nid is required.'];
  }
  if ($target_langcode === '') {
    return ['error' => 'A non-empty target_langcode is required.'];
  }

  $node = node_load($nid);
  if (!$node) {
    return ['error' => "Node $nid not found."];
  }

  if (!node_access('view', $node)) {
    return ['error' => "Access denied to node $nid."];
  }

  $result = ai_translate_translate_node($node, $target_langcode);
  if (empty($result['status'])) {
    return [
      'status' => FALSE,
      'source_nid' => $nid,
      'target_langcode' => $target_langcode,
      'error' => $result['message'],
    ];
  }

  $translated_node = $result['node'];
  return [
    'status' => TRUE,
    'message' => $result['message'] ?: 'Translation created successfully.',
    'source_nid' => $nid,
    'target_langcode' => $target_langcode,
    'translated_nid' => !empty($translated_node->nid) ? (int) $translated_node->nid : NULL,
    'translated_title' => !empty($translated_node->title) ? $translated_node->title : '',
    'url' => !empty($translated_node->nid) ? url('node/' . $translated_node->nid, ['absolute' => TRUE]) : '',
  ];
}

/**
 * Tool execution callback: list_supported_languages.
 */
function ai_translate_tool_list_supported_languages(array $parameters = []) {
  $languages = language_list();
  $default_langcode = language_default()->langcode;

  $result = [];
  foreach ($languages as $langcode => $lang) {
    $result[] = [
      'langcode' => $langcode,
      'name' => $lang->name,
      'native' => !empty($lang->native) ? $lang->native : $lang->name,
      'enabled' => !empty($lang->enabled),
      'is_default' => ($langcode === $default_langcode),
    ];
  }

  return [
    'count' => count($result),
    'languages' => $result,
  ];
}

/**
 * Tool execution callback: list_untranslated_nodes.
 */
function ai_translate_tool_list_untranslated_nodes(array $parameters) {
  $target_langcode = isset($parameters['target_langcode']) ? trim((string) $parameters['target_langcode']) : '';
  if ($target_langcode === '') {
    return ['error' => 'target_langcode parameter is required.'];
  }

  $languages = language_list();
  if (!isset($languages[$target_langcode])) {
    return ['error' => "Language '$target_langcode' is not a configured site language."];
  }

  $bundle = isset($parameters['bundle']) ? trim((string) $parameters['bundle']) : '';
  $limit = isset($parameters['limit']) ? max(1, min((int) $parameters['limit'], 100)) : 20;

  if (!module_exists('translation')) {
    return ['error' => 'Backdrop core translation module is not enabled.'];
  }

  $types = [];
  if ($bundle !== '') {
    if (!translation_supported_type($bundle)) {
      return ['error' => "Content type '$bundle' does not have translation enabled."];
    }
    $types[] = $bundle;
  }
  else {
    foreach (node_type_get_types() as $type) {
      if (translation_supported_type($type->type)) {
        $types[] = $type->type;
      }
    }
  }

  if (empty($types)) {
    return [
      'count' => 0,
      'nodes' => [],
      'message' => 'No translation-supported content types found on the site.',
    ];
  }

  // Query published nodes in translatable types not already in the target language.
  $query = db_select('node', 'n')
    ->fields('n', ['nid', 'title', 'type', 'langcode', 'tnid', 'created'])
    ->condition('n.status', NODE_PUBLISHED)
    ->condition('n.type', $types, 'IN')
    ->condition('n.langcode', $target_langcode, '!=')
    ->orderBy('n.created', 'DESC')
    ->range(0, $limit * 3); // fetch extra to account for filtering

  $candidates = $query->execute()->fetchAll();

  $untranslated = [];
  foreach ($candidates as $cand) {
    if (count($untranslated) >= $limit) {
      break;
    }
    $tnid = !empty($cand->tnid) ? $cand->tnid : $cand->nid;
    $translations = translation_node_get_translations($tnid);
    if (!isset($translations[$target_langcode])) {
      $untranslated[] = [
        'nid' => (int) $cand->nid,
        'title' => $cand->title,
        'type' => $cand->type,
        'source_langcode' => $cand->langcode,
        'url' => url('node/' . $cand->nid, ['absolute' => TRUE]),
      ];
    }
  }

  return [
    'target_langcode' => $target_langcode,
    'count' => count($untranslated),
    'nodes' => $untranslated,
  ];
}
