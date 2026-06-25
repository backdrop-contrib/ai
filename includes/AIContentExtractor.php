<?php

/**
 * @file
 * Shared helpers for extracting AI-ready text from entities and form values.
 */
class AIContentExtractor {

  public static function defaultSkipKeys() {
    return [
      'format' => TRUE,
      'summary_format' => TRUE,
      'weight' => TRUE,
      'delta' => TRUE,
      'item_id' => TRUE,
      'revision_id' => TRUE,
      'bundle' => TRUE,
      'actions' => TRUE,
      'add_more' => TRUE,
      'remove' => TRUE,
      'open' => TRUE,
      '_weight' => TRUE,
      'fid' => TRUE,
      'uid' => TRUE,
      'nid' => TRUE,
      'vid' => TRUE,
      'tid' => TRUE,
      'target_id' => TRUE,
      'form_id' => TRUE,
      'form_build_id' => TRUE,
      'form_token' => TRUE,
      'op' => TRUE,
      'metatags' => TRUE,
      'path' => TRUE,
      'menu' => TRUE,
      'revision' => TRUE,
      'revision_log' => TRUE,
      'revision_log_message' => TRUE,
    ];
  }

  public static function defaultSkipFieldTypes() {
    return [
      'file' => TRUE,
      'image' => TRUE,
      'media' => TRUE,
    ];
  }

  public static function defaultSkipStrings() {
    return [
      'Upload' => TRUE,
      'Remove' => TRUE,
      'Select existing file' => TRUE,
      'Generate with AI' => TRUE,
      'Generate Alt Text' => TRUE,
      'Crop' => TRUE,
      'Collapse' => TRUE,
    ];
  }

  public static function extractEntityText($entity_type, $entity, array $options = []) {
    if (!is_object($entity) || empty($entity_type)) {
      return '';
    }

    $view_mode = isset($options['view_mode']) ? $options['view_mode'] : 'full';
    $include_label = array_key_exists('include_label', $options) ? (bool) $options['include_label'] : TRUE;

    $preview_property = isset($options['preview_property']) ? $options['preview_property'] : '';
    if ($preview_property !== '' && !empty($entity->{$preview_property}) && is_string($entity->{$preview_property})) {
      $preview = trim(self::htmlToMarkdown($entity->{$preview_property}));
      if ($preview !== '') {
        return self::limitText($preview, $options);
      }
    }

    $entity_id = function_exists('entity_id') ? entity_id($entity_type, $entity) : NULL;
    if ($entity_id) {
      $text = self::renderEntityToMarkdown($entity_type, $entity, $view_mode);
      if ($text !== '') {
        return self::limitText($text, $options);
      }
    }

    $chunks = [];
    if ($include_label && function_exists('entity_label')) {
      $label = entity_label($entity_type, $entity);
      if (is_string($label)) {
        $label = trim(strip_tags($label));
        if ($label !== '') {
          $chunks[] = $label;
        }
      }
    }

    $visited_paragraph_ids = [];
    self::collectEntityFieldText($entity_type, $entity, $chunks, $visited_paragraph_ids, $options);
    return self::limitText(self::joinChunks($chunks), $options);
  }

  public static function renderEntityToMarkdown($entity_type, $entity, $view_mode = 'full') {
    if (!function_exists('entity_view') || !function_exists('backdrop_render')) {
      return '';
    }

    // Render as anonymous user so admin-only elements (Edit, Delete, operations)
    // are excluded from the output, producing cleaner embedding text. The
    // finally block guarantees the original user is restored even when the
    // render throws a PHP Error — otherwise the rest of the request would
    // keep running as anonymous.
    global $user;
    $original_user = $user;
    $user = backdrop_anonymous_user();

    try {
      $entity_id = function_exists('entity_id') ? entity_id($entity_type, $entity) : NULL;
      $entities = $entity_id ? [$entity_id => $entity] : [$entity];
      $build = entity_view_multiple($entity_type, $entities, $view_mode);
      self::stripRenderNoise($build);
      $html = backdrop_render($build);
    }
    catch (\Throwable $e) {
      return '';
    }
    finally {
      $user = $original_user;
    }

    return !empty($html) ? self::htmlToMarkdown($html) : '';
  }

  public static function htmlToMarkdown($html) {
    if (empty($html) || !is_string($html)) {
      return '';
    }
    $html = preg_replace('/<figure\b[^>]*>.*?<\/figure>/is', '', $html);
    $html = preg_replace('/<picture\b[^>]*>.*?<\/picture>/is', '', $html);
    $html = preg_replace('/<img\b[^>]*>/i', '', $html);
    $html = preg_replace('/<audio\b[^>]*>.*?<\/audio>/is', '', $html);
    $html = preg_replace('/<video\b[^>]*>.*?<\/video>/is', '', $html);
    $html = preg_replace('/<(script|style|nav|noscript)\b[^>]*>.*?<\/\1>/is', '', $html);
    // Strip operation/admin link lists (Edit, Delete, etc.) by CSS class.
    $html = preg_replace('/<ul[^>]*class="[^"]*\b(?:links|operations|action-links|local-tasks|tabs|node-links)\b[^"]*"[^>]*>.*?<\/ul>/is', '', $html);
    $html = preg_replace('/<div[^>]*class="[^"]*\b(?:links|operations|action-links|local-tasks)\b[^"]*"[^>]*>.*?<\/div>/is', '', $html);

    for ($i = 6; $i >= 1; $i--) {
      $hashes = str_repeat('#', $i);
      $html = preg_replace_callback('/<h' . $i . '\b[^>]*>(.*?)<\/h' . $i . '>/is', function ($m) use ($hashes) {
        $text = trim(strip_tags($m[1]));
        return $text !== '' ? "\n\n{$hashes} {$text}\n\n" : '';
      }, $html);
    }

    $html = preg_replace('/<(strong|b)\b[^>]*>(.*?)<\/\1>/is', '**$2**', $html);
    $html = preg_replace('/<(em|i)\b[^>]*>(.*?)<\/\1>/is', '*$2*', $html);
    $html = preg_replace('/<code\b[^>]*>(.*?)<\/code>/is', '`$1`', $html);
    $html = preg_replace_callback('/<pre\b[^>]*>(.*?)<\/pre>/is', function ($m) {
      $code = strip_tags($m[1]);
      $code = html_entity_decode($code, ENT_QUOTES | ENT_HTML5, 'UTF-8');
      return "\n\n```\n" . trim($code) . "\n```\n\n";
    }, $html);
    $html = preg_replace_callback('/<blockquote\b[^>]*>(.*?)<\/blockquote>/is', function ($m) {
      $inner = trim(strip_tags($m[1]));
      $lines = explode("\n", $inner);
      return "\n\n" . implode("\n", array_map(function ($l) { return '> ' . $l; }, $lines)) . "\n\n";
    }, $html);
    $html = preg_replace_callback('/<ul\b[^>]*>(.*?)<\/ul>/is', function ($m) {
      preg_match_all('/<li\b[^>]*>(.*?)<\/li>/is', $m[1], $items);
      $lines = [];
      foreach ($items[1] as $item) {
        $text = trim(strip_tags($item));
        if ($text !== '') {
          $lines[] = '- ' . $text;
        }
      }
      return $lines ? "\n\n" . implode("\n", $lines) . "\n\n" : '';
    }, $html);
    $html = preg_replace_callback('/<ol\b[^>]*>(.*?)<\/ol>/is', function ($m) {
      preg_match_all('/<li\b[^>]*>(.*?)<\/li>/is', $m[1], $items);
      $lines = [];
      $n = 1;
      foreach ($items[1] as $item) {
        $text = trim(strip_tags($item));
        if ($text !== '') {
          $lines[] = $n++ . '. ' . $text;
        }
      }
      return $lines ? "\n\n" . implode("\n", $lines) . "\n\n" : '';
    }, $html);
    $html = preg_replace('/<a\b[^>]*>(.*?)<\/a>/is', '$1', $html);
    $html = preg_replace('/<hr\b[^>]*>/i', "\n\n---\n\n", $html);
    $html = preg_replace('/<\/p>/i', "\n\n", $html);
    $html = preg_replace('/<p\b[^>]*>/i', '', $html);
    $html = preg_replace('/<\/(div|section|article|header|footer|main|aside)>/i', "\n\n", $html);
    $html = preg_replace('/<(div|section|article|header|footer|main|aside)\b[^>]*>/i', '', $html);
    $html = preg_replace('/<br\s*\/?>/i', "\n", $html);

    $markdown = strip_tags($html);
    $markdown = html_entity_decode($markdown, ENT_QUOTES | ENT_HTML5, 'UTF-8');
    $lines = explode("\n", $markdown);
    $lines = array_map(function ($l) { return rtrim(preg_replace('/[ \t]+/', ' ', $l)); }, $lines);
    $markdown = implode("\n", $lines);
    $markdown = preg_replace('/\n{3,}/', "\n\n", $markdown);
    return trim($markdown);
  }

  public static function extractNodeFormValuesText(array $values, $bundle, array $options = []) {
    if (empty($bundle) || empty($values) || !function_exists('field_info_instances')) {
      return '';
    }

    $chunks = [];
    $include_title = array_key_exists('include_title', $options) ? (bool) $options['include_title'] : TRUE;
    if ($include_title && !empty($values['title']) && is_scalar($values['title'])) {
      $title = trim(strip_tags((string) $values['title']));
      if ($title !== '') {
        $chunks[] = $title;
      }
    }

    $instances = field_info_instances('node', $bundle);
    $skip_field_types = self::defaultSkipFieldTypes();
    if (!empty($options['skip_field_types']) && is_array($options['skip_field_types'])) {
      $skip_field_types = $options['skip_field_types'] + $skip_field_types;
    }
    $exclude_fields = !empty($options['exclude_fields']) && is_array($options['exclude_fields'])
      ? array_fill_keys($options['exclude_fields'], TRUE)
      : [];
    foreach ($instances as $field_name => $instance) {
      if (in_array($field_name, ['revision_log', 'revision_log_message'], TRUE)) {
        continue;
      }
      if (isset($exclude_fields[$field_name])) {
        continue;
      }
      if (empty($values[$field_name])) {
        continue;
      }
      $field_info = function_exists('field_info_field') ? field_info_field($field_name) : [];
      $field_type = isset($field_info['type']) ? $field_info['type'] : '';
      if ($field_type !== '' && isset($skip_field_types[$field_type])) {
        continue;
      }
      self::collectTextRecursive($values[$field_name], $chunks, $options);
    }

    return self::limitText(self::joinChunks($chunks), $options);
  }

  public static function collectEntityFieldText($entity_type, $entity, array &$chunks, array &$visited_paragraph_ids, array $options = []) {
    if (!function_exists('entity_extract_ids') || !function_exists('field_info_instances') || !function_exists('field_info_field')) {
      return;
    }

    [, , $bundle] = entity_extract_ids($entity_type, $entity);
    if (empty($bundle)) {
      return;
    }

    $instances = field_info_instances($entity_type, $bundle);
    $skip_field_types = self::defaultSkipFieldTypes();
    if (!empty($options['skip_field_types']) && is_array($options['skip_field_types'])) {
      $skip_field_types = $options['skip_field_types'] + $skip_field_types;
    }
    $exclude_fields = !empty($options['exclude_fields']) && is_array($options['exclude_fields'])
      ? array_fill_keys($options['exclude_fields'], TRUE)
      : [];
    foreach ($instances as $field_name => $instance) {
      if (in_array($field_name, ['revision_log', 'revision_log_message'], TRUE)) {
        continue;
      }
      if (isset($exclude_fields[$field_name])) {
        continue;
      }
      if (empty($entity->{$field_name}) || !is_array($entity->{$field_name})) {
        continue;
      }

      $field_info = field_info_field($field_name);
      $field_type = isset($field_info['type']) ? $field_info['type'] : '';
      if ($field_type === 'paragraphs') {
        self::collectParagraphItemsText($entity->{$field_name}, $chunks, $visited_paragraph_ids, $options);
        continue;
      }
      if ($field_type !== '' && isset($skip_field_types[$field_type])) {
        continue;
      }

      self::collectTextRecursive($entity->{$field_name}, $chunks, $options);
    }
  }

  public static function collectParagraphItemsText(array $field_items, array &$chunks, array &$visited_paragraph_ids, array $options = []) {
    if (!function_exists('paragraphs_item_load')) {
      self::collectTextRecursive($field_items, $chunks, $options);
      return;
    }

    foreach ($field_items as $lang_items) {
      if (!is_array($lang_items)) {
        continue;
      }
      foreach ($lang_items as $item) {
        if (!is_array($item)) {
          if (is_string($item)) {
            self::collectTextRecursive($item, $chunks, $options);
          }
          continue;
        }

        $paragraph_id = 0;
        foreach (['item_id', 'value', 'target_id'] as $id_key) {
          if (!empty($item[$id_key]) && is_scalar($item[$id_key]) && ctype_digit((string) $item[$id_key])) {
            $paragraph_id = (int) $item[$id_key];
            break;
          }
        }

        if ($paragraph_id) {
          if (isset($visited_paragraph_ids[$paragraph_id])) {
            continue;
          }
          $visited_paragraph_ids[$paragraph_id] = TRUE;

          $paragraph = $item['entity'] ?? NULL;
          if (!$paragraph) {
            $paragraph = paragraphs_item_load($paragraph_id);
          }

          if ($paragraph && is_object($paragraph)) {
            self::collectEntityFieldText('paragraphs_item', $paragraph, $chunks, $visited_paragraph_ids, $options);
          }
          else {
            self::collectTextRecursive($item, $chunks, $options);
          }
        }
        else {
          self::collectTextRecursive($item, $chunks, $options);
        }
      }
    }
  }

  public static function collectTextRecursive($value, array &$chunks, array $options = []) {
    if (is_string($value)) {
      $plain = trim(html_entity_decode(strip_tags($value), ENT_QUOTES | ENT_HTML5, 'UTF-8'));
      $skip_strings = self::defaultSkipStrings();
      if (!empty($options['skip_strings']) && is_array($options['skip_strings'])) {
        $skip_strings = $options['skip_strings'] + $skip_strings;
      }
      if (isset($skip_strings[$plain])) {
        return;
      }
      if ($plain !== '' && empty($options['keep_numeric']) && preg_match('/^\d+$/', $plain)) {
        return;
      }

      if (strpos($value, '<') !== FALSE) {
        $candidate = trim(self::htmlToMarkdown($value));
      }
      else {
        $candidate = trim(html_entity_decode($value, ENT_QUOTES | ENT_HTML5, 'UTF-8'));
      }
      if ($candidate !== '') {
        $chunks[] = $candidate;
      }
      return;
    }

    if (is_object($value) || !is_array($value)) {
      return;
    }

    $skip_keys = self::defaultSkipKeys();
    if (!empty($options['skip_keys']) && is_array($options['skip_keys'])) {
      $skip_keys = $options['skip_keys'] + $skip_keys;
    }

    foreach ($value as $key => $item) {
      if (is_string($key) && ($key === '' || $key[0] === '#')) {
        continue;
      }
      if (is_string($key) && isset($skip_keys[$key])) {
        continue;
      }
      self::collectTextRecursive($item, $chunks, $options);
    }
  }

  protected static function stripRenderNoise(array &$build) {
    $noise_keys = [
      'links', 'comments', '#contextual_links', 'contextual_links',
      'operations', 'actions', 'local_tasks', 'view_mode_switch',
    ];
    foreach ($noise_keys as $key) {
      if (isset($build[$key])) {
        unset($build[$key]);
      }
    }

    if (!function_exists('element_children')) {
      return;
    }
    foreach (element_children($build) as $key) {
      if (preg_match('/^(?:contextual|admin|operations?|local_tasks?|links?)(?:$|_)/i', $key)) {
        unset($build[$key]);
      }
    }

    // Recurse into child elements to strip nested noise (e.g. per-entity links).
    foreach (element_children($build) as $key) {
      if (isset($build[$key]) && is_array($build[$key])) {
        self::stripRenderNoise($build[$key]);
      }
    }
  }

  protected static function joinChunks(array $chunks) {
    $chunks = array_values(array_unique(array_filter(array_map('trim', $chunks))));
    return implode("\n\n", $chunks);
  }

  protected static function limitText($text, array $options) {
    $text = trim((string) $text);
    $max_length = isset($options['max_length']) ? (int) $options['max_length'] : 0;
    if ($max_length <= 0 || $text === '') {
      return $text;
    }
    if (function_exists('mb_strlen') && function_exists('mb_substr')) {
      return mb_strlen($text, 'UTF-8') > $max_length ? mb_substr($text, 0, $max_length, 'UTF-8') : $text;
    }
    return strlen($text) > $max_length ? substr($text, 0, $max_length) : $text;
  }

}
