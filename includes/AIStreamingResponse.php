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

  public function send(): void {
    $response = backdrop_http_request($this->url, $this->options);

    if (!isset($response->code) || (int) $response->code !== 200) {
      throw new \Exception('Streaming API error: ' . ($response->code ?? 'unknown'));
    }

    if (!isset($response->data) || !is_string($response->data)) {
      return;
    }

    foreach (explode("\n", $response->data) as $line) {
      if (strpos($line, 'data: ') !== 0) {
        continue;
      }
      $json = substr($line, 6);
      if ($json === '[DONE]') {
        break;
      }
      $data = json_decode($json, TRUE);
      if (!is_array($data)) {
        continue;
      }
      $text = ($this->extractor)($data);
      if ($text !== NULL && $text !== '') {
        echo $text;
        @ob_flush();
        @flush();
      }
    }
  }

}
