# AI Async — Background Job Processing

## Background

AI inference calls typically take 1–10 seconds per request. When triggered from
a form widget (e.g. a "Generate with AI" button), that latency blocks the user's
HTTP request, delays the AJAX response, and can hit PHP/web-server timeouts on
slower models or longer prompts.

`ai_async` solves this with a fire-and-forget background job pattern:

1. The page request submits a job record to the database and returns immediately.
2. A non-blocking self-HTTP request triggers the background worker in a separate
   PHP process (1-second curl timeout — the parent request does not wait).
3. The browser polls a lightweight JSON status endpoint until the job completes.
4. Results are delivered to the page without a full reload.
5. If the self-HTTP trigger fails (e.g. firewall blocking loopback), `hook_cron()`
   drains any stale pending jobs as a fallback.

## Module location

```
modules/contrib/ai_async/
├── ai_async.info
├── ai_async.install
├── ai_async.module
├── includes/
│   ├── AIAsyncJob.inc
│   └── AIAsyncWorker.inc
└── js/
    └── ai_async.js
```

The module is a top-level AI module and declares `dependencies[] = ai` in its
`.info` file. Enable it at **Admin > Modules** like any other module.

## What it is

Although `ai_async` currently ships inside the AI module set, the
module itself is generic background job infrastructure rather than
AI-specific business logic.

It provides:

- A callback registry so only explicitly allowed functions can run in the
  background
- A job queue table storing submitted work, status, result, and error state
- A non-blocking process trigger that starts work in a separate PHP request
- A JSON status endpoint for browser polling
- A cron fallback so jobs can still run if the self-HTTP trigger fails

That makes it useful anywhere a Backdrop module needs a fire-and-forget pattern
for slow work, not just AI inference.

## How `ai_agent` Uses It

`ai_agent` is the part that owns the actual agent run. It loads the agent
config, executes the tool loop, handles approval pauses, and returns the final
result.

When a caller passes `options['async'] = TRUE` and the `ai_async` module is
enabled, `ai_agent_run()` does not execute the loop inline. Instead it calls
`ai_async_submit('ai_agent_async_run', ...)` and returns the job id.

Later, the async worker calls `ai_agent_async_run()`, which simply strips
the async flag and re-enters `ai_agent_run()`. The agent logic is the same
in both paths; only the execution context changes.

That gives the site two modes:

- Sync: immediate result in the current request.
- Async: same agent logic, but moved out of the request thread.

Async is the better fit when the agent may do several tool loops or pause for
approval. It reduces browser wait time and timeout risk, but it does not make
the model call itself faster.

## Security model

Only functions explicitly registered via `hook_ai_async_callbacks()` can be
invoked as background callbacks. Unregistered names are rejected before the job
is even inserted. Each job is issued a random 32-byte token; the process endpoint
validates the token before running the job. The status endpoint is gated to
logged-in users and enforces per-job UID ownership.

## Public API

### Submitting a job

```php
$job_id = ai_async_submit('mymodule_my_callback', array(
  'entity_type' => 'node',
  'entity_id'   => $node->nid,
  // ... any serializable data your callback needs
));
```

Returns a UUID string on success, or `FALSE` if the callback is not registered.
The job begins processing immediately in the background.

### Checking job status (PHP)

```php
$job = ai_async_get_job($job_id);
// $job->status: 'pending' | 'processing' | 'complete' | 'error'
// $job->getResult(): decoded result array, or NULL
// $job->error: error string if status === 'error'
```

### Polling from JavaScript

Include `ai_async.js` (via `#attached`) and call:

```js
AIAsync.poll(jobId, {
  interval:   2000,           // ms between polls (default: 1000)
  maxWait:    180000,         // give up after this many ms (default: 120000)
  onComplete: function (result) { /* result is the decoded PHP return value */ },
  onError:    function (message) { /* error string */ },
  onTimeout:  function () { /* maxWait exceeded */ }
});

// Returns a cancel handle:
var handle = AIAsync.poll(...);
handle.cancel(); // stop polling
```

The status URL is read from `Backdrop.settings.ai_async.statusUrl` if set,
otherwise defaults to `/ai/async/status/`. Supply it via Backdrop JS
settings when attaching the JS:

```php
$element['#attached']['js'][] = backdrop_get_path('module', 'ai_async') . '/js/ai_async.js';
$element['#attached']['js'][] = array(
  'type' => 'setting',
  'data' => array('ai_async' => array('statusUrl' => url('ai/async/status/'))),
);
```

## Registering a callback

Implement `hook_ai_async_callbacks()` in your module:

```php
function mymodule_ai_async_callbacks() {
  return array(
    'mymodule_my_callback' => t('Description shown in admin/debug contexts.'),
  );
}
```

Then define the callback function. It receives the `$data` array passed to
`ai_async_submit()` and must return a JSON-encodable value:

```php
function mymodule_my_callback(array $data) {
  $entity_type = $data['entity_type'];
  $entity_id   = $data['entity_id'];

  $entities = entity_load($entity_type, array($entity_id));
  $entity   = reset($entities);
  if (!$entity) {
    throw new Exception('Entity not found.');
  }

  // ... do AI work ...
  return array('result' => $generated_text);
}
```

Throwing an exception marks the job as `error` and stores the message. Do not
catch exceptions in your callback unless you want to suppress the error.

### Callback constraints

- The callback **must** be a named function (not a method or closure) — it is
  invoked via `call_user_func()` with the registered string name.
- The callback runs in a separate HTTP request with its own bootstrap. It has
  full access to the Backdrop API but **no access to the original `$form_state`**
  or session of the triggering request.
- Only saved entities (those with a database ID) can be loaded in the background.
  Unsaved/new entities must use a synchronous path.
- The callback result is `json_encode()`d and stored in the database. Keep it
  reasonably sized — it is not intended for large file transfers.

## Job lifecycle

```
submitted → pending → processing → complete
                               ↘ error
```

| Status | Meaning |
|---|---|
| `pending` | Inserted; self-HTTP trigger fired but worker not yet started |
| `processing` | Worker claimed the job (atomic DB transition prevents double-processing) |
| `complete` | Callback returned successfully; result stored |
| `error` | Callback threw an exception or token was invalid |

`hook_cron()` performs three maintenance tasks:

- Drains `pending` jobs older than 60 seconds (self-HTTP trigger failed).
- Resets `processing` jobs older than 10 minutes to `error` (zombie jobs).
- Deletes `complete` and `error` jobs older than 1 hour.

## Integrating with a form widget

The typical integration pattern for a "Generate" button:

**PHP submit handler:**

```php
function mymodule_widget_submit($form, &$form_state) {
  $entity_id = entity_id($entity_type, $entity);

  if ($entity_id && module_exists('ai_async')) {
    $job_id = ai_async_submit('mymodule_my_callback', array(
      'entity_type' => $entity_type,
      'entity_id'   => $entity_id,
      // ... other context
    ));
    if ($job_id) {
      $form_state['storage']['mymodule_pending_job'] = $job_id;
      $form_state['rebuild'] = TRUE;
      return;
    }
    // Fall through to synchronous path if submission failed.
  }

  // Synchronous fallback (new entities or ai_async not enabled):
  $result = mymodule_do_ai_work($entity, ...);
  // apply result to form_state ...
  $form_state['rebuild'] = TRUE;
}
```

**PHP form alter (show spinner while job is in flight):**

```php
$job_id = $form_state['storage']['mymodule_pending_job'] ?? NULL;
if ($job_id) {
  $job = ai_async_get_job($job_id);
  if ($job && $job->status === 'complete') {
    unset($form_state['storage']['mymodule_pending_job']);
    // apply $job->getResult() to form_state
  } elseif (!$job || $job->status === 'error') {
    unset($form_state['storage']['mymodule_pending_job']);
    // show error message
  } else {
    // still running — disable button, attach JS, show spinner
    $element['#attached']['js'][] = backdrop_get_path('module', 'ai_async') . '/js/ai_async.js';
    // ... attach your polling behavior JS
  }
}
```

**JavaScript (your module's behavior):**

```js
Backdrop.behaviors.mymoduleAsync = {
  attach: function (context, settings) {
    $('[data-my-async-job-id]', context).once('my-async', function () {
      var jobId = $(this).attr('data-my-async-job-id');
      AIAsync.poll(jobId, {
        interval: 2000,
        maxWait: 180000,
        onComplete: function (result) {
          // inject result into DOM
        },
        onError: function (msg) {
          // surface error to user
        }
      });
    });
  }
};
```

## Existing consumers

| Module | Callback | Trigger |
|---|---|---|
| `ai_field_automator` | `ai_field_automator_async_generate` | "Generate with AI" widget button on saved entities |

## Candidates for future integration

Any module that makes blocking AI calls during a page request is a candidate:

- **`ai_content`** — sidebar generation buttons (summarize, tone adjust, suggest title, classify taxonomy)
- **`ai_content_lifecycle`** — content analysis run on node save
- **`ai_related_content`** — related content generation on node save
- **`search_api_ai`** — embedding generation during Search API indexing batches
