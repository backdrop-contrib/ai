<?php

/**
 * @file
 * Shared base for AI provider adapters.
 */

abstract class AIAdapterBase implements AIProviderClient {

  /** @var string */
  protected $apiKey = '';

  /** @var AIApi|null */
  protected $api = NULL;

  public function __construct($api_key, ?AIApi $api = NULL) {
    $this->apiKey = trim((string) $api_key);
    $this->api = $api;
  }

  abstract protected function getDefaultHeaders(): array;

  public function getModelsByCapability($capability): array {
    $models = $this->getModels();
    backdrop_alter('ai_model_capabilities', $models, $capability, $this);
    return $models;
  }

  public function getChatModels(): array {
    return $this->getModelsByCapability('text');
  }

  public function getImageModels(): array {
    return $this->getModelsByCapability('image');
  }

  public function getVisionModels(): array {
    return $this->getModelsByCapability('vision');
  }

  public function getEmbeddingModels(): array {
    return $this->getModelsByCapability('embeddings');
  }

  public function getModerationModels(): array {
    return $this->getModelsByCapability('moderation');
  }

  public function getSpeechToTextModels(): array {
    return $this->getModelsByCapability('stt');
  }

  protected function makeRequest(string $url, array $body = [], array $extra_headers = [], string $method = 'POST', int $timeout = 30): array {
    $options = [
      'method' => strtoupper($method),
      'headers' => array_merge(
        ['Accept' => 'application/json'],
        $this->getDefaultHeaders(),
        $extra_headers
      ),
      'timeout' => $timeout,
    ];

    if ($options['method'] !== 'GET') {
      $options['headers']['Content-Type'] = 'application/json';
      $options['data'] = json_encode($body);
    }

    $response = backdrop_http_request($url, $options);
    if (!isset($response->code) || (int) $response->code < 200 || (int) $response->code >= 300) {
      throw new \Exception('API error (' . ($response->code ?? 'unknown') . '): ' . ($response->data ?? 'Unknown error'));
    }

    $data = json_decode((string) ($response->data ?? ''), TRUE);
    return is_array($data) ? $data : [];
  }

  protected function requestRaw(string $url, array $body = [], array $extra_headers = [], string $method = 'POST', int $timeout = 30): string {
    $options = [
      'method' => strtoupper($method),
      'headers' => array_merge(
        ['Accept' => '*/*'],
        $this->getDefaultHeaders(),
        $extra_headers
      ),
      'timeout' => $timeout,
    ];

    if ($options['method'] !== 'GET') {
      $options['data'] = json_encode($body);
    }

    $response = backdrop_http_request($url, $options);
    if (!isset($response->code) || (int) $response->code < 200 || (int) $response->code >= 300) {
      throw new \Exception('API error (' . ($response->code ?? 'unknown') . '): ' . ($response->data ?? 'Unknown error'));
    }

    return (string) ($response->data ?? '');
  }

  protected function makeMultipartRequest(string $url, array $fields, int $timeout = 30): array {
    $boundary = '----' . bin2hex(random_bytes(16));
    $body = '';

    foreach ($fields as $name => $value) {
      if (is_array($value) && isset($value['path'])) {
        $filename = !empty($value['filename']) ? $value['filename'] : basename($value['path']);
        $contents = file_get_contents($value['path']);
        if ($contents === FALSE) {
          throw new \Exception('Unable to read file: ' . $value['path']);
        }
        $mime = !empty($value['mime']) ? $value['mime'] : (function_exists('mime_content_type') ? mime_content_type($value['path']) : 'application/octet-stream');
        if (empty($mime)) {
          $mime = 'application/octet-stream';
        }

        $body .= "--{$boundary}\r\n";
        $body .= 'Content-Disposition: form-data; name="' . $name . '"; filename="' . $filename . "\"\r\n";
        $body .= 'Content-Type: ' . $mime . "\r\n\r\n";
        $body .= $contents . "\r\n";
        continue;
      }

      $body .= "--{$boundary}\r\n";
      $body .= 'Content-Disposition: form-data; name="' . $name . "\"\r\n\r\n";
      $body .= (string) $value . "\r\n";
    }

    $body .= "--{$boundary}--\r\n";

    $response = backdrop_http_request($url, [
      'method' => 'POST',
      'headers' => array_merge(
        ['Accept' => 'application/json', 'Content-Type' => 'multipart/form-data; boundary=' . $boundary],
        $this->getDefaultHeaders()
      ),
      'data' => $body,
      'timeout' => $timeout,
    ]);

    if (!isset($response->code) || (int) $response->code < 200 || (int) $response->code >= 300) {
      throw new \Exception('API error (' . ($response->code ?? 'unknown') . '): ' . ($response->data ?? 'Unknown error'));
    }

    $data = json_decode((string) ($response->data ?? ''), TRUE);
    return is_array($data) ? $data : [];
  }

  protected function buildStreamingResponse(string $url, array $options, callable $extractor) {
    return new AIStreamingResponse($url, $options, $extractor);
  }
}
