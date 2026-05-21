<?php


/**
 * @file
 * Views integration for ai_alt_bulk module.
 */

/**
 * Implements hook_views_data_alter().
 */
function ai_alt_bulk_views_data_alter(&$data) {
  $data['file_managed']['usage_links'] = [
    'title' => t('Usage Links'),
    'help' => t('Links to the entities that use this file.'),
    'field' => [
      'handler' => 'ai_alt_bulk_handler_field_usage_links',
    ],
  ];
}
