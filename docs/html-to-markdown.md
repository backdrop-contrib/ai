# HTML-to-Markdown Conversion for AI Context

Use Markdown to preserve headings, lists, and inline formatting when turning
HTML into text for an AI prompt. The shared extractor removes tags that add
little textual context and keeps readable text.

## Shared converter

`AIContentExtractor::htmlToMarkdown(string $html): string`

See [AIContentExtractor.php](../includes/AIContentExtractor.php).

`AIContentExtractor` is autoloaded by the base `ai` module. Declare a
dependency on `ai` when an integration uses this class.

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

## Examples from integrating modules

| Module | Usage |
|---|---|
| `ai_field_automator` | `AIFieldAutomatorBase.inc` converts selected field values for prompts. |
| `ai_content` | `ai_content.module` converts field text for editorial tools, including tone adjustment, summarization, and title suggestions. |
| `ai_related_content` | `ai_related_content.module` uses `extractEntityText()` to collect node text. |
| `ai_content_lifecycle` | `AIContentLifecycleAnalyzer.inc` uses `extractEntityText()` to collect content for analysis. |
| `ai_search` | `ai_search.module` uses the shared extractor for entity text. The AI Search Block submodule also converts retrieved text for prompt context. |

The AI Search embedding path uses `AIStringHelper::prepareText()` to prepare
plain text before requesting embeddings. AI SEO Advisor can pass rendered
HTML when the model needs to inspect heading hierarchy, tags, and nesting.

## When to use `htmlToMarkdown` vs `prepareText`

| Situation | Method |
|---|---|
| Building a prompt from entity/field content | `AIContentExtractor::htmlToMarkdown()` |
| Rendering an entity to provide context to AI | `AIContentExtractor::htmlToMarkdown(backdrop_render(entity_view(...)))` |
| Generating vector embeddings for similarity search | `AIStringHelper::prepareText()` (plain text, no Markdown) |
| Sanitising AI output for display | Neither — use `filter_xss()` or `check_plain()` |
| SEO / HTML-structure analysis | Pass raw HTML directly |

For entity/form context extraction, prefer `AIContentExtractor` instead of
re-implementing recursive field, paragraph, or rendered-entity traversal in each
submodule. It centralizes:

- `AIContentExtractor::extractEntityText()`
- `AIContentExtractor::renderEntityToMarkdown()`
- `AIContentExtractor::extractNodeFormValuesText()`
- `AIContentExtractor::collectTextRecursive()`
