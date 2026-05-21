<?php

/**
 * @file
 * Entity class for AI Log.
 */

class AILog {
  public $id;
  public $timestamp;
  public $uid;
  public $module;
  public $operation;
  public $model;
  public $provider;
  public $request_data;
  public $response_data;
  public $status;
  public $duration;
  public $error_message;

  /**
   * Constructor for AILog.
   */
  public function __construct(array $values = [], $entity_type = NULL) {
    foreach ($values as $key => $value) {
      $this->$key = $value;
    }
  }

  /**
   * Returns the label of the log entry.
   */
  public function label() {
    return t('Log @id: @op (@model)', [
      '@id' => $this->id,
      '@op' => $this->operation,
      '@model' => $this->model,
    ]);
  }

  /**
   * Implements EntityInterface::id().
   */
  public function id() {
    return isset($this->id) ? $this->id : NULL;
  }

  /**
   * Implements EntityInterface::entityType().
   */
  public function entityType() {
    return 'ai_log';
  }

  /**
   * Implements EntityInterface::uri().
   */
  public function uri() {
    return ['path' => 'admin/reports/ai/' . $this->id];
  }
}
