<?php


/**
 * @file
 * Bee command discovery tools for AI agents.
 */

/**
 * Return Bee tool definitions.
 */
function ai_tools_bee_definitions() {
  return [
    'get_bee_commands' => [
      'definition' => [
        'type' => 'function',
        'function' => [
          'name' => 'get_bee_commands',
          'description' => 'List all Bee commands available on this site, including aliases, arguments, and descriptions.',
          'parameters' => [
            'type' => 'object',
            'properties' => new stdClass(),
            'required' => [],
          ],
        ],
      ],
      'execute' => 'ai_tools_execute_get_bee_commands',
      'module' => 'ai_tools',
      'category' => 'bee',
      'requirements' => 'ai_tools_bee_requirements',
    ],
    'get_bee_command_help' => [
      'definition' => [
        'type' => 'function',
        'function' => [
          'name' => 'get_bee_command_help',
          'description' => 'Return help text and metadata for one Bee command.',
          'parameters' => [
            'type' => 'object',
            'properties' => [
              'command' => [
                'type' => 'string',
                'description' => 'The Bee command name or alias.',
              ],
            ],
            'required' => ['command'],
          ],
        ],
      ],
      'execute' => 'ai_tools_execute_get_bee_command_help',
      'module' => 'ai_tools',
      'category' => 'bee',
      'requirements' => 'ai_tools_bee_requirements',
    ],
  ];
}

/**
 * Check whether the Bee runtime is available.
 */
function ai_tools_bee_requirements() {
  return file_exists('/opt/bee/bee.php');
}

/**
 * Load the Bee command registry helpers if needed.
 */
function ai_tools_load_bee_runtime() {
  // Reuse existing Bee runtime when CLI already bootstrapped it.
  if (function_exists('bee_all_commands')) {
    return TRUE;
  }

  $bee_root = '/opt/bee';
  if (!file_exists($bee_root . '/bee.php')) {
    return FALSE;
  }

  // Load Bee internals in a safe order for web requests.
  require_once $bee_root . '/includes/miscellaneous.inc';
  require_once $bee_root . '/includes/globals.inc';
  require_once $bee_root . '/includes/filesystem.inc';
  require_once $bee_root . '/includes/input.inc';
  require_once $bee_root . '/includes/info.inc';
  require_once $bee_root . '/includes/dependencies.inc';
  require_once $bee_root . '/includes/command.inc';
  require_once $bee_root . '/commands/help.bee.inc';

  return function_exists('bee_all_commands');
}

/**
 * Execute get_bee_commands.
 */
function ai_tools_execute_get_bee_commands(array $args) {
  if (!ai_tools_load_bee_runtime()) {
    return ['error' => 'Bee runtime is unavailable.'];
  }

  $commands = bee_all_commands();
  ksort($commands);

  $result = [];
  foreach ($commands as $command => $descriptor) {
    $result[] = [
      'name' => $command,
      'description' => !empty($descriptor['description']) ? (string) $descriptor['description'] : '',
      'group' => !empty($descriptor['group']) ? (string) $descriptor['group'] : 'miscellaneous',
      'aliases' => !empty($descriptor['aliases']) && is_array($descriptor['aliases']) ? array_values($descriptor['aliases']) : [],
      'arguments' => !empty($descriptor['arguments']) && is_array($descriptor['arguments']) ? ai_tools_bee_argument_summaries($descriptor['arguments']) : [],
      'options' => !empty($descriptor['options']) && is_array($descriptor['options']) ? ai_tools_bee_option_summaries($descriptor['options']) : [],
      'examples' => !empty($descriptor['examples']) && is_array($descriptor['examples']) ? $descriptor['examples'] : [],
    ];
  }

  return [
    'commands' => $result,
    'count' => count($result),
  ];
}

/**
 * Execute get_bee_command_help.
 */
function ai_tools_execute_get_bee_command_help(array $args) {
  if (!ai_tools_load_bee_runtime()) {
    return ['error' => 'Bee runtime is unavailable.'];
  }

  $command = !empty($args['command']) ? trim((string) $args['command']) : '';
  if ($command === '') {
    return ['error' => 'Missing command name.'];
  }

  $commands = bee_all_commands();
  global $_bee_command_aliases;
  if (!empty($_bee_command_aliases[$command])) {
    $command = $_bee_command_aliases[$command];
  }

  if (empty($commands[$command])) {
    return ['error' => "Unknown Bee command: $command"];
  }

  $descriptor = $commands[$command];
  $descriptor['command'] = $command;

  return [
    'command' => $command,
    'description' => !empty($descriptor['description']) ? (string) $descriptor['description'] : '',
    'group' => !empty($descriptor['group']) ? (string) $descriptor['group'] : 'miscellaneous',
    'aliases' => !empty($descriptor['aliases']) && is_array($descriptor['aliases']) ? array_values($descriptor['aliases']) : [],
    'arguments' => !empty($descriptor['arguments']) && is_array($descriptor['arguments']) ? ai_tools_bee_argument_summaries($descriptor['arguments']) : [],
    'options' => !empty($descriptor['options']) && is_array($descriptor['options']) ? ai_tools_bee_option_summaries($descriptor['options']) : [],
    'examples' => !empty($descriptor['examples']) && is_array($descriptor['examples']) ? $descriptor['examples'] : [],
    'help' => ai_tools_bee_format_help($descriptor),
  ];
}

/**
 * Summarize Bee command arguments.
 */
function ai_tools_bee_argument_summaries(array $arguments) {
  $result = [];
  foreach ($arguments as $name => $summary) {
    $result[] = [
      'name' => is_string($name) ? $name : (string) $summary,
      'description' => is_array($summary) && !empty($summary['description']) ? (string) $summary['description'] : (is_string($summary) ? $summary : ''),
    ];
  }
  return $result;
}

/**
 * Summarize Bee command options.
 */
function ai_tools_bee_option_summaries(array $options) {
  $result = [];
  foreach ($options as $name => $summary) {
    $result[] = [
      'name' => is_string($name) ? $name : (string) $summary,
      'description' => is_array($summary) && !empty($summary['description']) ? (string) $summary['description'] : (is_string($summary) ? $summary : ''),
    ];
  }
  return $result;
}

/**
 * Render a plain-text help summary for one Bee command.
 */
function ai_tools_bee_format_help(array $descriptor) {
  $name = !empty($descriptor['command']) ? (string) $descriptor['command'] : '';
  $help = [];
  if ($name !== '') {
    $help[] = "Command: $name";
  }
  if (!empty($descriptor['description'])) {
    $help[] = "Description: " . $descriptor['description'];
  }
  return implode("\n", $help);
}
