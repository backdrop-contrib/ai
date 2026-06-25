<?php

/**
 * @file
 * Utility class for preparing strings for AI prompts.
 */
class AIStringHelper {

  public static function prepareText($text, array $remove_html_elements = [], $max_length = 10000) {
    $remove_html_elements = array_merge(['pre', 'code', 'script', 'iframe'], $remove_html_elements);
    $wrapped = '<div>' . $text . '</div>';

    $dom = new DOMDocument();
    // The XML prolog forces UTF-8 parsing; converting to HTML-ENTITIES via
    // mb_convert_encoding is deprecated since PHP 8.2.
    @$dom->loadHTML('<?xml encoding="UTF-8">' . $wrapped, LIBXML_HTML_NOIMPLIED | LIBXML_HTML_NODEFDTD);
    foreach ($remove_html_elements as $element) {
      // getElementsByTagName returns a live list — removing while iterating
      // forward skips every other node, so walk it backwards.
      $nodes = $dom->getElementsByTagName($element);
      for ($i = $nodes->length - 1; $i >= 0; $i--) {
        $node = $nodes->item($i);
        $node->parentNode->removeChild($node);
      }
    }
    $wrapper = $dom->getElementsByTagName('div')->item(0);
    $text = $wrapper ? $dom->saveHTML($wrapper) : $dom->saveHTML();
    $text = html_entity_decode(strip_tags($text));
    $text = preg_replace('/\s+/', ' ', $text);
    $text = preg_replace('/[^\w.?!,%\x27\x22\x20]/u', '', $text);
    if (strlen($text) > $max_length) {
      $text = substr($text, 0, $max_length);
    }
    return trim($text);
  }

}
