<?php

/**
 * @file
 * AI invalid argument exception.
 */

class AIInvalidArgumentException extends AIException {

  public function getCategory(): string {
    return 'invalid_argument';
  }

  public function getUserSafeMessage(): string {
    return 'Invalid arguments provided to AI operation.';
  }

}
