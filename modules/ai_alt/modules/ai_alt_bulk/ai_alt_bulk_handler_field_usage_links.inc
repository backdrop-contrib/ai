<?php


/**
 * @file
 * Views field handler for usage links.
 */

class ai_alt_bulk_handler_field_usage_links extends views_handler_field {
  function construct() {
    parent::construct();
    $this->additional_fields['fid'] = 'fid';
  }

  function query() {
    $this->add_additional_fields();
  }

  function render($values) {
    $fid = $this->get_value($values, 'fid');
    if (!$fid) {
      return '';
    }

    module_load_include('inc', 'ai_alt_tools', 'includes/alt.tools');
    $excluded_bundles = function_exists('ai_alt_tools_excluded_node_bundles') ? ai_alt_tools_excluded_node_bundles() : [];

    $usages = db_select('file_usage', 'fu')
      ->fields('fu', ['type', 'id', 'module'])
      ->condition('fid', $fid)
      ->execute()
      ->fetchAll();

    $links = [];
    foreach ($usages as $usage) {
      $entity_type = $usage->type;
      $entity_id = $usage->id;
      
      if (!_ai_alt_bulk_can_load_entity_type($entity_type)) {
        continue;
      }

      $entities = entity_load($entity_type, [$entity_id]);
      if (empty($entities[$entity_id])) {
        continue;
      }
      $entity = $entities[$entity_id];

      if ($entity_type == 'node' && !empty($excluded_bundles) && !empty($entity->type) && in_array($entity->type, $excluded_bundles, TRUE)) {
        continue;
      }
      
      // If it's a paragraph, try to find the host node.
      if ($entity_type == 'paragraphs_item' && method_exists($entity, 'hostEntity')) {
        $host = $entity->hostEntity();
        if ($host) {
          $entity = $host;
          $entity_type = $entity->entityType();
          if ($entity_type == 'node' && !empty($excluded_bundles) && !empty($entity->type) && in_array($entity->type, $excluded_bundles, TRUE)) {
            continue;
          }
        }
      }

      $uri = entity_uri($entity_type, $entity);
      if ($uri) {
        $label = entity_label($entity_type, $entity);
        $links[] = l($label, $uri['path']);
      }
    }

    if (empty($links)) {
      return t('No usage found');
    }
    
    // Deduplicate links.
    $links = array_unique($links);

    return implode('<br />', $links);
  }
}
