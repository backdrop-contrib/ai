# AI Moderation

AI Moderation automatically evaluates configured content targets with a
provider-neutral chat model. It stores a score, verdict, categories, reason code,
model, and entity reference for moderator review.

It is an enforcement and review layer, not a replacement for Forum,
Content Moderation, Workflows, or editorial publishing.

## What happens when content is submitted

For built-in nodes and comments, configured targets are evaluated automatically
when a form is submitted and again at presave using the cached verdict for that
save.

- Node title and text fields are evaluated together.
- Comment subject and body are evaluated together.
- Submitted text is treated as untrusted data and is never placed in a system
  message.
- Guardrails are disabled for the classifier call so abusive text reaches the
  classifier instead of being blocked before evaluation.
- Site context is disabled by default because conversational site instructions
  can cause a classifier to refuse instead of classify.
- Submissions larger than 100,000 bytes produce a provider-failure verdict
  before prefiltering or any provider request. They may still be allowed when
  the configured `on_failure` policy is **Allow**.
- Verdicts are stored in `ai_moderation_verdicts`.

When enforcement is **Log only**, content saves normally and the verdict is
available to moderators. When enforcement is **Enforce** and the action is
block, the content is saved unpublished/quarantined so it remains reportable;
the submitter sees a message that it was saved unpublished for moderation
review. Existing published content is moved unpublished by the built-in
presave hook.

Users with the `bypass ai moderation` permission skip automatic enforcement for
built-in node and comment forms. This permission is separate from administering
the moderation configuration and should be granted deliberately.

## Configuration

Open:

`Administration > Configuration > AI > AI Moderation`

### Profiles

Profiles contain:

- Label and machine name.
- Community policy prompt.
- Chat model.
- Allow and review action thresholds.
- Provider-failure action: queue for review, allow, or block.
- Enforcement: log only or enforce.
- Site-context behavior.

Use **Add new profile** to reveal a dedicated profile form. Complete the
machine name and policy fields, then choose **Save profile**. **Cancel** closes
the form without creating anything. There is no fixed profile limit.

### Action thresholds

The profile thresholds control moderation actions, not the visual editorial
labels:

- Score through `band_allow_max`: allow.
- Score through `band_review_max`: review.
- Score above `band_review_max`: block.

Scores are model-generated classifications, not calibrated probabilities.

### Editorial levels

The review UI displays a separate severity level:

- `0–9`: **Clear**
- `10–20`: **Needs review**
- `21–70`: **Smoldering**
- `71–100`: **Dumpster fire**

These labels are internal editorial language. The moderation action remains the
authoritative enforcement result.

## Provider moderation prefilter

The optional prefilter uses any enabled provider that advertises the
`moderation` capability. It is not limited to OpenAI. The available provider
models populate the **Moderation model** selector.

The prefilter:

- Runs before the normal chat evaluator.
- Uses the provider's native moderation endpoint and categories.
- Can escalate clearly high-risk content to block.
- Can never produce an allow decision.
- Falls through to normal chat evaluation when clear, unavailable, or failed.

Configure its escalation threshold separately. A provider failure does not
make OpenAI a hard dependency and does not silently allow content.

## Targets and Forum

Under **Content targets**, assign profiles to entity bundles. For the Backdrop
Forum module:

- Topics: `node.forum`
- Replies: `comment.comment_node_forum`

Only explicitly allowed public editorial fields are extracted. Built-in nodes
allow `body`, and comments allow `comment_body`. Entity integrations may
declare a `fields` allowlist in `hook_ai_moderation_entity_info()`; deployments
may override a bundle through `target_fields.<entity_type>.<bundle>` config.

Forum itself supplies topics, comments, permissions, and publication state. AI
Moderation supplies the automatic evaluation, quarantine behavior, levels, and
review UI. No Forum-specific adapter is required.

## Moderator reports

- **Review queue**: all stored verdicts, including allow results.
- **Reported content**: review, block, and error results only.
- Verdict detail pages show the entity's current or reloaded title/text, score,
  editorial level, categories, reason code, model, prefilter result, and source.

The report pages are administrative. Public users do not see internal levels,
categories, or model reasoning.

## Integration API

Custom entities can implement `hook_ai_moderation_entity_info()` to appear in
the target matrix, or call:

```php
$verdict = ai_moderation_gate_entity('forum_post', 'discussion', $id, $text, [
  'profile' => 'default',
]);
```

The custom entity owner decides whether and how to quarantine or publish based
on the returned action. The built-in node/comment hooks provide that behavior
for standard entities.

Other extension points:

- `hook_ai_moderation_verdict()` observes stored verdicts and cannot modify them.
- `hook_ai_moderation_verdict_alter()` can override verdicts before enforcement or storage.
- `ai_moderation_rescan_entity_async()` queues a re-scan using only an entity
  type and ID; submitted text is not serialized into the job.
- `moderate_text` is available through the AI tools registry for intentional
  tool/agent use. It is not the enforcement path.

Do not call `ai_tools_dispatch()` for submission enforcement. Tool permissions
are user-specific and can turn anonymous submissions into a bypass.

## Provider testing

The raw provider moderation endpoint tester lives in AI Explorer:

`Administration > Configuration > AI > AI Explorer > Moderation`

AI Explorer tests provider capability. AI Moderation tests saved profiles and
configured content targets.

## Privacy and storage

Verdict records retain scores, categories, structured reason codes,
model/provider, source, entity references, and the submitting user's UID (0
for anonymous submissions). The verdict table does not retain submitted text;
the administrator detail page reloads the referenced entity for examination.

AI request logging is controlled by the base AI module and its privacy
settings. Never commit provider keys or other secrets.

## Verification

Useful checks from the Backdrop root:

```bash
php -l modules/contrib/ai/modules/ai_moderation/ai_moderation.module
php -l modules/contrib/ai/modules/ai_moderation/includes/evaluate.inc
ddev bee updb -y
ddev bee cc all
```

Test in log-only mode first with clean, borderline, scam, and threatening
Forum topics. Confirm the verdicts in Reported content before enabling
quarantine enforcement.
