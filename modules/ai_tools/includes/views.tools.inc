<?php


/**
 * @file
 * Views tools for the AI Tools registry.
 * Only loaded when the Views module is enabled.
 */

function ai_tools_views_definitions() {
  return [
    'list_views' => [
      'definition' => [
        'type' => 'function',
        'function' => [
          'name' => 'list_views',
          'description' => 'List Backdrop Views with their labels, base tables, status, and displays.',
          'parameters' => [
            'type' => 'object',
            'properties' => new stdClass(),
          ],
        ],
      ],
      'execute' => 'ai_tools_execute_list_views',
      'module' => 'ai_tools',
      'category' => 'views',
      'operation' => 'read',
    ],
    'get_view_info' => [
      'definition' => [
        'type' => 'function',
        'function' => [
          'name' => 'get_view_info',
          'description' => 'Inspect one Backdrop View and return display, field, filter, sort, pager, and path configuration.',
          'parameters' => [
            'type' => 'object',
            'properties' => [
              'view_name' => [
                'type' => 'string',
                'description' => 'Machine name of the view.',
              ],
            ],
            'required' => ['view_name'],
          ],
        ],
      ],
      'execute' => 'ai_tools_execute_get_view_info',
      'module' => 'ai_tools',
      'category' => 'views',
      'operation' => 'read',
    ],
    'list_views_base_tables' => [
      'definition' => [
        'type' => 'function',
        'function' => [
          'name' => 'list_views_base_tables',
          'description' => 'List base tables available for creating Backdrop Views, such as node, users, taxonomy_term_data, and file_managed.',
          'parameters' => [
            'type' => 'object',
            'properties' => new stdClass(),
          ],
        ],
      ],
      'execute' => 'ai_tools_execute_list_views_base_tables',
      'module' => 'ai_tools',
      'category' => 'views',
      'operation' => 'read',
    ],
    'list_views_handlers' => [
      'definition' => [
        'type' => 'function',
        'function' => [
          'name' => 'list_views_handlers',
          'description' => 'List available Views handlers for a base table. Use this to find fields, filters, sorts, or arguments before creating a view.',
          'parameters' => [
            'type' => 'object',
            'properties' => [
              'base_table' => [
                'type' => 'string',
                'description' => 'Views base table, for example node, users, taxonomy_term_data, or file_managed.',
              ],
              'handler_type' => [
                'type' => 'string',
                'enum' => ['field', 'filter', 'sort', 'argument'],
                'description' => 'Handler type to list.',
              ],
            ],
            'required' => ['base_table'],
          ],
        ],
      ],
      'execute' => 'ai_tools_execute_list_views_handlers',
      'module' => 'ai_tools',
      'category' => 'views',
      'operation' => 'read',
    ],
    'create_basic_view' => [
      'definition' => [
        'type' => 'function',
        'function' => [
          'name' => 'create_basic_view',
          'description' => 'Create a simple Backdrop View with a default display and either a page or block display.',
          'parameters' => [
            'type' => 'object',
            'properties' => [
              'view_name' => [
                'type' => 'string',
                'description' => 'Machine name for the new view. Lowercase letters, numbers, and underscores are safest.',
              ],
              'label' => [
                'type' => 'string',
                'description' => 'Human-readable view label.',
              ],
              'description' => [
                'type' => 'string',
                'description' => 'Administrative description.',
              ],
              'base_table' => [
                'type' => 'string',
                'description' => 'Views base table. Defaults to node.',
              ],
              'display_type' => [
                'type' => 'string',
                'enum' => ['page', 'block'],
                'description' => 'Visible display to create. Defaults to page.',
              ],
              'path' => [
                'type' => 'string',
                'description' => 'Page path without leading slash. Required for page displays.',
              ],
              'style_plugin' => [
                'type' => 'string',
                'description' => 'Views style plugin. Defaults to default. Use table for tabular reports.',
              ],
              'items_per_page' => [
                'type' => 'integer',
                'description' => 'Items per page. Defaults to 10. Use 0 for no pager.',
              ],
              'fields' => [
                'type' => 'array',
                'items' => ['type' => 'string'],
                'description' => 'Field handler names to include from the base table, for example title, type, created.',
              ],
              'bundle' => [
                'type' => 'string',
                'description' => 'Optional node content type machine name. Only applies when base_table is node.',
              ],
              'published_only' => [
                'type' => 'boolean',
                'description' => 'For node views, filter to published content. Defaults to true.',
              ],
              'sort_field' => [
                'type' => 'string',
                'description' => 'Optional sort field from the base table, for example created or title.',
              ],
              'sort_order' => [
                'type' => 'string',
                'enum' => ['ASC', 'DESC'],
                'description' => 'Sort direction. Defaults to DESC.',
              ],
            ],
            'required' => ['view_name', 'label'],
          ],
        ],
      ],
      'execute' => 'ai_tools_execute_create_basic_view',
      'module' => 'ai_tools',
      'category' => 'views',
      'operation' => 'write',
      'destructive' => FALSE,
      'permission' => 'administer views',
    ],
    'add_view_display' => [
      'definition' => [
        'type' => 'function',
        'function' => [
          'name' => 'add_view_display',
          'description' => 'Add a simple page or block display to an existing Backdrop View.',
          'parameters' => [
            'type' => 'object',
            'properties' => [
              'view_name' => [
                'type' => 'string',
                'description' => 'Machine name of the existing view.',
              ],
              'display_type' => [
                'type' => 'string',
                'enum' => ['page', 'block'],
                'description' => 'Display type to add.',
              ],
              'title' => [
                'type' => 'string',
                'description' => 'Display title.',
              ],
              'path' => [
                'type' => 'string',
                'description' => 'Page path without leading slash. Required for page displays.',
              ],
              'items_per_page' => [
                'type' => 'integer',
                'description' => 'Items per page. Defaults to 10. Use 0 for no pager.',
              ],
            ],
            'required' => ['view_name', 'display_type'],
          ],
        ],
      ],
      'execute' => 'ai_tools_execute_add_view_display',
      'module' => 'ai_tools',
      'category' => 'views',
      'operation' => 'write',
      'destructive' => FALSE,
      'permission' => 'administer views',
    ],
    'get_views_result' => [
      'definition' => [
        'type'     => 'function',
        'function' => [
          'name'        => 'get_views_result',
          'description' => 'Execute a named Backdrop Views display and return its rows as structured data.',
          'parameters'  => [
            'type'       => 'object',
            'properties' => [
              'view_name' => [
                'type'        => 'string',
                'description' => 'Machine name of the view.',
              ],
              'display_id' => [
                'type'        => 'string',
                'description' => 'Display ID to execute (default: "default").',
              ],
              'arguments' => [
                'type'        => 'array',
                'items'       => ['type' => 'string'],
                'description' => 'Optional contextual filter arguments passed to the view.',
              ],
              'limit' => [
                'type'        => 'integer',
                'description' => 'Maximum rows to return (default 10, max 50).',
              ],
            ],
            'required' => ['view_name'],
          ],
        ],
      ],
      'execute'  => 'ai_tools_execute_get_views_result',
      'module'   => 'ai_tools',
      'category' => 'views',
      'operation' => 'read',
    ],
  ];
}

function ai_tools_execute_list_views(array $args) {
  $items = [];
  foreach (views_get_all_views() as $name => $view) {
    $displays = [];
    foreach ($view->display as $display_id => $display) {
      $options = isset($display->display_options) ? $display->display_options : [];
      $displays[] = [
        'id' => $display_id,
        'type' => $display->display_plugin,
        'title' => $display->display_title,
        'path' => isset($options['path']) ? $options['path'] : NULL,
      ];
    }
    $items[] = [
      'name' => $name,
      'label' => $view->human_name,
      'description' => $view->description,
      'base_table' => $view->base_table,
      'disabled' => !empty($view->disabled),
      'displays' => $displays,
    ];
  }

  return [
    'views' => $items,
    'count' => count($items),
  ];
}

function ai_tools_execute_get_view_info(array $args) {
  $view_name = trim($args['view_name'] ?? '');
  if ($view_name === '') {
    return ['error' => 'view_name is required.'];
  }

  $view = views_get_view($view_name);
  if (!$view) {
    return ['error' => "View '$view_name' not found."];
  }

  return ai_tools_views_view_summary($view);
}

function ai_tools_execute_list_views_base_tables(array $args) {
  $data = views_fetch_data();
  $base_tables = [];
  foreach ($data as $table => $table_data) {
    if (empty($table_data['table']['base'])) {
      continue;
    }
    $base = $table_data['table']['base'];
    $base_tables[] = [
      'table' => $table,
      'title' => isset($base['title']) ? (string) $base['title'] : $table,
      'help' => isset($base['help']) ? (string) $base['help'] : '',
      'base_field' => isset($base['field']) ? $base['field'] : '',
      'default_field' => isset($base['defaults']['field']) ? $base['defaults']['field'] : '',
    ];
  }

  return [
    'base_tables' => $base_tables,
    'count' => count($base_tables),
  ];
}

function ai_tools_execute_list_views_handlers(array $args) {
  $base_table = trim($args['base_table'] ?? 'node');
  $handler_type = trim($args['handler_type'] ?? '');
  $allowed = ['field', 'filter', 'sort', 'argument'];
  if ($handler_type !== '' && !in_array($handler_type, $allowed, TRUE)) {
    return ['error' => 'handler_type must be one of: ' . implode(', ', $allowed) . '.'];
  }

  $data = views_fetch_data($base_table);
  if (empty($data['table']['base'])) {
    return ['error' => "Base table '$base_table' was not found."];
  }

  $handlers = [];
  foreach ($data as $field => $field_data) {
    if ($field === 'table' || !is_array($field_data)) {
      continue;
    }
    foreach ($allowed as $type) {
      if ($handler_type !== '' && $type !== $handler_type) {
        continue;
      }
      if (empty($field_data[$type])) {
        continue;
      }
      $handlers[] = [
        'table' => $base_table,
        'field' => $field,
        'type' => $type,
        'title' => isset($field_data['title']) ? (string) $field_data['title'] : $field,
        'help' => isset($field_data['help']) ? (string) $field_data['help'] : '',
        'handler' => isset($field_data[$type]['handler']) ? $field_data[$type]['handler'] : '',
      ];
    }
  }

  return [
    'base_table' => $base_table,
    'handler_type' => $handler_type ?: 'all',
    'handlers' => $handlers,
    'count' => count($handlers),
  ];
}

function ai_tools_execute_create_basic_view(array $args) {
  if (!user_access('administer views')) {
    return ['error' => 'Access denied: You do not have permission to administer views.'];
  }
  $view_name = ai_tools_views_clean_machine_name($args['view_name'] ?? '');
  $label = trim($args['label'] ?? '');
  $description = trim($args['description'] ?? '');
  $base_table = trim($args['base_table'] ?? 'node');
  $display_type = trim($args['display_type'] ?? 'page');
  $path = trim($args['path'] ?? $view_name, '/');
  $style_plugin = trim($args['style_plugin'] ?? 'default');
  $items_per_page = isset($args['items_per_page']) ? (int) $args['items_per_page'] : 10;
  $fields = !empty($args['fields']) && is_array($args['fields']) ? $args['fields'] : [];
  $bundle = trim($args['bundle'] ?? '');
  $published_only = array_key_exists('published_only', $args) ? (bool) $args['published_only'] : TRUE;
  $sort_field = trim($args['sort_field'] ?? '');
  $sort_order = strtoupper(trim($args['sort_order'] ?? 'DESC')) === 'ASC' ? 'ASC' : 'DESC';

  if ($view_name === '') {
    return ['error' => 'A valid view_name is required.'];
  }
  if ($label === '') {
    return ['error' => 'label is required.'];
  }
  if (views_get_view($view_name)) {
    return ['error' => "View '$view_name' already exists."];
  }
  if (!in_array($display_type, ['page', 'block'], TRUE)) {
    return ['error' => 'display_type must be page or block.'];
  }
  if ($display_type === 'page' && $path === '') {
    return ['error' => 'path is required for page displays.'];
  }

  $table_data = views_fetch_data($base_table);
  if (empty($table_data['table']['base'])) {
    return ['error' => "Base table '$base_table' was not found."];
  }

  $view = views_new_view();
  $view->name = $view_name;
  $view->human_name = $label;
  $view->description = $description;
  $view->tag = 'default';
  $view->core = BACKDROP_VERSION;
  $view->base_table = $base_table;
  $view->disabled = FALSE;

  $default_display = $view->new_display('default', 'Defaults', 'default');
  foreach (ai_tools_views_default_display_options($base_table, $style_plugin, $items_per_page, $fields, $bundle, $published_only, $sort_field, $sort_order) as $option => $value) {
    $default_display->set_option($option, $value);
  }

  $display = $view->new_display($display_type, $label, $display_type);
  $display_options = ai_tools_views_visible_display_options($display_type, $label, $path, $style_plugin, $items_per_page);
  foreach ($display_options as $option => $value) {
    $display->set_option($option, $value);
  }

  try {
    $view->save();
  }
  catch (Exception $e) {
    return ['error' => 'Could not save the view: ' . $e->getMessage()];
  }

  $view = views_get_view($view_name, TRUE);
  return [
    'status' => 'created',
    'view' => ai_tools_views_view_summary($view),
    'edit_url' => url('admin/structure/views/view/' . $view_name),
  ];
}

function ai_tools_execute_add_view_display(array $args) {
  if (!user_access('administer views')) {
    return ['error' => 'Access denied: You do not have permission to administer views.'];
  }
  $view_name = trim($args['view_name'] ?? '');
  $display_type = trim($args['display_type'] ?? '');
  $title = trim($args['title'] ?? '');
  $path = trim($args['path'] ?? '', '/');
  $items_per_page = isset($args['items_per_page']) ? (int) $args['items_per_page'] : 10;

  if ($view_name === '') {
    return ['error' => 'view_name is required.'];
  }
  if (!in_array($display_type, ['page', 'block'], TRUE)) {
    return ['error' => 'display_type must be page or block.'];
  }
  if ($display_type === 'page' && $path === '') {
    return ['error' => 'path is required for page displays.'];
  }

  $view = views_get_view($view_name, TRUE);
  if (!$view) {
    return ['error' => "View '$view_name' not found."];
  }

  $title = $title ?: $view->human_name;
  $display = $view->new_display($display_type, $title);
  $display_id = $display->display->id;
  $style_plugin = $view->display['default']->display_options['style_plugin'] ?? 'default';
  $display_options = ai_tools_views_visible_display_options($display_type, $title, $path, $style_plugin, $items_per_page);
  foreach ($display_options as $option => $value) {
    $display->set_option($option, $value);
  }

  try {
    $view->save();
  }
  catch (Exception $e) {
    return ['error' => 'Could not save the view display: ' . $e->getMessage()];
  }

  return [
    'status' => 'display_added',
    'view_name' => $view_name,
    'display_id' => $display_id,
    'display_type' => $display_type,
    'edit_url' => url('admin/structure/views/view/' . $view_name),
  ];
}

function ai_tools_execute_get_views_result(array $args) {
  $view_name  = trim($args['view_name'] ?? '');
  $display_id = trim($args['display_id'] ?? 'default');
  $arguments  = $args['arguments'] ?? [];
  $limit      = min(max(1, (int) ($args['limit'] ?? 10)), 50);

  if ($view_name === '') {
    return ['error' => 'view_name is required.'];
  }

  $view = views_get_view($view_name);
  if (!$view) {
    return ['error' => "View '$view_name' not found."];
  }

  $view->set_display($display_id);
  if (!$view->access($display_id)) {
    return ['error' => 'Access denied: You do not have permission to access this view display.'];
  }
  $view->set_arguments($arguments);
  $view->set_items_per_page($limit);
  $view->execute();

  $rows = [];
  foreach ($view->result as $row) {
    $rows[] = (array) $row;
  }

  return [
    'view'    => $view_name,
    'display' => $display_id,
    'results' => $rows,
    'count'   => count($rows),
  ];
}

function ai_tools_views_default_display_options($base_table, $style_plugin, $items_per_page, array $fields, $bundle, $published_only, $sort_field, $sort_order) {
  $data = views_fetch_data($base_table);
  $default_field = isset($data['table']['base']['defaults']['field']) ? $data['table']['base']['defaults']['field'] : '';
  if (empty($fields)) {
    $fields = [$default_field];
  }

  $field_options = [];
  foreach ($fields as $field) {
    $field = ai_tools_views_clean_handler_name($field);
    if ($field === '' || empty($data[$field]['field'])) {
      continue;
    }
    $field_options[$field] = [
      'id' => $field,
      'table' => $base_table,
      'field' => $field,
    ];
  }
  if (empty($field_options) && $default_field !== '' && !empty($data[$default_field]['field'])) {
    $field_options[$default_field] = [
      'id' => $default_field,
      'table' => $base_table,
      'field' => $default_field,
    ];
  }

  $filters = [];
  if ($base_table === 'node') {
    if ($published_only && !empty($data['status']['filter'])) {
      $filters['status'] = [
        'id' => 'status',
        'table' => 'node',
        'field' => 'status',
        'value' => 1,
      ];
    }
    if ($bundle !== '' && !empty($data['type']['filter'])) {
      $filters['type'] = [
        'id' => 'type',
        'table' => 'node',
        'field' => 'type',
        'value' => [$bundle => $bundle],
      ];
    }
  }

  $sorts = [];
  $sort_field = ai_tools_views_clean_handler_name($sort_field);
  if ($sort_field !== '' && !empty($data[$sort_field]['sort'])) {
    $sorts[$sort_field] = [
      'id' => $sort_field,
      'table' => $base_table,
      'field' => $sort_field,
      'order' => $sort_order,
    ];
  }

  return [
    'access' => ['type' => 'none'],
    'cache' => ['type' => 'none'],
    'query' => ['type' => 'views_query'],
    'exposed_form' => ['type' => 'basic'],
    'pager' => ai_tools_views_pager_options($items_per_page),
    'style_plugin' => $style_plugin ?: 'default',
    'row_plugin' => 'fields',
    'fields' => $field_options,
    'filters' => $filters,
    'sorts' => $sorts,
  ];
}

function ai_tools_views_visible_display_options($display_type, $title, $path, $style_plugin, $items_per_page) {
  $options = [
    'title' => $title,
    'style_plugin' => $style_plugin ?: 'default',
    'row_plugin' => 'fields',
    'pager' => ai_tools_views_pager_options($items_per_page),
  ];
  if ($display_type === 'page') {
    $options['path'] = trim($path, '/');
  }
  return $options;
}

function ai_tools_views_pager_options($items_per_page) {
  $items_per_page = max(0, min(100, (int) $items_per_page));
  return [
    'type' => $items_per_page > 0 ? 'full' : 'none',
    'options' => [
      'items_per_page' => $items_per_page,
    ],
  ];
}

function ai_tools_views_view_summary($view) {
  $displays = [];
  foreach ($view->display as $display_id => $display) {
    $view->set_display($display_id);
    $handler = $view->display_handler;
    $displays[$display_id] = [
      'id' => $display_id,
      'type' => $display->display_plugin,
      'title' => $display->display_title,
      'path' => $handler ? $handler->get_option('path') : NULL,
      'style' => $handler ? $handler->get_option('style_plugin') : NULL,
      'row' => $handler ? $handler->get_option('row_plugin') : NULL,
      'pager' => $handler ? $handler->get_option('pager') : [],
      'fields' => $handler ? $handler->get_option('fields') : [],
      'filters' => $handler ? $handler->get_option('filters') : [],
      'sorts' => $handler ? $handler->get_option('sorts') : [],
      'arguments' => $handler ? $handler->get_option('arguments') : [],
    ];
  }

  return [
    'name' => $view->name,
    'label' => $view->human_name,
    'description' => $view->description,
    'base_table' => $view->base_table,
    'disabled' => !empty($view->disabled),
    'displays' => $displays,
  ];
}

function ai_tools_views_clean_machine_name($name) {
  $name = strtolower(trim((string) $name));
  $name = preg_replace('/[^a-z0-9_]+/', '_', $name);
  return trim($name, '_');
}

function ai_tools_views_clean_handler_name($name) {
  $name = trim((string) $name);
  if (strpos($name, '.') !== FALSE) {
    $parts = explode('.', $name);
    $name = end($parts);
  }
  return preg_replace('/[^A-Za-z0-9_]+/', '', $name);
}
