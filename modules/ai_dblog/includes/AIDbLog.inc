<?php

class AIDbLog {
  public function __construct() {}

  public function getAIExplanation($dblog) {
    // Generate a message to send to AI API
    $message = truncate_utf8(trim(strip_tags(dblog_format_message($dblog))), 256, TRUE);

    // Fetch the model from the settings
    $config = config_get('ai_dblog.settings');
    $model = !empty($config['model']) ? $config['model'] : '';
    if (empty($model)) {
      throw new Exception(t('No model is configured for AI dblog.'));
    }

    // Resolve API provider from the model (supports provider/model format).
    $stripped_model = '';
    $api = function_exists('ai_get_api_for_model') ? ai_get_api_for_model($model, $stripped_model) : NULL;
    if (!$api) {
      throw new Exception(t('No API provider is configured for model @model.', ['@model' => $model]));
    }

    // Call API (prefer chat path for compatibility across providers/models).
    try {
      $messages = [
        [
          'role' => 'system',
          'content' => 'You are a PHP, Backdrop CMS, and Drupal expert programmer. Please return all answers without using first, second, or third person voice.',
        ],
        [
          'role' => 'user',
          'content' => 'What does this error mean on my Backdrop site and how can I fix it? The error is: "' . $message . '"',
        ],
      ];

      $model_to_use = !empty($stripped_model) ? $stripped_model : $model;

      if (method_exists($api, 'chat')) {
        $result = $api->chat($model_to_use, $messages, 0.4, 2048, FALSE, ['operation' => 'dblog_analysis']);
      }
      else {
        $prompt = 'What does this error mean on my Backdrop site and how can I fix it? The error is: "' . $message . '"';
        $result = $api->completions($model_to_use, $prompt, 0.4, 2048);
      }
      // The explanation is derived from the (attacker-influenceable) watchdog
      // message and rendered as HTML in the admin report. Sanitize to a safe
      // tag subset before it is returned or cached.
      return filter_xss(nl2br((string) $result), ['br', 'p', 'strong', 'em', 'code', 'pre', 'ul', 'ol', 'li']);
    }
    catch (\Exception $e) {
      watchdog('ai_dblog', 'Error when trying to obtain a response from AI: @error', ['@error' => $e->getMessage()], WATCHDOG_ERROR);
      return t('No possible explanations were found, or the API service is not responding.');
    }
  }

  protected function generateHash($message) {
    return hash('sha256', $message);
  }

  protected function hashSearch($hash) {
    return db_query('SELECT explanation FROM {ai_dblog} WHERE hash = :hash', [':hash' => $hash])->fetchAssoc();
  }

  protected function insertExplanation($hash, $message, $explanation) {
    db_merge('ai_dblog')
      ->key([
        'hash' => $hash,
      ])
      ->fields([
        'message' => $message,
        // Store exactly what is rendered (already filter_xss'd in
        // getAIExplanation) so a cache hit and a cache miss display the same
        // sanitized HTML.
        'explanation' => trim($explanation),
      ])
      ->execute();
  }

  public function processExplanation($dblog) {
    $message = dblog_format_message($dblog);
    $hash = $this->generateHash($message);

    $existing_explanation = $this->hashSearch($hash);
    if ($existing_explanation) {
      return filter_xss($existing_explanation['explanation'], ['br', 'p', 'strong', 'em', 'code', 'pre', 'ul', 'ol', 'li']);
    }

    $explanation = $this->getAIExplanation($dblog);
    $this->insertExplanation($hash, $message, $explanation);

    return $explanation;
  }
}

