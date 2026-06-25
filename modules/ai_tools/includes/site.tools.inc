<?php


/**
 * @file
 * Site-level tools for the AI Tools registry.
 */

function ai_tools_site_definitions() {
  return [
    'get_site_info' => [
      'definition' => [
        'type'     => 'function',
        'function' => [
          'name'        => 'get_site_info',
          'description' => 'Return basic information about this Backdrop CMS site: name, slogan, URL, and version.',
          'parameters'  => [
            'type'       => 'object',
            'properties' => new stdClass(),
            'required'   => [],
          ],
        ],
      ],
      'execute'  => 'ai_tools_execute_get_site_info',
      'module'   => 'ai_tools',
      'category' => 'site',
      'permission' => 'access content overview',
    ],
    'get_content_types' => [
      'definition' => [
        'type'     => 'function',
        'function' => [
          'name'        => 'get_content_types',
          'description' => 'List all content types (machine names and labels) available on this Backdrop site.',
          'parameters'  => [
            'type'       => 'object',
            'properties' => new stdClass(),
            'required'   => [],
          ],
        ],
      ],
      'execute'  => 'ai_tools_execute_get_content_types',
      'module'   => 'ai_tools',
      'category' => 'site',
      'permission' => 'access content overview',
    ],
    'get_site_context' => [
      'definition' => [
        'type'     => 'function',
        'function' => [
          'name'        => 'get_site_context',
          'description' => 'Return the administrator-configured site context: the site\'s purpose, audience, editorial guidelines, and any custom instructions. Call this when you need to understand how to write for or assist with this specific site.',
          'parameters'  => [
            'type'       => 'object',
            'properties' => new stdClass(),
            'required'   => [],
          ],
        ],
      ],
      'execute'  => 'ai_tools_execute_get_site_context',
      'module'   => 'ai_tools',
      'category' => 'site',
      'permission' => 'administer site configuration',
    ],
  ];
}

function ai_tools_execute_get_site_info(array $args) {
  return [
    'site_name'       => config_get('system.core', 'site_name'),
    'slogan'          => config_get('system.core', 'site_slogan'),
    'url'             => url('<front>', ['absolute' => TRUE]),
    'backdrop_version' => BACKDROP_VERSION,
  ];
}

function ai_tools_execute_get_content_types(array $args) {
  $types = node_type_get_types();
  $result = [];
  foreach ($types as $machine => $info) {
    $result[] = [
      'machine_name' => $machine,
      'label'        => $info->name,
      'description'  => $info->description,
    ];
  }
  return ['content_types' => $result, 'count' => count($result)];
}

function ai_tools_execute_get_site_context(array $args) {
  if (!module_exists('ai_site_context')) {
    return ['error' => 'ai_site_context module is not enabled.'];
  }
  $config  = config('ai_site_context.settings');
  $context = token_replace(trim($config->get('global_prompt') ?: ''), [], ['sanitize' => FALSE]);
  if ($context === '') {
    return ['site_context' => null, 'note' => 'No site context has been configured.'];
  }
  return ['site_context' => $context];
}
