<?php

/**
 * @file
 * Bee commands for the AI Translate module. This project has no Drush, so
 * bee is the CLI entry point for triggering translations outside the UI.
 */

/**
 * Implements hook_bee_command().
 */
function ai_translate_bee_command() {
  $items = [];

  $items['ai-translate-node'] = [
    'description' => bt('Translate a single node into a target language using AI.'),
    'bootstrap' => BEE_BOOTSTRAP_FULL,
    'arguments' => [
      'nid' => bt('Node ID to translate.'),
      'target' => bt('Target language code (e.g. es, fr).'),
    ],
    'callback' => 'bee_ai_translate_node',
  ];

  $items['ai-translate-bulk'] = [
    'description' => bt('Translate all content of a given type (or all translatable types) missing a translation in the target language.'),
    'bootstrap' => BEE_BOOTSTRAP_FULL,
    'arguments' => [
      'target' => bt('Target language code (e.g. es, fr).'),
    ],
    'options' => [
      'type' => bt('Restrict to a single content type machine name. Defaults to all translation-enabled types.'),
      'limit' => bt('Maximum number of nodes to translate in this run. Defaults to 50.'),
    ],
    'callback' => 'bee_ai_translate_bulk',
  ];

  return $items;
}

/**
 * Bee callback: translate one node.
 */
function bee_ai_translate_node($arguments = [], $options = []) {
  unset($options);
  module_load_include('inc', 'ai_translate', 'includes/ai_translate.translate');

  $nid = (int) ($arguments['nid'] ?? 0);
  $target_langcode = (string) ($arguments['target'] ?? '');

  $node = $nid ? node_load($nid) : NULL;
  if (empty($node)) {
    bee_message(bt('Node @nid was not found.', ['@nid' => $nid]), 'error');
    return;
  }

  $result = ai_translate_node($node, $target_langcode);
  if (!empty($result['status'])) {
    bee_message(bt('Translated node @nid (%title) into @target as new node @new_nid.', [
      '@nid' => $nid,
      '%title' => $node->title,
      '@target' => $target_langcode,
      '@new_nid' => $result['node']->nid,
    ]));
  }
  else {
    bee_message(bt('Translation failed for node @nid: @message', [
      '@nid' => $nid,
      '@message' => $result['message'],
    ]), 'error');
  }
}

/**
 * Bee callback: translate all eligible nodes missing the target language.
 */
function bee_ai_translate_bulk($arguments = [], $options = []) {
  module_load_include('inc', 'ai_translate', 'includes/ai_translate.translate');

  $target_langcode = (string) ($arguments['target'] ?? '');
  $languages = language_list();
  if (!isset($languages[$target_langcode])) {
    bee_message(bt('@lang is not a configured site language.', ['@lang' => $target_langcode]), 'error');
    return;
  }

  $limit = isset($options['limit']) ? (int) $options['limit'] : 50;
  $requested_type = $options['type'] ?? NULL;

  $types = $requested_type ? [$requested_type] : array_keys(array_filter(node_type_get_types(), function ($type) {
    return translation_supported_type($type->type);
  }));

  if (empty($types)) {
    bee_message(bt('No content types have translation enabled.'), 'warning');
    return;
  }

  $query = db_select('node', 'n')
    ->fields('n', ['nid'])
    ->condition('n.type', $types, 'IN')
    ->condition('n.langcode', $target_langcode, '!=')
    ->range(0, $limit);
  $nids = $query->execute()->fetchCol();

  if (empty($nids)) {
    bee_message(bt('No candidate nodes found for target language @lang.', ['@lang' => $target_langcode]));
    return;
  }

  $translated = 0;
  $skipped = 0;
  $failed = 0;

  foreach ($nids as $nid) {
    $node = node_load($nid);
    if (empty($node)) {
      continue;
    }

    $tnid = !empty($node->tnid) ? $node->tnid : $node->nid;
    $existing = translation_node_get_translations($tnid);
    if (isset($existing[$target_langcode])) {
      $skipped++;
      continue;
    }

    $result = ai_translate_node($node, $target_langcode);
    if (!empty($result['status'])) {
      $translated++;
      bee_message(bt('Translated @nid: %title', ['@nid' => $nid, '%title' => $node->title]));
    }
    else {
      $failed++;
      bee_message(bt('Skipped @nid: @message', ['@nid' => $nid, '@message' => $result['message']]), 'warning');
    }
  }

  bee_message(bt('Bulk translation complete: @translated translated, @skipped already translated, @failed failed.', [
    '@translated' => $translated,
    '@skipped' => $skipped,
    '@failed' => $failed,
  ]));
}
