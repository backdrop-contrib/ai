<?php

/**
 * @file
 * Menu tools for the AI Tools registry.
 */

function ai_tools_menu_definitions() {
  return [
    'list_menus' => [
      'definition' => [
        'type'     => 'function',
        'function' => [
          'name'        => 'list_menus',
          'description' => 'List all menus on this site with their machine names, labels, and link counts.',
          'parameters'  => [
            'type'       => 'object',
            'properties' => new stdClass(),
            'required'   => [],
          ],
        ],
      ],
      'execute'    => 'ai_tools_execute_list_menus',
      'operation'  => 'read',
      'permission' => 'administer menu',
      'module'     => 'ai_tools',
      'category'   => 'menu',
    ],
    'get_menu_tree' => [
      'definition' => [
        'type'     => 'function',
        'function' => [
          'name'        => 'get_menu_tree',
          'description' => 'Return the links in one menu as a flat list in tree order, with mlid, parent mlid (plid), depth, title, path, weight, and enabled/expanded flags. Use the mlid values to edit links or to place new links under a parent.',
          'parameters'  => [
            'type'       => 'object',
            'properties' => [
              'menu_name' => [
                'type'        => 'string',
                'description' => 'Menu machine name, for example main-menu.',
              ],
              'max_depth' => [
                'type'        => 'integer',
                'description' => 'Maximum depth to return. Omit for the whole tree.',
              ],
            ],
            'required' => ['menu_name'],
          ],
        ],
      ],
      'execute'    => 'ai_tools_execute_get_menu_tree',
      'operation'  => 'read',
      'permission' => 'administer menu',
      'module'     => 'ai_tools',
      'category'   => 'menu',
    ],
    'save_menu_link' => [
      'definition' => [
        'type'     => 'function',
        'function' => [
          'name'        => 'save_menu_link',
          'description' => 'Create or edit a menu link. Provide mlid to edit an existing link; omit it to create a new one in menu_name. Paths may be internal paths (node/12), URL aliases (about-us), <front>, or external URLs.',
          'parameters'  => [
            'type'       => 'object',
            'properties' => [
              'mlid' => [
                'type'        => 'integer',
                'description' => 'Menu link ID to edit. Omit to create a new link.',
              ],
              'menu_name' => [
                'type'        => 'string',
                'description' => 'Menu machine name. Required when creating a link.',
              ],
              'title' => [
                'type'        => 'string',
                'description' => 'Link text. Required when creating a link.',
              ],
              'path' => [
                'type'        => 'string',
                'description' => 'Link path. Required when creating a link.',
              ],
              'description' => [
                'type'        => 'string',
                'description' => 'Text shown when hovering over the link.',
              ],
              'parent_mlid' => [
                'type'        => 'integer',
                'description' => 'mlid of the parent link in the same menu. 0 places the link at the top level.',
              ],
              'weight' => [
                'type'        => 'integer',
                'description' => 'Sort weight. Lower values sort first.',
              ],
              'enabled' => [
                'type'        => 'boolean',
                'description' => 'Whether the link is shown. Defaults to true for new links.',
              ],
              'expanded' => [
                'type'        => 'boolean',
                'description' => 'Whether child links are always shown.',
              ],
            ],
            'required' => [],
          ],
        ],
      ],
      'execute'     => 'ai_tools_execute_save_menu_link',
      'operation'   => 'write',
      'destructive' => FALSE,
      'permission'  => 'administer menu',
      'module'      => 'ai_tools',
      'category'    => 'menu',
    ],
    'delete_menu_link' => [
      'definition' => [
        'type'     => 'function',
        'function' => [
          'name'        => 'delete_menu_link',
          'description' => 'Delete a custom menu link and move its children up one level. Links provided by modules cannot be deleted; disable them with save_menu_link instead.',
          'parameters'  => [
            'type'       => 'object',
            'properties' => [
              'mlid' => [
                'type'        => 'integer',
                'description' => 'Menu link ID to delete.',
              ],
            ],
            'required' => ['mlid'],
          ],
        ],
      ],
      'execute'     => 'ai_tools_execute_delete_menu_link',
      'operation'   => 'write',
      'destructive' => TRUE,
      'permission'  => 'administer menu',
      'module'      => 'ai_tools',
      'category'    => 'menu',
    ],
  ];
}

function ai_tools_execute_list_menus(array $args) {
  $result = [];
  foreach (menu_load_all() as $menu_name => $menu) {
    $result[] = [
      'menu_name'   => $menu_name,
      'title'       => $menu['title'],
      'description' => $menu['description'] ?? '',
      'link_count'  => (int) db_query('SELECT COUNT(*) FROM {menu_links} WHERE menu_name = :menu_name', [':menu_name' => $menu_name])->fetchField(),
    ];
  }
  return ['menus' => $result, 'count' => count($result)];
}

function ai_tools_execute_get_menu_tree(array $args) {
  $menu_name = trim($args['menu_name'] ?? '');
  if ($menu_name === '' || !menu_load($menu_name)) {
    return ['error' => "Menu '$menu_name' not found."];
  }
  $max_depth = !empty($args['max_depth']) ? (int) $args['max_depth'] : NULL;

  $links = [];
  ai_tools_menu_flatten_tree(menu_tree_all_data($menu_name, NULL, $max_depth), $links);
  return ['menu_name' => $menu_name, 'links' => $links, 'count' => count($links)];
}

/**
 * Flattens a menu tree into a list of link summaries in tree order.
 */
function ai_tools_menu_flatten_tree(array $tree, array &$links) {
  foreach ($tree as $branch) {
    $links[] = ai_tools_menu_link_summary($branch['link']);
    if (!empty($branch['below'])) {
      ai_tools_menu_flatten_tree($branch['below'], $links);
    }
  }
}

/**
 * Returns the fields of a menu link that a model needs.
 */
function ai_tools_menu_link_summary(array $link) {
  return [
    'mlid'     => (int) $link['mlid'],
    'plid'     => (int) $link['plid'],
    'depth'    => (int) $link['depth'],
    'title'    => $link['link_title'],
    'path'     => $link['link_path'],
    'weight'   => (int) $link['weight'],
    'enabled'  => empty($link['hidden']),
    'expanded' => !empty($link['expanded']),
    'custom'   => $link['module'] === 'menu',
  ];
}

function ai_tools_execute_save_menu_link(array $args) {
  if (!user_access('administer menu')) {
    return ['error' => 'Access denied: You do not have permission to administer menus.'];
  }

  $mlid = !empty($args['mlid']) ? (int) $args['mlid'] : 0;
  if ($mlid) {
    $item = menu_link_load($mlid, TRUE);
    if (!$item) {
      return ['error' => "No menu link found with mlid $mlid."];
    }
  }
  else {
    foreach (['menu_name', 'title', 'path'] as $required) {
      if (trim($args[$required] ?? '') === '') {
        return ['error' => "$required is required when creating a menu link."];
      }
    }
    $item = [
      'module'  => 'menu',
      'plid'    => 0,
      'weight'  => 0,
      'hidden'  => 0,
      'options' => [],
    ];
    $item['menu_name'] = trim($args['menu_name']);
  }

  if (isset($args['menu_name']) && $mlid && trim($args['menu_name']) !== $item['menu_name']) {
    return ['error' => 'Moving a link to another menu is not supported. Delete it and create a new one.'];
  }
  if (!menu_load($item['menu_name'])) {
    return ['error' => "Menu '{$item['menu_name']}' not found."];
  }

  if (isset($args['title'])) {
    $title = trim($args['title']);
    if ($title === '') {
      return ['error' => 'title cannot be empty.'];
    }
    $item['link_title'] = $title;
  }

  // Mirror menu_edit_item_validate(): store system paths, keep query and
  // fragment in options, and refuse paths the user cannot reach.
  if (isset($args['path'])) {
    $path = backdrop_get_normal_path(trim($args['path']));
    if (!url_is_external($path)) {
      $parsed = parse_url($path);
      unset($item['options']['query'], $item['options']['fragment']);
      if (isset($parsed['query'])) {
        $item['options']['query'] = backdrop_get_query_array($parsed['query']);
      }
      if (isset($parsed['fragment'])) {
        $item['options']['fragment'] = $parsed['fragment'];
      }
      if (isset($parsed['path'])) {
        $path = $parsed['path'];
      }
    }
    if ($path === '' || !backdrop_valid_path($path, TRUE)) {
      return ['error' => "The path '$path' is either invalid or you do not have access to it."];
    }
    $item['link_path'] = $path;
  }

  if (isset($args['description'])) {
    $item['options']['attributes']['title'] = $args['description'];
  }
  if (isset($args['parent_mlid'])) {
    $plid = (int) $args['parent_mlid'];
    if ($plid) {
      $parent = menu_link_load($plid, TRUE);
      if (!$parent || $parent['menu_name'] !== $item['menu_name']) {
        return ['error' => "Parent link $plid not found in menu '{$item['menu_name']}'."];
      }
      if ($mlid && ($plid === $mlid || ai_tools_menu_link_is_descendant($plid, $item))) {
        return ['error' => 'A link cannot be placed under itself or one of its children.'];
      }
    }
    $item['plid'] = $plid;
  }
  if (isset($args['weight'])) {
    $item['weight'] = (int) $args['weight'];
  }
  if (isset($args['enabled'])) {
    $item['hidden'] = $args['enabled'] ? 0 : 1;
  }
  if (isset($args['expanded'])) {
    $item['expanded'] = $args['expanded'] ? 1 : 0;
  }
  if ($mlid) {
    // Mark module-provided links as edited so a menu rebuild keeps the change.
    $item['customized'] = 1;
  }

  $saved_mlid = menu_link_save($item);
  if (!$saved_mlid) {
    return ['error' => 'There was an error saving the menu link.'];
  }

  $saved = menu_link_load($saved_mlid, TRUE);
  return [
    'status' => $mlid ? 'updated' : 'created',
    'link'   => ai_tools_menu_link_summary($saved),
  ];
}

/**
 * Checks whether a link is below another link in the menu tree.
 */
function ai_tools_menu_link_is_descendant($candidate_mlid, array $ancestor) {
  $candidate = menu_link_load($candidate_mlid, TRUE);
  if (!$candidate) {
    return FALSE;
  }
  for ($i = 1; $i <= MENU_MAX_DEPTH; $i++) {
    if ((int) $candidate['p' . $i] === (int) $ancestor['mlid']) {
      return TRUE;
    }
  }
  return FALSE;
}

function ai_tools_execute_delete_menu_link(array $args) {
  if (!user_access('administer menu')) {
    return ['error' => 'Access denied: You do not have permission to administer menus.'];
  }

  $mlid = (int) ($args['mlid'] ?? 0);
  $item = $mlid ? menu_link_load($mlid, TRUE) : FALSE;
  if (!$item) {
    return ['error' => "No menu link found with mlid $mlid."];
  }
  // The menu UI only deletes links it created; module links come back on the
  // next menu rebuild.
  if ($item['module'] !== 'menu') {
    return ['error' => "Link $mlid is provided by the {$item['module']} module and cannot be deleted. Disable it with save_menu_link instead."];
  }

  menu_link_delete($mlid);
  return [
    'status'    => 'deleted',
    'mlid'      => $mlid,
    'title'     => $item['link_title'],
    'menu_name' => $item['menu_name'],
  ];
}
