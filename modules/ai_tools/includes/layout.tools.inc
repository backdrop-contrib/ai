<?php

/**
 * @file
 * Layout tools for the AI Tools registry.
 */

function ai_tools_layout_definitions() {
  $layout_name = [
    'type'        => 'string',
    'description' => 'Layout machine name, for example default or home.',
  ];

  return [
    'list_layouts' => [
      'definition' => [
        'type'     => 'function',
        'function' => [
          'name'        => 'list_layouts',
          'description' => 'List all layouts with their machine names, labels, paths, templates, selection conditions, and block counts. Several layouts can share a path; conditions and weight decide which one renders.',
          'parameters'  => [
            'type'       => 'object',
            'properties' => new stdClass(),
            'required'   => [],
          ],
        ],
      ],
      'execute'    => 'ai_tools_execute_list_layouts',
      'operation'  => 'read',
      'permission' => 'administer layouts',
      'module'     => 'ai_tools',
      'category'   => 'layout',
    ],
    'get_layout' => [
      'definition' => [
        'type'     => 'function',
        'function' => [
          'name'        => 'get_layout',
          'description' => 'Inspect one layout: its template, every region with the blocks placed in it (in order, with block UUIDs), and its selection conditions. Use the UUIDs to move or remove blocks.',
          'parameters'  => [
            'type'       => 'object',
            'properties' => ['layout' => $layout_name],
            'required'   => ['layout'],
          ],
        ],
      ],
      'execute'    => 'ai_tools_execute_get_layout',
      'operation'  => 'read',
      'permission' => 'administer layouts',
      'module'     => 'ai_tools',
      'category'   => 'layout',
    ],
    'get_layout_for_path' => [
      'definition' => [
        'type'     => 'function',
        'function' => [
          'name'        => 'get_layout_for_path',
          'description' => 'Return which layout renders a given page path, for example node/12 or about-us.',
          'parameters'  => [
            'type'       => 'object',
            'properties' => [
              'path' => [
                'type'        => 'string',
                'description' => 'Internal path or URL alias of the page.',
              ],
            ],
            'required' => ['path'],
          ],
        ],
      ],
      'execute'    => 'ai_tools_execute_get_layout_for_path',
      'operation'  => 'read',
      'permission' => 'administer layouts',
      'module'     => 'ai_tools',
      'category'   => 'layout',
    ],
    'list_layout_blocks' => [
      'definition' => [
        'type'     => 'function',
        'function' => [
          'name'        => 'list_layout_blocks',
          'description' => 'List the blocks that can be added to a layout, keyed as module:delta. Only blocks whose required contexts the layout provides are listed.',
          'parameters'  => [
            'type'       => 'object',
            'properties' => [
              'layout' => $layout_name,
              'search' => [
                'type'        => 'string',
                'description' => 'Only return blocks whose key, label, or description contains this text.',
              ],
            ],
            'required' => ['layout'],
          ],
        ],
      ],
      'execute'    => 'ai_tools_execute_list_layout_blocks',
      'operation'  => 'read',
      'permission' => 'administer layouts',
      'module'     => 'ai_tools',
      'category'   => 'layout',
    ],
    'add_layout_block' => [
      'definition' => [
        'type'     => 'function',
        'function' => [
          'name'        => 'add_layout_block',
          'description' => 'Place a block in a layout region. Get valid block keys from list_layout_blocks and region names from get_layout.',
          'parameters'  => [
            'type'       => 'object',
            'properties' => [
              'layout' => $layout_name,
              'region' => [
                'type'        => 'string',
                'description' => 'Region machine name.',
              ],
              'block' => [
                'type'        => 'string',
                'description' => 'Block key as module:delta, for example system:main-menu or block:about.',
              ],
              'position' => [
                'type'        => 'integer',
                'description' => 'Zero-based position within the region. Omit to add the block last.',
              ],
              'title_display' => [
                'type'        => 'string',
                'enum'        => ['default', 'custom', 'none'],
                'description' => 'How the block title is shown. Use custom together with title.',
              ],
              'title' => [
                'type'        => 'string',
                'description' => 'Custom block title, used when title_display is custom.',
              ],
            ],
            'required' => ['layout', 'region', 'block'],
          ],
        ],
      ],
      'execute'     => 'ai_tools_execute_add_layout_block',
      'operation'   => 'write',
      'destructive' => FALSE,
      'permission'  => 'administer layouts',
      'module'      => 'ai_tools',
      'category'    => 'layout',
    ],
    'move_layout_block' => [
      'definition' => [
        'type'     => 'function',
        'function' => [
          'name'        => 'move_layout_block',
          'description' => 'Move a placed block to another region or position in the same layout.',
          'parameters'  => [
            'type'       => 'object',
            'properties' => [
              'layout' => $layout_name,
              'uuid' => [
                'type'        => 'string',
                'description' => 'UUID of the placed block, from get_layout.',
              ],
              'region' => [
                'type'        => 'string',
                'description' => 'Target region machine name.',
              ],
              'position' => [
                'type'        => 'integer',
                'description' => 'Zero-based position within the target region. Omit to place the block last.',
              ],
            ],
            'required' => ['layout', 'uuid', 'region'],
          ],
        ],
      ],
      'execute'     => 'ai_tools_execute_move_layout_block',
      'operation'   => 'write',
      'destructive' => FALSE,
      'permission'  => 'administer layouts',
      'module'      => 'ai_tools',
      'category'    => 'layout',
    ],
    'remove_layout_block' => [
      'definition' => [
        'type'     => 'function',
        'function' => [
          'name'        => 'remove_layout_block',
          'description' => 'Remove a placed block from a layout. The block itself is not deleted and can be added again.',
          'parameters'  => [
            'type'       => 'object',
            'properties' => [
              'layout' => $layout_name,
              'uuid' => [
                'type'        => 'string',
                'description' => 'UUID of the placed block, from get_layout.',
              ],
            ],
            'required' => ['layout', 'uuid'],
          ],
        ],
      ],
      'execute'     => 'ai_tools_execute_remove_layout_block',
      'operation'   => 'write',
      'destructive' => TRUE,
      'permission'  => 'administer layouts',
      'module'      => 'ai_tools',
      'category'    => 'layout',
    ],
  ];
}

function ai_tools_execute_list_layouts(array $args) {
  $result = [];
  foreach (layout_load_all() as $layout) {
    $result[] = [
      'layout'      => $layout->name,
      'title'       => $layout->title,
      'path'        => $layout->getPath(),
      'template'    => $layout->layout_template,
      'disabled'    => (bool) $layout->disabled,
      'weight'      => (int) $layout->weight,
      'conditions'  => ai_tools_layout_condition_summaries($layout),
      'block_count' => count($layout->content),
    ];
  }
  return ['layouts' => $result, 'count' => count($result)];
}

function ai_tools_execute_get_layout(array $args) {
  $layout = ai_tools_layout_load($args);
  if (is_array($layout)) {
    return $layout;
  }

  $template = layout_get_layout_template_info($layout->layout_template);
  $regions = [];
  foreach ($template['regions'] ?? [] as $region => $label) {
    $blocks = [];
    foreach ($layout->positions[$region] ?? [] as $uuid) {
      if (isset($layout->content[$uuid])) {
        $blocks[] = ai_tools_layout_block_summary($layout->content[$uuid]);
      }
    }
    $regions[] = [
      'region' => $region,
      'label'  => (string) $label,
      'blocks' => $blocks,
    ];
  }

  return [
    'layout'      => $layout->name,
    'title'       => $layout->title,
    'path'        => $layout->getPath(),
    'template'    => $layout->layout_template,
    'disabled'    => (bool) $layout->disabled,
    'conditions'  => ai_tools_layout_condition_summaries($layout),
    'regions'     => $regions,
    'has_unsaved_ui_changes' => (bool) tempstore_get('layout.layout', $layout->name),
  ];
}

function ai_tools_execute_get_layout_for_path(array $args) {
  $path = trim($args['path'] ?? '');
  if ($path === '<front>') {
    $path = config_get('system.core', 'site_frontpage');
  }
  $path = backdrop_get_normal_path($path);
  if ($path === '') {
    return ['error' => 'path is required.'];
  }
  $router_item = menu_get_item($path);
  if (!$router_item) {
    return ['error' => "No page found at '$path'."];
  }
  // Without access menu_get_item() omits the loaded path arguments, which
  // layout selection needs.
  if (empty($router_item['access'])) {
    return ['error' => "You do not have access to '$path', so its layout cannot be determined."];
  }
  $layout = layout_get_layout_by_path($path, $router_item);
  if (!$layout) {
    return ['error' => "No layout matches '$path'."];
  }
  return [
    'path'     => $path,
    'layout'   => $layout->name,
    'title'    => $layout->title,
    'template' => $layout->layout_template,
  ];
}

function ai_tools_execute_list_layout_blocks(array $args) {
  $layout = ai_tools_layout_load($args);
  if (is_array($layout)) {
    return $layout;
  }

  $search = backdrop_strtolower(trim($args['search'] ?? ''));
  $result = [];
  foreach (ai_tools_layout_available_blocks($layout) as $key => $info) {
    $label = (string) ($info['info'] ?? $key);
    $description = (string) ($info['description'] ?? '');
    if ($search !== '' && strpos(backdrop_strtolower($key . ' ' . $label . ' ' . $description), $search) === FALSE) {
      continue;
    }
    $result[] = [
      'block'       => $key,
      'label'       => $label,
      'description' => $description,
    ];
  }
  return ['layout' => $layout->name, 'blocks' => $result, 'count' => count($result)];
}

function ai_tools_execute_add_layout_block(array $args) {
  $layout = ai_tools_layout_load_for_edit($args);
  if (is_array($layout)) {
    return $layout;
  }

  $region = trim($args['region'] ?? '');
  if ($error = ai_tools_layout_check_region($layout, $region)) {
    return $error;
  }

  $key = trim($args['block'] ?? '');
  if (!isset(ai_tools_layout_available_blocks($layout)[$key])) {
    return ['error' => "Block '$key' is not available for layout '{$layout->name}'. Use list_layout_blocks to find valid block keys."];
  }

  $title_display = $args['title_display'] ?? '';
  if ($title_display !== '' && !in_array($title_display, [LAYOUT_TITLE_DEFAULT, LAYOUT_TITLE_CUSTOM, LAYOUT_TITLE_NONE], TRUE)) {
    return ['error' => 'title_display must be default, custom, or none.'];
  }

  list($module, $delta) = explode(':', $key, 2);
  $position = isset($args['position']) ? (int) $args['position'] : NULL;
  $block = $layout->addBlock($module, $delta, $region, $position);
  if ($title_display !== '') {
    $block->settings['title_display'] = $title_display;
  }
  if (isset($args['title'])) {
    $block->settings['title'] = $args['title'];
  }
  $layout->save();

  return [
    'status' => 'added',
    'layout' => $layout->name,
    'region' => $region,
    'block'  => ai_tools_layout_block_summary($block),
  ];
}

function ai_tools_execute_move_layout_block(array $args) {
  $layout = ai_tools_layout_load_for_edit($args);
  if (is_array($layout)) {
    return $layout;
  }

  $uuid = trim($args['uuid'] ?? '');
  if (!isset($layout->content[$uuid])) {
    return ['error' => "No block with UUID '$uuid' in layout '{$layout->name}'."];
  }
  $region = trim($args['region'] ?? '');
  if ($error = ai_tools_layout_check_region($layout, $region)) {
    return $error;
  }

  $position = isset($args['position']) ? (int) $args['position'] : NULL;
  $layout->setBlockPosition($uuid, $region, $position);
  $layout->save();

  return [
    'status'   => 'moved',
    'layout'   => $layout->name,
    'region'   => $region,
    'position' => $layout->getBlockPosition($uuid, 'position'),
    'block'    => ai_tools_layout_block_summary($layout->content[$uuid]),
  ];
}

function ai_tools_execute_remove_layout_block(array $args) {
  $layout = ai_tools_layout_load_for_edit($args);
  if (is_array($layout)) {
    return $layout;
  }

  $uuid = trim($args['uuid'] ?? '');
  if (!isset($layout->content[$uuid])) {
    return ['error' => "No block with UUID '$uuid' in layout '{$layout->name}'."];
  }

  $summary = ai_tools_layout_block_summary($layout->content[$uuid]);
  $region = $layout->getBlockPosition($uuid);
  $layout->removeBlock($uuid);
  $layout->save();

  return [
    'status' => 'removed',
    'layout' => $layout->name,
    'region' => $region,
    'block'  => $summary,
  ];
}

/**
 * Loads the layout named in the tool arguments, or returns an error array.
 */
function ai_tools_layout_load(array $args) {
  $name = trim($args['layout'] ?? '');
  $layout = $name !== '' ? layout_load($name) : FALSE;
  if (!$layout) {
    return ['error' => "Layout '$name' not found. Use list_layouts to find layout names."];
  }
  return $layout;
}

/**
 * Loads a layout for saving, or returns an error array.
 *
 * Refuses layouts with unsaved edits in the layout UI: saving would drop
 * either those edits or ours, and the UI holds the layout locked.
 */
function ai_tools_layout_load_for_edit(array $args) {
  if (!user_access('administer layouts')) {
    return ['error' => 'Access denied: You do not have permission to administer layouts.'];
  }
  $layout = ai_tools_layout_load($args);
  if (is_array($layout)) {
    return $layout;
  }
  if (tempstore_get('layout.layout', $layout->name)) {
    return ['error' => "Layout '{$layout->name}' has unsaved changes in the layout editor. Save or cancel them at admin/structure/layouts/manage/{$layout->name} first."];
  }
  return $layout;
}

/**
 * Returns an error array if the region does not exist in the layout template.
 */
function ai_tools_layout_check_region(Layout $layout, $region) {
  $template = layout_get_layout_template_info($layout->layout_template);
  if (!isset($template['regions'][$region])) {
    return ['error' => "Region '$region' does not exist in template '{$layout->layout_template}'. Valid regions: " . implode(', ', array_keys($template['regions'] ?? [])) . '.'];
  }
  return NULL;
}

/**
 * Returns the blocks the layout UI would offer for a layout, keyed by
 * module:delta (or module:delta:child).
 *
 * @see layout_block_add_page()
 */
function ai_tools_layout_available_blocks(Layout $layout) {
  $list = [];
  foreach (layout_get_block_info() as $module => $module_blocks) {
    foreach ((array) $module_blocks as $delta => $info) {
      $children = layout_create_handler('block', $module . ':' . $delta)->getChildren();
      $candidates = is_array($children) ? $children : [NULL => $info];
      foreach ($candidates as $child_id => $candidate) {
        if ($layout->hasContexts($candidate['required contexts'] ?? [])) {
          $key = $module . ':' . $delta . ($child_id !== '' && $child_id !== NULL ? ':' . $child_id : '');
          $list[$key] = $candidate;
        }
      }
    }
  }
  ksort($list);
  return $list;
}

/**
 * Returns the fields of a placed block that a model needs.
 */
function ai_tools_layout_block_summary(Block $block) {
  $key = $block->module . ':' . $block->delta;
  if (!empty($block->childDelta)) {
    $key .= ':' . $block->childDelta;
  }
  return [
    'uuid'          => $block->uuid,
    'block'         => $key,
    'label'         => html_entity_decode(strip_tags($block->getAdminTitle()), ENT_QUOTES, 'UTF-8'),
    'title_display' => $block->settings['title_display'],
    'title'         => $block->settings['title'],
  ];
}

/**
 * Returns plain-text summaries of a layout's selection conditions.
 */
function ai_tools_layout_condition_summaries(Layout $layout) {
  $summaries = [];
  foreach ($layout->conditions as $condition) {
    $summaries[] = html_entity_decode(strip_tags($condition->summary()), ENT_QUOTES, 'UTF-8');
  }
  return $summaries;
}
