# AI Rate Limiting

## Background

Every external AI API call costs money and counts against upstream provider
quotas. Without a shared rate limit layer, multiple modules making concurrent
requests can easily exceed provider limits, produce runaway API costs, or trigger
429 errors from the provider that are difficult to diagnose.

`ai_rate_limit` solves this with a database-backed counter that is shared
across all PHP processes and web workers. Any module that calls through
`AIApi` gets rate limit enforcement automatically — no per-module work
required.

## Module location

```
modules/contrib/ai/modules/ai_rate_limit/
├── ai_rate_limit.info
├── ai_rate_limit.install
├── ai_rate_limit.module
└── config/
    └── ai_rate_limit.settings.json
```

Enable at **Admin > Modules**. Requires the base `ai` module.

## How it works

1. Every external AI call in `AIApi` calls `$this->checkRateLimit($operation)`
   before making the HTTP request.
2. `checkRateLimit()` calls `ai_rate_limit_check($provider, $operation)`.
3. `ai_rate_limit_check()` counts recent events in the database and compares
   against configured limits.
4. If under the limit: the event is recorded and the call proceeds.
5. If over the limit: a `WATCHDOG_WARNING` is logged and an exception is thrown.
   The calling module's existing try/catch records the failure.

The database table (`ai_rate_limit_events`) is the source of truth. It tracks
every allowed call by provider, operation, user, and timestamp. `hook_cron()`
purges records older than 2× the longest configured window to keep the table
small.

## Configuration

Admin UI: **Admin > Config > AI > Rate Limiting**

### Enable rate limiting

The top-level `Enable rate limiting` checkbox is the master switch.
When unchecked, all requests are allowed through and no rate-limit checks run.
When checked, the global limit is active immediately using the configured values.

### Global limit

Counts all provider calls combined. Useful as an absolute cost cap.
Enabling rate limiting turns this on immediately using the default
`60 requests / 60 seconds` unless you change it.

The UI exposes two fields:

- `Request limit`
- `Window`

Together they define the shared site-wide bucket. For example:

- limit `2` with window `60` = up to 2 requests within 60 seconds
- limit `60` with window `60` = up to 60 requests within 60 seconds

### Per-provider limits

Each enabled provider (AI, Anthropic, Google Gemini, Ollama, etc.) can have
its own limit and window independently. Provider-specific limits are optional
and are enabled with a checkbox per provider in the UI.

Only providers that are enabled in **AI settings** are shown here.
Each provider row has:

- `Enable provider-specific limit`
- `Request limit`
- `Window (seconds)`

If the provider-specific checkbox is off, that provider falls back to the
global limit. If it is on, the provider uses its own bucket instead.

### Default config (no limits active)

```json
{
  "enabled": false,
  "global_limit": 60,
  "global_window": 60,
  "provider_limits": {}
}
```

The module defaults to disabled. When you enable it, the global default is
`60 requests / 60 seconds`.

### Example: Global limit only

```json
{
  "enabled": true,
  "global_limit": 60,
  "global_window": 60,
  "provider_limits": {}
}
```

This allows up to 60 requests across all enabled providers within 60 seconds.

### Example: Global limit plus AI-specific override

```json
{
  "enabled": true,
  "global_limit": 60,
  "global_window": 60,
  "provider_limits": {
    "provider_name": {"limit": 20, "window": 60}
  }
}
```

In that example:

- all providers are still subject to the global `60 / 60` cap
- AI is additionally capped at `20 / 60`

The per-provider limit applies across **all operations** for that provider.
Chat, embeddings, images, moderation, and other operations share the same
provider bucket.

## Operations tracked

| Method | Operation name |
|---|---|
| `AIApi::chat()` | `chat` |
| `AIApi::completions()` | `completions` |
| `AIApi::images()` | `images` |
| `AIApi::textToSpeech()` | `text_to_speech` |
| `AIApi::speechToText()` | `speech_to_text` |
| `AIApi::moderation()` | `moderation` |
| `AIApi::embedding()` | `embedding` |
| `AIApi::describeImage()` | `chat` (uses chat endpoint) |

## Public API

### Check and register a call

```php
if (!ai_rate_limit_check($provider, $operation)) {
  // Limit exceeded — do not make the API call.
  throw new Exception('Rate limit exceeded.');
}
```

`ai_rate_limit_check()` performs the check and event registration while
holding a lock, so concurrent requests cannot overshoot the configured limit.
Do not call it speculatively — only call it immediately before making the API
request.

Modules that call `AIApi` directly get this check automatically. Only call
`ai_rate_limit_check()` directly if your module makes AI HTTP requests
through a different code path.

### Get current usage

```php
$status = ai_rate_limit_status();
// Returns: ['provider_name' => ['count' => 12, 'limit' => 60, 'window' => 60], ...]
```

Useful for admin dashboards or status pages.

## What happens when a limit is exceeded

1. A `WATCHDOG_WARNING` entry is created identifying the provider, operation,
   count, limit, and window.
2. `ai_rate_limit_check()` returns `FALSE`.
3. `AIApi::checkRateLimit()` throws an `\Exception`.
4. The exception propagates to the calling module's try/catch — in
   `ai_field_automator` this marks the job as failed; in `ai_async` it
   marks the background job as error.
5. No API call is made to the external provider.

The rate limit is per-server, not per-user. A single busy background process
can exhaust the limit for all users. If you need per-user limiting, implement
`hook_ai_rate_limit_check()` (not yet built) or add a custom check using
Backdrop's flood control before calling `AIApi`.

## Database table

`ai_rate_limit_events`

| Column | Type | Description |
|---|---|---|
| `id` | serial | Auto-increment PK |
| `provider` | varchar(64) | Provider machine name |
| `operation` | varchar(64) | Operation name |
| `uid` | int | User ID that triggered the call |
| `timestamp` | int | Unix timestamp |

Indexes: `(provider, timestamp)`, `(operation, timestamp)`.

`hook_cron()` deletes rows older than 2× the longest configured window.

## Adding rate limit awareness to a new module

If your module makes AI calls through `AIApi`, no action is required —
`checkRateLimit()` runs automatically before every call.

If your module makes AI calls through a custom HTTP client or bypass path:

```php
if (module_exists('ai_rate_limit')) {
  if (!ai_rate_limit_check('my_provider', 'my_operation')) {
    watchdog('my_module', 'Rate limit exceeded, skipping AI call.', [], WATCHDOG_WARNING);
    return;
  }
}
// ... make the API call ...
```
