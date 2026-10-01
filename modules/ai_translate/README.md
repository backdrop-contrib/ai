# AI Translate

Translates node content into other configured site languages using an AI
provider, via Backdrop's built-in node translation sets (core `translation`
module — one full node per language, linked by translation set).

## What it does

- Adds an "AI Translate" tab to translation-enabled content types, letting a
  user with the `use ai translate` permission pick a target language and
  create a new translation with AI-translated field values.
- Translates the title plus text, formatted text (with summary), link
  titles, image alt/title text, and file descriptions.
- Recursively clones and translates Paragraphs items, including nested
  Paragraphs fields, while preserving their order and paragraph types.
- Optionally follows entity-reference fields up to a configurable depth,
  translating referenced content along with it — but only when the current
  user can view the referenced entity, and only for node references (other
  entity types are left untranslated and unchanged).
- Runs through the Batch API so a slow AI call never blocks a form submit.
- Ships `bee ai-translate-node` and `bee ai-translate-bulk` for CLI use
  (this project has no Drush).

## What it does not do (v1)

- Does not translate interface/UI strings (`locale` string translation) —
  node content only.
- Does not configure languages — at least two enabled languages must already
  exist under Regional and language settings.
- Has no pluggable "framework mode" for other translation tooling.
- The set of translatable field types is fixed in code
  (`_ai_translate_supported_field_types()`), not admin-configurable.

## Security note

Every translation checks `node_access('view', ...)` on the source node
before translating it, and on any referenced entity before following it.
This mirrors the access-check gap fixed upstream in Drupal AI Translate
1.4.1 (SA-CONTRIB-2026-120/121) — a user should never be able to trigger
translation of content they could not otherwise view.

## Installation

- Install this module using the official
  [Backdrop CMS instructions](https://backdropcms.org/user-guide/modules).
- Requires the `ai`, `locale`, and `translation` modules.
- Configure a model at **admin/config/ai/ai-translate** before using the
  "AI Translate" tab or the bee commands.

## Issues

Bugs and feature requests should be reported in the
[Issue Queue](https://github.com/backdrop-contrib/ai_translate/issues).

## Current Maintainer

[Justin Keiser](https://github.com/keiserjb)

## Credits

- Created for Backdrop CMS by [Justin Keiser](https://github.com/keiserjb).

- Developed with AI assistance.

## License

This project is GPL v2 software. See the LICENSE.txt file in this directory for complete text.
