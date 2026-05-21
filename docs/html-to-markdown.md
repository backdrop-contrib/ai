# HTML-to-Markdown Conversion for AI Context

## Background

Every module that sends content to an AI provider needs to convert node/entity
content from raw HTML into something an AI model can consume efficiently. Before
this work, each module solved this independently — typically with `strip_tags()`
and a whitespace-collapsing regex — losing all semantic structure in the process.

Markdown is the right target format for AI context because:

- Language models are trained heavily on Markdown (GitHub, Stack Overflow,
  documentation sites) and parse its structure natively.
- It preserves semantic information — `## Heading`, `- list item`, `**bold**` —
  that `strip_tags()` discards entirely.
- It is far more token-efficient than raw HTML (no tag noise).
- Images, figures, audio and video are stripped entirely since they carry no
  useful text context for summarization or retrieval tasks.
- Link URLs are dropped; only link text is kept. URLs add token noise without
  contributing meaning for AI tasks.

## The Canonical Method

`AIContentExtractor::htmlToMarkdown(string $html): string`

Location: `modules/contrib/ai/includes/AIContentExtractor.php`

`AIContentExtractor` is autoloaded by every `ai_*` submodule via the base `ai`
module's `hook_autoload_info()`, so no additional include or dependency
declaration is required to call it from any module in the ai/search_api_ai
namespace.

### What it converts

| HTML element | Markdown output |
|---|---|
| `<h1>`–`<h6>` | ATX headings `# ` … `###### ` |
| `<strong>`, `<b>` | `**text**` |
| `<em>`, `<i>` | `*text*` |
| `<code>` (inline) | `` `text` `` |
| `<pre>` | Fenced code block ` ``` ` |
| `<blockquote>` | `> ` prefixed lines |
| `<ul>` / `<li>` | `- item` |
| `<ol>` / `<li>` | `1. item` |
| `<a href="...">text</a>` | `text` (URL stripped) |
| `<hr>` | `---` |
| `<p>`, `<div>`, structural block tags | Blank-line separation |
| `<br>` | Newline |
| `<figure>`, `<picture>`, `<img>`, `<audio>`, `<video>` | Stripped entirely |
| `<script>`, `<style>`, `<nav>`, `<noscript>` | Stripped entirely |
| All remaining tags | Stripped; text content preserved |

HTML entities are decoded to UTF-8. Whitespace is normalized (multiple spaces/
tabs per line collapsed; no more than one consecutive blank line).

### Usage

```php
$markdown = AIContentExtractor::htmlToMarkdown($html);
```

For use with a length limit (e.g. before inserting into a prompt):

```php
$context = mb_substr(AIContentExtractor::htmlToMarkdown($html), 0, 6000);
```

## Where it is used

### Base method (canonical implementation)

| File | Notes |
|---|---|
| `modules/contrib/ai/includes/AIContentExtractor.php` | Static method — single source of truth |

### ai_field_automator (contrib)

| File | Usage |
|---|---|
| `modules/contrib/ai_field_automator/includes/AIFieldAutomatorBase.inc` | `renderEntityToText()` and the unsaved-entity preview path call `AIContentExtractor::htmlToMarkdown()` directly |

### ai_content (contrib submodule of ai)

| File | Usage |
|---|---|
| `modules/contrib/ai/modules/ai_content/ai_content.module` | 4 prompt-building calls: tone adjustment, summarization, title suggestion, taxonomy classification. Replaced `StringHelper::prepareText()` which stripped `$` and other common characters via an aggressive character-class regex. |

### ai_related_content (contrib)

| File | Usage |
|---|---|
| `modules/contrib/ai_related_content/ai_related_content.module` | Full entity render path (replaces block-tag regex + `html_entity_decode(strip_tags(...))`), paragraph render path, and field-item text extraction |

### ai_content_lifecycle (contrib)

| File | Usage |
|---|---|
| `modules/contrib/ai_content_lifecycle/includes/AIContentLifecycleAnalyzer.inc` | Field value collection loop that assembles entity text for lifecycle AI analysis |

### search_api_ai (contrib)

| File | Usage |
|---|---|
| `modules/contrib/search_api_ai/search_api_ai.module` | `search_api_ai_render_entity_text()` — renders entity to HTML then converts for embedding/indexing context |
| `modules/contrib/search_api_ai/modules/search_api_ai_search_block/search_api_ai_search_block.module` | Entity render → AI prompt context path |
| `modules/contrib/search_api_ai/modules/search_api_ai_simple_chatbot/search_api_ai_simple_chatbot.module` | Vector search result chunk content used as chatbot context |

## Intentional exceptions

### search_api_ai — embedding pipeline

`search_api_ai.module` lines that call `StringHelper::prepareText()` before
generating vector embeddings are left unchanged. Vector embeddings benefit from
clean plain text; introducing Markdown syntax markers (`##`, `**`, `-`) would
shift the embedding space in unpredictable ways and could degrade similarity
search quality.

### ai_seo_advisor

`ai_seo_advisor` deliberately passes full rendered HTML to the AI. Its system
prompt instructs the model to act as "an SEO analysis expert specialized in
evaluating HTML content from an SEO perspective" — heading hierarchy, tag
structure, and nesting are part of what the AI is asked to analyse. Converting to
Markdown would remove that structural information before the analysis runs.

## When to use `htmlToMarkdown` vs `prepareText`

| Situation | Method |
|---|---|
| Building a prompt from entity/field content | `AIContentExtractor::htmlToMarkdown()` |
| Rendering an entity to provide context to AI | `AIContentExtractor::htmlToMarkdown(backdrop_render(entity_view(...)))` |
| Generating vector embeddings for similarity search | `StringHelper::prepareText()` (plain text, no Markdown) |
| Sanitising AI output for display | Neither — use `filter_xss()` or `check_plain()` |
| SEO / HTML-structure analysis | Pass raw HTML directly |

For entity/form context extraction, prefer `AIContentExtractor` instead of
re-implementing recursive field, paragraph, or rendered-entity traversal in each
submodule. It centralizes:

- `AIContentExtractor::extractEntityText()`
- `AIContentExtractor::renderEntityToMarkdown()`
- `AIContentExtractor::extractNodeFormValuesText()`
- `AIContentExtractor::collectTextRecursive()`
