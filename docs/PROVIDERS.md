# AI Provider Architecture

The AI module for Backdrop CMS uses a pluggable provider architecture that allows integration with various AI service providers using a unified interface.

## How it Works

The core module defines an `AIClientInterface` (in `ai.module` or via the adapters) that all provider adapters must implement.

### 1. Provider Registration
Providers register themselves using `hook_ai_provider_info()`:

```php
function my_provider_ai_provider_info() {
  return [
    'my_provider' => [
      'label' => t('My AI Provider'),
      'description' => t('Description of the service.'),
      'class' => 'MyProviderAdapter',
      // ... metadata ...
    ],
  ];
}
```

### 2. Provider Adapters
Each provider must provide an adapter class. These classes are responsible for translating the unified AI-style requests into the provider's specific API format.

Adapters are located in the `includes/` folder of the provider module and autoloaded via `hook_autoload_info()`.

### 3. Settings Integration
Providers can add their own settings to the main AI settings form (`admin/config/ai/settings`) using `hook_ai_provider_settings_alter()`.

## Currently Supported Providers

- **AI**: The default provider (built-in).
- **Anthropic**: Integration for Claude models (ai_anthropic).
- **Google Gemini**: Integration for Gemini models (ai_google_gemini).
- **Groq**: Ultra-fast inference for various open models (ai_groq).
- **Ollama**: Local AI model support (ai_ollama).
- **OpenRouter**: Aggregator for hundreds of AI models (ai_openrouter).

## Development Guidelines for New Providers

1. Create a new module named `ai_[provider_name]`.
2. Implement `hook_ai_provider_info()`.
3. Create an adapter class in `includes/` that implements the expected methods:
   - `chat()`
   - `completions()`
   - `getModels()`
   - `embeddings()` (optional)
   - `images()` (optional)
4. Use `watchdog()` for error logging.
5. Respect the `max_tokens` and `temperature` parameters.
