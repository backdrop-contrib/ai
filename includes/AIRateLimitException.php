<?php

/**
 * @file
 * AI rate limit exception.
 */

class AIRateLimitException extends AIException {

  public function getCategory(): string {
    return 'rate_limit';
  }

  public function isRetryable(): bool {
    return TRUE;
  }

  public function getUserSafeMessage(): string {
    if (!empty($this->context['retry_after'])) {
      return 'AI rate limit reached. Try again in ' . (int) $this->context['retry_after'] . ' seconds.';
    }
    return 'AI rate limit reached. Try again shortly.';
  }

  /**
   * Return the recommended retry delay in seconds, if known.
   */
  public function getRetryAfter(): ?int {
    return !empty($this->context['retry_after']) ? (int) $this->context['retry_after'] : NULL;
  }

}
