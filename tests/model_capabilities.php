<?php

/**
 * @file
 * Offline capability regression tests using real adapters and stubbed HTTP.
 * Run: php modules/contrib/ai/tests/model_capabilities.php
 */

define('REQUEST_TIME', time());
define('WATCHDOG_ERROR', 3);
define('WATCHDOG_WARNING', 4);
define('WATCHDOG_DEBUG', 7);
$fixture_config = [];
$fixture_cache = [];
$fixture_http = [];
$fixture_requests = [];
$fixture_logs = [];
$checks = 0;

function config_get($name, $key) {
  return $GLOBALS['fixture_config'][$name][$key] ?? NULL;
}
function config($name) {
  return new class($name) {
    private $name;
    public function __construct($name) { $this->name = $name; }
    public function get($key) { return config_get($this->name, $key); }
    public function set($key, $value) { $GLOBALS['fixture_config'][$this->name][$key] = $value; return $this; }
    public function save() {}
  };
}
function &backdrop_static($name, $default = NULL) {
  static $values = [];
  if (!array_key_exists($name, $values)) { $values[$name] = $default; }
  return $values[$name];
}
function cache_get($key, $bin = 'cache') {
  return isset($GLOBALS['fixture_cache'][$key]) ? (object) ['data' => $GLOBALS['fixture_cache'][$key]] : FALSE;
}
function cache_set($key, $data, $bin = 'cache', $expire = 0) {
  $GLOBALS['fixture_cache'][$key] = $data;
}
function cache_clear_all($key, $bin = 'cache', $wildcard = FALSE) {
  foreach (array_keys($GLOBALS['fixture_cache']) as $candidate) {
    if ($candidate === $key || ($wildcard && strpos($candidate, $key) === 0)) {
      unset($GLOBALS['fixture_cache'][$candidate]);
    }
  }
}
function backdrop_http_request($url, $options) {
  $GLOBALS['fixture_requests'][] = $url;
  foreach ($GLOBALS['fixture_http'] as $prefix => $response) {
    if (strpos($url, $prefix) === 0) {
      return (object) ['code' => $response['code'] ?? 200, 'data' => json_encode($response['body'])];
    }
  }
  throw new RuntimeException('Unexpected HTTP endpoint: ' . parse_url($url, PHP_URL_PATH));
}
function watchdog($channel, $message, $variables, $severity) { $GLOBALS['fixture_logs'][] = $message; }
function backdrop_alter($type, &$data, ...$context) {}
function t($text, $args = []) { return strtr($text, $args); }
function url($path, $options = []) { return $path; }
function backdrop_get_path($type, $name) { return 'modules/contrib/' . $name; }
function module_invoke_all($hook) { return []; }
function backdrop_set_message($message) {}
function form_set_error($name, $message) { $GLOBALS['fixture_form_errors'][] = $message; }

require_once dirname(__DIR__) . '/ai.module';
require_once dirname(__DIR__) . '/ai.capabilities.inc';
foreach (['AIProviderClient', 'AIAdapterBase', 'AICompatibleTrait', 'AIApi'] as $class) {
  require_once dirname(__DIR__) . '/includes/' . $class . '.php';
}
foreach (['openrouter' => 'AIOpenRouterAdapter', 'litellm' => 'AILiteLLMAdapter', 'anthropic' => 'AIAnthropicAdapter', 'google_gemini' => 'AIGoogleGeminiAdapter', 'openai' => 'AIOpenAIAdapter', 'groq' => 'AIGroqAdapter', 'mistral' => 'AIMistralAdapter', 'elevenlabs' => 'AIElevenLabsAdapter', 'aws_bedrock' => 'AIBedrockAdapter'] as $provider => $class) {
  require_once dirname(__DIR__, 2) . '/ai_provider_' . $provider . '/includes/' . $class . '.php';
}

function same($expected, $actual, $label) {
  $GLOBALS['checks']++;
  if ($expected !== $actual) {
    throw new RuntimeException($label . ': expected ' . json_encode($expected) . ', got ' . json_encode($actual));
  }
}
function ids($adapter, $capability) {
  $ids = array_keys($adapter->getModelsByCapability($capability));
  sort($ids);
  return $ids;
}
function catalog(array $models) {
  return ['data' => array_map(function ($id) { return ['id' => $id]; }, $models)];
}
class CapabilityFixtureApi extends AIApi {
  public function __construct($provider, $client) { $this->provider = $provider; $this->client = $client; }
}

// Manual assignments must take effect even when automatic discovery throws.
$client = new class {
  public function getModels() { return ['chat' => 'Chat', 'vector' => 'Vector']; }
  public function getModelsByCapability($capability) { throw new RuntimeException('Metadata unavailable'); }
};
$api = new CapabilityFixtureApi('fixture', $client);
foreach (ai_capability_labels() as $capability => $label) {
  $fixture_config['ai.settings']['providers']['fixture']['manual_capability_models'] = [$capability => ['vector']];
  same(['vector'], ids($api, $capability), 'Manual assignment works for ' . $capability);
  $fixture_config['ai.settings']['providers']['fixture']['manual_capability_models'] = [$capability => []];
  same([], ids($api, $capability), 'Explicit none works for ' . $capability);
}
foreach (['text' => 'getChatModels', 'vision' => 'getVisionModels', 'image' => 'getImageModels', 'embeddings' => 'getEmbeddingModels', 'moderation' => 'getModerationModels', 'stt' => 'getSpeechToTextModels', 'tool_calling' => 'getToolCallingModels', 'thinking' => 'getThinkingModels'] as $capability => $method) {
  $fixture_config['ai.settings']['providers']['fixture']['manual_capability_models'] = [$capability => ['fixture/vector']];
  same(['vector' => 'Vector'], $api->{$method}(), 'Convenience getter override ' . $method);
}
// Exercise the actual save handler and form rebuild, using in-memory config.
$fixture_form_errors = [];
$validation_state = ['input' => []];
ai_capabilities_form_validate([], $validation_state);
same(1, count($fixture_form_errors), 'Truncated matrix submission rejected');
$fixture_form_errors = [];
$validation_state['input']['capabilities_complete'] = 'complete';
ai_capabilities_form_validate([], $validation_state);
same([], $fixture_form_errors, 'Complete zero-selection submission accepted');
$form_state = ['values' => ['capability_overrides' => ['text' => 1, 'embeddings' => 1], 'manual_capability_models' => ['text' => ['chat' => 'chat']]]];
ai_capabilities_form_submit(['#provider_id' => 'fixture'], $form_state);
same(['text' => ['chat'], 'embeddings' => []], ai_get_provider_manual_capability_models('fixture'), 'Save preserves explicit empty override');
$matrix = ai_capability_matrix_element($client->getModels(), ai_get_provider_manual_capability_models('fixture'), ['manual_capability_models']);
same(1, $matrix['matrix']['override__embeddings']['#default_value'], 'Empty override remains checked after reload');
same(0, ai_get_provider_capability_overrides('fixture')['embeddings'], 'Overview reports zero override');
$matrix = ai_capability_matrix_element([], ['embeddings' => []], ['manual_capability_models']);
same(1, $matrix['matrix']['override__embeddings']['#default_value'], 'Empty catalog preserves override toggle');
same(FALSE, ai_capability_matrix_key('text', 'a/b') === ai_capability_matrix_key('text', 'a-b'), 'Punctuation in model IDs does not collide');
$form_state['values']['capability_overrides'] = [];
ai_capabilities_form_submit(['#provider_id' => 'fixture'], $form_state);
same([], ai_get_provider_manual_capability_models('fixture'), 'Unticking override restores auto');
$fixture_config['ai.settings']['providers']['fixture']['manual_capability_models'] = ['text' => ['vector']];
ai_manual_capability_bypass(TRUE);
try {
  $api->getChatModels();
  throw new RuntimeException('Auto preview unexpectedly applied the manual override');
}
catch (RuntimeException $e) {
  same('Metadata unavailable', $e->getMessage(), 'Auto preview bypasses override');
}
finally {
  ai_manual_capability_bypass(FALSE);
}

// Modalities are authoritative; audio chat is not dedicated transcription/TTS.
$cards = [];
foreach (['chat' => [['text'], ['text']], 'vision' => [['text', 'image'], ['text']], 'vector' => [['text'], ['embeddings']], 'image' => [['text'], ['image']], 'speech' => [['text'], ['speech']], 'transcript' => [['audio'], ['transcription']], 'audio-chat' => [['text', 'audio'], ['text', 'audio']]] as $id => $modalities) {
  $cards[] = ['id' => $id, 'architecture' => ['input_modalities' => $modalities[0], 'output_modalities' => $modalities[1]], 'supported_parameters' => $id === 'chat' ? ['tools', 'reasoning'] : []];
}
$cards[] = ['id' => 'unknown'];
$fixture_http['https://openrouter.ai/api/v1/models?output_modalities=all'] = ['body' => ['data' => $cards]];
$router = new AIOpenRouterAdapter('fixture');
same(['audio-chat', 'chat', 'vision'], ids($router, 'chat'), 'OpenRouter chat');
same(['vector'], ids($router, 'embeddings'), 'OpenRouter embeddings');
same(['vision'], ids($router, 'vision'), 'OpenRouter vision');
same(['image'], ids($router, 'image'), 'OpenRouter images');
same(['speech'], ids($router, 'tts'), 'OpenRouter dedicated speech');
same(['transcript'], ids($router, 'stt'), 'OpenRouter dedicated transcription');
same(['audio-chat'], ids($router, 'audio'), 'OpenRouter audio input');
same(['chat'], ids($router, 'tools'), 'OpenRouter tools');
same(['chat'], ids($router, 'thinking'), 'OpenRouter thinking');
same([], ids($router, 'moderation'), 'OpenRouter unsupported operation');
same([], ids($router, 'invented'), 'Unknown capability is not full catalog');
$fixture_config['ai_provider_openrouter.settings']['enabled_models'] = ['chat'];
same([], ids($router, 'embeddings'), 'OpenRouter enabled-model restriction preserved');
unset($fixture_config['ai_provider_openrouter.settings']);

// LiteLLM aliases carry no semantic information; use model_info.mode.
$fixture_http['http://localhost:4000/v1/models'] = ['body' => catalog(['alpha', 'beta', 'unknown', 'mixed'])];
$fixture_http['http://localhost:4000/model/info'] = ['body' => ['data' => [
  ['model_name' => 'alpha', 'model_info' => ['mode' => 'embedding']],
  ['model_name' => 'beta', 'model_info' => ['mode' => 'chat', 'supports_vision' => TRUE, 'supports_function_calling' => TRUE, 'supports_reasoning' => TRUE]],
  ['model_name' => 'mixed', 'model_info' => ['mode' => 'chat']],
  ['model_name' => 'mixed', 'model_info' => ['mode' => 'embedding']],
]]];
$lite = new AILiteLLMAdapter('fixture');
same(['alpha'], ids($lite, 'embeddings'), 'LiteLLM embedding alias');
same(['beta'], ids($lite, 'text'), 'LiteLLM excludes unknown and conflicting aliases');
same(['beta'], ids($lite, 'vision'), 'LiteLLM vision flag');
same(['beta'], ids($lite, 'tools'), 'LiteLLM tool flag');
same(['beta'], ids($lite, 'thinking'), 'LiteLLM reasoning flag');
same([], ids($lite, 'image'), 'LiteLLM unimplemented image operation');
$fixture_http['http://localhost:4000/model/info'] = ['code' => 403, 'body' => ['error' => 'fixture secret must not be logged']];
$lite_denied = new AILiteLLMAdapter('other-fixture-key');
same([], ids($lite_denied, 'text'), 'LiteLLM unavailable metadata stays unclassified');
$fixture_config['ai.settings']['providers']['litellm']['manual_capability_models'] = ['text' => ['unknown']];
same(['unknown'], ids(new CapabilityFixtureApi('litellm', $lite_denied), 'text'), 'Manual classification works without metadata permission');
same(FALSE, strpos(json_encode($fixture_logs), 'fixture secret') !== FALSE, 'Metadata error bodies not logged');

// Explicit false beats legacy family rules for Claude vision.
$fixture_http['https://api.anthropic.com/v1/models'] = ['body' => ['data' => [
  ['id' => 'claude-sonnet-4', 'capabilities' => ['image_input' => ['supported' => FALSE]]],
  ['id' => 'claude-new', 'capabilities' => ['image_input' => ['supported' => TRUE], 'thinking' => ['supported' => TRUE]]],
]]];
$claude = new AIAnthropicAdapter('fixture');
same(['claude-new'], ids($claude, 'vision'), 'Claude metadata supersedes ID assumptions');
same(['claude-new'], ids($claude, 'thinking'), 'Claude thinking metadata');
same([], ids($claude, 'embeddings'), 'Claude has no embeddings');

$fixture_http['https://generativelanguage.googleapis.com/v1beta/models'] = ['body' => ['models' => [
  ['name' => 'models/gemini-text', 'supportedGenerationMethods' => ['generateContent'], 'inputModalities' => ['TEXT'], 'outputModalities' => ['TEXT']],
  ['name' => 'models/vector', 'supportedGenerationMethods' => ['embedContent']],
  ['name' => 'models/gemini-tts', 'supportedGenerationMethods' => ['generateContent'], 'outputModalities' => ['AUDIO']],
  ['name' => 'models/gemini-vision', 'supportedGenerationMethods' => ['generateContent'], 'inputModalities' => ['IMAGE', 'TEXT'], 'outputModalities' => ['TEXT'], 'thinking' => TRUE],
]]];
$gemini = new AIGoogleGeminiAdapter('fixture');
same(['gemini-text', 'gemini-vision'], ids($gemini, 'text'), 'Gemini audio-only output excluded from chat');
same(['gemini-text', 'gemini-vision'], ids($gemini, 'tool_calling'), 'Gemini tool calling metadata');
same(['vector'], ids($gemini, 'embeddings'), 'Gemini method beats model name');
same(['gemini-vision'], ids($gemini, 'vision'), 'Gemini explicit text-only input respected');
same(['gemini-vision'], ids($gemini, 'thinking'), 'Gemini thinking metadata');

$fixture_http['https://api.openai.com/v1/models'] = ['body' => catalog(['gpt-4o', 'gpt-image-1', 'gpt-4o-mini-tts', 'gpt-4o-transcribe', 'text-embedding-3-small', 'new-family', 'whisper-1'])];
$openai = new AIOpenAIAdapter('fixture');
same(['gpt-4o'], ids($openai, 'text'), 'OpenAI image and speech models excluded from chat');
same(['gpt-4o'], ids($openai, 'vision'), 'OpenAI vision is not entire catalog');
same(['gpt-4o-mini-tts'], ids($openai, 'tts'), 'OpenAI new TTS naming');
same(['gpt-4o-transcribe', 'whisper-1'], ids($openai, 'stt'), 'OpenAI transcription naming');
same([], ids($openai, 'invented'), 'OpenAI unknown capability empty');
same(TRUE, isset($openai->getModels()['new-family']), 'New OpenAI families remain manually assignable');
$fixture_http['https://api.groq.com/openai/v1/models'] = ['body' => catalog(['llama-3.3-70b-versatile', 'whisper-large-v3', 'canopylabs/orpheus-v1-english', 'meta-llama/llama-guard-4-12b'])];
$groq = new AIGroqAdapter('fixture');
same(['llama-3.3-70b-versatile'], ids($groq, 'text'), 'Groq excludes speech and moderation models');
same(['llama-3.3-70b-versatile'], ids($groq, 'tool_calling'), 'Groq tool calling');
same(['whisper-large-v3'], ids($groq, 'stt'), 'Groq speech-to-text');

class BedrockCapabilityFixture extends AIBedrockAdapter {
  public function __construct() {}
  public function getModels(): array {
    $this->modelMetadata = [
      'arbitrary.text' => ['inputModalities' => ['TEXT'], 'outputModalities' => ['TEXT']],
      'arbitrary.vision' => ['inputModalities' => ['IMAGE', 'TEXT'], 'outputModalities' => ['TEXT']],
      'arbitrary.vector' => ['inputModalities' => ['TEXT'], 'outputModalities' => ['EMBEDDING']],
    ];
    return ['us.arbitrary.text' => 'Text', 'us.arbitrary.vision' => 'Vision', 'arbitrary.vector' => 'Vector'];
  }
}
$bedrock = new BedrockCapabilityFixture();
same(['us.arbitrary.text', 'us.arbitrary.vision'], ids($bedrock, 'text'), 'Bedrock inference profile metadata');
same(['us.arbitrary.vision'], ids($bedrock, 'vision'), 'Bedrock image input');
same(['arbitrary.vector'], ids($bedrock, 'embedding'), 'Bedrock embedding output');

$fixture_http['https://api.mistral.ai/v1/models'] = ['body' => ['data' => [
  ['id' => 'chat', 'capabilities' => ['completion_chat' => TRUE, 'function_calling' => TRUE]],
  ['id' => 'fim', 'capabilities' => ['completion_fim' => TRUE]],
]]];
$mistral = new AIMistralAdapter('fixture');
same(['chat'], ids($mistral, 'tools'), 'Mistral tool metadata');
same(['fim'], ids($mistral, 'insert'), 'Mistral FIM metadata');
same([], ids($mistral, 'vision'), 'Mistral unsupported flags empty');

$fixture_http['https://api.elevenlabs.io/v1/models'] = ['body' => [
  ['model_id' => 'voice', 'can_do_text_to_speech' => TRUE],
  ['model_id' => 'conversion', 'can_do_voice_conversion' => TRUE],
  ['model_id' => 'eleven_monolingual_v1', 'can_do_text_to_speech' => TRUE, 'can_do_voice_conversion' => TRUE],
]];
$eleven = new AIElevenLabsAdapter('fixture');
same(['voice'], ids($eleven, 'tts'), 'ElevenLabs TTS metadata');
same(['conversion'], ids($eleven, 'speech_to_speech'), 'ElevenLabs voice conversion metadata excludes retired models');
same(['scribe_v1', 'scribe_v2'], ids($eleven, 'stt'), 'ElevenLabs separate transcription catalog');
same([], ids($eleven, 'audio'), 'Transcription is not audio chat');
same([], ids($eleven, 'text'), 'ElevenLabs never appears as chat');

// Shared fallback must not advertise every catalog entry for all capabilities.
$fallback = new class('fixture') extends AIOpenAIAdapter {
  use AICompatibleTrait;
  public function getModels(): array { return ['untyped' => 'Untyped']; }
};
same([], ids($fallback, 'text'), 'Untyped compatible provider remains unclassified');
$fixture_config['ai.settings']['providers']['untyped']['manual_capability_models'] = ['text' => ['untyped']];
same(['untyped'], ids(new CapabilityFixtureApi('untyped', $fallback), 'text'), 'Untyped provider can be assigned manually');

// Model refresh invalidates native metadata as well as selector lists.
$fixture_config['ai.settings']['providers']['openrouter'] = [];
$before = count($fixture_requests);
ai_clear_models_cache();
$router->getModels();
same($before + 1, count($fixture_requests), 'Refresh invalidates OpenRouter native catalog cache');

echo $checks . " capability regression checks passed.\n";
