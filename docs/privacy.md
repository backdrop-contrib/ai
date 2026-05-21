# AI Privacy & Data Handling

## Background

Every AI API call sends content — node body text, field values, user-entered
prompts — to an external third-party service. For many sites this is acceptable,
but there are legitimate reasons to restrict it:

- **GDPR / data sovereignty**: content containing personal data may not be
  permitted to leave the jurisdiction.
- **Content classification**: draft or confidential content types should not be
  processed externally before review.
- **Role-based restrictions**: content entered by untrusted or anonymous users
  should not be sent to external services automatically.
- **Audit compliance**: the AI log stores full prompt and response text; some
  compliance frameworks require that this data not be retained.

`ai_privacy` addresses all of these without requiring changes to individual
AI modules.

## Module location

```
modules/contrib/ai/modules/ai_privacy/
├── ai_privacy.info
├── ai_privacy.module
└── config/
    └── ai_privacy.settings.json
```

Enable at **Admin > Modules**. Requires the base `ai` module.

## What it controls

### 1. Global kill switch

A single checkbox that blocks **all** external AI processing site-wide across
every module in the AI ecosystem. Use this for emergency shutoff, planned
maintenance, or environments where no external AI calls should ever be made
(e.g. staging with production content).

### 2. Per content-type opt-out

Each content type's edit form gains an "AI Privacy" fieldset with an
"Allow external AI processing" checkbox. Unchecking it blocks all AI calls
for nodes of that type — on save, via the "Generate with AI" widget buttons,
and via background async jobs.

Individual field automator configurations are preserved; only execution is
blocked. Re-enabling the content type immediately restores processing.

### 3. Role-based opt-out

Selected roles can be denied. When the user who triggers an AI call (saves a
node, clicks "Generate with AI") belongs to a denied role, the request is
blocked. This does not affect automated background processing unless the
triggering account also belongs to a denied role.

Typical use: block AI processing for content submitted by the `anonymous` role
or a `contributor` role that has not been vetted.

### 4. Log content redaction

The existing `ai_log` table stores the full request and response JSON for
every AI call, including complete prompt text. When log redaction is enabled,
`request_data` and `response_data` are replaced with `[redacted]` before the
row is written. Metadata (module, operation, model, provider, duration, status,
uid) is still stored for audit purposes.

Enable this when storing full prompts in the database is a compliance concern.
Note: redaction applies to new log entries only — existing rows are not
retroactively modified.

## Configuration

Admin UI: **Admin > Config > AI > Privacy & Data Handling**

Per-content-type: **Admin > Structure > Content types > [type] > Edit**,
collapse the "AI Privacy" fieldset.

### Default config (all processing allowed, no redaction)

```json
{
  "global_enabled": true,
  "denied_entity_bundles": {},
  "denied_roles": [],
  "log_redact_content": false
}
```

### Example: deny processing for the `confidential` content type

```json
{
  "global_enabled": true,
  "denied_entity_bundles": {"node": ["confidential"]},
  "denied_roles": [],
  "log_redact_content": false
}
```

This is set via the content type edit form, not directly in the JSON.

## Enforcement points

The privacy check fires at every point where content could be sent externally:

| Location | What is blocked |
|---|---|
| `AIFieldAutomatorRunner::processEntity()` | On-save field automation |
| `ai_field_automator_async_generate()` | Background async jobs |
| `_ai_field_automator_widget_action_submit()` | "Generate with AI" button clicks |
| `AIApi::log()` | Log content redaction |

Modules that call `AIApi` directly are not automatically gated by the
content-type or role checks — those checks require entity context that the API
layer does not have. For those modules, call `ai_privacy_entity_allowed()`
explicitly before making the request (see below).

## Public API

### Check whether processing is allowed

```php
if (!ai_privacy_entity_allowed($entity_type, $bundle)) {
  // Skip all AI processing for this entity.
  return;
}
```

The function accepts an optional third argument for the account to check roles
against (defaults to the global `$user`):

```php
if (!ai_privacy_entity_allowed('node', 'article', $node_author)) {
  return;
}
```

### Check order

`ai_privacy_entity_allowed()` applies checks in this order, returning
`FALSE` on the first failure:

1. **Global kill switch** — `global_enabled` is `FALSE`
2. **Bundle deny list** — entity type + bundle is in `denied_entity_bundles`
3. **Role deny list** — account belongs to a role in `denied_roles`
4. **`hook_ai_privacy_check()`** — any implementation returns `FALSE`

### Extend with custom logic

Implement `hook_ai_privacy_check($entity_type, $bundle, $account)` to add
site-specific rules. Return `FALSE` to block processing, `TRUE` or `NULL` to
allow:

```php
function mymodule_ai_privacy_check($entity_type, $bundle, $account) {
  // Block AI processing for nodes flagged as personally identifiable.
  if ($entity_type === 'node' && mymodule_is_pii_bundle($bundle)) {
    return FALSE;
  }
}
```

### Check log redaction status

```php
if (module_exists('ai_privacy') && ai_privacy_log_redact()) {
  // Log content will be redacted.
}
```

This is called internally by `AIApi::log()` and does not normally need to
be called from other modules.

## What is and is not covered

| Scenario | Covered |
|---|---|
| On-save field automation blocked for denied bundle | Yes |
| Widget "Generate with AI" button blocked for denied bundle | Yes |
| Background async job blocked for denied bundle | Yes |
| Role-based block for triggering user | Yes |
| Full prompt/response redacted from AI log | Yes |
| Modules calling AIApi directly without entity context | Partial — call `ai_privacy_entity_allowed()` manually |
| Content already sent before module was enabled | No — only new requests are affected |
| Retroactive redaction of existing log entries | No — only new log entries are redacted |
| Per-field granularity (allow body, deny title) | No — opt-out is per bundle, not per field |

## Relationship to the AI log

The existing `ai_log` (enabled at **Admin > Config > AI > Settings**)
records every AI API call including the full prompt sent and response received.
This is the primary audit trail for what content was sent to which provider.

`ai_privacy` complements the log:
- The log tells you **what was sent** (retrospective audit).
- `ai_privacy` controls **whether it is sent** (prospective enforcement).
- Log redaction lets you keep the audit trail's metadata while dropping the
  content itself when retention of prompt text is a compliance issue.

Both should be enabled for sites with data handling obligations.
