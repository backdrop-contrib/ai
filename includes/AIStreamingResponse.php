<?php

/**
 * @file
 * Shared streaming response wrapper for SSE-based adapter streaming.
 */
class AIStreamingResponse {

  protected $url;
  protected $options;
  protected $extractor;

  public function __construct(string $url, array $options, callable $extractor) {
    $this->url = $url;
    $this->options = $options;
    $this->extractor = $extractor;
  }

  /**
   * Stream the provider response to the client as chunks arrive.
   *
   * Uses a cURL write callback so each SSE line is echoed the moment it is
   * received. backdrop_http_request() buffers the whole body, which would
   * turn "streaming" into wait-then-dump — it remains only as the fallback
   * when cURL is unavailable.
   */
  public function send(): void {
    if (function_exists('curl_init')) {
      $this->sendStreaming();
      return;
    }
    $this->sendBuffered();
  }

  /**
   * Real incremental streaming over cURL.
   */
  protected function sendStreaming(): void {
    $headers = [];
    foreach (($this->options['headers'] ?? []) as $name => $value) {
      $headers[] = $name . ': ' . $value;
    }

    $line_buffer = '';
    $error_body = '';
    $status_code = 0;

    $ch = curl_init($this->url);
    curl_setopt($ch, CURLOPT_CUSTOMREQUEST, strtoupper((string) ($this->options['method'] ?? 'POST')));
    if (isset($this->options['data'])) {
      curl_setopt($ch, CURLOPT_POSTFIELDS, $this->options['data']);
    }
    curl_setopt($ch, CURLOPT_HTTPHEADER, $headers);
    curl_setopt($ch, CURLOPT_TIMEOUT, (int) ($this->options['timeout'] ?? 300));
    curl_setopt($ch, CURLOPT_RETURNTRANSFER, FALSE);
    curl_setopt($ch, CURLOPT_WRITEFUNCTION, function ($handle, $chunk) use (&$line_buffer, &$error_body, &$status_code) {
      if (!$status_code) {
        $status_code = (int) curl_getinfo($handle, CURLINFO_RESPONSE_CODE);
      }
      if ($status_code !== 200) {
        // Collect the error body instead of echoing it to the client.
        $error_body .= $chunk;
        return strlen($chunk);
      }

      $line_buffer .= $chunk;
      while (($pos = strpos($line_buffer, "\n")) !== FALSE) {
        $line = substr($line_buffer, 0, $pos);
        $line_buffer = substr($line_buffer, $pos + 1);
        $this->processLine(rtrim($line, "\r"));
      }
      return strlen($chunk);
    });

    $ok = curl_exec($ch);
    if (!$status_code) {
      $status_code = (int) curl_getinfo($ch, CURLINFO_RESPONSE_CODE);
    }
    $curl_error = curl_error($ch);
    curl_close($ch);

    if ($status_code === 200 && $line_buffer !== '') {
      $this->processLine(rtrim($line_buffer, "\r"));
    }

    if ($status_code !== 200) {
      $detail = $error_body !== '' ? ': ' . substr(trim(strip_tags($error_body)), 0, 500) : '';
      throw new \Exception('Streaming API error (' . ($status_code ?: 'unknown') . ')' . $detail);
    }
    if ($ok === FALSE && $curl_error !== '') {
      throw new \Exception('Streaming transport error: ' . $curl_error);
    }
  }

  /**
   * Fallback: fetch the whole body, then emit the chunks.
   */
  protected function sendBuffered(): void {
    $response = backdrop_http_request($this->url, $this->options);

    if (!isset($response->code) || (int) $response->code !== 200) {
      throw new \Exception('Streaming API error: ' . ($response->code ?? 'unknown'));
    }

    if (!isset($response->data) || !is_string($response->data)) {
      return;
    }

    foreach (explode("\n", $response->data) as $line) {
      $this->processLine(rtrim($line, "\r"));
    }
  }

  /**
   * Decode one SSE line and echo any extracted text.
   */
  protected function processLine(string $line): void {
    if (strpos($line, 'data: ') !== 0) {
      return;
    }
    $json = substr($line, 6);
    if ($json === '[DONE]') {
      return;
    }
    $data = json_decode($json, TRUE);
    if (!is_array($data)) {
      return;
    }
    $text = ($this->extractor)($data);
    if ($text !== NULL && $text !== '') {
      echo $text;
      @ob_flush();
      @flush();
    }
  }

}
