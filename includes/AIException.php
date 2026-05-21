<?php

/**
 * @file
 * Base exception for the AI stack.
 */

class AIException extends \Exception {

  protected $provider;
  protected $operation;
  protected $context;

  public function __construct(string $message = '', int $code = 0, ?\Throwable $previous = NULL, ?string $provider = NULL, ?string $operation = NULL, array $context = []) {
    parent::__construct($message, $code, $previous);
    $this->provider = $provider;
    $this->operation = $operation;
    $this->context = $context;
  }

  public function getProvider(): ?string {
    return $this->provider;
  }

  public function getOperation(): ?string {
    return $this->operation;
  }

  public function getContext(): array {
    return $this->context;
  }

  /**
   * Return normalized error category.
   */
  public function getCategory(): string {
    return 'ai_error';
  }

  /**
   * Whether caller may reasonably retry the operation.
   */
  public function isRetryable(): bool {
    return FALSE;
  }

  /**
   * Return a user-safe message for UI display.
   */
  public function getUserSafeMessage(): string {
    return 'AI request failed.';
  }

  /**
   * Export basic exception metadata for logs or structured callers.
   */
  public function toArray(): array {
    return [
      'type' => get_class($this),
      'category' => $this->getCategory(),
      'message' => $this->getMessage(),
      'user_message' => $this->getUserSafeMessage(),
      'retryable' => $this->isRetryable(),
      'provider' => $this->getProvider(),
      'operation' => $this->getOperation(),
      'context' => $this->getContext(),
    ];
  }

}
