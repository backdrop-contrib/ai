# AI Site Context

AI Site Context adds a shared description and behavioral instructions to
supported AI prompts. Configure the site context and mode for each operation at
`admin/config/ai/ai-site-context`. See AI Guardrails for application-level
checks that block or redact matches.

## How it works

Supported calls through `AIApi` fire
`hook_ai_chat_messages_alter()` or `hook_ai_prompt_alter()` with a
`$context` array that includes an `operation` key. `ai_site_context`
reads that key, looks it up in the configured operation→mode map, and injects
accordingly.

### Injection modes

| Mode | What is injected |
|---|---|
| `full` | Guardrails + global site context (system message) |
| `prompt_only` | Global site context only — no guardrails |
| `none` | Nothing injected |

### Default operation map

| Operation | Default mode | Rationale |
|---|---|---|
| `chat` | `full` | General-purpose chat; behavioral rules matter |
| `completion` | `full` | Legacy completions; same as chat |
| `chatbot` | `full` | User-facing conversation |
| `field_generation` | `prompt_only` | Automated field fills; guardrails add no value |
| `content_tools` | `prompt_only` | Summarize/tone/title; has its own system prompt |
| `image_generation` | `prompt_only` | Light site context useful; guardrails not |
| `metatag` | `prompt_only` | Site context for metadata |
| `seo_analysis` | `none` | Receives raw HTML; site context is noise |
| `moderation` | `none` | Classification task; no context needed |
| `embedding_generate` | `none` | Vectors must not include injected text |
| `text_to_speech` | `none` | Audio conversion; no context applies |
| `speech_to_text` | `none` | Transcription; no context applies |
| `ai_translate` | `none` | Translation |
| `content_lifecycle` | `none` | Content analysis |

All modes and the default for unlisted operations are configurable at
`admin/config/ai/ai-site-context`.

## Behavioral instructions

Use the **Guardrails** field for instructions about factual accuracy,
untrusted text, sensitive information, and refusal behavior. These are part
of the prompt sent to the model. Longer instructions use more input tokens.
Review settings with the models and features your site uses.

## Passing an operation from a calling module

Calls to `ai_chat()` or `AIApi::chat()` can pass an `operation`
key that overrides the default `'chat'` value in the alter context:

```php
// Standard wrapper function — 6th argument is $context_extra.
ai_chat($model, $messages, $temperature, $max_tokens, FALSE, [
  'operation' => 'field_generation',
]);

// Direct chat() call — 6th argument is $context_extra.
$api->chat($model, $messages, $temperature, $max_tokens, FALSE, [
  'operation' => 'my_custom_operation',
]);
```

Add the operation to the mode map at `admin/config/ai/ai-site-context`
or define a default in config if it does not need to be user-configurable.

### Disabling injection entirely for a specific call

Set `disable_site_context => TRUE` in the context extra. The prompt and chat-message alter hooks respect this flag. Chat injection
also skips the browser provider.

```php
ai_chat($model, $messages, $temperature, $max_tokens, FALSE, [
  'disable_site_context' => TRUE,
]);
```

## Modules that pass an operation

| Module | Operation passed | Mode |
|---|---|---|
| `ai_field_automator` | `field_generation` | `prompt_only` |
| `ai_content` | `content_tools` | `prompt_only` |

Modules that do not pass an explicit operation receive the mode configured for
`chat` (default: `full`). This is the safe default — chatbot, search block,
and any module that calls `ai_chat()` without an operation hint will
continue to receive full injection as before.

## Admin UI

`admin/config/ai/ai-site-context`

- **Enable/disable** toggle — kills all injection when unchecked.
- **Guardrails** textarea — the behavioral rules block, injected for `full` mode.
- **Global Site Context** textarea — the site identity block, injected for
  `full` and `prompt_only` modes.
- **Token browser** — browse available Backdrop tokens for use in both fields.
- **Injection mode per operation** — a select per known operation.
- **Default mode for unlisted operations** — applied to any operation name not
  in the explicit map.
