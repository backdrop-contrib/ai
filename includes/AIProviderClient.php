<?php

/**
 * @file
 * Interface for AI provider adapters.
 */

interface AIProviderClient {

  public function getModels(): array;

  public function getModelsByCapability($capability): array;

  public function getChatModels(): array;

  public function getImageModels(): array;

  public function getVisionModels(): array;

  public function getEmbeddingModels(): array;

  public function getModerationModels(): array;

  public function getSpeechToTextModels(): array;

  public function completions(string $model, string $prompt, $temperature, $max_tokens = 512, bool $stream_response = FALSE);

  public function chat(string $model, array $messages, $temperature, $max_tokens = 1024, bool $stream_response = FALSE, array $context_extra = []);

  public function images(string $model, string $prompt, string $size, string $response_format, string $quality = 'standard', string $style = 'natural', ?string $output_format = NULL);

  public function textToSpeech(string $model, string $input, string $voice, string $response_format);

  public function speechToText(string $model, string $file, string $task = 'transcribe', $temperature = 0.4, string $response_format = 'verbose_json');

  public function moderation(string $input, string $model = 'omni-moderation-latest'): array;

  public function embedding(string $input, string $model, bool $log = TRUE): array;

  public function chatWithTools(string $model, array $messages, array $tools, $temperature, $max_tokens = 1024, string $tool_choice = 'auto', array $context_extra = []): array;
}
