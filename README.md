# AI

AI connects Backdrop CMS to AI providers. Configure a provider once and share
its models with modules that generate text, describe images, moderate content,
or power assistants and search.

## Requirements

- PHP 8.2 or later
- [Key](https://backdropcms.org/project/key)

You must configure at least one AI provider before you can use the provided
services.

## Documentation

Find setup, examples, and troubleshooting in the [project wiki](https://github.com/backdrop-contrib/ai/wiki).
Developer documentation is available in [docs/](docs/).

## Installation

- Install this module using the official [Backdrop CMS instructions](https://backdropcms.org/user-guide/modules).

- Enable the core `ai` module and the feature modules that meet your needs.

## First setup

1. Install and enable a provider module for the service you want to use.
2. In **Configuration → AI → Settings** (`admin/config/ai/settings`), open
   **Providers**. Enable the provider and select a Key entry if it requires
   credentials.
3. Under **Default Models**, select a model for each operation your site uses.
4. Enable the feature modules you need. To test a provider, try a short prompt
   with AI Explorer.

## Model capability assignments

Open `admin/config/ai/settings/capabilities` and choose **Configure** for a
provider. Automatic classification uses API metadata where available and
provider fallback rules otherwise.

Enable **Override** above a capability column, select the models that support
it, then save. This replaces automatic classification for that column across
the shared AI API and its capability-specific selectors. Select no models to
disable a capability; untick Override to restore automatic detection. Models
can belong to several categories. Manual assignments cannot add an operation
that the provider adapter does not implement.

Columns cover text/chat, vision, image generation, embeddings, TTS, STT,
moderation, tools, thinking, fill-in-the-middle, audio input, OCR, and voice
conversion. Audio input does not imply dedicated TTS/STT support. Unknown
capabilities remain unclassified and can be assigned manually.

Overrides are exportable under
`ai.settings: providers.PROVIDER.manual_capability_models`. Missing capability
keys mean automatic detection; an empty array explicitly means no models.
Saving clears model caches. Large submissions that exceed PHP input limits are
rejected without saving partial assignments.

Offline regression tests:
`php modules/contrib/ai/tests/model_capabilities.php` from the site root.

## Model catalog

Cron syncs a model catalog once a day from the public
[models.dev](https://models.dev) feed, with the
[LiteLLM price map](https://github.com/BerriAI/litellm) filling models and
fields models.dev lacks. Configure or sync it at
`admin/config/ai/settings/catalog`. Entries are stored in the
`ai_model_catalog` table, not in config, and cover only enabled providers with
a hosted catalog entry.

The catalog is a hint, never the authority:

- Capabilities: it fills tool calling, vision, and thinking only when the
  provider reports no models for that capability, and only with models the
  provider lists. Manual capability assignments always win.
- Temperature: adapters that drop rejected params (OpenAI, Anthropic, Bedrock)
  omit temperature up front for models the catalog says refuse it.
- Output tokens: `max_tokens` is capped at the model's catalog output limit.
- Model lists: labels show the release date and a deprecated marker.
- Prices: `ai_usage` reads per-token and cached-input prices from it.

When models.dev fails the previous catalog is kept; a LiteLLM failure only skips the fallback.

## Included Submodules

### **ai_alt**
Generates alt text for images using AI. AI Alt Bulk generates global alt text
for multiple image files.

### **ai_ckeditor**
Adds an AI prompt action to CKEditor editing workflows.

### **ai_content**
Provides assistive tools for tone adjustment, summaries, title suggestions,
and content analysis during the editing process.

### **ai_dblog**
Uses AI to help analyze watchdog errors and suggest likely explanations.

### **ai_devel**
Adds AI-powered realistic content generation to Devel Generate commands.

### **ai_explorer**
Interactive UI for testing chat, image, audio, and speech AI capabilities.

### **ai_guardrails**
Checks supported AI inputs and outputs for configured patterns, including
potential prompt injection and personal information. Rules can block or
redact matches.

### **ai_mcp**
Makes registered AI tools available to configured Model Context Protocol clients.

### **ai_moderation**
Evaluates configured content against policy profiles and queues verdicts for review.

### **ai_privacy**
Lets integrations apply content-type and role restrictions before sending
content to AI providers. Optional redaction removes prompt and response
content from newly written AI log entries.

### **ai_rate_limit**
Enforces request limits for the site, specific providers, and individual users.

### **ai_site_context**
Adds shared site context to AI requests that use it.

### **ai_tools**
Function-calling framework used by agents and integrations to register and
dispatch tools to AI providers. Add AI Alt Tools, AI Paragraphs Tools, or AI
Webform Tools to register tools for those features.

### **ai_translate**
Creates AI-assisted translations of nodes in Backdrop’s node translation sets.

### **ai_upgrade**
Optional admin UI for migrating legacy `openai_*` modules to their `ai_*`
replacements.

### **ai_usage**
Records operation usage and estimated cost, with reporting and optional budgets.

## Issues

Bugs and feature requests should be reported in the [Issue Queue](https://github.com/backdrop-contrib/ai/issues).

## Current Maintainer

[Justin Keiser](https://github.com/keiserjb)

## Credits

- Created for Backdrop CMS by [Justin Keiser](https://github.com/keiserjb).
- Inspired by the Drupal [OpenAI module](https://www.drupal.org/project/openai)
  and the Drupal [AI module](https://www.drupal.org/project/ai).
- Developed with AI assistance.

## License

This project is GPL v2 software. See the LICENSE.txt file in this directory for complete text.
