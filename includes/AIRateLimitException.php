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
    return 'AI rate limit reached. Try again shortly.';
  }

}
