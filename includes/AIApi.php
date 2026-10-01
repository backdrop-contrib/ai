<?php

/**
 * @file
 * Core AI API wrapper.
 */

class AIApi {

  protected $client;
  protected $provider;
  protected $callerModule;
  protected $providerUsage = [];

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

  public function checkRateLimit($operation, $model = '') {
    if (module_exists('ai_rate_limit') && function_exists('ai_rate_limit_check')) {
      if (!ai_rate_limit_check($this->provider, $operation)) {
        $violation = function_exists('ai_rate_limit_get_last_violation') ? ai_rate_limit_get_last_violation() : [];
        $message = 'Rate limit exceeded for provider ' . $this->provider . ' and operation ' . $operation . '.';
        if (!empty($violation['retry_after'])) {
          $message .= ' Try again in ' . $violation['retry_after'] . 's.';
        }
        throw new AIRateLimitException(
          $message,
          0,
          NULL,
          $this->provider,
          $operation,
          $violation
        );
      }
    }

    // Usage budgets are intentionally a separate opt-in policy from request
    // frequency limits. The optional submodule returns a status object so the
    // core API can expose a useful typed exception without depending on the
    // submodule's storage implementation.
    if (function_exists('ai_usage_budget_check')) {
      $budget = ai_usage_budget_check($this->provider, $operation, $model);
      if (empty($budget['allowed'])) {
        throw new AIUsageLimitException(
          $budget['message'] ?? 'AI usage budget reached.',
          0,
          NULL,
          $this->provider,
          $operation,
          $budget
        );
      }
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
   * Capture a provider usage envelope without coupling the parent API to the
   * optional ai_usage submodule.
   */
  public function setProviderUsage(array $usage): void {
    $input = $this->firstUsageNumber($usage, ['input_tokens', 'prompt_tokens', 'inputTokenCount', 'promptTokenCount']);
    $output = $this->firstUsageNumber($usage, ['output_tokens', 'completion_tokens', 'candidatesTokenCount']);
    $cached = $this->firstUsageNumber($usage, ['cached_tokens', 'cache_read_input_tokens', 'cachedContentTokenCount']);
    if ($cached <= 0) {
      $cached = $this->firstUsageNumber($usage['prompt_tokens_details'] ?? [], ['cached_tokens'])
        ?: $this->firstUsageNumber($usage['input_tokens_details'] ?? [], ['cached_tokens']);
    }
    $reasoning = $this->firstUsageNumber($usage, ['reasoning_tokens', 'thoughtsTokenCount']);
    if ($reasoning <= 0) {
      $reasoning = $this->firstUsageNumber($usage['completion_tokens_details'] ?? [], ['reasoning_tokens'])
        ?: $this->firstUsageNumber($usage['output_tokens_details'] ?? [], ['reasoning_tokens']);
    }
    $total = $this->firstUsageNumber($usage, ['total_tokens', 'totalTokenCount']);
    if ($total <= 0) {
      $total = $input + $output;
    }

    $this->providerUsage = [
      'input_tokens' => $input,
      'output_tokens' => $output,
      'cached_tokens' => $cached,
      'reasoning_tokens' => $reasoning,
      'total_tokens' => $total,
    ];
  }

  /**
   * Return usage metadata captured by the current provider adapter call.
   */
  public function getProviderUsage(): array {
    return $this->providerUsage;
  }

  /**
   * Start a provider operation and clear usage left by the previous call.
   */
  protected function beginUsageOperation(string $operation, string $model, array $context_extra = []): array {
    $this->providerUsage = [];
    return $this->buildContext($operation, $model, $context_extra);
  }

  /**
   * Record a completed operation through the optional usage submodule.
   */
  protected function recordUsageOperation(string $operation, string $model, array &$context, $request, $response, bool $status, $error_message = NULL): void {
    if (!function_exists('ai_usage_record_operation')) {
      return;
    }

    $this->finalizeContext($context);
    $record_operation = !empty($context['operation']) ? $context['operation'] : $operation;
    $record = [
      'uid' => !empty($GLOBALS['user']->uid) ? (int) $GLOBALS['user']->uid : 0,
      'module' => $this->detectCallerModule(),
      'operation' => $record_operation,
      'provider' => $this->provider,
      'model' => $model,
      'request_id' => $context['request_id'] ?? '',
      'context_group' => $context['usage_group'] ?? '',
      'status' => $status,
      'duration_ms' => $context['duration_ms'] ?? 0,
      'usage' => $this->providerUsage,
      'request' => $request,
      'response' => $response,
      'error_message' => $error_message,
    ];
    ai_usage_record_operation($record);
  }

  /**
   * Read the first non-negative numeric value from a provider usage array.
   */
  protected function firstUsageNumber(array $usage, array $keys): int {
    foreach ($keys as $key) {
      if (isset($usage[$key]) && is_numeric($usage[$key])) {
        return max(0, (int) $usage[$key]);
      }
    }
    return 0;
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
   * Give other modules a chance to supply a fallback response for a failed
   * provider call, instead of the exception always propagating to the
   * caller (mirrors Drupal AI's AiExceptionEvent-driven failover).
   *
   * A subscriber (e.g. a secondary-provider module) can implement
   * hook_ai_provider_failure_alter(&$context, $exception, $operation) and
   * set $context['failover_response'] (a string, for chat()) or
   * $context['failover_result'] (an array shaped like chatWithTools()'s
   * normal return: 'finish_reason', 'content', 'tool_calls', 'raw') to have
   * that value used instead of the exception being thrown. Leaving both
   * unset preserves today's behavior of the original exception propagating.
   */
  protected function applyProviderFailureAlter(\Throwable $exception, string $operation, array &$context): void {
    if (function_exists('backdrop_alter')) {
      backdrop_alter('ai_provider_failure', $context, $exception, $operation);
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

  /**
   * Apply the site's manual capability overrides for this provider.
   *
   * Provider APIs disagree about how (or whether) they advertise capabilities,
   * so the override configured at admin/config/ai/settings/capabilities is enforced here
   * for every provider rather than in each adapter.
   */
  protected function applyCapabilityOverride(array $models, $capability): array {
    if (!function_exists('ai_filter_models_by_manual_capability') || empty($this->provider)) {
      return $models;
    }
    if (ai_manual_capability_bypass()) {
      return $models;
    }
    $manual = ai_get_provider_manual_capability_models($this->provider);
    $canonical = ai_normalize_capability_name($capability);
    if (!array_key_exists($canonical, $manual)) {
      return $models;
    }
    // An explicit empty list disables the capability without fetching models.
    if (!$manual[$canonical]) {
      return [];
    }
    return ai_filter_models_by_manual_capability($this->getModels(), $this->provider, $canonical);
  }

  public function getModelsByCapability($capability): array {
    $capability = ai_normalize_capability_name($capability);
    if (!ai_manual_capability_bypass() && array_key_exists($capability, ai_get_provider_manual_capability_models($this->provider))) {
      return $this->applyCapabilityOverride([], $capability);
    }
    $models = method_exists($this->client, 'getModelsByCapability')
      ? $this->client->getModelsByCapability($capability)
      : [];
    return $this->applyCapabilityOverride($models, $capability);
  }

  public function getChatModels(): array {
    if (!ai_manual_capability_bypass() && array_key_exists('text', ai_get_provider_manual_capability_models($this->provider))) {
      return $this->applyCapabilityOverride([], 'text');
    }
    if (!method_exists($this->client, 'getChatModels')) {
      return $this->getModelsByCapability('text');
    }
    return $this->applyCapabilityOverride($this->client->getChatModels(), 'text');
  }

  public function getImageModels(): array {
    if (!ai_manual_capability_bypass() && array_key_exists('image', ai_get_provider_manual_capability_models($this->provider))) {
      return $this->applyCapabilityOverride([], 'image');
    }
    if (!method_exists($this->client, 'getImageModels')) {
      return $this->getModelsByCapability('image');
    }
    return $this->applyCapabilityOverride($this->client->getImageModels(), 'image');
  }

  public function getVisionModels(): array {
    if (!ai_manual_capability_bypass() && array_key_exists('vision', ai_get_provider_manual_capability_models($this->provider))) {
      return $this->applyCapabilityOverride([], 'vision');
    }
    if (!method_exists($this->client, 'getVisionModels')) {
      return $this->getModelsByCapability('vision');
    }
    return $this->applyCapabilityOverride($this->client->getVisionModels(), 'vision');
  }

  public function getEmbeddingModels(): array {
    if (!ai_manual_capability_bypass() && array_key_exists('embeddings', ai_get_provider_manual_capability_models($this->provider))) {
      return $this->applyCapabilityOverride([], 'embeddings');
    }
    if (!method_exists($this->client, 'getEmbeddingModels')) {
      return $this->getModelsByCapability('embeddings');
    }
    return $this->applyCapabilityOverride($this->client->getEmbeddingModels(), 'embeddings');
  }

  public function getModerationModels(): array {
    if (!ai_manual_capability_bypass() && array_key_exists('moderation', ai_get_provider_manual_capability_models($this->provider))) {
      return $this->applyCapabilityOverride([], 'moderation');
    }
    if (!method_exists($this->client, 'getModerationModels')) {
      return $this->getModelsByCapability('moderation');
    }
    return $this->applyCapabilityOverride($this->client->getModerationModels(), 'moderation');
  }

  public function getSpeechToTextModels(): array {
    if (!ai_manual_capability_bypass() && array_key_exists('stt', ai_get_provider_manual_capability_models($this->provider))) {
      return $this->applyCapabilityOverride([], 'stt');
    }
    if (!method_exists($this->client, 'getSpeechToTextModels')) {
      return $this->getModelsByCapability('stt');
    }
    return $this->applyCapabilityOverride($this->client->getSpeechToTextModels(), 'stt');
  }

  public function getToolCallingModels(): array {
    if (!ai_manual_capability_bypass() && array_key_exists('tool_calling', ai_get_provider_manual_capability_models($this->provider))) {
      return $this->applyCapabilityOverride([], 'tool_calling');
    }
    if (!method_exists($this->client, 'getToolCallingModels')) {
      return $this->getModelsByCapability('tool_calling');
    }
    return $this->applyCapabilityOverride($this->client->getToolCallingModels(), 'tool_calling');
  }

  public function getThinkingModels(): array {
    if (!ai_manual_capability_bypass() && array_key_exists('thinking', ai_get_provider_manual_capability_models($this->provider))) {
      return $this->applyCapabilityOverride([], 'thinking');
    }
    if (!method_exists($this->client, 'getThinkingModels')) {
      return $this->getModelsByCapability('thinking');
    }
    return $this->applyCapabilityOverride($this->client->getThinkingModels(), 'thinking');
  }

  public function getDecisionModels(): array {
    if (!ai_manual_capability_bypass() && array_key_exists('decision', ai_get_provider_manual_capability_models($this->provider))) {
      return $this->applyCapabilityOverride([], 'decision');
    }
    if (!method_exists($this->client, 'getDecisionModels')) {
      return $this->getModelsByCapability('decision');
    }
    return $this->applyCapabilityOverride($this->client->getDecisionModels(), 'decision');
  }

  public function completions(string $model, string $prompt, $temperature, $max_tokens = 512, bool $stream_response = FALSE, array $context_extra = []) {
    $this->checkRateLimit('completions', $model);
    $start_time = microtime(TRUE);
    $context = $this->beginUsageOperation('completions', $model, $context_extra);
    $request = [
      'model' => $model,
      'prompt' => $prompt,
      'temperature' => $temperature,
      'max_tokens' => $max_tokens,
      'stream' => $stream_response,
    ];

    try {
      $response = $this->client->completions($model, $prompt, $temperature, $max_tokens, $stream_response);
      $this->recordUsageOperation('completions', $model, $context, $request, $stream_response ? NULL : $response, TRUE);
      $this->log('completions', $model, $request, $stream_response ? '[stream]' : $response, TRUE, microtime(TRUE) - $start_time, NULL, $stream_response);
      return $response;
    }
    catch (\Throwable $e) {
      $this->recordUsageOperation('completions', $model, $context, $request, NULL, FALSE, $e->getMessage());
      $this->log('completions', $model, $request, NULL, FALSE, microtime(TRUE) - $start_time, $e->getMessage());
      throw $e;
    }
  }

  public function chat(string $model, array $messages, $temperature, $max_tokens = 1024, bool $stream_response = FALSE, array $context_extra = []) {
    $this->checkRateLimit('chat', $model);
    $start_time = microtime(TRUE);
    $log_messages = (isset($context_extra['log_messages']) && is_array($context_extra['log_messages'])) ? $context_extra['log_messages'] : $messages;
    unset($context_extra['log_messages']);
    $context = $this->beginUsageOperation('chat', $model, $context_extra);
    $this->applyChatMessageAlter($messages, $context);
    if (!empty($context['guardrail_blocked'])) {
      // Blocked requests are the ones an auditor most needs to see.
      $this->recordUsageOperation('chat', $model, $context, $log_messages, (string) ($context['guardrail_message'] ?? ''), FALSE, 'Blocked by guardrails before the provider call.');
      $this->log('chat', $model, $log_messages, (string) ($context['guardrail_message'] ?? ''), FALSE, microtime(TRUE) - $start_time, 'Blocked by guardrails before the provider call.');
      return (string) ($context['guardrail_message'] ?? '');
    }

    try {
      $response = $this->client->chat($model, $messages, $temperature, $max_tokens, $stream_response, $context);
    }
    catch (\Throwable $e) {
      $this->applyProviderFailureAlter($e, 'chat', $context);
      if (array_key_exists('failover_response', $context)) {
        $this->recordUsageOperation('chat', $model, $context, $log_messages, (string) $context['failover_response'], FALSE, 'Provider call failed; served a failover response instead: ' . $e->getMessage());
        $this->log('chat', $model, $log_messages, (string) $context['failover_response'], TRUE, microtime(TRUE) - $start_time, 'Provider call failed; served a failover response instead: ' . $e->getMessage());
        return (string) $context['failover_response'];
      }
      $this->recordUsageOperation('chat', $model, $context, $log_messages, NULL, FALSE, $e->getMessage());
      $this->log('chat', $model, $log_messages, NULL, FALSE, microtime(TRUE) - $start_time, $e->getMessage());
      throw $this->normalizeException($e, 'chat', $context);
    }
    $this->finalizeContext($context);

    if (!$stream_response) {
      $this->applyChatResponseAlter($response, $context);
      if (!empty($context['guardrail_blocked'])) {
        $this->recordUsageOperation('chat', $model, $context, $log_messages, $response, FALSE, 'Response blocked by guardrails.');
        $this->log('chat', $model, $log_messages, $response, FALSE, microtime(TRUE) - $start_time, 'Response blocked by guardrails.');
        return (string) ($context['guardrail_message'] ?? '');
      }
    }

    $this->recordUsageOperation('chat', $model, $context, $log_messages, $stream_response ? NULL : $response, TRUE);
    $this->log('chat', $model, $log_messages, $stream_response ? '[stream]' : $response, TRUE, microtime(TRUE) - $start_time, NULL, $stream_response);

    return $response;
  }

  public function images(string $model, string $prompt, string $size = '1024x1024', string $response_format = 'url', string $quality = 'standard', string $style = 'natural', ?string $output_format = NULL) {
    $this->checkRateLimit('images', $model);
    $context = $this->beginUsageOperation('images', $model);
    try {
      $response = $this->client->images($model, $prompt, $size, $response_format, $quality, $style, $output_format);
      $this->recordUsageOperation('images', $model, $context, ['prompt' => $prompt, 'size' => $size], NULL, TRUE);
      return $response;
    }
    catch (\Throwable $e) {
      $this->recordUsageOperation('images', $model, $context, ['prompt' => $prompt, 'size' => $size], NULL, FALSE, $e->getMessage());
      throw $this->normalizeException($e, 'images', ['model' => $model]);
    }
  }

  public function textToSpeech(string $model, string $input, string $voice, string $response_format) {
    $this->checkRateLimit('text_to_speech', $model);
    $context = $this->beginUsageOperation('text_to_speech', $model);
    try {
      $response = $this->client->textToSpeech($model, $input, $voice, $response_format);
      $this->recordUsageOperation('text_to_speech', $model, $context, ['input' => $input], NULL, TRUE);
      return $response;
    }
    catch (\Throwable $e) {
      $this->recordUsageOperation('text_to_speech', $model, $context, ['input' => $input], NULL, FALSE, $e->getMessage());
      throw $this->normalizeException($e, 'text_to_speech', ['model' => $model]);
    }
  }

  public function speechToText(string $model, string $file, string $task = 'transcribe', $temperature = 0.4, string $response_format = 'verbose_json') {
    $this->checkRateLimit('speech_to_text', $model);
    $context = $this->beginUsageOperation('speech_to_text', $model);
    try {
      $response = $this->client->speechToText($model, $file, $task, $temperature, $response_format);
      $this->recordUsageOperation('speech_to_text', $model, $context, ['task' => $task], $response, TRUE);
      return $response;
    }
    catch (\Throwable $e) {
      $this->recordUsageOperation('speech_to_text', $model, $context, ['task' => $task], NULL, FALSE, $e->getMessage());
      throw $this->normalizeException($e, 'speech_to_text', ['model' => $model, 'task' => $task]);
    }
  }

  public function moderation(string $input, string $model = 'omni-moderation-latest'): array {
    $this->checkRateLimit('moderation', $model);
    $context = $this->beginUsageOperation('moderation', $model);
    try {
      $response = $this->client->moderation($input, $model);
      $this->recordUsageOperation('moderation', $model, $context, ['input' => $input], NULL, TRUE);
      return $response;
    }
    catch (\Throwable $e) {
      $this->recordUsageOperation('moderation', $model, $context, ['input' => $input], NULL, FALSE, $e->getMessage());
      throw $this->normalizeException($e, 'moderation', ['model' => $model]);
    }
  }

  public function embedding(string $input, string $model, bool $log = TRUE): array {
    $this->checkRateLimit('embedding', $model);
    $context = $this->beginUsageOperation('embedding', $model);
    try {
      $response = $this->client->embedding($input, $model, $log);
      $this->recordUsageOperation('embedding', $model, $context, ['input' => $input], NULL, TRUE);
      return $response;
    }
    catch (\Throwable $e) {
      $this->recordUsageOperation('embedding', $model, $context, ['input' => $input], NULL, FALSE, $e->getMessage());
      throw $this->normalizeException($e, 'embedding', ['model' => $model]);
    }
  }

  public function chatWithTools(string $model, array $messages, array $tools, $temperature, $max_tokens = 1024, string $tool_choice = 'auto', array $context_extra = []): array {
    $this->checkRateLimit('chat', $model);
    $start_time = microtime(TRUE);
    $context = $this->beginUsageOperation('chat', $model, $context_extra);
    $context['tool_count'] = count($tools);
    $this->applyChatMessageAlter($messages, $context);
    if (!empty($context['guardrail_blocked'])) {
      // Blocked requests are the ones an auditor most needs to see.
      $this->recordUsageOperation('chat', $model, $context, $messages, $context['guardrail_message'] ?? '', FALSE, 'Blocked by guardrails before the provider call.');
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
      $log_request = [
        'messages' => $messages,
        'tools' => $tools,
        'temperature' => $temperature,
        'max_tokens' => $max_tokens,
        'tool_choice' => $tool_choice,
        'context' => $context,
      ];

      $this->applyProviderFailureAlter($e, 'chat', $context);
      if (array_key_exists('failover_result', $context)) {
        $result = $context['failover_result'];
        $this->recordUsageOperation('chat', $model, $context, ['messages' => $messages, 'tools' => $tools], $result, FALSE, 'Provider call failed; served a failover response instead: ' . $e->getMessage());
        $this->log('chat', $model, $log_request, $result, TRUE, microtime(TRUE) - $start_time, 'Provider call failed; served a failover response instead: ' . $e->getMessage());
        return $result;
      }

      $this->recordUsageOperation('chat', $model, $context, ['messages' => $messages, 'tools' => $tools], NULL, FALSE, $e->getMessage());
      $this->log('chat', $model, $log_request, NULL, FALSE, microtime(TRUE) - $start_time, $e->getMessage());
      throw $this->normalizeException($e, 'chat', $context);
    }
    $this->finalizeContext($context);

    $usage_status = TRUE;
    $usage_error = NULL;
    if (isset($response['content']) && is_string($response['content'])) {
      $content = $response['content'];
      $this->applyChatResponseAlter($content, $context);
      $response['content'] = $content;
      if (!empty($context['guardrail_blocked'])) {
        $usage_status = FALSE;
        $usage_error = 'Response blocked by guardrails.';
        $response['finish_reason'] = 'guardrail_blocked';
        $response['content'] = (string) ($context['guardrail_message'] ?? '');
        $response['tool_calls'] = [];
      }
    }

    $this->recordUsageOperation('chat', $model, $context, ['messages' => $messages, 'tools' => $tools], $response, $usage_status, $usage_error);
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

  /**
   * Evaluate structured questions against an input text.
   *
   * @param string $input
   *   The input context to evaluate.
   * @param array $questions
   *   Array of question definitions.
   * @param string $model
   *   Model ID, or empty string to use provider default.
   * @param array $context_extra
   *   Optional extra execution context.
   *
   * @return array
   *   Array of evaluated outcome arrays.
   *
   * @throws AIInvalidArgumentException
   * @throws AIException
   */
  public function decide(string $input, array $questions, string $model = '', array $context_extra = []): array {
    if (empty($questions)) {
      return [];
    }

    // Pre-flight validation of questions.
    foreach ($questions as $idx => $question) {
      if (!is_array($question)) {
        throw new AIInvalidArgumentException('Each question must be an array.', 0, NULL, $this->provider, 'decision', ['index' => $idx]);
      }
      $type = $question['type'] ?? 'boolean';
      if (!in_array($type, ['boolean', 'choice', 'score'], TRUE)) {
        throw new AIInvalidArgumentException('Unsupported question type: ' . $type, 0, NULL, $this->provider, 'decision', ['question' => $question]);
      }
      if ($type === 'choice') {
        if (empty($question['options']) || !is_array($question['options'])) {
          throw new AIInvalidArgumentException('Choice questions must include a non-empty options array.', 0, NULL, $this->provider, 'decision', ['question' => $question]);
        }
      }
      if (empty($question['prompt']) && empty($question['question'])) {
        throw new AIInvalidArgumentException('Question must include a prompt.', 0, NULL, $this->provider, 'decision', ['question' => $question]);
      }
    }

    $this->checkRateLimit('decision', $model);
    $start_time = microtime(TRUE);
    $context = $this->beginUsageOperation('decision', $model, $context_extra);
    $context['question_count'] = count($questions);

    $log_request = [
      'input' => $input,
      'questions' => $questions,
      'model' => $model,
      'context' => $context,
    ];

    try {
      $response = $this->client->decide($input, $questions, $model, $context);
    }
    catch (\Throwable $e) {
      $this->applyProviderFailureAlter($e, 'decision', $context);
      if (array_key_exists('failover_result', $context) && is_array($context['failover_result'])) {
        $result = $context['failover_result'];
        $this->recordUsageOperation('decision', $model, $context, ['input' => $input, 'questions' => $questions], $result, FALSE, 'Provider call failed; served a failover response instead: ' . $e->getMessage());
        $this->log('decision', $model, $log_request, $result, TRUE, microtime(TRUE) - $start_time, 'Provider call failed; served a failover response instead: ' . $e->getMessage());
        return $result;
      }

      $this->recordUsageOperation('decision', $model, $context, ['input' => $input, 'questions' => $questions], NULL, FALSE, $e->getMessage());
      $this->log('decision', $model, $log_request, NULL, FALSE, microtime(TRUE) - $start_time, $e->getMessage());
      throw $this->normalizeException($e, 'decision', $context);
    }

    $this->finalizeContext($context);
    $this->recordUsageOperation('decision', $model, $context, ['input' => $input, 'questions' => $questions], $response, TRUE);
    $this->log('decision', $model, $log_request, $response, TRUE, microtime(TRUE) - $start_time);

    return $response;
  }

  public function describeImage(string $imageUrl, bool $sendImageData = TRUE, ?string $modelOverride = NULL): string {
    $config = config('ai_alt.settings');
    $describePrompt = $config->get('prompt');
    $model = $modelOverride ?: $config->get('model');

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
          $messages[$m_index]['content'][$c_index]['image_url']['url'] = '[redacted image-url]';
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
