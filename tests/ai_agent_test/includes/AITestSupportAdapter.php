<?php

/**
 * Deterministic provider adapter for AI tests.
 */
class AITestSupportAdapter implements AIProviderClient {

  public static $chatCalls = [];
  public static $chatWithToolsCalls = [];

  protected function extractRequestedContentTypeLabel($message) {
    if (preg_match('/\bdescription\s+for\s+(.+?)\s+to\s+be\b/i', $message, $matches)) {
      return trim($matches[1], " \t\n\r\0\x0B\\\"'");
    }
    if (preg_match('/\bcontent\s+types?\s+(?:called|named)\s+["\']?(.+?)["\']?(?:\s+with\b|\s+add\b|\s+also\b|\s*[,!?]|\s*$)/i', $message, $matches)) {
      return trim($matches[1]);
    }
    if (preg_match('/\bcreate\s+(?:a|an)\s+(.+?)\s+content\s+type\b/i', $message, $matches)) {
      return trim($matches[1]);
    }
    return '';
  }

  protected function contentTypeMachineName($label) {
    $machine_name = strtolower(preg_replace('/[^A-Za-z0-9]+/', '_', (string) $label));
    return trim($machine_name, '_');
  }

  protected function extractRequestedContentTypeDescription($message) {
    if (preg_match('/\b(?:update|change|set)\s+the\s+description\s+for\s+.+?\s+to\s+be\s+["\']?(.+?)["\']?\s*$/i', $message, $matches)) {
      return trim($matches[1]);
    }
    if (preg_match('/\b(?:update|change|set)\s+the\s+description\s+for\s+.+?\s+to\s+be\s+["\']?(.+?)["\']?(?:\s*[,!?]|\s*$)/i', $message, $matches)) {
      return trim($matches[1]);
    }
    if (preg_match('/\bdescription\s+for\s+.+?\s+to\s+be\s+["\']?(.+?)["\']?(?:\s*[,!?]|\s*$)/i', $message, $matches)) {
      return trim($matches[1]);
    }
    return '';
  }

  public function __construct($apiKey = '', $wrapper = NULL) {
  }

  public function getModels(): array {
    return [
      'gpt-4o-mini' => 'GPT-4o Mini',
    ];
  }

  public function getModelsByCapability($capability): array {
    if (in_array($capability, ['tools', 'tool_calling', 'text'], TRUE)) {
      return $this->getModels();
    }
    return [];
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

  public function completions(string $model, string $prompt, $temperature, $max_tokens = 512, bool $stream_response = FALSE) {
    return 'completion:' . $prompt;
  }

  public function chat(string $model, array $messages, $temperature, $max_tokens = 1024, bool $stream_response = FALSE, array $context_extra = []) {
    self::$chatCalls[] = [
      'model' => $model,
      'messages' => $messages,
      'context_extra' => $context_extra,
    ];
    if (!empty($context_extra['throw_runtime_exception'])) {
      throw new \Exception('Adapter runtime failure.');
    }
    if (!empty($context_extra['throw_unsupported_exception'])) {
      throw new \Exception('Tool calling is not supported by this provider.');
    }
    if (!empty($context_extra['operation']) && $context_extra['operation'] === 'ai_assistants_router') {
      $user_message = '';
      foreach (array_reverse($messages) as $message_entry) {
        if (!empty($message_entry['role']) && $message_entry['role'] === 'user') {
          $payload = json_decode((string) ($message_entry['content'] ?? ''), TRUE);
          $user_message = is_array($payload) && isset($payload['user_message'])
            ? (string) $payload['user_message']
            : (string) ($message_entry['content'] ?? '');
          break;
        }
      }
      if (stripos($user_message, 'content type') !== FALSE && stripos($user_message, 'field') !== FALSE) {
        return '{"strategy":"ask_many","agent_ids":["content_type_agent","field_agent"],"reason":"content type and fields"}';
      }
      if (stripos($user_message, 'description') !== FALSE) {
        return '{"strategy":"choose_one","agent_ids":["content_type_agent"],"reason":"content type edit"}';
      }
      if (stripos($user_message, 'taxonomy') !== FALSE && stripos($user_message, 'add dutch') !== FALSE) {
        return '{"strategy":"choose_one","agent_ids":["taxonomy_agent"],"reason":"taxonomy request"}';
      }
      if (stripos($user_message, 'hello multi') !== FALSE) {
        return '{"strategy":"ask_many","agent_ids":["assistant_test_agent","assistant_test_agent_two"],"reason":"multi-agent test"}';
      }
      if (stripos($user_message, 'content type') !== FALSE) {
        return '{"strategy":"choose_one","agent_ids":["content_type_agent"],"reason":"content type request"}';
      }
      return '{"strategy":"choose_one","agent_ids":[],"reason":"no matching test agent"}';
    }
    if (!empty($context_extra['test_provider_email_response'])) {
      return 'Contact us at secret@example.com.';
    }
    return 'chat';
  }

  public function images(string $model, string $prompt, string $size, string $response_format, string $quality = 'standard', string $style = 'natural', ?string $output_format = NULL) {
    return ['data' => []];
  }

  public function textToSpeech(string $model, string $input, string $voice, string $response_format) {
    return '';
  }

  public function speechToText(string $model, string $file, string $task = 'transcribe', $temperature = 0.4, string $response_format = 'verbose_json') {
    return '';
  }

  public function moderation(string $input, string $model = 'omni-moderation-latest'): array {
    return ['flagged' => FALSE];
  }

  public function embedding(string $input, string $model, bool $log = TRUE): array {
    return ['data' => [['embedding' => [0.1, 0.2]]]];
  }

  public function chatWithTools(string $model, array $messages, array $tools, $temperature, $max_tokens = 1024, string $tool_choice = 'auto', array $context_extra = []): array {
    self::$chatWithToolsCalls[] = [
      'model' => $model,
      'messages' => $messages,
      'tools' => $tools,
    ];
    if (!empty($context_extra['throw_runtime_exception'])) {
      throw new \Exception('Adapter runtime failure.');
    }
    if (!empty($context_extra['throw_unsupported_exception'])) {
      throw new \Exception('Tool calling is not supported by this provider.');
    }

    $tool_names = [];
    foreach ($tools as $tool) {
      if (!empty($tool['function']['name'])) {
        $tool_names[] = (string) $tool['function']['name'];
      }
    }

    $has_tool_message = FALSE;
    $last_tool_content = '';
    foreach ($messages as $message) {
      if (!empty($message['role']) && $message['role'] === 'tool') {
        $has_tool_message = TRUE;
        $last_tool_content = isset($message['content']) ? (string) $message['content'] : '';
      }
    }

    if ($has_tool_message) {
      return [
        'finish_reason' => 'stop',
        'content' => 'Final answer: ' . $last_tool_content,
        'tool_calls' => [],
        'raw' => [],
      ];
    }

    $user_text = '';
    foreach (array_reverse($messages) as $message) {
      if (!empty($message['role']) && $message['role'] === 'user') {
        $user_text = (string) ($message['content'] ?? '');
        break;
      }
    }

    if (in_array('edit_content_type', $tool_names, TRUE) && (stripos($user_text, 'description') !== FALSE || stripos($user_text, 'update') !== FALSE)) {
      $label = $this->extractRequestedContentTypeLabel($user_text);
      if ($label === '') {
        $label = 'Generated Type';
      }
      $machine_name = $this->contentTypeMachineName($label);
      if ($machine_name === '') {
        $machine_name = 'generated_type';
      }
      $description = $this->extractRequestedContentTypeDescription($user_text);
      if ($description === '') {
        $description = 'Updated description';
      }

      return [
        'finish_reason' => 'tool_calls',
        'content' => '',
        'tool_calls' => [
          [
            'id' => 'call-1',
            'name' => 'edit_content_type',
            'arguments' => [
              'type' => $machine_name,
              'description' => $description,
            ],
          ],
        ],
        'raw' => [],
      ];
    }

    if (in_array('create_content_type', $tool_names, TRUE)) {
      // Simulate a cautious content-type specialist: on create+fields requests,
      // plan first and wait for a follow-up instead of creating immediately.
      if (stripos($user_text, 'field') !== FALSE && stripos($user_text, 'PREVIOUS_AGENT_RESULT_FROM_') === FALSE) {
        return [
          'finish_reason' => 'stop',
          'content' => 'Plan first: create content type, then add fields after confirmation.',
          'tool_calls' => [],
          'raw' => [],
        ];
      }

      $label = $this->extractRequestedContentTypeLabel($user_text);
      if ($label === '') {
        $label = 'Generated Type';
      }
      $machine_name = $this->contentTypeMachineName($label);
      if ($machine_name === '') {
        $machine_name = 'generated_type';
      }

      return [
        'finish_reason' => 'tool_calls',
        'content' => '',
        'tool_calls' => [
          [
            'id' => 'call-1',
            'name' => 'create_content_type',
            'arguments' => [
              'type' => $machine_name,
              'name' => $label,
            ],
          ],
        ],
        'raw' => [],
      ];
    }

    if (in_array('test_write_message', $tool_names, TRUE)) {
      $tool_name = 'test_write_message';
    }
    elseif (in_array('test_controlled_message', $tool_names, TRUE)) {
      $tool_name = 'test_controlled_message';
    }
    elseif (in_array('test_routed_message', $tool_names, TRUE)) {
      $tool_name = 'test_routed_message';
    }
    else {
      $tool_name = 'test_echo_message';
    }
    $tool_arguments = [
      'message' => $user_text,
    ];
    if (in_array($tool_name, ['test_write_message', 'test_controlled_message', 'test_routed_message'], TRUE)) {
      $tool_arguments['channel'] = '#wrong-channel';
    }

    return [
      'finish_reason' => 'tool_calls',
      'content' => '',
      'tool_calls' => [
        [
          'id' => 'call-1',
          'name' => $tool_name,
          'arguments' => $tool_arguments,
        ],
      ],
      'raw' => [],
    ];
  }

  public function decide(string $input, array $questions, string $model = '', array $context_extra = []): array {
    $results = [];
    foreach ($questions as $q) {
      $id = isset($q['id']) ? $q['id'] : 'q';
      $type = isset($q['type']) ? $q['type'] : 'boolean';
      if ($type === 'boolean') {
        $results[] = [
          'id' => $id,
          'type' => 'boolean',
          'answer' => TRUE,
          'probability' => 0.95,
          'probabilities' => ['true' => 0.95, 'false' => 0.05],
        ];
      }
      elseif ($type === 'choice') {
        $chosen = !empty($q['options']) ? reset($q['options']) : 'default';
        $results[] = [
          'id' => $id,
          'type' => 'choice',
          'answer' => $chosen,
          'probability' => 0.9,
          'probabilities' => [$chosen => 0.9],
        ];
      }
      else {
        $results[] = [
          'id' => $id,
          'type' => 'score',
          'answer' => 1.0,
          'probability' => 1.0,
          'probabilities' => ['1' => 1.0],
        ];
      }
    }
    return $results;
  }
}
