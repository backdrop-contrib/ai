<?php

/**
 * @file
 * File reference rewrite helpers for the AI Alt module.
 */

/**
 * Rewrite exact file URLs in referencing entities after a file move.
 *
 * @return int
 *   The number of entities saved.
 */
function _ai_alt_rewrite_referenced_file_urls($file, $old_uri, $new_uri) {
  $old_url = file_create_url($old_uri);
  $new_url = file_create_url($new_uri);
  if ($old_url === $new_url) {
    return 0;
  }

  $refs = _ai_alt_collect_file_usage_refs($file);
  if (empty($refs)) {
    return 0;
  }

  $visited = [];
  $saved = 0;

  foreach ($refs as $ref) {
    if (!_ai_alt_can_load_entity_type($ref['type'])) {
      continue;
    }

    $entities = entity_load($ref['type'], [$ref['id']]);
    if (empty($entities[$ref['id']])) {
      continue;
    }

    $entity = $entities[$ref['id']];
    if (_ai_alt_rewrite_entity_file_urls($ref['type'], $entity, $old_url, $new_url, $visited)) {
      $saved++;
    }
  }

  return $saved;
}

/**
 * Check whether an entity type can be loaded with entity_load().
 */
function _ai_alt_can_load_entity_type($entity_type) {
  $info = entity_get_info($entity_type);
  return !empty($info);
}

/**
 * Collect entity references for a file from file usage.
 */
function _ai_alt_collect_file_usage_refs($file) {
  $usage = file_usage_list($file);
  $refs = [];

  foreach ($usage as $module => $types) {
    foreach ($types as $type => $ids) {
      foreach ($ids as $id => $count) {
        $refs[] = [
          'module' => $module,
          'type' => $type,
          'id' => (int) $id,
        ];
      }
    }
  }

  usort($refs, function ($a, $b) {
    return strcmp($a['module'] . ':' . $a['type'] . ':' . $a['id'], $b['module'] . ':' . $b['type'] . ':' . $b['id']);
  });

  return $refs;
}

/**
 * Recursively rewrite exact file URLs within an entity tree.
 */
function _ai_alt_rewrite_entity_file_urls($entity_type, &$entity, $old_url, $new_url, array &$visited) {
  list($entity_id, $revision_id) = entity_extract_ids($entity_type, $entity);
  $visit_key = $entity_type . ':' . (string) $entity_id . ':' . (string) $revision_id;
  if (isset($visited[$visit_key])) {
    return FALSE;
  }
  $visited[$visit_key] = TRUE;

  $entity_info = entity_get_info($entity_type);
  if (empty($entity_info)) {
    return FALSE;
  }

  $bundle_key = !empty($entity_info['entity keys']['bundle']) ? $entity_info['entity keys']['bundle'] : NULL;
  $bundle = $bundle_key && isset($entity->{$bundle_key}) ? $entity->{$bundle_key} : $entity_type;
  $instances = field_info_instances($entity_type, $bundle);
  $changed = FALSE;

  foreach ($instances as $field_name => $instance) {
    $field_info = field_info_field($field_name);
    if (empty($entity->{$field_name}) || !is_array($entity->{$field_name})) {
      continue;
    }

    foreach ($entity->{$field_name} as $lang => $items) {
      if (empty($items) || !is_array($items)) {
        continue;
      }

      switch ($field_info['type']) {
        case 'text':
        case 'text_long':
        case 'text_with_summary':
          foreach ($items as $delta => $item) {
            $item_changed = FALSE;
            if (!empty($item['value']) && strpos($item['value'], $old_url) !== FALSE) {
              $entity->{$field_name}[$lang][$delta]['value'] = str_replace($old_url, $new_url, $item['value']);
              $item_changed = TRUE;
            }
            if (isset($item['summary']) && !empty($item['summary']) && strpos($item['summary'], $old_url) !== FALSE) {
              $entity->{$field_name}[$lang][$delta]['summary'] = str_replace($old_url, $new_url, $item['summary']);
              $item_changed = TRUE;
            }
            $changed = $changed || $item_changed;
          }
          break;

        case 'paragraphs':
          foreach ($items as $item) {
            if (empty($item['value'])) {
              continue;
            }
            $pid = (int) $item['value'];
            $vid = !empty($item['revision_id']) ? (int) $item['revision_id'] : NULL;

            $child = NULL;
            if ($vid) {
              $controller = entity_get_controller('paragraphs_item');
              if (method_exists($controller, 'loadRevision')) {
                $child = $controller->loadRevision($vid);
              }
            }
            if (!$child) {
              $loaded = entity_load('paragraphs_item', [$pid]);
              $child = isset($loaded[$pid]) ? $loaded[$pid] : NULL;
            }
            if (!$child) {
              continue;
            }
            if (_ai_alt_rewrite_entity_file_urls('paragraphs_item', $child, $old_url, $new_url, $visited)) {
              $changed = TRUE;
            }
          }
          break;
      }
    }
  }

  if ($changed) {
    entity_save($entity_type, $entity);
  }

  return $changed;
}

/**
 * Create a redirect from an old file URL to a new file URL.
 */
function _ai_alt_create_file_redirect($old_uri, $new_uri) {
  if (!module_exists('redirect')) {
    return FALSE;
  }

  module_load_include('module', 'redirect');
  $old_url = file_create_url($old_uri);
  $new_url = file_create_url($new_uri);
  $old_parsed = redirect_parse_url($old_url);
  $new_parsed = redirect_parse_url($new_url);
  if (empty($old_parsed['url']) || empty($new_parsed['url']) || $old_parsed['url'] === $new_parsed['url']) {
    return FALSE;
  }

  $redirect = new Redirect([
    'source' => $old_parsed['url'],
    'redirect' => $new_parsed['url'],
    'status_code' => 301,
  ]);
  try {
    redirect_save($redirect);
  }
  catch (\Exception $e) {
    watchdog('ai_alt', 'Failed to create redirect from @old to @new: @error', [
      '@old' => $old_parsed['url'],
      '@new' => $new_parsed['url'],
      '@error' => $e->getMessage(),
    ], WATCHDOG_WARNING);
    return FALSE;
  }

  return TRUE;
}
