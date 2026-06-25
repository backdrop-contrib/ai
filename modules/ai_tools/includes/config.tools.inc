<?php


/**
 * @file
 * Configuration introspection tools for the AI Tools registry.
 * Read-only — never exposes secrets or credentials.
 */

/**
 * Any config key matching these patterns is blocked.
 */
define('AI_TOOLS_CONFIG_BLOCKLIST', '/key|secret|password|pass|token|credential|private|salt|hash/i');

function ai_tools_config_definitions() {
  return [
    'list_config_names' => [
      'definition' => [
        'type'     => 'function',
        'function' => [
          'name'        => 'list_config_names',
          'description' => 'List configuration file names available on this Backdrop site. Useful for discovering config to inspect with get_config.',
          'parameters'  => [
            'type'       => 'object',
            'properties' => [
              'prefix' => [
                'type'        => 'string',
                'description' => 'Optional prefix to filter results (e.g. "system", "ai", "views").',
              ],
              'limit' => [
                'type'        => 'integer',
                'description' => 'Maximum results (default 20, max 100).',
              ],
            ],
            'required' => [],
          ],
        ],
      ],
      'execute'     => 'ai_tools_execute_list_config_names',
      'module'      => 'ai_tools',
      'category'    => 'config',
      'permission'  => 'administer site configuration',
    ],
    'get_config' => [
      'definition' => [
        'type'     => 'function',
        'function' => [
          'name'        => 'get_config',
          'description' => 'Read a Backdrop configuration object by its name and return its values. Sensitive keys (passwords, tokens, secrets) are automatically redacted.',
          'parameters'  => [
            'type'       => 'object',
            'properties' => [
              'config_name' => [
                'type'        => 'string',
                'description' => 'Config file name without .json extension (e.g. "system.core", "ai.settings").',
              ],
              'key' => [
                'type'        => 'string',
                'description' => 'Optional specific key to return instead of the full config object.',
              ],
            ],
            'required' => ['config_name'],
          ],
        ],
      ],
      'execute'     => 'ai_tools_execute_get_config',
      'module'      => 'ai_tools',
      'category'    => 'config',
      'permission'  => 'administer site configuration',
    ],
  ];
}

// -----------------------------------------------------------------------
// Executors
// -----------------------------------------------------------------------

function ai_tools_execute_list_config_names(array $args) {
  if (!user_access('administer site configuration')) {
    return ['error' => 'Access denied: You do not have permission to administer site configuration.'];
  }

  $limit  = min(max(1, (int) ($args['limit'] ?? 20)), 100);
  $prefix = preg_replace('/[^a-zA-Z0-9_\-.]/', '', trim($args['prefix'] ?? ''));

  $config_dir = config_get_config_directory();
  if (!$config_dir || !is_dir($config_dir)) {
    return ['error' => 'Config directory not found.'];
  }

  $files = glob($config_dir . '/' . ($prefix ? $prefix . '*' : '*') . '.json');
  if (!$files) {
    return ['names' => [], 'count' => 0];
  }

  sort($files);
  $names = [];
  foreach (array_slice($files, 0, $limit) as $file) {
    $names[] = basename($file, '.json');
  }

  return ['names' => $names, 'count' => count($names), 'total_available' => count($files)];
}

function ai_tools_execute_get_config(array $args) {
  if (!user_access('administer site configuration')) {
    return ['error' => 'Access denied: You do not have permission to administer site configuration.'];
  }

  $config_name = trim($args['config_name'] ?? '');
  if ($config_name === '') {
    return ['error' => 'config_name is required.'];
  }

  // Prevent path traversal.
  if (strpos($config_name, '/') !== FALSE || strpos($config_name, '..') !== FALSE) {
    return ['error' => 'Invalid config name.'];
  }

  $config = config($config_name);
  if ($config->isNew()) {
    return ['error' => "Config '$config_name' not found."];
  }

  $specific_key = trim($args['key'] ?? '');
  if ($specific_key !== '') {
    if (preg_match(AI_TOOLS_CONFIG_BLOCKLIST, $specific_key)) {
      return ['error' => "Key '$specific_key' is redacted for security."];
    }
    $value = $config->get($specific_key);
    // A non-sensitive key name can still return a subtree containing secrets
    // (e.g. key "providers" → each provider's api_key). Redact recursively.
    if (is_array($value)) {
      $value = _ai_tools_redact_config($value);
    }
    return ['config' => $config_name, 'key' => $specific_key, 'value' => $value];
  }

  // Return full config with sensitive keys redacted.
  $data = $config->get();
  $data = _ai_tools_redact_config($data);
  return ['config' => $config_name, 'data' => $data];
}

/**
 * Recursively redact sensitive keys from a config array.
 */
function _ai_tools_redact_config(array $data) {
  foreach ($data as $key => $value) {
    if (preg_match(AI_TOOLS_CONFIG_BLOCKLIST, $key)) {
      $data[$key] = '[REDACTED]';
    }
    elseif (is_array($value)) {
      $data[$key] = _ai_tools_redact_config($value);
    }
  }
  return $data;
}
