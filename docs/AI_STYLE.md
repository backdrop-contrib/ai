# AI Module Style Guide

Use this guide for `ai`, `ai_*`, `ai_tools`, `ai_field_automator`, and `search_api_ai` provider integration work. Goal: new code should look like one maintained system, even when modules have older mixed style.

This guide extends the official Backdrop PHP coding standards:
https://docs.backdropcms.org/php-standards

## Baseline

- Follow Backdrop PHP conventions: 2-space indentation, braces on same line, concise docblocks, early returns.
- Use Unix line endings, no trailing whitespace, and one final newline.
- Begin PHP files with `<?php` followed by a blank line and the `/** @file */` docblock.
- Use short array syntax `[]` for arrays. These AI modules require PHP 8.2, so prefer modern PHP syntax even where older Backdrop-era examples still show `array()`.
- Keep procedural `.module` and `.inc` hooks procedural. Use classes for adapters, service-like helpers, and plugins.
- Namespaces are encouraged for coherent class families that are loaded together, such as field automator base classes and plugins. Keep standalone Backdrop hook files and current provider adapters in the global namespace unless the module has a clear autoload/registry path for namespaced classes. Existing Drupal-style port namespaces in `ai_eca` and `search_api_ai_pgvector/src` may stay isolated until those modules are intentionally ported or replaced.
- Always use curly braces for control structures. Use `elseif`, not `else if`.
- Do not wrap control-structure conditions across multiple lines; split complex conditions into named variables first.
- Do not introduce unrelated refactors while fixing behavior.
- Use ASCII in code and logs. Avoid emojis in watchdog messages.

## Naming

- Provider IDs must match config and router keys:
  - `ai`
  - `anthropic`
  - `google_gemini`
  - `aws_bedrock`
  - `browser`
  - `elevenlabs`
  - `groq`
  - `litellm`
  - `ollama`
  - `openrouter`
- Watchdog channels should match module machine names, e.g. `ai_google_gemini`, `ai_field_automator`.
- Functions and local variables should use snake_case: `$api_key`, `$base_url`, `$provider_id`, `$field_name`.
- Class names should use UpperCamelCase.
- Class methods and properties should use lowerCamelCase: `$baseUrl`, `$realApiKey`, `getModelsByCapability()`.
- Use stable common names:
  - `$model` for provider-local model ID.
  - `$messages` for AI-style chat messages.
  - `$temperature`, `$max_tokens`, `$stream_response` for public API parameters.
  - `$response` for raw HTTP/SDK response object.
  - `$result` for decoded/normalized response arrays.

## Provider Adapters

- REST-native adapters should extend `AIAdapterBase`.
- AI-compatible SDK adapters should implement `AIClientInterface` and use `AICompatibleTrait`.
- Constructors should accept `($api_key, ?AIApi $api = NULL)` when practical.
- Always expose the full capability surface, even when a provider does not support an operation (return `[]`):
  - `getModels()`
  - `getModelsByCapability($capability)`
  - `getChatModels()`
  - `getImageModels()`
  - `getVisionModels()`
  - `getEmbeddingModels()`
  - `getModerationModels()`
- `getModelsByCapability()` must call `backdrop_alter('ai_model_capabilities', ...)` before returning so sites can override capability assignments.
- Keep AI-format inputs at router boundary. Convert inside adapter only:
  - AI text block: `['type' => 'text', 'text' => $text]`
  - AI image block: `['type' => 'image_url', 'image_url' => ['url' => $data_uri]]`
- Unsupported operations should log `WATCHDOG_WARNING` and return an empty value compatible with caller expectations, not fatal.

## Streaming Responses

Use an anonymous class with a `send()` method — do not import or instantiate Symfony `StreamedResponse`:

```php
return new class($stream) {
  protected $stream;
  public function __construct($stream) { $this->stream = $stream; }
  public function send() {
    foreach ($this->stream as $chunk) {
      $text = $chunk->choices[0]->delta->content ?? '';
      if ($text !== '') { echo $text; @ob_flush(); @flush(); }
    }
  }
};
```

- Keep the anonymous class declaration inline with the `return` statement.
- Always flush after each chunk: `@ob_flush(); @flush();`.
- Return the object directly; the router calls `->send()`.
- For providers without native streaming, implement a non-streaming fallback and return a compatible object.

## Error Handling

- In classes, catch global exceptions: `catch (\Exception $e)`.
- Log with placeholders, not string concatenation:

```php
watchdog('ai_example', 'Request failed for model @model: @message', [
  '@model' => $model,
  '@message' => $e->getMessage(),
], WATCHDOG_WARNING);
```

- Use severity consistently:
  - `WATCHDOG_ERROR`: operation failed and caller cannot use result.
  - `WATCHDOG_WARNING`: provider unsupported, invalid config, retryable external failure.
  - `WATCHDOG_DEBUG`: diagnostic fallback details.
  - `WATCHDOG_INFO`/`NOTICE`: successful admin-visible state changes only.
- Do not log API keys, full prompts, image/base64 data, request bodies with secrets, or raw provider responses unless explicitly redacted.

## Comments And Strings

- Comments inside functions should be capitalized sentences with punctuation.
- Put comments on the line immediately before the code they describe.
- Prefer single-quoted strings unless the string needs interpolation or avoids awkward escaping in translated strings.
- Use spaces around string concatenation dots: `'prefix ' . $value`.

## HTTP And SDK Calls

- Use `AIAdapterBase::dispatch()` or `buildRequestOptions()` for REST JSON requests when possible.
- Use `backdrop_http_request()` for REST-native providers unless SDK is already required by adapter.
- Use raw `curl_*` only when SDK/Backdrop HTTP cannot support required behavior; isolate it in one helper.
- Set timeouts on all external requests.
- Normalize decoded responses before returning to callers.

## Config And Forms

- Store provider settings under the provider module or `ai.settings` provider map; do not duplicate long-lived config keys unless needed for migration.
- Provider selection values should be fully-qualified when crossing module boundaries: `provider/model`.
- Use Key module references for secrets. Never store raw API keys in config exports.
- Form element names should mirror config keys.

## Tool And Agent Integrations

- Tool definition keys, function names, and registry names should match unless there is a strong reason:
  - `ai_image_to_text`
  - `ai_tools_execute_image_to_text`
- Tool executors must validate IDs, permissions/access, MIME/type, and size before sending local content to AI providers.
- Tool results should be arrays; dispatcher JSON-encodes them.

## Field Automator

- Keep automator plugins thin. Shared prompt building, model calls, file validation, and value storage belong in `AIFieldAutomatorBase`.
- `allowedFieldTypes()` describes target fields. `allowedInputs()` describes source/context fields.
- Multi-value source fields should produce deterministic value order. Avoid sending every source file to every generated prompt unless tool explicitly needs a batch.
- Field automator errors should log and return empty values so entity save can continue unless caller explicitly requires failure.

## Refactor Priority

When normalizing existing code, work in this order:

1. Provider adapters: constructor signatures, capability helpers, error logging, message conversion.
2. Tool executors: validation/access/return shapes.
3. Field automator base/plugins: shared helpers, exception handling, deterministic deltas.
4. Debug-heavy legacy modules: watchdog cleanup, remove emojis, replace string-concat logs.
5. Search/RAG integrations: provider/model naming and vector error handling.

Keep each patch behavior-focused and test with `php -l` plus a no-network registry/load check where possible.

## Compliance Status

### Provider Adapters — Complete

All ten provider adapters listed below have been brought into style compliance with this guide. "Complete" here means *style* (naming, error handling, casing, capability helpers) — not feature completeness.

| Adapter | Module | Notes |
|---|---|---|
| `OpenRouterAdapter` | `ai_openrouter` | Watchdog channels, `searchForImageInResponse`, `FALSE`/`TRUE`, retry named vars, `CURLOPT_TIMEOUT` |
| `BrowserAdapter` | `ai_browser` | Unsupported ops → `WATCHDOG_WARNING` + empty return |
| `OllamaAdapter` | `ai_ollama` | Symfony import removed, anonymous streaming class, unsupported ops warned |
| `CowFartAdapter` | `ai_cowfart_provider` (custom, not contrib) | `$api_key` param, emojis removed from watchdog, capability helpers added |
| `GroqAdapter` | `ai_groq` | One-liner methods expanded, unused `$start_time` removed, indentation fixed |
| `LiteLLMAdapter` | `ai_litellm` | `getModelsByCapability()` added, full capability helper set added |
| `AnthropicAdapter` | `ai_anthropic` | `_underscore` methods renamed to camelCase, full capability helper set added |
| `GoogleGeminiAdapter` | `ai_google_gemini` | `_underscore` methods renamed, `null` → `NULL`, `t()` removed from adapter logic |
| `BedrockAdapter` | `ai_aws_bedrock` | Named condition vars, `??` null coalescing, docblock ordering fixed, `$ok` → `$is_match` |
| `ElevenLabsAdapter` | `ai_elevenlabs` | Extends `AIAdapterBase`, uses `backdrop_http_request`, detailed error mapping |

### Not Yet Audited

Modules present in the repo that fall outside the adapter matrix above. They either ship no provider adapter or layer features on top of `ai`. Style-audit status is listed for each — code-level feature status is tracked in `docs/proposals/`, not here.

| Module | Type | Style audit |
|---|---|---|
| `ai_guardrails` | Policy helpers | Pending (small module, no adapter) |
| `ai_site_context` | Context helpers | Pending (small module, no adapter) |
| `ai_related_content` | Content feature | Pending (no adapter) |
| `ai_content_lifecycle` | Workflow | Pending (no adapter) |
| `ai_metatag` | SEO feature | Pending (no adapter) |
| `ai_seo_advisor` | SEO feature | Pending (no adapter) |
| `ai_tools` | Tool registry | Pending — executors follow "Tool And Agent Integrations" rules |
| `ai_field_automator` | Field plugin base | Pending — plugins follow "Field Automator" rules |
| `ai_agent` + submodules (`ai_assistant`, `ai_agent_content_audit`, `ai_agent_maintenance`, `ai_async`) | Agent runtime | Pending |
| `ai_mcp` | MCP bridge | Pending |
| `search_api_ai` | RAG integration | Pending |

### Remaining Work

Adapter style compliance is done. For non-adapter modules above, open a targeted audit before landing large changes in them. Feature work (new tools, new agents) tracked in `docs/proposals/` — not this guide.

### Conclusion

The codebase is now significantly more cohesive. By centralizing logic in base classes and traits and enforcing a unified style guide for naming, error handling, and capability mapping, the adapter layer operates as a single, predictable system rather than a collection of disparate integrations. New code should adhere strictly to the patterns established here to maintain this uniformity.
