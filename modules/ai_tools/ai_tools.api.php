<?php

/**
 * @file
 * Hooks provided by AI Tools.
 */

/**
 * Register tools that AI models may call.
 *
 * Tools from every implementation are merged, passed through
 * hook_ai_tools_alter(), then filtered per user by ai_tools_get_tools().
 * Callers run a tool with ai_tools_dispatch($name, $arguments).
 *
 * The module that owns the integrated feature can implement this hook without
 * depending on ai_tools: when ai_tools is disabled the hook never runs. Guard
 * on module_exists() for any other module the tools need.
 *
 * @return array
 *   Tools keyed by name. The key must equal definition.function.name, because
 *   ai_tools_dispatch() looks tools up by that name. Each tool is an array:
 *   - definition: (required) An OpenAI-style function definition with 'type'
 *     set to 'function' and a 'function' array holding 'name', 'description'
 *     and 'parameters'. 'parameters' must be a JSON Schema object; list
 *     required arguments in 'required'.
 *   - execute: (required) Name of a function that receives the decoded
 *     arguments array and returns a JSON-encodable value or a JSON string.
 *     Return ['error' => '...'] on failure; exceptions are caught and
 *     returned to the model as an error.
 *   - operation: What the tool does. Agents run 'read' and 'propose' tools
 *     without approval; MCP also treats 'explain' and 'transform' as
 *     read-only. Any other value, or a missing one, is treated as 'write' and
 *     needs approval before it runs.
 *   - destructive: (optional) TRUE when the tool deletes or overwrites data.
 *     Always requires approval, whatever 'operation' says. Defaults to FALSE.
 *   - permission: (optional) Permission the current user needs. Tools the
 *     user lacks it for are left out of the registry.
 *   - requirements: (optional) Callable returning TRUE when the tool may be
 *     offered, for example when a third-party module or API key is present.
 *     Tools whose requirements fail are left out of the registry.
 *   - execute_args: (optional) Fixed values passed to the executor. Each key
 *     arrives in the arguments array prefixed with an underscore, so
 *     ['agent_id' => 'x'] becomes $args['_agent_id'].
 *   - module: (optional) Module that provides the tool. Informational.
 *   - category: (optional) Group name used by UIs such as the Tool Explorer.
 *
 * Executors run with the current user's permissions. A 'permission' entry
 * only hides the tool; executors that write must still check access to the
 * specific object they change.
 *
 * @see ai_tools_get_tools()
 * @see ai_tools_dispatch()
 */
function hook_ai_tools() {
  return [
    'get_campaign_summary' => [
      'definition' => [
        'type' => 'function',
        'function' => [
          'name' => 'get_campaign_summary',
          'description' => 'Return a summary of a campaign node by node ID.',
          'parameters' => [
            'type' => 'object',
            'properties' => [
              'nid' => [
                'type' => 'integer',
                'description' => 'The node ID of the campaign.',
              ],
            ],
            'required' => ['nid'],
            'additionalProperties' => FALSE,
          ],
        ],
      ],
      'execute' => 'mymodule_execute_get_campaign_summary',
      'operation' => 'read',
      'destructive' => FALSE,
      'permission' => 'access content',
      'requirements' => 'mymodule_tool_requirements',
      'module' => 'mymodule',
      'category' => 'campaigns',
    ],
  ];
}

/**
 * Alter registered tools before they are filtered for the current user.
 *
 * Use this to change descriptions, tighten permissions, mark a tool as
 * needing approval, or remove a tool with unset().
 *
 * @param array $tools
 *   Tools from all hook_ai_tools() implementations, keyed by name.
 *
 * @see hook_ai_tools()
 */
function hook_ai_tools_alter(array &$tools) {
  // Only let Views administrators run arbitrary View displays.
  if (isset($tools['get_views_result'])) {
    $tools['get_views_result']['permission'] = 'administer views';
  }
  // Never offer node creation on this site.
  unset($tools['create_node']);
}
