# AI Provider Module Guide

This document explains how to create a pluggable provider module for the `ai` Backdrop module (examples: `ai_openrouter`, `ai_ollama`, `ai_litellm`). Drop this file into `modules/contrib/ai/` so it's shipped with the main module.

Goal
- Build a small provider module that registers itself with `ai` via `hook_ai_provider_info()` and implements an adapter class (in `includes/`) exposing methods the `ai` core expects.
- Follow the current error contract: adapters should throw on unsupported capabilities and provider/runtime failures so `AIApi` can normalize those into shared `AIException` types.

Checklist
- [ ] Create module folder `modules/contrib/ai_<provider>/`
- [ ] Add `<module>.info` with metadata
- [ ] Implement `hook_autoload_info()` to register the adapter class
- [ ] Implement `hook_ai_provider_info()` with provider metadata
- [ ] Implement `hook_ai_provider_settings_alter()` to add provider settings to central AI settings form
- [ ] Add an adapter class in `includes/` implementing the required methods (see skeleton)
- [ ] Enable module, configure provider in AI settings, and test

Design notes
- Providers register metadata (id, label, adapter class) so `ai` can list and use them.
- The Key module should store API keys; use `#type => 'key_select'` in provider settings to avoid storing secrets in plaintext.
- The `ai` core expects provider adapters to provide specific helper methods (see adapter skeleton).

Files & structure

modules/contrib/ai_myprovider/
- ai_myprovider.info
- ai_myprovider.module
- includes/MyProviderAdapter.php
- README.md (optional)

Minimal `ai_myprovider.info` (Backdrop) - replace myprovider with your id

```ini
name = AI MyProvider
description = Integrates MyProvider with the AI module
core = 1.x
package = AI
version = "1.0"
```

Add `hook_autoload_info()` so Backdrop can load the adapter class automatically:

```php
function ai_myprovider_autoload_info() {
  return [
    'MyProviderAdapter' => 'includes/MyProviderAdapter.php',
  ];
}
```

Register the provider with `ai` via `hook_ai_provider_info()`:

```php
function ai_myprovider_ai_provider_info() {
  return [
    'myprovider' => [
      'label' => t('MyProvider'),
      'description' => t('MyProvider API integration.'),
      'class' => 'MyProviderAdapter',
      'website' => 'https://myprovider.example',
      'key_url' => 'https://myprovider.example/account/api-keys', // optional
      'models_url' => 'https://myprovider.example/models', // optional
    ],
  ];
}
```

Provider settings: add fields into the central AI settings form so admins can enable/configure the provider without leaving AI settings.

```php
function ai_myprovider_ai_provider_settings_alter(&$settings, $context) {
  if ($context['provider'] !== 'myprovider') {
    return;
  }

  // If your provider uses API keys stored via Key module
  $available_keys = key_get_key_names_as_options();
  $settings['api_key_myprovider'] = [
    '#type' => 'key_select',
    '#title' => t('MyProvider API Key'),
    '#default_value' => config_get('ai.settings', 'api_key_myprovider') ?: '',
    '#options' => $available_keys,
    '#key_filters' => ['type' => 'authentication'],
    '#description' => t('Add your API key via the Key module and select it here.'),
  ];

  // If you need a base URL (for local providers like Ollama)
  $settings['myprovider_base_url'] = [
    '#type' => 'textfield',
    '#title' => t('MyProvider Base URL'),
    '#default_value' => config_get('ai_myprovider.settings', 'base_url') ?: 'http://localhost:11434',
    '#description' => t('Base URL of the local MyProvider server.'),
  ];

  // Save provider-specific settings via a submit handler if needed
  if (isset($context['form'])) {
    $context['form']['#submit'][] = 'ai_myprovider_settings_submit';
  }
}

function ai_myprovider_settings_submit($form, &$form_state) {
  if (!empty($form_state['values']['myprovider_base_url'])) {
    $c = config('ai_myprovider.settings');
    $c->set('base_url', $form_state['values']['myprovider_base_url']);
    $c->save();
  }
}
```

Adapter class skeleton - implement provider-specific calls

Place `includes/MyProviderAdapter.php` and define the adapter class. The central `ai` code expects adapter instances to expose methods such as `getModels()`, `getChatModels()`, `getImageModels()`, `getVisionModels()`, `getEmbeddingModels()`, `getModerationModels()`, `getSpeechToTextModels()`, and operational methods `completions()`, `chat()`, `chatWithTools()`, `images()`, `textToSpeech()`, `speechToText()`, `moderation()`, and `embedding()`.

```php
<?php
class MyProviderAdapter extends AIAdapterBase {
  protected $provider_id;

  public function __construct($api_key = NULL, ?AIApi $api = NULL, $provider_id = 'myprovider') {
    parent::__construct($api_key, $api);
    $this->provider_id = $provider_id;
  }

  protected function getDefaultHeaders(): array {
    return [
      'Authorization' => 'Bearer ' . $this->apiKey,
    ];
  }

  // Model discovery - return arrays: model_id => friendly name
  public function getModels() {
    // Fetch all available models from your provider's API
    // Return associative array: ['model-id' => 'Display Name', ...]
    return [];
  }

  /**
   * Filter models by capability.
   *
   * This helper method is used by the capability-specific methods below.
   * Implement logic to detect which models support which capabilities,
   * either via API metadata or pattern matching.
   *
   * @param string $capability
   *   The capability: 'text', 'vision', 'image', 'embeddings', 'moderation'.
   *
   * @return array
   *   Filtered array of model_id => display name.
   */
  public function getModelsByCapability($capability): array {
    $all_models = $this->getModels();
    $filtered = [];

    foreach ($all_models as $id => $name) {
      $ok = FALSE;

      // Implement your capability detection logic here
      // This could be:
      // - Checking API metadata (if your provider returns capability info)
      // - Pattern matching on model names
      // - Hardcoded lists of known models
      switch ($capability) {
        case 'text':
          // Detect text/chat models
          $ok = TRUE; // Most models support text
          break;

        case 'image':
          // Detect image generation models
          $ok = strpos($id, 'dall-e') !== FALSE || strpos($id, 'stable-diffusion') !== FALSE;
          break;

        case 'vision':
          // Detect vision models (image input)
          $ok = strpos($id, 'vision') !== FALSE || strpos($id, 'gpt-4o') !== FALSE;
          break;

        case 'embeddings':
        case 'embedding':
          // Detect embedding models
          $ok = strpos($id, 'embed') !== FALSE;
          break;

        case 'moderation':
          // Detect moderation models
          $ok = strpos($id, 'moderation') !== FALSE;
          break;
      }

      if ($ok) {
        $filtered[$id] = $name;
      }
    }

    // Allow site-specific overrides via Backdrop's alter hook system
    // This lets site admins override capability detection in custom modules
    backdrop_alter('ai_model_capabilities', $filtered, $capability, $this->provider_id);

    return $filtered;
  }

  public function getChatModels() {
    return $this->getModelsByCapability('text');
  }

  public function getImageModels() {
    return $this->getModelsByCapability('image');
  }

  public function getVisionModels() {
    return $this->getModelsByCapability('vision');
  }

  public function getEmbeddingModels() {
    return $this->getModelsByCapability('embeddings');
  }

  public function getModerationModels() {
    return $this->getModelsByCapability('moderation');
  }

  public function getSpeechToTextModels() {
    return [];
  }

  public function completions(string $model, string $prompt, $temperature = 0.7, $max_tokens = 512, bool $stream_response = FALSE) {
    return $this->chat($model, [['role' => 'user', 'content' => $prompt]], $temperature, $max_tokens, $stream_response);
  }

  // Operations
  public function chat(string $model, array $messages, $temperature = 0.7, $max_tokens = 1024, bool $stream_response = FALSE, array $context_extra = []) {
    try {
      // Implement provider request here.
      return 'provider response';
    }
    catch (\Exception $e) {
      watchdog('ai_myprovider', 'Chat error: @message', ['@message' => $e->getMessage()], WATCHDOG_ERROR);
      throw $e;
    }
  }

  public function images(string $model, string $prompt, string $size, string $response_format, string $quality = 'standard', string $style = 'natural', ?string $output_format = NULL) {
    throw new \RuntimeException('Image generation is not supported by MyProvider.');
  }

  public function textToSpeech(string $model, string $input, string $voice, string $response_format) {
    throw new \RuntimeException('Text-to-speech is not supported by MyProvider.');
  }

  public function speechToText(string $model, string $file, string $task = 'transcribe', $temperature = 0.4, string $response_format = 'verbose_json') {
    throw new \RuntimeException('Speech-to-text is not supported by MyProvider.');
  }

  public function moderation(string $input, string $model = 'myprovider-moderation'): array {
    throw new \RuntimeException('Moderation is not supported by MyProvider.');
  }

  public function embedding(string $input, string $model, bool $log = TRUE): array {
    throw new \RuntimeException('Embeddings are not supported by MyProvider.');
  }

  public function chatWithTools(string $model, array $messages, array $tools, $temperature, $max_tokens = 1024, string $tool_choice = 'auto', array $context_extra = []): array {
    throw new \RuntimeException('Tool calling is not supported by MyProvider.');
  }
}
```

Error-handling rules

- Do not swallow provider failures and return `''`, `[]`, or fake error arrays for core operations.
- If provider/network request fails, log and `throw`.
- If capability is unsupported, `throw new \RuntimeException('... not supported ...')`.
- `AIApi` normalizes thrown adapter errors into shared AI exceptions for callers.
- Use empty arrays only for true successful empty results, not as failure sentinels.

## Model Capability Detection

Different providers handle model capabilities differently:

### AI
AI's API returns capability metadata, so the adapter can reliably detect which models support which features.

### Ollama
Ollama provides some capability metadata via its native API (e.g., 'families' array), but it's not always complete. The adapter uses a combination of:
- API metadata when available
- Pattern matching on model names (e.g., 'llava' for vision, 'embed' for embeddings)
- The `backdrop_alter('ai_model_capabilities')` hook for site-specific overrides

### OpenRouter
OpenRouter doesn't provide reliable capability metadata, so the adapter primarily relies on:
- Pattern matching on model names
- The `backdrop_alter('ai_model_capabilities')` hook for site-specific overrides

### Best Practices

1. **Use `getModelsByCapability()`**: This helper method centralizes capability detection logic
2. **Support the alter hook**: Always call `backdrop_alter('ai_model_capabilities')` to allow site admins to override your detection
3. **Document limitations**: If your provider doesn't reliably report capabilities, document this and provide examples of using the alter hook
4. **Provide fallbacks**: When in doubt, return all models and let users filter via the alter hook

See `modules/contrib/ai/examples/model_capability_override.php` for examples of using the alter hook.
