# AI Usage

Optional usage-ledger submodule for the Backdrop AI package.

## Purpose

`ai_usage` records one normalized, privacy-conscious row for each provider operation routed through `AIApi`. It is optional and records only when the submodule is enabled and its setting is turned on. The parent `ai` module remains fully functional without it.

The ledger provides cost estimates, alerts, quotas, and budget enforcement as one optional, privacy-conscious feature. It does not store prompts, responses, API keys, or raw provider payloads.

## Current implementation

- Normalized records for provider, model, operation, module, user, request ID, optional context group, status, and duration.
- Input/output/cached/reasoning/total token fields.
- Provider-reported usage when an adapter exposes it through the parent API.
- Conservative four-characters-per-token estimates when provider metadata is unavailable.
- Manual per-model input/output USD pricing with explicit cost-source labels.
- Prices from the AI model catalog (models.dev with a LiteLLM fallback); manual rows override catalog prices.
- Cached input billed at the catalog cache-read rate.
- Price list at `admin/reports/ai-usage/pricing`, sortable and filterable by provider, model, capability, and deprecation.
- Optional cheaper model switching for chat, tool-calling, and decision calls, set site-wide and overridden per module.
- Filterable admin report at `admin/reports/ai-usage`.
- CSV export at `admin/reports/ai-usage/export`.
- Recording and retention settings at `admin/config/ai/ai-usage`.
- Cron retention purge with `0` meaning retain raw records indefinitely.
- Alter hook: `hook_ai_usage_record_alter(&$row, $record)`.
- Optional monthly per-user and anonymous token/cost budgets.
- Report-only or hard-block budget behavior, with one threshold alert per user and period.
- Alter hook: `hook_ai_usage_budget_alert_alter(&$status, $row)`.

## Delivered capability

- Usage ledger and operation lifecycle instrumentation.
- Provider-reported or explicitly estimated usage across chat, tools, completions, embeddings, image, moderation, and audio operations.
- Synced and manual provider/model pricing with clearly labeled estimated costs (`manual` or `synced`).
- Cheaper model switching: a call runs on the cheapest priced model from the same provider with every capability it needs (text, tool calling, vision), in the requested model's catalog family or a smaller tier of it (gpt to gpt-mini), and never a deprecated one. The requested model must be priced and in the catalog. A failed cheaper call is retried once on the requested model, except for streams. The ledger records both models.
- Filtered dashboard summaries, per-call records, CSV export, monthly budget status, and threshold alerts.
- Optional authenticated-user and anonymous token/cost quotas with report-only or hard-block enforcement.
- Retention controls and alter hooks for privacy, alert delivery, and site-specific governance.

## Optional enhancements

The shipped feature is complete without these additions. They may be added for larger installations or more accounting precision:

- Durable pre-aggregated rollups for very large ledgers.
- Projected-token reservations before a provider call, instead of enforcement based on completed rows.

## Architecture

The parent `AIApi` owns the operation lifecycle and calls `ai_usage_record_operation()` only when the optional submodule is available. `ai_usage` owns persistence, cost estimation, reporting, alerts, and quota policy. Provider adapters may call `AIApi::setProviderUsage()` after receiving an authoritative provider usage envelope.

Cheaper model switching uses `hook_ai_operation_model_alter(&$model, $info)`, which `AIApi` invokes before chat, tool-calling, and decision calls. `$info` holds `provider`, `operation`, `module`, and `capabilities`. The parent module knows nothing about prices; `ai_usage` implements the hook because it owns pricing. Embeddings, images, audio, and moderation never switch models.

Synced prices come from the parent module's model catalog (`ai_model_catalog`, configured at `admin/config/ai/settings/catalog`). Providers without a hosted catalog entry (local runtimes, proxies, character- or second-billed services) get no synced prices.

## Installation

- Install this module using the official [Backdrop CMS instructions](https://backdropcms.org/user-guide/modules).
- Requires the `ai` module.
- Turn on recording and set retention at `admin/config/ai/ai-usage`.

## Issues

Bugs and feature requests should be reported in the [Issue Queue](https://github.com/backdrop-contrib/ai/issues).

## Current Maintainer

[Justin Keiser](https://github.com/keiserjb)

## Credits

- Created for Backdrop CMS by [Justin Keiser](https://github.com/keiserjb).

- Developed with AI assistance.

## License

This project is GPL v2 software. See the LICENSE.txt file in this directory for complete text.
