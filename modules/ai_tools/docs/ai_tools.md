# AI Tools

`ai_tools` is the tool-calling layer for this Backdrop site.

It gives language models a controlled way to inspect and query site data by exposing selected PHP callbacks as structured "tools" or "functions". In practice, this is the bridge between "the model can talk" and "the model can actually do useful work against the site".

## What It Does

The module provides:

- A registry of callable tools via `ai_tools_get_tools()`
- A dispatcher via `ai_tools_dispatch()` that executes a named tool with decoded arguments
- A Tool Explorer UI at `admin/config/ai/ai-explorer/tool-explorer` (old path `admin/config/ai/tool-explorer` still works)
- Built-in read and write tools for common Backdrop domains
- `hook_ai_tools()` and `hook_ai_tools_alter()` so any module can register or change tools; see `ai_tools.api.php`

The built-in tool families currently cover:

- Site information
- Nodes/content (read, plus `create_node`)
- Taxonomy (read, plus term and vocabulary writes)
- Users
- Content types (read, plus create/edit)
- Fields (read, plus field storage/instance create/edit)
- Entities
- Config (read only)
- Menus (read, plus create/edit/delete links), when the Menu module is enabled
- Layouts (read, which layout renders a path, plus add/move/remove blocks)
- Media (`ai_image_to_text`)
- Bee command help
- Views, when the Views module is enabled

Submodules add tools for other contrib modules:

- `ai_webform_tools`: Webform read/build tools (needs `webform`)
- `ai_paragraphs_tools`: Paragraphs read/build tools (needs `paragraphs`)
- `ai_alt_tools`: image alt text tools

Other `ai_*` modules register their own tools, for example `ai_metatag`, `ai_seo_advisor`, `ai_search`, `ai_related_content`, `ai_content_lifecycle` and `ai_translate`.

See `views-agent.md` for the Views Agent tool set, approval behavior, and examples.

## How It Works

The runtime flow is simple:

1. A caller builds a normal chat prompt plus a list of tool definitions.
2. The model decides whether it needs a tool.
3. If it emits a tool call, `ai_tools_dispatch()` runs the matching PHP callback.
4. The tool result is returned to the model as a tool message.
5. The model uses that result to continue the conversation or request another tool.

This means the model is not querying the database directly. It only sees the structured inputs and outputs we choose to expose.

## Why This Matters

Without tools, a model can only answer from its prompt and prior training. With tools, it can ground answers in live site data.

That opens up things like:

- Site-aware Q&A
- Content discovery assistants
- Admin copilots that understand content types, fields, views, and config
- Search assistants that combine semantic reasoning with structured retrieval
- Guided workflows where the model inspects the site before answering

Examples:

- "What are the latest articles on this site?"
- "What vocabularies exist and what terms are in them?"
- "Show me the fields on the Article content type."
- "What Views are available for AI search?"
- "What is the site name and what content types exist?"

## What The Explorer Is For

The Tool Explorer is the manual test harness for this system.

It lets you:

- Pick any configured model
- Choose which tools the model is allowed to see
- Submit a prompt
- Inspect the tool-calling trace round by round

This is useful for:

- Verifying tool schemas
- Debugging model/provider compatibility
- Watching multi-step tool use
- Testing prompts before embedding tool calling elsewhere

## How To Create A Tool

Any module can add tools through `hook_ai_tools()`. `ai_tools.api.php` documents every key a tool entry accepts.

Each tool needs:

- A `definition` that tells the model what the tool is called, what it does, and what arguments it accepts
- An `execute` callback that runs the actual PHP logic
- An `operation`: `read` for tools that only read. Anything else, or a missing value, is treated as a write, and agents ask for approval before running it

The registry key must match `definition.function.name`.

### Where to put the tools

The hook can live in any module, so a contrib integration does not have to live in `ai`:

- In the integrated module itself, if its maintainers want it. No dependency on `ai_tools` is needed: the hook only runs when `ai_tools` is enabled.
- In an `ai_tools` submodule, like `ai_webform_tools`, for generic integrations whose upstream will not carry AI code.
- In a standalone `ai_<thing>` module, like `ai_metatag`, when the integration also has its own UI, settings or cron.

Guard on `module_exists()` for any module the tools need, or set a `requirements` callback.

### Step 1: Implement `hook_ai_tools()`

Register the tool in your module:

```php
/**
 * Implements hook_ai_tools().
 */
function mymodule_ai_tools() {
  $tools = [];

  $tools['get_campaign_summary'] = [
    'definition' => [
      'type' => 'function',
      'function' => [
        'name' => 'get_campaign_summary',
        'description' => 'Return a summary of a foundation campaign node by node ID.',
        'parameters' => [
          'type' => 'object',
          'properties' => [
            'nid' => [
              'type' => 'integer',
              'description' => 'The node ID of the campaign.',
            ],
          ],
          'required' => ['nid'],
          'additionalProperties' => FALSE,
        ],
      ],
    ],
    'execute' => 'mymodule_execute_get_campaign_summary',
    'operation' => 'read',
    'destructive' => FALSE,
    'module' => 'mymodule',
    'category' => 'campaigns',
  ];

  return $tools;
}
```

Important schema rules:

- `parameters` must be an object schema, not an empty array
- Use explicit property types
- Mark required arguments in `required`
- Prefer `additionalProperties => FALSE` unless you intentionally want loose input

### Step 2: Write the executor callback

The executor receives decoded arguments as an array and should return JSON-friendly data.

```php
/**
 * Executor for get_campaign_summary.
 */
function mymodule_execute_get_campaign_summary(array $args) {
  $nid = isset($args['nid']) ? (int) $args['nid'] : 0;
  if ($nid <= 0) {
    return ['error' => 'Missing or invalid nid.'];
  }

  $node = node_load($nid);
  if (!$node || $node->type !== 'foundation_campaign') {
    return ['error' => 'Campaign not found.'];
  }

  return [
    'nid' => (int) $node->nid,
    'title' => $node->title,
    'status' => (int) $node->status,
    'url' => url('node/' . $node->nid, ['absolute' => TRUE]),
  ];
}
```

### Step 3: Test it in Tool Explorer

Go to `admin/config/ai/ai-explorer/tool-explorer`, expose only the new tool, and try prompts like:

- "Summarize campaign node 123."
- "Look up campaign 123 and tell me its title."

That gives you a clean way to verify:

- The model understands the tool description
- The schema is valid
- The arguments are generated correctly
- The result shape is useful to the model

### Tool Design Guidelines

Good tools are:

- Narrow
- Explicit
- Predictable
- JSON-friendly
- Mostly read-only unless there is a strong need for writes

Avoid:

- Huge payloads when a summary will do
- Vague argument names like `value` or `data`
- Side effects hidden behind innocent-looking names
- Returning HTML blobs when structured arrays are better

### Write Tools Need Extra Discipline

Write tools must:

- Declare `'operation' => 'write'`, and `'destructive' => TRUE` if they delete or overwrite data, so agents and MCP clients ask for approval
- Use clear names such as `create_`, `edit_`, `delete_`
- Set a `permission`, and check access to the specific object inside the executor too. `permission` only hides the tool
- Validate input and return `['error' => ...]` on failure
- Use tight schema definitions

## Is This "Agents"?

Partly, but not completely.

`ai_tools` gives you the action layer that agentic systems need. It lets the model inspect the environment, retrieve context, and chain calls. That is necessary for agents.

But tools alone are not a full agent system.

A fuller "agent" usually also includes:

- A goal loop
- Planning or task decomposition
- State or memory across steps
- Retry/recovery rules
- Approval for write actions
- Guardrails, permissions, and auditing

So the right way to think about this module is:

- `ai_tools` is the tool substrate
- It is enough for tool-using assistants
- It is one of the core building blocks for agents
- It is not, by itself, a full autonomous agent framework

## What This Enables Next

If the goal is agent-like behavior inside Backdrop, this module is the correct foundation.

`ai_agents` builds on it: it runs the tool loop, holds thread state, and pauses for approval before write tools. `ai_mcp` exposes the same registry to MCP clients.

## Current Boundaries

Right now this module is best suited for:

- Controlled tool-assisted chat
- Testing model/provider tool support
- Building higher-level assistants on a clean registry

It is not trying to be:

- A generic autonomous agent runtime
- A scheduler
- A workflow engine
- A replacement for application logic

## Files To Know

Paths are relative to `modules/contrib/ai/modules/ai_tools/`.

- `ai_tools.module`: registry, filtering, dispatcher
- `ai_tools.api.php`: hook documentation
- `includes/explorer.inc`: Tool Explorer UI
- `includes/*.tools.inc`: built-in tools, one file per domain
- `modules/`: contrib integration submodules
- `docs/views-agent.md`: Views tools and the Views agent
