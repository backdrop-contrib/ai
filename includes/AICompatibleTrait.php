<?php

/**
 * @file
 * Shared helpers for adapters that use common chat-completions request/response formats.
 */

trait AICompatibleTrait {

  /**
   * Default capability filter for adapters using common chat-completions formats.
   */
  public function getModelsByCapability($capability): array {
    return $this->getModels();
  }

  /**
   * Normalizes a chat-completions response into the standard tool shape.
   */
  protected function normalizeToolResponse(array $result): array {
    $choice = $result['choices'][0] ?? [];
    $message = $choice['message'] ?? [];
    $finish_reason = $choice['finish_reason'] ?? 'stop';
    $content = trim($message['content'] ?? '');
    $tool_calls = [];

    foreach ($message['tool_calls'] ?? [] as $tc) {
      $args = $tc['function']['arguments'] ?? '{}';
      if (is_string($args)) {
        $args = json_decode($args, TRUE) ?? [];
      }
      $tool_calls[] = [
        'id' => $tc['id'] ?? '',
        'name' => $tc['function']['name'] ?? '',
        'arguments' => $args,
      ];
    }

    return [
      'finish_reason' => $finish_reason,
      'content' => $content,
      'tool_calls' => $tool_calls,
      'raw' => $result,
    ];
  }

}
