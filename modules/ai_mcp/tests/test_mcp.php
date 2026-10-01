<?php

/**
 * @file
 * Verification and regression test suite for the AI MCP Server module.
 *
 * Can be executed via CLI:
 *   ddev exec php modules/contrib/ai/modules/ai_mcp/tests/test_mcp.php
 * Or directly with PHP if Backdrop is bootstrapped.
 */

if (!defined('BACKDROP_ROOT')) {
  define('BACKDROP_ROOT', getcwd());
}

// Bootstrap Backdrop if not already bootstrapped.
if (!function_exists('backdrop_bootstrap')) {
  require_once BACKDROP_ROOT . '/core/includes/bootstrap.inc';
  backdrop_bootstrap(BACKDROP_BOOTSTRAP_FULL);
}

// Ensure required modules are loaded.
module_load_include('inc', 'ai_mcp', 'includes/ai_mcp.auth');
module_load_include('inc', 'ai_mcp', 'includes/ai_mcp.tools');
module_load_include('inc', 'ai_mcp', 'includes/ai_mcp.jsonrpc');
module_load_include('inc', 'ai_mcp', 'includes/ai_mcp.http');
module_load_include('inc', 'ai_tools', 'includes/ai_tools.registry');

$passed = 0;
$failed = 0;

function test_assert($condition, $description) {
  global $passed, $failed;
  if ($condition) {
    $passed++;
    print "  [PASS] {$description}\n";
  }
  else {
    $failed++;
    print "  [FAIL] {$description}\n";
  }
}

print "============================================================\n";
print " AI MCP Server Test Suite\n";
print "============================================================\n\n";

// ---------------------------------------------------------------------------
// 1. Tool Schema Conversion Tests
// ---------------------------------------------------------------------------
print "--- 1. Testing Tool Schema Mapping (_ai_mcp_tool_to_mcp_schema) ---\n";

$sample_read_tool = [
  'definition' => [
    'function' => [
      'name' => 'sample_reader',
      'description' => 'Reads sample records.',
      'parameters' => [
        'type' => 'object',
        'properties' => [
          'id' => ['type' => 'integer', 'description' => 'Record ID'],
        ],
        'required' => ['id'],
      ],
    ],
  ],
  'operation' => 'read',
  'destructive' => FALSE,
];

$schema = _ai_mcp_tool_to_mcp_schema('sample_reader', $sample_read_tool);
test_assert(isset($schema['name']) && $schema['name'] === 'sample_reader', 'Schema includes expected name');
test_assert(isset($schema['description']) && $schema['description'] === 'Reads sample records.', 'Schema includes expected description');
test_assert(isset($schema['inputSchema']['properties']['id']), 'Schema preserves inputSchema properties');
test_assert(isset($schema['annotations']['readOnlyHint']) && $schema['annotations']['readOnlyHint'] === TRUE, 'Annotations mark read operation as readOnlyHint: TRUE');
test_assert(isset($schema['annotations']['destructiveHint']) && $schema['annotations']['destructiveHint'] === FALSE, 'Annotations mark non-destructive tool as destructiveHint: FALSE');

$sample_destructive_tool = [
  'definition' => [
    'function' => [
      'name' => 'sample_purger',
      'description' => 'Permanently deletes records.',
    ],
  ],
  'operation' => 'delete',
  'destructive' => TRUE,
];

$schema_dest = _ai_mcp_tool_to_mcp_schema('sample_purger', $sample_destructive_tool);
test_assert($schema_dest['annotations']['readOnlyHint'] === FALSE, 'Annotations mark delete operation as readOnlyHint: FALSE');
test_assert($schema_dest['annotations']['destructiveHint'] === TRUE, 'Annotations mark destructive tool as destructiveHint: TRUE');

// ---------------------------------------------------------------------------
// 2. Server Configuration CRUD Tests
// ---------------------------------------------------------------------------
print "\n--- 2. Testing Server Configuration CRUD ---\n";

$test_server_id = 'test_mcp_suite_' . mt_rand(1000, 9999);
$test_token = 'secret-token-suite-' . mt_rand(100000, 999999);
$test_server_config = [
  'id' => $test_server_id,
  'label' => 'Test Suite MCP Server',
  'auth_key' => $test_token,
  'tools' => ['list_content_types'],
  'oauth_enabled' => FALSE,
  'uid' => 1,
];

ai_mcp_server_save($test_server_config);
$loaded = ai_mcp_server_load($test_server_id);

test_assert(is_array($loaded), 'Server config successfully saved and loaded');
test_assert($loaded['id'] === $test_server_id, 'Loaded server ID matches');
test_assert($loaded['label'] === 'Test Suite MCP Server', 'Loaded server label matches');
test_assert($loaded['auth_key'] === $test_token, 'Loaded server auth_key matches');
test_assert($loaded['tools'] === ['list_content_types'], 'Loaded server allowed tools match');

$all_servers = ai_mcp_server_load_all();
test_assert(isset($all_servers[$test_server_id]), 'Saved server appears in ai_mcp_server_load_all()');

// ---------------------------------------------------------------------------
// 3. Authentication Extraction Tests
// ---------------------------------------------------------------------------
print "\n--- 3. Testing Authentication & Token Extraction ---\n";

unset($_SERVER['HTTP_AUTHORIZATION']);
unset($_SERVER['REDIRECT_HTTP_AUTHORIZATION']);
test_assert(ai_mcp_authenticate_request() === FALSE, 'Authentication fails when no Authorization header provided');

$_SERVER['HTTP_AUTHORIZATION'] = 'Bearer invalid-dummy-token';
test_assert(ai_mcp_authenticate_request() === FALSE, 'Authentication fails with invalid Bearer token');

$_SERVER['HTTP_AUTHORIZATION'] = 'Bearer ' . $test_token;
$auth_server = ai_mcp_authenticate_request();
test_assert(is_array($auth_server) && $auth_server['id'] === $test_server_id, 'Authentication succeeds with valid configured Bearer token');
unset($_SERVER['HTTP_AUTHORIZATION']);

// ---------------------------------------------------------------------------
// 4. JSON-RPC Protocol Dispatch Tests
// ---------------------------------------------------------------------------
print "\n--- 4. Testing JSON-RPC Method Dispatching ---\n";

// 4.1. Handshake initialize
$init_req = [
  'jsonrpc' => '2.0',
  'id' => 'req-init-1',
  'method' => 'initialize',
  'params' => [
    'protocolVersion' => '2024-11-05',
    'capabilities' => [],
    'clientInfo' => ['name' => 'test-client', 'version' => '1.0'],
  ],
];
$init_res = ai_mcp_jsonrpc_dispatch($init_req, $loaded);
test_assert($init_res['jsonrpc'] === '2.0', 'Initialize response contains jsonrpc 2.0');
test_assert($init_res['id'] === 'req-init-1', 'Initialize response id matches request id');
test_assert(isset($init_res['result']['protocolVersion']) && $init_res['result']['protocolVersion'] === '2024-11-05', 'Protocol version matches MCP spec 2024-11-05');
test_assert(isset($init_res['result']['capabilities']['tools']), 'Capabilities declare tools support');
test_assert(isset($init_res['result']['serverInfo']['name']) && $init_res['result']['serverInfo']['name'] === 'Test Suite MCP Server', 'Server info contains configured label');

// 4.2. Notification handling
$notify_req = [
  'jsonrpc' => '2.0',
  'method' => 'notifications/initialized',
  'params' => [],
];
$notify_res = ai_mcp_jsonrpc_dispatch($notify_req, $loaded);
test_assert($notify_res === NULL, 'Notification returns NULL so HTTP handler can return 202 without body');

// 4.3. Ping
$ping_req = [
  'jsonrpc' => '2.0',
  'id' => 'req-ping-2',
  'method' => 'ping',
  'params' => [],
];
$ping_res = ai_mcp_jsonrpc_dispatch($ping_req, $loaded);
test_assert(isset($ping_res['result']), 'Ping returns result');

// 4.4. tools/list with filtering
$list_req = [
  'jsonrpc' => '2.0',
  'id' => 'req-list-3',
  'method' => 'tools/list',
  'params' => [],
];
$list_res = ai_mcp_jsonrpc_dispatch($list_req, $loaded);
test_assert(isset($list_res['result']['tools']) && is_array($list_res['result']['tools']), 'tools/list returns tools array');
test_assert(count($list_res['result']['tools']) === 1, 'tools/list strictly respects server allowed tools filter');
test_assert($list_res['result']['tools'][0]['name'] === 'list_content_types', 'Returned tool matches allowed tool name');

// 4.5. tools/call for allowed tool
$call_req = [
  'jsonrpc' => '2.0',
  'id' => 'req-call-4',
  'method' => 'tools/call',
  'params' => [
    'name' => 'list_content_types',
    'arguments' => [],
  ],
];
$call_res = ai_mcp_method_tools_call('req-call-4', $call_req['params'], $loaded);
test_assert(isset($call_res['result']['content'][0]['text']), 'tools/call returns text content in envelope');
test_assert($call_res['result']['isError'] === FALSE, 'tools/call executes successfully with isError: FALSE');
$decoded_content = json_decode($call_res['result']['content'][0]['text'], TRUE);
test_assert(is_array($decoded_content) && isset($decoded_content['content_types']), 'Tool execution output contains valid content types');

// 4.6. tools/call for unallowed tool (filter violation)
$disallowed_call = [
  'name' => 'search_nodes',
  'arguments' => [],
];
$disallowed_res = ai_mcp_method_tools_call('req-call-5', $disallowed_call, $loaded);
test_assert(isset($disallowed_res['error']) && $disallowed_res['error']['code'] === -32602, 'Disallowed tool call returns JSON-RPC -32602 error');

// 4.6b. Write tools need the server's explicit opt-in. MCP has no approval
// step, so a misled client must not be able to change the site by default.
$write_tool = ['operation' => 'write'];
test_assert(ai_mcp_tool_is_write($write_tool), 'operation=write counts as a write tool');
test_assert(ai_mcp_tool_is_write([]), 'A tool with no operation counts as a write tool (fail closed)');
test_assert(ai_mcp_tool_is_write(['operation' => 'read', 'destructive' => TRUE]), 'A destructive tool counts as a write tool');
test_assert(!ai_mcp_tool_is_write(['operation' => 'read']), 'operation=read is not a write tool');
test_assert(!ai_mcp_tool_is_write(['operation' => 'propose']), 'operation=propose (queued for human review) is not a write tool');
test_assert(!ai_mcp_server_allows_tool(['tools' => []], 'some_write', $write_tool), 'Write tool refused when server has not opted in');
test_assert(!ai_mcp_server_allows_tool(['tools' => ['some_write']], 'some_write', $write_tool), 'Write tool refused even when allowlisted, without opt-in');
test_assert(ai_mcp_server_allows_tool(['tools' => [], 'allow_write_tools' => TRUE], 'some_write', $write_tool), 'Write tool allowed once server opts in');
test_assert(!ai_mcp_server_allows_tool(['tools' => ['other'], 'allow_write_tools' => TRUE], 'some_write', $write_tool), 'Opt-in does not override the allowlist');
$open_server = $loaded;
$open_server['tools'] = [];
$open_server['allow_write_tools'] = FALSE;
$open_list = ai_mcp_method_tools_list('req-list-w', [], $open_server);
$registry = ai_tools_get_tools();
$listed_writes = [];
foreach ($open_list['result']['tools'] as $listed) {
  if (isset($registry[$listed['name']]) && ai_mcp_tool_is_write($registry[$listed['name']])) {
    $listed_writes[] = $listed['name'];
  }
}
test_assert(empty($listed_writes), 'tools/list hides write tools when server has not opted in' . ($listed_writes ? ' (listed: ' . implode(', ', $listed_writes) . ')' : ''));
foreach ($registry as $registry_name => $registry_tool) {
  if (ai_mcp_tool_is_write($registry_tool)) {
    // Calls are refused before dispatch, so this never executes the tool.
    $refused = ai_mcp_method_tools_call('req-call-w', ['name' => $registry_name, 'arguments' => []], $open_server);
    test_assert(isset($refused['error']) && $refused['error']['code'] === -32602, "tools/call refuses write tool $registry_name without opt-in");
    break;
  }
}

// 4.7. Unknown method dispatch
$unknown_req = [
  'jsonrpc' => '2.0',
  'id' => 'req-unknown-6',
  'method' => 'invalid/method/name',
  'params' => [],
];
$unknown_res = ai_mcp_jsonrpc_dispatch($unknown_req, $loaded);
test_assert(isset($unknown_res['error']) && $unknown_res['error']['code'] === -32601, 'Unknown method returns -32601 (Method not found)');

// ---------------------------------------------------------------------------
// 5. End-to-End Live HTTP Endpoint Tests
// ---------------------------------------------------------------------------
print "\n--- 5. Testing Live HTTP Endpoint (http://localhost/ai/mcp) ---\n";

function mcp_http_request($url, $method = 'GET', $headers = [], $data = NULL) {
  $ch = curl_init($url);
  curl_setopt($ch, CURLOPT_RETURNTRANSFER, TRUE);
  curl_setopt($ch, CURLOPT_HEADER, TRUE);
  curl_setopt($ch, CURLOPT_CUSTOMREQUEST, $method);

  if ($data !== NULL) {
    curl_setopt($ch, CURLOPT_POSTFIELDS, is_string($data) ? $data : json_encode($data));
  }

  $header_lines = [];
  foreach ($headers as $k => $v) {
    $header_lines[] = "{$k}: {$v}";
  }
  curl_setopt($ch, CURLOPT_HTTPHEADER, $header_lines);

  $response = curl_exec($ch);
  $http_code = curl_getinfo($ch, CURLINFO_HTTP_CODE);
  $header_size = curl_getinfo($ch, CURLINFO_HEADER_SIZE);
  curl_close($ch);

  $header_str = substr($response, 0, $header_size);
  $body_str = substr($response, $header_size);

  return [
    'code' => $http_code,
    'headers' => $header_str,
    'body' => $body_str,
  ];
}

$base_url = 'http://localhost/ai/mcp';

// 5.1. GET SSE Probe
$get_res = mcp_http_request($base_url, 'GET');
test_assert($get_res['code'] === 200, 'GET /ai/mcp returns HTTP 200');
test_assert(stripos($get_res['headers'], 'Content-Type: text/event-stream') !== FALSE, 'GET /ai/mcp returns text/event-stream content type');
test_assert(strpos($get_res['body'], 'data: {"endpoint":') !== FALSE, 'GET /ai/mcp returns SSE endpoint discovery payload');

// 5.2. POST without token
$no_auth_res = mcp_http_request($base_url, 'POST', ['Content-Type' => 'application/json'], json_encode($init_req));
test_assert($no_auth_res['code'] === 401, 'POST /ai/mcp without token returns HTTP 401 Unauthorized');

// 5.3. POST with wrong token
$bad_auth_res = mcp_http_request($base_url, 'POST', [
  'Content-Type' => 'application/json',
  'Authorization' => 'Bearer wrong-key',
], json_encode($init_req));
test_assert($bad_auth_res['code'] === 401, 'POST /ai/mcp with invalid token returns HTTP 401 Unauthorized');

// 5.4. POST with invalid JSON
$bad_json_res = mcp_http_request($base_url, 'POST', [
  'Content-Type' => 'application/json',
  'Authorization' => 'Bearer ' . $test_token,
], 'not-a-valid-json');
test_assert($bad_json_res['code'] === 400, 'POST /ai/mcp with malformed JSON returns HTTP 400 Bad Request');

// 5.5. POST valid initialize
$live_init_res = mcp_http_request($base_url, 'POST', [
  'Content-Type' => 'application/json',
  'Authorization' => 'Bearer ' . $test_token,
], json_encode($init_req));
test_assert($live_init_res['code'] === 200, 'POST /ai/mcp initialize returns HTTP 200');
$live_init_data = json_decode($live_init_res['body'], TRUE);
test_assert(isset($live_init_data['result']['protocolVersion']) && $live_init_data['result']['protocolVersion'] === '2024-11-05', 'Live initialize returns MCP protocol 2024-11-05');

// 5.6. POST notification
$live_notify_res = mcp_http_request($base_url, 'POST', [
  'Content-Type' => 'application/json',
  'Authorization' => 'Bearer ' . $test_token,
], json_encode($notify_req));
test_assert($live_notify_res['code'] === 202, 'POST /ai/mcp notifications/initialized returns HTTP 202 Accepted');

// 5.7. POST live tools/list
$live_list_res = mcp_http_request($base_url, 'POST', [
  'Content-Type' => 'application/json',
  'Authorization' => 'Bearer ' . $test_token,
], json_encode($list_req));
test_assert($live_list_res['code'] === 200, 'POST /ai/mcp tools/list returns HTTP 200');
$live_list_data = json_decode($live_list_res['body'], TRUE);
test_assert(count($live_list_data['result']['tools']) === 1, 'Live tools/list applies server allowlist');

// 5.8. POST live tools/call
$live_call_res = mcp_http_request($base_url, 'POST', [
  'Content-Type' => 'application/json',
  'Authorization' => 'Bearer ' . $test_token,
], json_encode([
  'jsonrpc' => '2.0',
  'id' => 'live-call-1',
  'method' => 'tools/call',
  'params' => [
    'name' => 'list_content_types',
    'arguments' => [],
  ],
]));
test_assert($live_call_res['code'] === 200, 'POST /ai/mcp tools/call returns HTTP 200');
$live_call_data = json_decode($live_call_res['body'], TRUE);
test_assert(isset($live_call_data['result']['content'][0]['text']), 'Live tools/call returns content text envelope');
test_assert(strpos($live_call_data['result']['content'][0]['text'], 'baseball') !== FALSE, 'Live tools/call returned site content types');

// ---------------------------------------------------------------------------
// 6. Cleanup
// ---------------------------------------------------------------------------
print "\n--- 6. Cleanup ---\n";
ai_mcp_server_delete($test_server_id);
test_assert(ai_mcp_server_load($test_server_id) === FALSE, 'Temporary test server cleaned up successfully');

print "\n============================================================\n";
print " Test Results: {$passed} Passed, {$failed} Failed\n";
print "============================================================\n";

if ($failed > 0) {
  exit(1);
}
exit(0);
