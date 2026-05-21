<?php

/**
 * @file
 * AI unsupported capability exception.
 */

class AIUnsupportedCapabilityException extends AIException {

  public function getCategory(): string {
    return 'unsupported_capability';
  }

  public function getUserSafeMessage(): string {
    return 'Selected AI provider does not support this feature.';
  }

}
