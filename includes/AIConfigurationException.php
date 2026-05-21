<?php

/**
 * @file
 * AI configuration/setup exception.
 */

class AIConfigurationException extends AIException {

  public function getCategory(): string {
    return 'configuration';
  }

  public function getUserSafeMessage(): string {
    return 'AI provider is not configured correctly.';
  }

}
