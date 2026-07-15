<?php

/**
 * @file
 * Hooks provided by AI Moderation.
 */

/**
 * Declare entity types and bundles that administrators may target.
 *
 * Return an array keyed by entity type. Each value contains a label, a bundles
 * array keyed by bundle machine name, and an extract callback receiving the
 * entity object and returning plain text.
 */
function hook_ai_moderation_entity_info() {
  return [
    'forum_post' => [
      'label' => t('Forum posts'),
      'bundles' => ['discussion' => t('Discussion')],
      'extract' => 'mymodule_extract_forum_post_text',
    ],
  ];
}

/**
 * React after a moderation verdict is stored.
 *
 * Use hook_ai_moderation_verdict_alter() to override the verdict before it is
 * stored. This hook receives the stored verdict and context for side effects.
 */
function hook_ai_moderation_verdict(array $verdict, array $context) {
  // Notify a moderator when a custom forum post needs review.
}

/**
 * Alter a moderation verdict before it is stored or enforced.
 */
function hook_ai_moderation_verdict_alter(array &$verdict, array $context) {
  // Implement policy-specific overrides here.
}
