<?php

/**
 * @file
 * Utility class for preparing strings for AI prompts.
 */
class AIStringHelper {

  public static function prepareText($text, array $remove_html_elements = [], $max_length = 10000) {
    $remove_html_elements = array_merge(['pre', 'code', 'script', 'iframe'], $remove_html_elements);
    $text = '<div>' . $text . '</div>';

    $dom = new DOMDocument();
    @$dom->loadHTML(mb_convert_encoding($text, 'HTML-ENTITIES', 'UTF-8'), LIBXML_HTML_NOIMPLIED | LIBXML_HTML_NODEFDTD);
    foreach ($remove_html_elements as $element) {
      $nodes = $dom->getElementsByTagName($element);
      foreach ($nodes as $node) {
        $node->parentNode->removeChild($node);
      }
    }
    $text = $dom->saveHTML($dom->getElementsByTagName('div')->item(0));
    $text = html_entity_decode(strip_tags($text));
    $text = preg_replace('/\s+/', ' ', $text);
    $text = preg_replace('/[^\w.?!,%\x27\x22\x20]/u', '', $text);
    if (strlen($text) > $max_length) {
      $text = substr($text, 0, $max_length);
    }
    return trim($text);
  }

}
