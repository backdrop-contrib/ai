<?php

/**
 * @file
 * Shared base for AI provider adapters.
 */

abstract class AIAdapterBase implements AIProviderClient {

  /** @var string */
  protected $apiKey = '';

  /** @var AIApi|null */
  protected $api = NULL;

  public function __construct($api_key, ?AIApi $api = NULL) {
    $this->apiKey = trim((string) $api_key);
    $this->api = $api;
  }

  abstract protected function getDefaultHeaders(): array;

  /**
   * Compact a provider error body for exception messages.
   *
   * Provider errors can be multi-kilobyte HTML pages; exception messages flow
   * into watchdog and sometimes the UI, so strip markup and cap the length.
   */
  protected function formatErrorBody($response): string {
    $body = trim(strip_tags($this->responseBody($response)));
    if ($body === '') {
      // Transport failures (timeout, DNS, TLS) come back as code -1 with an
      // empty body; backdrop_http_request() puts the cURL/socket message in
      // ->error. Surface it instead of collapsing to "Unknown error".
      if (!empty($response->error)) {
        return trim((string) $response->error);
      }
      return 'Unknown error';
    }
    return strlen($body) > 600 ? substr($body, 0, 600) . '…' : $body;
  }

  /**
   * Return a response body, decoding chunked transfer encoding.
   *
   * backdrop_http_request() sends HTTP/1.0 and leaves the body as received,
   * but some providers' load balancers (DeepL's, for one) still answer with
   * Transfer-Encoding: chunked. Undecoded, the chunk-size lines break
   * json_decode() and a successful response reads as empty.
   */
  protected function responseBody($response): string {
    $body = (string) ($response->data ?? '');
    $encoding = strtolower((string) ($response->headers['transfer-encoding'] ?? ''));
    if ($body === '' || strpos($encoding, 'chunked') === FALSE) {
      return $body;
    }

    $decoded = '';
    $offset = 0;
    $length = strlen($body);
    while ($offset < $length) {
      $line_end = strpos($body, "\r\n", $offset);
      if ($line_end === FALSE) {
        return $body;
      }
      $size_hex = trim(explode(';', substr($body, $offset, $line_end - $offset), 2)[0]);
      if ($size_hex === '' || !ctype_xdigit($size_hex)) {
        // Not actually chunked; leave it alone.
        return $body;
      }
      $size = hexdec($size_hex);
      if ($size === 0) {
        break;
      }
      $decoded .= substr($body, $line_end + 2, $size);
      $offset = $line_end + 2 + $size + 2;
    }
    return $decoded;
  }

  public function getModelsByCapability($capability): array {
    // A catalog alone is not evidence of support for a particular operation.
    $models = [];
    backdrop_alter('ai_model_capabilities', $models, $capability, $this);
    return $models;
  }

  public function getChatModels(): array {
    return $this->getModelsByCapability('text');
  }

  public function getImageModels(): array {
    return $this->getModelsByCapability('image');
  }

  public function getVisionModels(): array {
    return $this->getModelsByCapability('vision');
  }

  public function getEmbeddingModels(): array {
    return $this->getModelsByCapability('embeddings');
  }

  public function getModerationModels(): array {
    return $this->getModelsByCapability('moderation');
  }

  public function getSpeechToTextModels(): array {
    return $this->getModelsByCapability('stt');
  }

  public function getDecisionModels(): array {
    return $this->getModelsByCapability('decision');
  }

  /**
   * Evaluate structured questions against an input context.
   *
   * Provides fallback emulation using tool calling or JSON schema formatting
   * when an adapter does not implement a native decision endpoint.
   */
  public function decide(string $input, array $questions, string $model = '', array $context_extra = []): array {
    if (empty($questions)) {
      return [];
    }
    foreach ($questions as $q) {
      if (($q['type'] ?? '') === 'box') {
        throw new \InvalidArgumentException('Box questions are not supported without native decision support.');
      }
    }

    if ($model === '') {
      $chat_models = $this->getChatModels();
      $model = !empty($chat_models) ? (string) array_key_first($chat_models) : 'default';
    }

    // Attempt tool calling first if supported by the adapter.
    $decisions = $this->emulateDecisionViaTools($input, $questions, $model, $context_extra);
    if (!empty($decisions)) {
      return $this->normalizeDecisions($questions, $decisions);
    }

    // Fall back to JSON-prompted chat.
    $decisions = $this->emulateDecisionViaChat($input, $questions, $model, $context_extra);
    return $this->normalizeDecisions($questions, $decisions);
  }

  /**
   * Emulate decision via tool calling.
   */
  protected function emulateDecisionViaTools(string $input, array $questions, string $model, array $context_extra): array {
    $tools = [
      [
        'type' => 'function',
        'function' => [
          'name' => 'record_decisions',
          'description' => 'Record decision evaluation results for each question.',
          'parameters' => [
            'type' => 'object',
            'properties' => [
              'decisions' => [
                'type' => 'array',
                'description' => 'List of decision evaluation results matching the questions.',
                'items' => [
                  'type' => 'object',
                  'properties' => [
                    'id' => ['type' => 'string'],
                    'type' => ['type' => 'string', 'enum' => ['boolean', 'choice', 'score']],
                    'answer' => ['description' => 'The decision outcome (boolean for boolean, string choice for choice, number for score)'],
                    'probability' => ['type' => 'number', 'description' => 'Confidence probability between 0.0 and 1.0'],
                    'probabilities' => ['type' => 'object', 'description' => 'Probability distribution map across outcomes'],
                  ],
                  'required' => ['id', 'type', 'answer', 'probability'],
                ],
              ],
            ],
            'required' => ['decisions'],
          ],
        ],
      ],
    ];

    $messages = [
      [
        'role' => 'system',
        'content' => 'You are a decision evaluation engine. Evaluate the user text against the questions and call record_decisions with the results.',
      ],
      [
        'role' => 'user',
        'content' => "Questions:\n" . json_encode($questions, JSON_UNESCAPED_SLASHES) . "\n\nInput to evaluate:\n" . $input,
      ],
    ];

    try {
      // chatWithTools() takes a string tool_choice; with one tool, 'required'
      // forces record_decisions.
      $response = $this->chatWithTools($model, $messages, $tools, 0.0, 1024, 'required', $context_extra);
      if (!empty($response['tool_calls'])) {
        foreach ($response['tool_calls'] as $tool_call) {
          $call_name = $tool_call['name'] ?? ($tool_call['function']['name'] ?? '');
          if ($call_name === 'record_decisions') {
            $raw_args = $tool_call['arguments'] ?? ($tool_call['function']['arguments'] ?? '');
            $args = is_array($raw_args) ? $raw_args : json_decode((string) $raw_args, TRUE);
            if (!empty($args['decisions']) && is_array($args['decisions'])) {
              return $args['decisions'];
            }
          }
        }
      }
    }
    catch (\Throwable $e) {
      // Tool calling may not be supported; fall through to chat.
    }

    return [];
  }

  /**
   * Emulate decision via JSON prompt in standard chat.
   */
  protected function emulateDecisionViaChat(string $input, array $questions, string $model, array $context_extra): array {
    $schema_example = json_encode([
      [
        'id' => 'q1',
        'type' => 'boolean',
        'answer' => TRUE,
        'probability' => 0.95,
        'probabilities' => ['true' => 0.95, 'false' => 0.05],
      ],
    ], JSON_UNESCAPED_SLASHES);

    $messages = [
      [
        'role' => 'system',
        'content' => 'You are a calibrated decision engine. Evaluate the input against the provided questions. Output ONLY a valid JSON array matching this format: ' . $schema_example . '. Do NOT include any markdown formatting, backticks, or explanatory text.',
      ],
      [
        'role' => 'user',
        'content' => "Questions:\n" . json_encode($questions, JSON_UNESCAPED_SLASHES) . "\n\nInput to evaluate:\n" . $input,
      ],
    ];

    try {
      $response = $this->chat($model, $messages, 0.0, 1024, FALSE, $context_extra);
      $content = is_array($response) ? ($response['content'] ?? '') : (string) $response;
      $content = trim(preg_replace('/^```(?:json)?\s*|\s*```$/i', '', trim($content)));
      $parsed = json_decode($content, TRUE);
      if (is_array($parsed)) {
        return isset($parsed['decisions']) && is_array($parsed['decisions']) ? $parsed['decisions'] : $parsed;
      }
    }
    catch (\Exception $e) {
      watchdog('ai', 'Decision chat emulation failed for model @model: @message', [
        '@model' => $model,
        '@message' => $e->getMessage(),
      ], WATCHDOG_WARNING);
    }

    return [];
  }

  /**
   * Normalize raw decision items against question definitions.
   */
  protected function normalizeDecisions(array $questions, array $raw_decisions): array {
    $indexed_raw = [];
    foreach ($raw_decisions as $item) {
      if (is_array($item) && !empty($item['id'])) {
        $indexed_raw[$item['id']] = $item;
      }
    }

    $normalized = [];
    foreach ($questions as $idx => $q) {
      $id = isset($q['id']) ? (string) $q['id'] : ('q' . $idx);
      $type = isset($q['type']) ? (string) $q['type'] : 'boolean';
      $raw = $indexed_raw[$id] ?? ($raw_decisions[$idx] ?? []);

      $probability = isset($raw['probability']) && is_numeric($raw['probability']) ? (float) $raw['probability'] : 0.5;
      $probability = max(0.0, min(1.0, $probability));

      if ($type === 'boolean') {
        $ans = $raw['answer'] ?? FALSE;
        if (is_string($ans)) {
          $ans_lc = strtolower(trim($ans));
          $ans_bool = in_array($ans_lc, ['true', '1', 'yes', 'y'], TRUE);
        }
        else {
          $ans_bool = (bool) $ans;
        }

        $p_true = $ans_bool ? $probability : round(1.0 - $probability, 4);
        $p_false = round(1.0 - $p_true, 4);

        $normalized[] = [
          'id' => $id,
          'type' => 'boolean',
          'answer' => $ans_bool,
          'probability' => $probability,
          'probabilities' => [
            'true' => $p_true,
            'false' => $p_false,
          ],
        ];
      }
      elseif ($type === 'choice') {
        $options = isset($q['options']) && is_array($q['options']) ? array_values($q['options']) : [];
        $ans = isset($raw['answer']) ? (string) $raw['answer'] : '';

        // Match case-insensitively if exact match not found.
        $matched_choice = !empty($options) ? $options[0] : $ans;
        foreach ($options as $opt) {
          if (strcasecmp((string) $opt, $ans) === 0) {
            $matched_choice = $opt;
            break;
          }
        }

        $probs = isset($raw['probabilities']) && is_array($raw['probabilities']) ? $raw['probabilities'] : [];
        if (empty($probs) && !empty($options)) {
          $remainder = count($options) > 1 ? round((1.0 - $probability) / (count($options) - 1), 4) : 0.0;
          foreach ($options as $opt) {
            $probs[$opt] = ($opt === $matched_choice) ? $probability : $remainder;
          }
        }

        $normalized[] = [
          'id' => $id,
          'type' => 'choice',
          'answer' => $matched_choice,
          'probability' => $probability,
          'probabilities' => $probs,
        ];
      }
      else {
        // Score question.
        $score_ans = isset($raw['answer']) && is_numeric($raw['answer']) ? (float) $raw['answer'] : 0.0;
        $normalized[] = [
          'id' => $id,
          'type' => 'score',
          'answer' => $score_ans,
          'probability' => $probability,
          'probabilities' => isset($raw['probabilities']) && is_array($raw['probabilities']) ? $raw['probabilities'] : [(string) $score_ans => 1.0],
        ];
      }
    }

    return $normalized;
  }

  /**
   * Pass common provider usage envelopes to the parent API wrapper.
   */
  protected function captureProviderUsage(array $response): array {
    if (!$this->api || !method_exists($this->api, 'setProviderUsage')) {
      return $response;
    }

    foreach (['usage', 'usageMetadata', 'usage_metadata', 'usage_info'] as $key) {
      if (!empty($response[$key]) && is_array($response[$key])) {
        $this->api->setProviderUsage($response[$key]);
        break;
      }
    }
    return $response;
  }

  protected function makeRequest(string $url, array $body = [], array $extra_headers = [], string $method = 'POST', int $timeout = 30): array {
    $options = [
      'method' => strtoupper($method),
      'headers' => array_merge(
        ['Accept' => 'application/json'],
        $this->getDefaultHeaders(),
        $extra_headers
      ),
      'timeout' => $timeout,
    ];

    if ($options['method'] !== 'GET') {
      $options['headers']['Content-Type'] = 'application/json';
      $options['data'] = json_encode($body);
    }

    $response = backdrop_http_request($url, $options);
    if (!isset($response->code) || (int) $response->code < 200 || (int) $response->code >= 300) {
      throw new \Exception('API error (' . ($response->code ?? 'unknown') . '): ' . $this->formatErrorBody($response));
    }

    $data = json_decode($this->responseBody($response), TRUE);
    return is_array($data) ? $this->captureProviderUsage($data) : [];
  }

  protected function requestRaw(string $url, array $body = [], array $extra_headers = [], string $method = 'POST', int $timeout = 30): string {
    $options = [
      'method' => strtoupper($method),
      'headers' => array_merge(
        ['Accept' => '*/*'],
        $this->getDefaultHeaders(),
        $extra_headers
      ),
      'timeout' => $timeout,
    ];

    if ($options['method'] !== 'GET') {
      $options['data'] = json_encode($body);
    }

    $response = backdrop_http_request($url, $options);
    if (!isset($response->code) || (int) $response->code < 200 || (int) $response->code >= 300) {
      throw new \Exception('API error (' . ($response->code ?? 'unknown') . '): ' . $this->formatErrorBody($response));
    }

    return $this->responseBody($response);
  }

  protected function makeMultipartRequest(string $url, array $fields, int $timeout = 30): array {
    $boundary = '----' . bin2hex(random_bytes(16));
    $body = '';

    foreach ($fields as $name => $value) {
      if (is_array($value) && isset($value['path'])) {
        $filename = !empty($value['filename']) ? $value['filename'] : basename($value['path']);
        // Quotes or CRLF in a filename would corrupt the multipart framing.
        $filename = str_replace(['\\', '"', "\r", "\n"], '', $filename);
        $contents = file_get_contents($value['path']);
        if ($contents === FALSE) {
          throw new \Exception('Unable to read file: ' . $value['path']);
        }
        $mime = !empty($value['mime']) ? $value['mime'] : (function_exists('mime_content_type') ? mime_content_type($value['path']) : 'application/octet-stream');
        if (empty($mime)) {
          $mime = 'application/octet-stream';
        }

        $body .= "--{$boundary}\r\n";
        $body .= 'Content-Disposition: form-data; name="' . $name . '"; filename="' . $filename . "\"\r\n";
        $body .= 'Content-Type: ' . $mime . "\r\n\r\n";
        $body .= $contents . "\r\n";
        continue;
      }

      $body .= "--{$boundary}\r\n";
      $body .= 'Content-Disposition: form-data; name="' . $name . "\"\r\n\r\n";
      $body .= (string) $value . "\r\n";
    }

    $body .= "--{$boundary}--\r\n";

    $response = backdrop_http_request($url, [
      'method' => 'POST',
      'headers' => array_merge(
        ['Accept' => 'application/json', 'Content-Type' => 'multipart/form-data; boundary=' . $boundary],
        $this->getDefaultHeaders()
      ),
      'data' => $body,
      'timeout' => $timeout,
    ]);

    if (!isset($response->code) || (int) $response->code < 200 || (int) $response->code >= 300) {
      throw new \Exception('API error (' . ($response->code ?? 'unknown') . '): ' . $this->formatErrorBody($response));
    }

    $data = json_decode($this->responseBody($response), TRUE);
    return is_array($data) ? $this->captureProviderUsage($data) : [];
  }

  protected function buildStreamingResponse(string $url, array $options, callable $extractor, ?callable $retry = NULL) {
    return new AIStreamingResponse($url, $options, $extractor, $retry);
  }

  /**
   * Sampling params a model may refuse, as dotted payload paths.
   *
   * New model families drop sampling controls (temperature first). Requests
   * send them; an error naming one removes it for that model, so no
   * model-name lists are needed. The model catalog, when it knows a model
   * refuses temperature, removes it up front.
   */
  protected function optionalParams(): array {
    return ['temperature', 'top_p', 'top_k'];
  }

  /**
   * Name the optional param a provider error rejects, if any.
   *
   * @return string|null
   *   The dotted path from optionalParams(), present in $payload, that the
   *   error refuses; NULL for any other error.
   */
  protected function rejectedParam(string $message, array $payload): ?string {
    if (!preg_match('/unsupported|not supported|(?:doesn\'t|does not) support|deprecated|not allowed|not permitted/i', $message)) {
      return NULL;
    }
    foreach ($this->optionalParams() as $path) {
      $parts = explode('.', $path);
      if ($this->payloadHasPath($payload, $parts) && preg_match('/\b' . preg_quote(end($parts), '/') . '\b/i', $message)) {
        return $path;
      }
    }
    return NULL;
  }

  /**
   * Send a payload, dropping optional params the model rejects.
   *
   * Params a model refused before are left out up front; a new refusal is
   * remembered for 30 days so a model that regains support gets them back.
   *
   * @param callable $send
   *   Callable(array $payload) performing the request; it throws on error.
   */
  protected function sendDroppingRejectedParams(string $model, array $payload, callable $send) {
    $payload = $this->withoutRejectedParams($model, $payload);
    for ($attempt = 0; ; $attempt++) {
      try {
        return $send($payload);
      }
      catch (\Exception $e) {
        $path = $attempt < count($this->optionalParams()) ? $this->rejectedParam($e->getMessage(), $payload) : NULL;
        if ($path === NULL) {
          throw $e;
        }
        $this->rememberRejectedParam($model, $path);
        $payload = $this->payloadWithoutPath($payload, explode('.', $path));
      }
    }
  }

  /**
   * Retry callable for AIStreamingResponse that drops a rejected param.
   *
   * The payload is kept as an array rather than decoded back from the
   * request body, which would turn empty JSON objects into arrays.
   */
  protected function streamRetryDroppingRejectedParams(string $model, array $payload): callable {
    return function (string $message, array $options) use ($model, &$payload) {
      $path = $this->rejectedParam($message, $payload);
      if ($path === NULL) {
        return NULL;
      }
      $this->rememberRejectedParam($model, $path);
      $payload = $this->payloadWithoutPath($payload, explode('.', $path));
      $options['data'] = json_encode($payload);
      return $options;
    };
  }

  /**
   * Remove the optional params a model has refused before.
   */
  protected function withoutRejectedParams(string $model, array $payload): array {
    $cache = cache_get($this->rejectedParamsCid($model));
    $rejected = ($cache && is_array($cache->data)) ? $cache->data : [];
    $entry = $this->api && function_exists('ai_model_catalog_entry') ? ai_model_catalog_entry($this->api->getProviderId(), $model) : NULL;
    if ($entry && $entry['temperature'] === 0) {
      foreach ($this->optionalParams() as $path) {
        if (substr($path, -strlen('temperature')) === 'temperature') {
          $rejected[] = $path;
        }
      }
    }
    foreach (array_unique($rejected) as $path) {
      $payload = $this->payloadWithoutPath($payload, explode('.', $path));
    }
    return $payload;
  }

  protected function rememberRejectedParam(string $model, string $path): void {
    $cid = $this->rejectedParamsCid($model);
    $cache = cache_get($cid);
    $known = ($cache && is_array($cache->data)) ? $cache->data : [];
    $known[] = $path;
    cache_set($cid, array_values(array_unique($known)), 'cache', REQUEST_TIME + 30 * 86400);
    watchdog('ai', '@class: model @model rejected @param; omitting it for this model.', [
      '@class' => get_class($this),
      '@model' => $model,
      '@param' => $path,
    ], WATCHDOG_NOTICE);
  }

  protected function rejectedParamsCid(string $model): string {
    return 'ai:rejected_params:' . get_class($this) . ':' . $model;
  }

  protected function payloadHasPath(array $payload, array $parts): bool {
    foreach ($parts as $part) {
      if (!is_array($payload) || !array_key_exists($part, $payload)) {
        return FALSE;
      }
      $payload = $payload[$part];
    }
    return TRUE;
  }

  protected function payloadWithoutPath(array $payload, array $parts): array {
    $key = array_shift($parts);
    if (!array_key_exists($key, $payload)) {
      return $payload;
    }
    if (empty($parts)) {
      unset($payload[$key]);
    }
    elseif (is_array($payload[$key])) {
      $payload[$key] = $this->payloadWithoutPath($payload[$key], $parts);
    }
    return $payload;
  }
}
