<?php

/**
 * @file
 * AI usage budget exception.
 */

class AIUsageLimitException extends AIException {

  public function getCategory(): string {
    return 'usage_limit';
  }

  public function getUserSafeMessage(): string {
    return 'AI usage budget reached. Try again after the budget period resets.';
  }

}
