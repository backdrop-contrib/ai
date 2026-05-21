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

  $operation   = isset($tool['operation'])   ? $tool['operation']   : 'read';
  $destructive = !empty($tool['destructive']);

  $read_only_ops = ['explain', 'read', 'transform'];
  $read_only = in_array($operation, $read_only_ops);

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
