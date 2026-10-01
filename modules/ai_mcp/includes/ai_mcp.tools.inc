<?php

/**
 * @file
 * Tool schema mapping helpers for the AI MCP module.
 *
 * Converts ai_tools registry entries into MCP-compatible tool descriptors.
 */

/**
 * Maps a single ai_tools entry to an MCP tool schema object.
 *
 * @param string $tool_key
 *   The registry key (tool name) from ai_tools_get_tools().
 * @param array $tool
 *   The tool definition array as returned by ai_tools_get_tools().
 *
 * @return array
 *   MCP tool descriptor with name, description, inputSchema, and annotations.
 */
function _ai_mcp_tool_to_mcp_schema($tool_key, array $tool) {
  $fn = isset($tool['definition']['function']) ? $tool['definition']['function'] : [];

  $name        = isset($fn['name'])        ? $fn['name']        : $tool_key;
  $description = isset($fn['description']) ? $fn['description'] : '';
  $parameters  = isset($fn['parameters'])  ? $fn['parameters']  : ['type' => 'object', 'properties' => new stdClass()];

  $operation   = isset($tool['operation']) ? $tool['operation'] : '';
  $destructive = !empty($tool['destructive']);

  $read_only_ops = ['explain', 'read', 'transform'];
  $read_only = !$destructive && in_array($operation, $read_only_ops, TRUE);

  return [
    'name'        => $name,
    'description' => $description,
    'inputSchema' => $parameters,
    'annotations' => [
      'readOnlyHint'    => $read_only,
      'destructiveHint' => $destructive,
    ],
  ];
}

/**
 * Whether a tool can change site state.
 *
 * Fails closed: anything not explicitly a read-type operation counts as a
 * write. 'propose' tools only queue a proposal for a person to review, so
 * they are not treated as writes.
 */
function ai_mcp_tool_is_write(array $tool) {
  if (!empty($tool['destructive'])) {
    return TRUE;
  }
  $operation = isset($tool['operation']) ? (string) $tool['operation'] : '';
  return !in_array($operation, ['explain', 'read', 'transform', 'propose'], TRUE);
}

/**
 * Whether a server may list and call a tool.
 *
 * MCP has no server-side approval step: a tools/call runs immediately, and
 * any confirmation is left to the client. So write tools are only exposed
 * when the server explicitly opts in with 'allow_write_tools', and the
 * per-server allowlist (when set) still applies on top.
 */
function ai_mcp_server_allows_tool(array $server, $tool_key, array $tool) {
  $allowed = isset($server['tools']) ? array_filter((array) $server['tools']) : [];
  if (!empty($allowed) && !in_array($tool_key, $allowed, TRUE)) {
    return FALSE;
  }
  if (ai_mcp_tool_is_write($tool) && empty($server['allow_write_tools'])) {
    return FALSE;
  }
  return TRUE;
}
