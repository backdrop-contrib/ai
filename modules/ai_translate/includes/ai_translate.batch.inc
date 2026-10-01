<?php

/**
 * @file
 * Batch API wrapper around ai_translate_node(), used by both the node
 * "AI Translate" tab (single node) and bee's ai-translate-bulk command
 * (many nodes).
 */

/**
 * Build a batch definition that translates one node.
 */
function ai_translate_batch_for_node($nid, $target_langcode) {
  return ai_translate_batch_for_nodes([$nid], $target_langcode);
}

/**
 * Build a batch definition that translates a list of nodes into the same
 * target language.
 */
function ai_translate_batch_for_nodes(array $nids, $target_langcode) {
  $operations = [];
  foreach ($nids as $nid) {
    $operations[] = ['_ai_translate_batch_operation', [$nid, $target_langcode]];
  }

  return [
    'title' => t('Translating content with AI...'),
    'operations' => $operations,
    'finished' => '_ai_translate_batch_finished',
    'file' => backdrop_get_path('module', 'ai_translate') . '/includes/ai_translate.batch.inc',
  ];
}

/**
 * Batch operation: translate a single node.
 */
function _ai_translate_batch_operation($nid, $target_langcode, array &$context) {
  module_load_include('inc', 'ai_translate', 'includes/ai_translate.translate');

  $node = node_load($nid);
  if (empty($node)) {
    $context['results']['failed'][] = t('Node @nid no longer exists.', ['@nid' => $nid]);
    return;
  }

  $result = ai_translate_node($node, $target_langcode);

  $context['message'] = t('Translating %title into @language...', [
    '%title' => $node->title,
    '@language' => _ai_translate_language_label($target_langcode),
  ]);

  if (!empty($result['status'])) {
    $context['results']['succeeded'][] = [
      'nid' => $result['node']->nid,
      'title' => $node->title,
      'target_langcode' => $target_langcode,
    ];
  }
  else {
    $context['results']['failed'][] = t('%title: @message', [
      '%title' => $node->title,
      '@message' => $result['message'],
    ]);
  }
}

/**
 * Batch 'finished' callback: summarize results for the user.
 */
function _ai_translate_batch_finished($success, array $results, array $operations) {
  unset($operations);

  if (!$success) {
    backdrop_set_message(t('The translation batch did not complete due to an unexpected error.'), 'error');
    return;
  }

  $succeeded = $results['succeeded'] ?? [];
  $failed = $results['failed'] ?? [];

  if (!empty($succeeded)) {
    $message = format_plural(count($succeeded), 'Translated 1 item.', 'Translated @count items.');
    $links = [];
    foreach ($succeeded as $translation) {
      if (empty($translation['nid'])) {
        continue;
      }
      $links[] = l(t('View translation'), 'node/' . (int) $translation['nid']);
      $links[] = l(t('Edit'), 'node/' . (int) $translation['nid'] . '/edit');
    }
    if (!empty($links)) {
      $message .= ' ' . implode(' | ', $links);
    }
    backdrop_set_message($message);
  }

  foreach ($failed as $message) {
    backdrop_set_message($message, 'warning');
  }
}

/**
 * Helper: human-readable language name for batch progress messages.
 */
function _ai_translate_language_label($langcode) {
  $languages = language_list();
  return isset($languages[$langcode]) ? $languages[$langcode]->name : $langcode;
}
