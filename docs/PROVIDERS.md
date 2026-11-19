# AI Provider Architecture

The AI module for Backdrop CMS uses a pluggable provider architecture that allows integration with various AI service providers using a unified interface.

## How it Works

The core module uses [`AIProviderClient`](../includes/AIProviderClient.php)
as the provider contract. The [`AIAdapterBase`](../includes/AIAdapterBase.php)
base class implements that interface. Site integrations use
[`AIApi`](../includes/AIApi.php) and work with model IDs in `provider/model`
form when models cross module boundaries.

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

## Finding a provider

Install a separate provider module for the service you want to use, such as
OpenAI (`ai_provider_openai`), Anthropic (`ai_provider_anthropic`), Google
Gemini (`ai_provider_google_gemini`), or Ollama (`ai_provider_ollama`).
Then enable that provider under **Configuration → AI → Settings**.

## Development Guidelines for New Providers

1. Create a new module named `ai_provider_[provider_name]` with a dependency on `ai`.
2. Implement `hook_ai_provider_info()`.
3. Put the adapter in `includes/` and extend `AIAdapterBase` or implement
   `AIProviderClient`. Use provider-local model IDs inside the adapter and
   implement the operations it supports, including as applicable:
   - `chat()`
   - `completions()`
   - `getModels()`
   - `embedding()` (optional)
   - `images()` (optional)
4. Use Key references for credentials. Use `watchdog()` for diagnostics
   and report unsupported operations and provider failures through the
   shared exception contract.
5. Respect the `max_tokens` and `temperature` parameters.

See the [bundled example](../examples/README.md) for a minimal implementation.
