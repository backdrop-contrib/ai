<?php

/**
 * @file
 * "AI Translate" node tab: target-language picker, submitted through the
 * Batch API so the AI call does not block a raw synchronous form submit.
 */

/**
 * Form builder for node/%node/ai-translate.
 */
function ai_translate_node_form(array $form, array &$form_state, $node) {
  $form['#node'] = $node;

  $tnid = !empty($node->tnid) ? $node->tnid : $node->nid;
  $existing = translation_node_get_translations($tnid);
  $source_langcode = !empty($node->langcode) ? $node->langcode : LANGUAGE_NONE;

  $options = [];
  foreach (language_list(TRUE) as $langcode => $language) {
    if ($langcode === $source_langcode || isset($existing[$langcode])) {
      continue;
    }
    $options[$langcode] = $language->name;
  }

  if (empty($options)) {
    $form['message'] = [
      '#type' => 'item',
      '#markup' => t('This content already has a translation in every configured language.'),
    ];
    return $form;
  }

  $form['target_langcode'] = [
    '#type' => 'select',
    '#title' => t('Translate into'),
    '#options' => $options,
    '#required' => TRUE,
  ];

  $model = function_exists('_ai_translate_get_model') ? _ai_translate_get_model() : config_get('ai_translate.settings', 'model');
  if (empty($model)) {
    $link = l(t('AI Translate settings'), 'admin/config/ai/ai-translate');
    $form['no_model'] = [
      '#type' => 'item',
      '#markup' => t('No AI model is configured or available. An administrator needs to set one under !link before this will work.', ['!link' => $link]),
    ];
    return $form;
  }

  $form['actions'] = ['#type' => 'actions'];
  $form['actions']['submit'] = [
    '#type' => 'submit',
    '#value' => t('Translate with AI'),
  ];

  return $form;
}

/**
 * Submit handler: hand off to the Batch API.
 */
function ai_translate_node_form_submit(array $form, array &$form_state) {
  $node = $form['#node'];
  $target_langcode = $form_state['values']['target_langcode'];

  module_load_include('inc', 'ai_translate', 'includes/ai_translate.batch');
  batch_set(ai_translate_batch_for_node($node->nid, $target_langcode));
  $form_state['redirect'] = 'node/' . $node->nid;
}
