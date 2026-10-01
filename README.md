# AI Module Suite

The Backdrop CMS AI module provides a provider-agnostic foundation for AI
features in Backdrop CMS. It supports multiple providers, shared APIs, and
feature modules for text generation, embeddings, moderation, assistants, and
search.

## Requirements

You must configure at least one AI provider before you can use the provided
services.

## Documentation

Please review the bundled documentation in this module family and the project
issue queue for updates.

## Installation

- Install this module using the official [Backdrop CMS instructions](https://backdropcms.org/user-guide/modules).

- Enable the core `ai` module and the feature modules that meet your needs.

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

## Included Submodules

### **ai_alt**
Generates alt text for images using AI.

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
Active input and output guardrails that block prompt injection attacks, PII
leakage, and configurable content violations.

### **ai_moderation**
Interface for testing AI content moderation with model selection.

### **ai_privacy**
Controls which content types, bundles, and roles may send content to external
AI providers. Includes log redaction to prevent prompt and response storage.

### **ai_rate_limit**
Enforces per-provider request rate limits to prevent runaway costs and respect
upstream provider limits.

### **ai_tools**
Function-calling framework used by agents and integrations to register and
dispatch tools to AI providers.

### **ai_upgrade**
Optional admin UI for migrating legacy `openai_*` modules to their `ai_*`
replacements.

## Issues

Bugs and feature requests should be reported in the project issue queue.

## Current Maintainer

[Justin Keiser](https://github.com/keiserjb)

## Credits

- Created for Backdrop CMS by [Justin Keiser](https://github.com/keiserjb).
- Inspired by the Drupal [OpenAI module](https://www.drupal.org/project/openai)
  and the Drupal [AI module](https://www.drupal.org/project/ai).
- Developed with AI assistance.

## License

This project is GPL v2 software. See the LICENSE.txt file in this directory for complete text.
