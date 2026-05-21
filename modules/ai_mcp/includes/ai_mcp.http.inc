<?php

/**
 * @file
 * HTTP transport handler for the AI MCP endpoint.
 *
 * Handles both GET (SSE probe) and POST (JSON-RPC dispatch) on ai/mcp.
 */

/**
 * Page callback for ai/mcp — branches on REQUEST_METHOD.
 */
function ai_mcp_http_handler() {
  module_load_include('inc', 'ai_mcp', 'includes/ai_mcp.auth');
  module_load_include('inc', 'ai_mcp', 'includes/ai_mcp.jsonrpc');

  $method = isset($_SERVER['REQUEST_METHOD']) ? $_SERVER['REQUEST_METHOD'] : 'GET';

  if ($method === 'GET') {
    _ai_mcp_sse_probe();
  }
  else {
    _ai_mcp_post_handler();
  }
}

/**
 * Handles the GET SSE probe — unauthenticated endpoint discovery.
 */
function _ai_mcp_sse_probe() {
  header('Content-Type: text/event-stream');
  header('Cache-Control: no-cache');
  header('X-Accel-Buffering: no');

  $endpoint = url('ai/mcp', ['absolute' => TRUE]);
  print "data: " . json_encode(['endpoint' => $endpoint]) . "\n\n";

  // Flush output and exit cleanly.
  if (ob_get_level() > 0) {
    ob_flush();
  }
  flush();
  exit;
}

/**
 * Handles POST requests — authenticates then dispatches JSON-RPC.
 */
function _ai_mcp_post_handler() {
  // Authenticate before processing the body.
  $server = ai_mcp_authenticate_request();
  if ($server === FALSE) {
    http_response_code(401);
    header('Content-Type: application/json');
    $error = ai_mcp_jsonrpc_error(NULL, -32000, 'Unauthorized: valid Bearer token required');
    print json_encode($error);
    exit;
  }

  // Read and decode the request body.
  $raw = file_get_contents('php://input');
  if (empty($raw)) {
    http_response_code(400);
    header('Content-Type: application/json');
    $error = ai_mcp_jsonrpc_error(NULL, -32700, 'Parse error: empty request body');
    print json_encode($error);
    exit;
  }

  $body = json_decode($raw, TRUE);
  if (!is_array($body)) {
    http_response_code(400);
    header('Content-Type: application/json');
    $error = ai_mcp_jsonrpc_error(NULL, -32700, 'Parse error: invalid JSON');
    print json_encode($error);
    exit;
  }

  // Validate JSON-RPC 2.0 envelope.
  if (!isset($body['jsonrpc']) || $body['jsonrpc'] !== '2.0' || !isset($body['method'])) {
    header('Content-Type: application/json');
    $id = isset($body['id']) ? $body['id'] : NULL;
    $error = ai_mcp_jsonrpc_error($id, -32600, 'Invalid Request');
    print json_encode($error);
    exit;
  }

  $result = ai_mcp_jsonrpc_dispatch($body, $server);

  header('Content-Type: application/json');
  print json_encode($result);
  exit;
}
