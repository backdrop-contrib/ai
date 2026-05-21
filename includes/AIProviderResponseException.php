<?php

/**
 * @file
 * AI provider response/runtime exception.
 */

class AIProviderResponseException extends AIException {

  public function getCategory(): string {
    return 'provider_response';
  }

  public function isRetryable(): bool {
    return TRUE;
  }

  public function getUserSafeMessage(): string {
    return 'AI provider request failed. Try again.';
  }

}
