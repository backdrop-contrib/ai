# Views Agent

The Views Agent is the Backdrop assistant specialist for Views. It can inspect existing Views, execute displays, and create simple page or block Views through the `ai_tools` tool registry.

The implementation lives in:

- `modules/contrib/ai_tools/includes/views.tools.inc`
- `modules/contrib/ai_agent/config/install/ai_agent.settings.json`
- `config_3925413841b7cdf12a96360339e59b6d/active/ai_agent.settings.json`

## Capabilities

The agent can:

- List configured Views.
- Inspect one View's base table, displays, fields, filters, sorts, pager, path, and arguments.
- List Views base tables available on the site.
- List available field, filter, sort, or argument handlers for a base table.
- Execute an existing View display and return structured result rows.
- Create a simple View with one default display and one page or block display.
- Add a simple page or block display to an existing View.

The agent should treat "report", "list", "directory", "table", "page of content", and "block of content" as likely Views requests.

## Limits

The current create tools are intentionally conservative. They support the common first pass of a View:

- Base table selection.
- Page or block display.
- Path for page displays.
- Style plugin, usually `default` or `table`.
- Items per page.
- Base-table fields.
- Optional node bundle filter.
- Optional node published filter.
- Optional single sort.

They do not currently build complex Views features such as:

- Relationships.
- Contextual filters.
- Exposed filters.
- Field-specific formatter options.
- Access rules beyond the defaults.
- Menu links.
- Attachments, feeds, or data exports.
- Complex style plugin settings.
- Deleting Views.

For complex requests, the agent should create the simple View it can safely create, then explain what needs manual follow-up in the Views UI.

## Tools

### `list_views`

Read-only. Lists all configured Views with:

- Machine name.
- Human label.
- Description.
- Base table.
- Disabled status.
- Displays and paths.

Use this before answering questions like "what Views exist?" or when a user refers to a View by label instead of machine name.

### `get_view_info`

Read-only. Inspects one View by machine name.

Returns the effective display configuration, including options inherited from the default display. This is useful before modifying a View or explaining how it works.

### `list_views_base_tables`

Read-only. Lists base tables available to Views from `views_fetch_data()`.

Common base tables include:

- `node` for content.
- `users` for user accounts.
- `taxonomy_term_data` for taxonomy terms.
- `file_managed` for files.
- Search API index tables for search-backed Views.

Use this when the agent needs to choose a correct `base_table` for `create_basic_view`.

### `list_views_handlers`

Read-only. Lists handlers for one base table.

Inputs:

- `base_table`: required.
- `handler_type`: optional, one of `field`, `filter`, `sort`, or `argument`.

Use this before creating a View when the requested fields or filters are unclear.

Example: for a content list, call it with `base_table: node` and `handler_type: field` to discover fields such as `title`, `created`, `type`, and `status`.

### `create_basic_view`

Write tool. Requires approval.

Creates a simple View with:

- A default display.
- One page or block display.
- Selected base-table fields.
- Pager settings.
- Optional node bundle and published filters.
- Optional sort.

Important inputs:

- `view_name`: machine name. Prefer lowercase letters, numbers, and underscores.
- `label`: human-readable label.
- `base_table`: defaults to `node`.
- `display_type`: `page` or `block`.
- `path`: required for page displays.
- `fields`: field handler names from the base table.
- `bundle`: optional node content type machine name.
- `published_only`: defaults to true for node Views.
- `sort_field` and `sort_order`: optional single sort.

The tool returns the created View summary and edit URL. The agent must not say the View was created until this tool returns success.

### `add_view_display`

Write tool. Requires approval.

Adds a simple page or block display to an existing View.

Use this when a user asks for another display of an existing View, for example:

- "Add a block version of the biographies View."
- "Add a page display at `/reports/content` to this View."

### `get_views_result`

Read-only. Executes a View display and returns rows as structured data.

Inputs:

- `view_name`: required.
- `display_id`: defaults to `default`.
- `arguments`: optional contextual filter arguments.
- `limit`: defaults to 10, max 50.

Use this to preview a View, answer questions from View data, or verify an existing display returns rows.

## Approval Behavior

Read tools run immediately.

Write tools are marked with:

```php
'operation' => 'write',
```

The AI Agent approval layer pauses before write tools and asks the user to approve the action. The agent should clearly describe the action that needs approval and wait for the approval flow to complete.

Current write tools:

- `create_basic_view`
- `add_view_display`

## Example Requests

### List existing Views

User:

> What Views are available?

Expected tool flow:

1. `list_views`
2. Summarize the names, labels, base tables, and useful paths.

### Inspect a View

User:

> How is the biographies View set up?

Expected tool flow:

1. `list_views` if the machine name is unclear.
2. `get_view_info` with the selected machine name.
3. Explain displays, paths, fields, filters, and sorts.

### Create a content listing page

User:

> Create a page View that lists published biography content with title and post date at `/biographies-report`.

Expected tool flow:

1. `list_views_base_tables` if needed.
2. `list_views_handlers` for `node` if needed.
3. Request approval for `create_basic_view`.
4. After approval, call `create_basic_view` with something like:

```json
{
  "view_name": "biographies_report",
  "label": "Biographies Report",
  "base_table": "node",
  "display_type": "page",
  "path": "biographies-report",
  "fields": ["title", "created"],
  "bundle": "biography",
  "published_only": true,
  "sort_field": "created",
  "sort_order": "DESC"
}
```

### Add a block display

User:

> Add a block display to the biographies View.

Expected tool flow:

1. `get_view_info` for `biographies`.
2. Request approval for `add_view_display`.
3. After approval, call `add_view_display`.

## Testing

Useful checks after changing Views tools:

```bash
php -l modules/contrib/ai_tools/includes/views.tools.inc
ddev bee cc all
```

Runtime checks:

```bash
ddev exec bee ev "module_load_include('inc', 'ai_tools', 'includes/views.tools'); print implode(chr(10), array_keys(ai_tools_views_definitions()));"
ddev exec bee ev "module_load_include('inc', 'ai_tools', 'includes/views.tools'); print json_encode(ai_tools_execute_list_views_base_tables(array()));"
```

For a create smoke test, create a uniquely named temporary View, inspect the response, and delete it immediately.

## Maintenance Notes

- Keep tool schemas compatible with common tool-calling APIs: `parameters` must be an object schema, and empty `properties` should be `new stdClass()`, not `[]`.
- Keep create tools narrow and predictable. If a feature requires deep Views UI behavior, add a purpose-built tool instead of overloading `create_basic_view`.
- Prefer structured arrays over rendered HTML in tool results.
- Preserve approval on any tool that writes config.
- After changing active agent settings, clear caches.
