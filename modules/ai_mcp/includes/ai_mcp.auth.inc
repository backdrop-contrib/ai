<?php

/**
 * @file
 * Authentication helpers for the AI MCP endpoint.
 */

/**
 * Authenticates an incoming MCP request by Bearer token.
 *
 * Reads the Authorization header, extracts the Bearer token, and finds the
 * first server config whose auth_key matches.
 *
 * @return array|FALSE
 *   The matching server config array, or FALSE if no match found.
 */
function ai_mcp_authenticate_request() {
  $token = _ai_mcp_extract_bearer_token();
  if (empty($token)) {
    return FALSE;
  }

  $servers = ai_mcp_server_load_all();
  foreach ($servers as $server) {
    if (!empty($server['auth_key']) && hash_equals($server['auth_key'], $token)) {
      return $server;
    }
  }

  return FALSE;
}

/**
 * Extracts a Bearer token from the Authorization header.
 *
 * @return string|FALSE
 *   The token string, or FALSE if no valid Bearer header is present.
 */
function _ai_mcp_extract_bearer_token() {
  // Check the standard Authorization header.
  $auth_header = '';

  if (function_exists('getallheaders')) {
    $headers = getallheaders();
    // Headers are case-insensitive; search for Authorization.
    foreach ($headers as $name => $value) {
      if (strtolower($name) === 'authorization') {
        $auth_header = $value;
        break;
      }
    }
  }

  // Fallback: Apache/PHP may expose it via $_SERVER.
  if (empty($auth_header)) {
    if (!empty($_SERVER['HTTP_AUTHORIZATION'])) {
      $auth_header = $_SERVER['HTTP_AUTHORIZATION'];
    }
    elseif (!empty($_SERVER['REDIRECT_HTTP_AUTHORIZATION'])) {
      $auth_header = $_SERVER['REDIRECT_HTTP_AUTHORIZATION'];
    }
  }

  if (empty($auth_header)) {
    return FALSE;
  }

  if (stripos($auth_header, 'Bearer ') === 0) {
    return trim(substr($auth_header, 7));
  }

  return FALSE;
}

/**
 * Builds a JSON-RPC 2.0 error response array.
 *
 * @param mixed $id
 *   The request id (may be NULL for parse errors).
 * @param int $code
 *   JSON-RPC error code.
 * @param string $message
 *   Human-readable error message.
 *
 * @return array
 *   JSON-RPC 2.0 error response array.
 */
function ai_mcp_jsonrpc_error($id, $code, $message) {
  return [
    'jsonrpc' => '2.0',
    'id'      => $id,
    'error'   => [
      'code'    => (int) $code,
      'message' => (string) $message,
    ],
  ];
}
