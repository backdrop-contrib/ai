<?php

/**
 * @file
 * Core AI API wrapper.
 */

class AIApi {

  protected $client;
  protected $provider;
  protected $callerModule;

  public function __construct($api_key, $provider = NULL) {
    $this->provider = $provider ?: (function_exists('ai_default_provider_id') ? ai_default_provider_id() : '');
    $this->client = $this->initializeProviderClient($api_key, $this->provider);
  }

  public function setCallerModule($module) {
    $this->callerModule = $module;
    return $this;
  }

  /**
   * Determine which module should own log entries.
   */
  protected function detectCallerModule() {
    if (!empty($this->callerModule)) {
      return $this->callerModule;
    }

    $backtrace = debug_backtrace(DEBUG_BACKTRACE_IGNORE_ARGS, 10);
    $module = 'ai';
    $skip_functions = [
      'ai_get_api',
      'ai_get_api_for_model',
      'ai_chat',
      'ai_moderation',
    ];

    foreach ($backtrace as $step) {
      if (!empty($step['file']) && preg_match('@/modules/(?:contrib|custom)/([a-z0-9_]+)/@', $step['file'], $matches)) {
        if ($matches[1] !== 'ai') {
          $module = $matches[1];
          break;
        }
      }

      if (isset($step['class']) && preg_match('/^(ai_[a-z0-9_]+)/', $step['class'], $matches)) {
        $module = $matches[1];
        break;
      }

      if (isset($step['function']) && str_starts_with($step['function'], 'ai_')) {
        if (in_array($step['function'], $skip_functions, TRUE)) {
          continue;
        }
        $candidate = function_exists('ai_caller_module_from_function') ? ai_caller_module_from_function($step['function']) : '';
        if ($candidate !== '' && $candidate !== 'ai') {
          $module = $candidate;
          break;
        }
      }
    }

    return $module;
  }

  /**
   * Persist one AI log row when logging is enabled.
   */
  protected function log($operation, $model, $request_data, $response_data, $status, $duration, $error_message = NULL, $force_suppress = FALSE) {
    if ($force_suppress || !config_get('ai.settings', 'ai_log_enabled')) {
      return;
    }

    try {
      global $user;

      $module = $this->detectCallerModule();
      $redact = module_exists('ai_privacy') && function_exists('ai_privacy_log_redact') && ai_privacy_log_redact();

      $row = [
        'timestamp' => REQUEST_TIME,
        'uid' => !empty($user->uid) ? $user->uid : 0,
        'module' => $module,
        'operation' => $operation,
        'model' => $model,
        'provider' => $this->provider,
        'request_data' => $redact ? '[redacted]' : (is_string($request_data) ? $request_data : json_encode($request_data)),
        'response_data' => $redact ? '[redacted]' : (is_string($response_data) ? $response_data : json_encode($response_data)),
        'status' => $status ? 1 : 0,
        'duration' => (float) $duration,
        'error_message' => $error_message,
      ];

      db_insert('ai_log')
        ->fields($row)
        ->execute();
    }
    catch (\Exception $e) {
      watchdog('ai', 'Failed to log AI request: @error', ['@error' => $e->getMessage()], WATCHDOG_DEBUG);
    }
  }

  public function checkRateLimit($operation) {
    if (!module_exists('ai_rate_limit') || !function_exists('ai_rate_limit_check')) {
      return;
    }

    if (!ai_rate_limit_check($this->provider, $operation)) {
      throw new AIRateLimitException(
        'Rate limit exceeded for provider ' . $this->provider . ' and operation ' . $operation . '.',
        0,
        NULL,
        $this->provider,
        $operation
      );
    }
  }

  /**
   * Build shared lifecycle context for an AI operation.
   */
  protected function buildContext(string $operation, string $model, array $context_extra = []): array {
    $request_id = !empty($context_extra['request_id']) ? (string) $context_extra['request_id'] : uniqid('ai_', TRUE);
    $context = $context_extra;
    $context += [
      'operation' => $operation,
      'model' => $model,
      'provider' => $this->provider,
      'request_id' => $request_id,
      'request_started_at' => microtime(TRUE),
    ];
    $context['skip_ai_message_alter'] = TRUE;

    return $context;
  }

  /**
   * Run pre-request message alters once from the core wrapper.
   */
  protected function applyChatMessageAlter(array &$messages, array &$context): void {
    if (function_exists('backdrop_alter')) {
      backdrop_alter('ai_chat_messages', $messages, $context);
    }
  }

  /**
   * Run post-response alters for non-streaming chat responses.
   */
  protected function applyChatResponseAlter(&$response, array &$context): void {
    if (function_exists('backdrop_alter')) {
      backdrop_alter('ai_chat_response', $response, $context);
    }
  }

  /**
   * Finalize lifecycle metadata after a provider call.
   */
  protected function finalizeContext(array &$context): void {
    $started = !empty($context['request_started_at']) ? (float) $context['request_started_at'] : microtime(TRUE);
    $context['request_finished_at'] = microtime(TRUE);
    $context['duration_ms'] = (int) round(($context['request_finished_at'] - $started) * 1000);
  }

  /**
   * Normalize runtime exceptions into shared AI exceptions.
   */
  protected function normalizeException(\Throwable $exception, string $operation, array $context = []): AIException {
    if ($exception instanceof AIException) {
      return $exception;
    }

    $message = $exception->getMessage();
    $message_lc = strtolower($message);

    if (strpos($message_lc, 'not supported') !== FALSE || strpos($message_lc, 'unsupported') !== FALSE) {
      return new AIUnsupportedCapabilityException($message, 0, $exception, $this->provider, $operation, $context);
    }

    if (strpos($message_lc, 'rate limit') !== FALSE) {
      return new AIRateLimitException($message, 0, $exception, $this->provider, $operation, $context);
    }

    return new AIProviderResponseException($message, 0, $exception, $this->provider, $operation, $context);
  }

  protected function initializeProviderClient($api_key, $provider) {
    $providers = function_exists('ai_get_providers') ? ai_get_providers(TRUE) : module_invoke_all('ai_provider_info');
    if (empty($providers[$provider]['class'])) {
      throw new AIConfigurationException('Unknown provider: ' . $provider, 0, NULL, $provider, 'bootstrap');
    }

    $class = $providers[$provider]['class'];
    $provider_module = 'ai_provider_' . $provider;
    if (!class_exists($class) && module_exists($provider_module)) {
      @module_load_include('php', $provider_module, 'includes/' . $class);
    }

    if (!class_exists($class)) {
      throw new AIConfigurationException('Provider class not found for ' . $provider . ': ' . $class, 0, NULL, $provider, 'bootstrap');
    }

    $ref = new \ReflectionClass($class);
    $ctor = $ref->getConstructor();
    if ($ctor && $ctor->getNumberOfParameters() >= 2) {
      return $ref->newInstance($api_key, $this);
    }
    if ($ctor && $ctor->getNumberOfParameters() === 1) {
      return $ref->newInstance($api_key);
    }
    return $ref->newInstance();
  }

  public function getModels(): array {
    if (!method_exists($this->client, 'getModels')) {
      return [];
    }
    return $this->client->getModels();
  }

  public function getModelsByCapability($capability): array {
    if (method_exists($this->client, 'getModelsByCapability')) {
      return $this->client->getModelsByCapability($capability);
    }
    return $this->getModels();
  }

  public function getChatModels(): array {
    return method_exists($this->client, 'getChatModels') ? $this->client->getChatModels() : $this->getModelsByCapability('text');
  }

  public function getImageModels(): array {
    return method_exists($this->client, 'getImageModels') ? $this->client->getImageModels() : $this->getModelsByCapability('image');
  }

  public function getVisionModels(): array {
    return method_exists($this->client, 'getVisionModels') ? $this->client->getVisionModels() : $this->getModelsByCapability('vision');
  }

  public function getEmbeddingModels(): array {
    return method_exists($this->client, 'getEmbeddingModels') ? $this->client->getEmbeddingModels() : $this->getModelsByCapability('embeddings');
  }

  public function getModerationModels(): array {
    return method_exists($this->client, 'getModerationModels') ? $this->client->getModerationModels() : $this->getModelsByCapability('moderation');
  }

  public function getSpeechToTextModels(): array {
    return method_exists($this->client, 'getSpeechToTextModels') ? $this->client->getSpeechToTextModels() : $this->getModelsByCapability('stt');
  }

  public function completions(string $model, string $prompt, $temperature, $max_tokens = 512, bool $stream_response = FALSE, array $context_extra = []) {
    $this->checkRateLimit('completions');
    $start_time = microtime(TRUE);
    $request = [
      'model' => $model,
      'prompt' => $prompt,
      'temperature' => $temperature,
      'max_tokens' => $max_tokens,
      'stream' => $stream_response,
    ];

    try {
      $response = $this->client->completions($model, $prompt, $temperature, $max_tokens, $stream_response);
      $this->log('completions', $model, $request, $stream_response ? '[stream]' : $response, TRUE, microtime(TRUE) - $start_time, NULL, $stream_response);
      return $response;
    }
    catch (\Throwable $e) {
      $this->log('completions', $model, $request, NULL, FALSE, microtime(TRUE) - $start_time, $e->getMessage());
      throw $e;
    }
  }

  public function chat(string $model, array $messages, $temperature, $max_tokens = 1024, bool $stream_response = FALSE, array $context_extra = []) {
    $this->checkRateLimit('chat');
    $start_time = microtime(TRUE);
    $log_messages = (isset($context_extra['log_messages']) && is_array($context_extra['log_messages'])) ? $context_extra['log_messages'] : $messages;
    unset($context_extra['log_messages']);
    $context = $this->buildContext('chat', $model, $context_extra);
    $this->applyChatMessageAlter($messages, $context);
    if (!empty($context['guardrail_blocked'])) {
      // Blocked requests are the ones an auditor most needs to see.
      $this->log('chat', $model, $log_messages, (string) ($context['guardrail_message'] ?? ''), FALSE, microtime(TRUE) - $start_time, 'Blocked by guardrails before the provider call.');
      return (string) ($context['guardrail_message'] ?? '');
    }

    try {
      $response = $this->client->chat($model, $messages, $temperature, $max_tokens, $stream_response, $context);
    }
    catch (\Throwable $e) {
      $this->log('chat', $model, $log_messages, NULL, FALSE, microtime(TRUE) - $start_time, $e->getMessage());
      throw $this->normalizeException($e, 'chat', $context);
    }
    $this->finalizeContext($context);

    if (!$stream_response) {
      $this->applyChatResponseAlter($response, $context);
      if (!empty($context['guardrail_blocked'])) {
        $this->log('chat', $model, $log_messages, $response, FALSE, microtime(TRUE) - $start_time, 'Response blocked by guardrails.');
        return (string) ($context['guardrail_message'] ?? '');
      }
    }

    $this->log('chat', $model, $log_messages, $stream_response ? '[stream]' : $response, TRUE, microtime(TRUE) - $start_time, NULL, $stream_response);

    return $response;
  }

  public function images(string $model, string $prompt, string $size = '1024x1024', string $response_format = 'url', string $quality = 'standard', string $style = 'natural', ?string $output_format = NULL) {
    $this->checkRateLimit('images');
    try {
      return $this->client->images($model, $prompt, $size, $response_format, $quality, $style, $output_format);
    }
    catch (\Throwable $e) {
      throw $this->normalizeException($e, 'images', ['model' => $model]);
    }
  }

  public function textToSpeech(string $model, string $input, string $voice, string $response_format) {
    $this->checkRateLimit('text_to_speech');
    try {
      return $this->client->textToSpeech($model, $input, $voice, $response_format);
    }
    catch (\Throwable $e) {
      throw $this->normalizeException($e, 'text_to_speech', ['model' => $model]);
    }
  }

  public function speechToText(string $model, string $file, string $task = 'transcribe', $temperature = 0.4, string $response_format = 'verbose_json') {
    $this->checkRateLimit('speech_to_text');
    try {
      return $this->client->speechToText($model, $file, $task, $temperature, $response_format);
    }
    catch (\Throwable $e) {
      throw $this->normalizeException($e, 'speech_to_text', ['model' => $model, 'task' => $task]);
    }
  }

  public function moderation(string $input, string $model = 'omni-moderation-latest'): array {
    $this->checkRateLimit('moderation');
    try {
      return $this->client->moderation($input, $model);
    }
    catch (\Throwable $e) {
      throw $this->normalizeException($e, 'moderation', ['model' => $model]);
    }
  }

  public function embedding(string $input, string $model, bool $log = TRUE): array {
    $this->checkRateLimit('embedding');
    try {
      return $this->client->embedding($input, $model, $log);
    }
    catch (\Throwable $e) {
      throw $this->normalizeException($e, 'embedding', ['model' => $model]);
    }
  }

  public function chatWithTools(string $model, array $messages, array $tools, $temperature, $max_tokens = 1024, string $tool_choice = 'auto', array $context_extra = []): array {
    $this->checkRateLimit('chat');
    $start_time = microtime(TRUE);
    $context = $this->buildContext('chat', $model, $context_extra);
    $context['tool_count'] = count($tools);
    $this->applyChatMessageAlter($messages, $context);
    if (!empty($context['guardrail_blocked'])) {
      // Blocked requests are the ones an auditor most needs to see.
      $this->log('chat', $model, $messages, (string) ($context['guardrail_message'] ?? ''), FALSE, microtime(TRUE) - $start_time, 'Blocked by guardrails before the provider call.');
      return [
        'finish_reason' => 'guardrail_blocked',
        'content' => (string) ($context['guardrail_message'] ?? ''),
        'tool_calls' => [],
        'raw' => [],
      ];
    }

    try {
      $response = $this->client->chatWithTools($model, $messages, $tools, $temperature, $max_tokens, $tool_choice, $context);
    }
    catch (\Throwable $e) {
      $this->log('chat', $model, [
        'messages' => $messages,
        'tools' => $tools,
        'temperature' => $temperature,
        'max_tokens' => $max_tokens,
        'tool_choice' => $tool_choice,
        'context' => $context,
      ], NULL, FALSE, microtime(TRUE) - $start_time, $e->getMessage());
      throw $this->normalizeException($e, 'chat', $context);
    }
    $this->finalizeContext($context);

    if (isset($response['content']) && is_string($response['content'])) {
      $content = $response['content'];
      $this->applyChatResponseAlter($content, $context);
      $response['content'] = $content;
      if (!empty($context['guardrail_blocked'])) {
        $response['finish_reason'] = 'guardrail_blocked';
        $response['content'] = (string) ($context['guardrail_message'] ?? '');
        $response['tool_calls'] = [];
      }
    }

    $this->log('chat', $model, [
      'messages' => $messages,
      'tools' => $tools,
      'temperature' => $temperature,
      'max_tokens' => $max_tokens,
      'tool_choice' => $tool_choice,
      'context' => $context,
    ], $response, TRUE, microtime(TRUE) - $start_time);

    return $response;
  }

  public function describeImage(string $imageUrl, bool $sendImageData = TRUE): string {
    $config = config('ai_alt.settings');
    $describePrompt = $config->get('prompt');
    $model = $config->get('model');

    if (empty($describePrompt) || empty($model)) {
      watchdog('ai_alt', 'AI alt text prompt or model configuration missing.', [], WATCHDOG_ERROR);
      return '';
    }

    if ($sendImageData) {
      $data_uri = $this->buildImageDataUri($imageUrl);
      if ($data_uri) {
        $imageUrl = $data_uri;
      }
      else {
        watchdog('ai_alt', 'Failed to read image bytes for AI alt generation: @image', ['@image' => $imageUrl], WATCHDOG_WARNING);
      }
    }

    $messages = [
      [
        'role' => 'user',
        'content' => [
          ['type' => 'text', 'text' => $describePrompt],
          ['type' => 'image_url', 'image_url' => ['url' => $imageUrl, 'detail' => 'low']],
        ],
      ],
    ];
    $log_messages = $this->sanitizeMessagesForLog($messages);

    // Route through the chat() wrapper so rate limits, guardrails, logging,
    // and exception normalization apply to vision calls like any other chat.
    // A model configured for a different provider resolves through the
    // global wrapper instead of sending a prefixed id to this client.
    list($prefix, $bare) = ai_parse_model_id($model);
    if (!empty($prefix) && $prefix !== $this->provider && function_exists('ai_chat')) {
      $result = ai_chat($model, $messages, 0.4, 300, FALSE, ['operation' => 'describe_image', 'log_messages' => $log_messages]);
    }
    else {
      $result = $this->chat($bare, $messages, 0.4, 300, FALSE, ['operation' => 'describe_image', 'log_messages' => $log_messages]);
    }
    return trim((string) $result);
  }

  /**
   * Redact data URI image payloads before persisting request logs.
   */
  protected function sanitizeMessagesForLog(array $messages): array {
    foreach ($messages as $m_index => $message) {
      if (empty($message['content']) || !is_array($message['content'])) {
        continue;
      }
      foreach ($message['content'] as $c_index => $item) {
        if (empty($item['type']) || $item['type'] !== 'image_url') {
          continue;
        }
        if (empty($item['image_url']['url']) || !is_string($item['image_url']['url'])) {
          continue;
        }
        $url = $item['image_url']['url'];
        if (strpos($url, 'data:') !== 0) {
          continue;
        }

        if (preg_match('/^data:([^;]+);base64,(.*)$/s', $url, $matches)) {
          $mime = $matches[1];
          $bytes = (int) floor(strlen(preg_replace('/\s+/', '', $matches[2])) * 3 / 4);
          $messages[$m_index]['content'][$c_index]['image_url']['url'] = 'data:' . $mime . ';base64,[redacted ' . $bytes . ' bytes]';
        }
        else {
          $messages[$m_index]['content'][$c_index]['image_url']['url'] = '[redacted data-uri]';
        }
      }
    }

    return $messages;
  }

  /**
   * Build data URI from local file, stream wrapper URI, or readable URL.
   */
  protected function buildImageDataUri(string $imageUrl) {
    if (strpos($imageUrl, 'data:') === 0) {
      return $imageUrl;
    }

    $image_data = FALSE;
    $mime_type = NULL;

    $candidate_paths = [];

    if (file_stream_wrapper_valid_scheme(file_uri_scheme($imageUrl))) {
      $candidate_paths[] = backdrop_realpath($imageUrl);
    }
    elseif (is_file($imageUrl)) {
      $candidate_paths[] = $imageUrl;
    }
    else {
      $path = parse_url($imageUrl, PHP_URL_PATH);
      if (!empty($path)) {
        $candidate_paths[] = BACKDROP_ROOT . '/' . ltrim($path, '/');
      }
    }

    foreach ($candidate_paths as $candidate_path) {
      if (!empty($candidate_path) && is_file($candidate_path) && is_readable($candidate_path)) {
        $image_data = file_get_contents($candidate_path);
        if ($image_data !== FALSE) {
          $mime_type = file_get_mimetype($candidate_path);
          break;
        }
      }
    }

    if ($image_data === FALSE) {
      $image_data = @file_get_contents($imageUrl);
    }

    if ($image_data === FALSE || $image_data === '') {
      return FALSE;
    }

    if (empty($mime_type)) {
      $finfo = finfo_open(FILEINFO_MIME_TYPE);
      if ($finfo) {
        $mime_type = finfo_buffer($finfo, $image_data) ?: NULL;
        finfo_close($finfo);
      }
    }

    if (empty($mime_type)) {
      $mime_type = 'image/jpeg';
    }

    return 'data:' . $mime_type . ';base64,' . base64_encode($image_data);
  }
}
