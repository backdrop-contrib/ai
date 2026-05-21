<?php

/**
 * @file
 * JSON-RPC 2.0 dispatcher for the AI MCP endpoint.
 *
 * Routes incoming method calls to the appropriate MCP handler function.
 */

/**
 * Dispatches a decoded JSON-RPC request body to the appropriate handler.
 *
 * @param array $body
 *   Decoded JSON-RPC 2.0 request body.
 * @param array $server
 *   Authenticated server config array.
 *
 * @return array
 *   JSON-RPC 2.0 response array (success or error).
 */
function ai_mcp_jsonrpc_dispatch(array $body, array $server) {
  module_load_include('inc', 'ai_mcp', 'includes/ai_mcp.auth');
  module_load_include('inc', 'ai_mcp', 'includes/ai_mcp.tools');

  $id     = isset($body['id'])     ? $body['id']     : NULL;
  $method = isset($body['method']) ? $body['method'] : '';
  $params = isset($body['params']) ? $body['params'] : [];

  switch ($method) {
    case 'initialize':
      return ai_mcp_method_initialize($id, $params, $server);

    case 'tools/list':
      return ai_mcp_method_tools_list($id, $params, $server);

    case 'tools/call':
      return ai_mcp_method_tools_call($id, $params, $server);

    default:
      return ai_mcp_jsonrpc_error($id, -32601, 'Method not found');
  }
}

/**
 * Handles the MCP 'initialize' method.
 *
 * Returns protocol version, capabilities, and server identification.
 *
 * @param mixed $id
 *   JSON-RPC request id.
 * @param array $params
 *   Request parameters (unused for initialize).
 * @param array $server
 *   Authenticated server config array.
 *
 * @return array
 *   JSON-RPC 2.0 success response.
 */
function ai_mcp_method_initialize($id, array $params, array $server) {
  $label = !empty($server['label']) ? $server['label'] : 'Backdrop MCP';

  return [
    'jsonrpc' => '2.0',
    'id'      => $id,
    'result'  => [
      'protocolVersion' => '2024-11-05',
      'capabilities'    => [
        'tools' => new stdClass(),
      ],
      'serverInfo'      => [
        'name'    => $label,
        'version' => '1.0',
      ],
    ],
  ];
}

/**
 * Handles the MCP 'tools/list' method.
 *
 * Returns all available tools (optionally filtered by server config).
 *
 * @param mixed $id
 *   JSON-RPC request id.
 * @param array $params
 *   Request parameters (cursor support reserved for future use).
 * @param array $server
 *   Authenticated server config array.
 *
 * @return array
 *   JSON-RPC 2.0 success response with tools array.
 */
function ai_mcp_method_tools_list($id, array $params, array $server) {
  $all_tools = ai_tools_get_tools();

  // Filter to the server's allowed tool set if configured.
  $allowed = isset($server['tools']) ? $server['tools'] : [];
  if (!empty($allowed) && is_array($allowed)) {
    $allowed_keys = array_filter($allowed);
    if (!empty($allowed_keys)) {
      $all_tools = array_intersect_key($all_tools, array_flip($allowed_keys));
    }
  }

  $mcp_tools = [];
  foreach ($all_tools as $key => $tool) {
    $mcp_tools[] = _ai_mcp_tool_to_mcp_schema($key, $tool);
  }

  return [
    'jsonrpc' => '2.0',
    'id'      => $id,
    'result'  => [
      'tools' => $mcp_tools,
    ],
  ];
}

/**
 * Handles the MCP 'tools/call' method.
 *
 * Dispatches the named tool via ai_tools_dispatch() and wraps the result
 * in the MCP content envelope.
 *
 * @param mixed $id
 *   JSON-RPC request id.
 * @param array $params
 *   Request parameters: 'name' (string) and 'arguments' (array).
 * @param array $server
 *   Authenticated server config array.
 *
 * @return array
 *   JSON-RPC 2.0 response with MCP content envelope.
 */
function ai_mcp_method_tools_call($id, array $params, array $server) {
  $tool_name = isset($params['name'])      ? $params['name']      : '';
  $arguments = isset($params['arguments']) ? $params['arguments'] : [];

  if (empty($tool_name)) {
    return ai_mcp_jsonrpc_error($id, -32602, 'Invalid params: tool name is required');
  }

  if (!is_array($arguments)) {
    $arguments = [];
  }

  // Enforce the server's tool allowlist.
  $allowed = isset($server['tools']) ? $server['tools'] : [];
  if (!empty($allowed) && is_array($allowed)) {
    $allowed_keys = array_filter($allowed);
    if (!empty($allowed_keys) && !in_array($tool_name, $allowed_keys)) {
      return ai_mcp_jsonrpc_error($id, -32602, 'Tool not available on this server');
    }
  }

  // Verify the tool exists in the registry (also enforces user permissions).
  $all_tools = ai_tools_get_tools();
  if (!isset($all_tools[$tool_name])) {
    return [
      'jsonrpc' => '2.0',
      'id'      => $id,
      'result'  => [
        'content' => [
          [
            'type' => 'text',
            'text' => 'Unknown or inaccessible tool: ' . $tool_name,
          ],
        ],
        'isError' => TRUE,
      ],
    ];
  }

  // Dispatch and wrap in MCP content envelope.
  try {
    $result_text = ai_tools_dispatch($tool_name, $arguments);
    return [
      'jsonrpc' => '2.0',
      'id'      => $id,
      'result'  => [
        'content' => [
          [
            'type' => 'text',
            'text' => $result_text,
          ],
        ],
        'isError' => FALSE,
      ],
    ];
  }
  catch (\Exception $e) {
    return [
      'jsonrpc' => '2.0',
      'id'      => $id,
      'result'  => [
        'content' => [
          [
            'type' => 'text',
            'text' => 'Tool execution error: ' . $e->getMessage(),
          ],
        ],
        'isError' => TRUE,
      ],
    ];
  }
}
