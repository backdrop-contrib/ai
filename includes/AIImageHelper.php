<?php

/**
 * @file
 * Utility class for image generation requests and responses.
 *
 * Providers return generated images in different shapes (base64, a remote
 * URL, a data: URL, or a URL buried in a chat-style payload) and accept image
 * dimensions in different forms (pixel sizes, aspect ratios). Callers use this
 * class so they can work with any image-capable provider.
 */
class AIImageHelper {

  /**
   * Largest generated image accepted, in bytes.
   */
  const MAX_BYTES = 20971520;

  /**
   * Aspect ratios image APIs commonly accept (Gemini, OpenRouter).
   */
  const ASPECT_RATIOS = ['1:1', '2:3', '3:2', '3:4', '4:3', '4:5', '5:4', '9:16', '16:9', '21:9'];

  /**
   * Turn an images() result into validated image bytes.
   *
   * @param mixed $result
   *   The value returned by AIApi::images().
   *
   * @return array
   *   An array with 'data' (binary image) and 'ext' (jpg, png or webp).
   *
   * @throws \RuntimeException
   *   When the response holds no usable image.
   */
  public static function extract($result): array {
    $item = is_array($result) && isset($result['data'][0]) && is_array($result['data'][0]) ? $result['data'][0] : [];

    if (!empty($item['b64_json'])) {
      $data = base64_decode($item['b64_json'], TRUE);
      if ($data === FALSE) {
        throw new \RuntimeException('The provider returned base64 image data that could not be decoded.');
      }
    }
    else {
      $url = !empty($item['url']) && is_string($item['url']) ? $item['url'] : static::findUrl($result);
      if (empty($url)) {
        throw new \RuntimeException('The provider did not return an image.');
      }
      if (strpos($url, 'data:image/') === 0) {
        $data = static::parseDataUrl($url);
        if ($data === NULL) {
          throw new \RuntimeException('Could not parse the image data URL returned by the provider.');
        }
      }
      else {
        $data = static::download($url);
        if ($data === FALSE) {
          throw new \RuntimeException('Failed to download the generated image from the returned URL.');
        }
      }
    }

    $validated = static::validate($data);
    if (!$validated) {
      throw new \RuntimeException('The provider returned data that is not a supported image.');
    }
    return $validated;
  }

  /**
   * Validate image bytes and return a safe extension.
   *
   * @return array|false
   *   An array with 'data' and 'ext', or FALSE for anything other than a
   *   JPEG, PNG or WebP image within the size limit.
   */
  public static function validate($data) {
    if (!is_string($data) || $data === '' || strlen($data) > static::MAX_BYTES || !class_exists('finfo')) {
      return FALSE;
    }
    $extensions = [
      'image/jpeg' => 'jpg',
      'image/png' => 'png',
      'image/webp' => 'webp',
    ];
    $mime_type = (new finfo(FILEINFO_MIME_TYPE))->buffer($data);
    return isset($extensions[$mime_type]) ? ['data' => $data, 'ext' => $extensions[$mime_type]] : FALSE;
  }

  /**
   * Download an image only from a public HTTPS host, within the size limit.
   *
   * @return string|false
   *   The response body, or FALSE.
   */
  public static function download($url) {
    $host = is_string($url) ? parse_url($url, PHP_URL_HOST) : FALSE;
    if (!is_string($url) || parse_url($url, PHP_URL_SCHEME) !== 'https' || !$host) {
      return FALSE;
    }
    // Refuse private and reserved addresses so a provider response cannot
    // point the site at internal services.
    $addresses = filter_var($host, FILTER_VALIDATE_IP) ? [$host] : @gethostbynamel($host);
    if (empty($addresses)) {
      return FALSE;
    }
    foreach ($addresses as $address) {
      if (!filter_var($address, FILTER_VALIDATE_IP, FILTER_FLAG_NO_PRIV_RANGE | FILTER_FLAG_NO_RES_RANGE)) {
        return FALSE;
      }
    }
    // Connect to the address that was just checked rather than to the host
    // name, so a second DNS lookup cannot return a different (internal)
    // address. The host name is still sent for SNI, certificate verification
    // and the Host header.
    $parts = parse_url($url);
    $port = isset($parts['port']) ? ':' . $parts['port'] : '';
    $pinned_url = 'https://' . $addresses[0] . $port . ($parts['path'] ?? '/')
      . (isset($parts['query']) ? '?' . $parts['query'] : '');
    $context = stream_context_create([
      'http' => [
        'timeout' => 15,
        'follow_location' => 0,
        'max_redirects' => 0,
        'header' => 'Host: ' . $host . $port,
      ],
      'ssl' => [
        'verify_peer' => TRUE,
        'verify_peer_name' => TRUE,
        'peer_name' => $host,
      ],
    ]);
    $handle = @fopen($pinned_url, 'rb', FALSE, $context);
    if (!$handle) {
      return FALSE;
    }
    $data = '';
    while (!feof($handle)) {
      $chunk = fread($handle, 8192);
      if ($chunk === FALSE || strlen($data .= $chunk) > static::MAX_BYTES) {
        fclose($handle);
        return FALSE;
      }
    }
    fclose($handle);
    return $data !== '' ? $data : FALSE;
  }

  /**
   * Load an image a caller wants to send to a model, refusing unsafe sources.
   *
   * Accepted sources:
   * - data:image/...;base64 URLs.
   * - Backdrop stream URIs (public://, private://, any registered scheme).
   * - URLs of this site's own files, read from their stream wrapper.
   * - Other https URLs on public hosts, via download().
   *
   * Raw filesystem paths and other schemes (file://, php://, phar://, plain
   * http) are refused, so a message can never make the server read local
   * files such as settings.php or fetch internal addresses.
   *
   * @param string $source
   *   The image reference from a chat message or caller.
   *
   * @return array
   *   An array with 'data' (binary image) and 'mime' (image/png, image/jpeg,
   *   image/gif or image/webp).
   *
   * @throws \InvalidArgumentException
   *   When the source is refused, unreadable or not a supported image.
   */
  public static function loadInputImage(string $source): array {
    if (strpos($source, 'data:') === 0) {
      $data = static::parseDataUrl($source);
    }
    else {
      $uri = static::siteFileUri($source);
      if ($uri !== NULL) {
        $data = @file_get_contents($uri, FALSE, NULL, 0, static::MAX_BYTES + 1);
      }
      elseif (parse_url($source, PHP_URL_SCHEME) === 'https') {
        $data = static::download($source);
      }
      else {
        throw new \InvalidArgumentException('Image sources must be a data URL, a site file or an https URL.');
      }
    }

    $mime_types = ['image/png', 'image/jpeg', 'image/gif', 'image/webp'];
    // Sniff the bytes rather than trusting the extension: image style
    // derivatives that change format keep the source file's extension.
    $mime = is_string($data) && $data !== '' && class_exists('finfo')
      ? (new finfo(FILEINFO_MIME_TYPE))->buffer($data)
      : NULL;
    if (!in_array($mime, $mime_types, TRUE) || strlen($data) > static::MAX_BYTES) {
      throw new \InvalidArgumentException('The image source is unreadable or not a supported image.');
    }
    return ['data' => $data, 'mime' => $mime];
  }

  /**
   * Map a stream URI or a URL of this site's files to a stream URI.
   *
   * @param string $source
   *   A stream URI (public://...) or an absolute URL.
   *
   * @return string|null
   *   A stream URI under a registered Backdrop scheme, or NULL when the
   *   source is not one of this site's files.
   */
  protected static function siteFileUri(string $source): ?string {
    $scheme = file_uri_scheme($source);
    if ($scheme && $scheme !== 'http' && $scheme !== 'https') {
      $uri = $source;
    }
    else {
      $uri = NULL;
      foreach (array_keys(file_get_stream_wrappers(STREAM_WRAPPERS_VISIBLE)) as $wrapper_scheme) {
        $base = file_create_url($wrapper_scheme . '://');
        if ($base && strpos($source, $base) === 0) {
          $target = rawurldecode((string) parse_url(substr($source, strlen($base)), PHP_URL_PATH));
          $uri = $wrapper_scheme . '://' . $target;
          break;
        }
      }
      if ($uri === NULL) {
        return NULL;
      }
    }
    // Only Backdrop-managed schemes; never php://, phar://, file:// or data:.
    if (!file_stream_wrapper_valid_scheme(file_uri_scheme($uri))) {
      throw new \InvalidArgumentException('Image sources must use a Backdrop file scheme.');
    }
    // Refuse traversal out of the scheme's directory.
    if (preg_match('#(^|/)\.\.(/|$)#', file_uri_target($uri))) {
      throw new \InvalidArgumentException('Image sources must not contain "..".');
    }
    return $uri;
  }

  /**
   * Decode a data:image/... URL.
   *
   * @return string|null
   *   The binary image, or NULL when the URL is malformed.
   */
  public static function parseDataUrl($data_url) {
    if (!is_string($data_url) || strpos($data_url, 'data:image/') !== 0) {
      return NULL;
    }
    $comma = strpos($data_url, ',');
    if ($comma === FALSE || strpos(substr($data_url, 0, $comma), ';base64') === FALSE) {
      return NULL;
    }
    $binary = base64_decode(substr($data_url, $comma + 1), TRUE);
    return $binary === FALSE ? NULL : $binary;
  }

  /**
   * Search a response for the first image URL or data URL.
   */
  public static function findUrl($data) {
    if (is_string($data)) {
      $data = trim($data);
      if (strpos($data, 'data:image/') === 0) {
        return $data;
      }
      if (parse_url($data, PHP_URL_SCHEME) === 'https' && filter_var($data, FILTER_VALIDATE_URL)) {
        return $data;
      }
      if (preg_match('/https:\/\/[^\s\]\"]+\.(png|jpg|jpeg|webp)/i', $data, $m)) {
        return $m[0];
      }
      return NULL;
    }
    if (is_array($data)) {
      foreach ($data as $item) {
        $found = static::findUrl($item);
        if (!empty($found)) {
          return $found;
        }
      }
    }
    return NULL;
  }

  /**
   * Describe a response's structure for logging, without its contents.
   */
  public static function describe($data) {
    if (is_array($data)) {
      $parts = [];
      foreach ($data as $key => $value) {
        $parts[] = $key . ':' . (is_array($value) ? 'array(' . count($value) . ')' : gettype($value));
      }
      return '{' . implode(', ', array_slice($parts, 0, 10)) . '}';
    }
    return is_string($data) ? 'string length=' . strlen($data) : gettype($data);
  }

  /**
   * Parse a requested size into a width/height ratio.
   *
   * @param string $size
   *   A pixel size ("1536x1024") or an aspect ratio ("3:2").
   *
   * @return float|null
   *   Width divided by height, or NULL when the value is not understood.
   */
  public static function ratio(string $size): ?float {
    if (preg_match('/^\s*(\d+(?:\.\d+)?)\s*[x:]\s*(\d+(?:\.\d+)?)\s*$/i', $size, $m) && (float) $m[2] > 0) {
      return (float) $m[1] / (float) $m[2];
    }
    return NULL;
  }

  /**
   * Map a requested size to the closest supported aspect ratio.
   *
   * @param string $size
   *   A pixel size or an aspect ratio.
   * @param array $allowed
   *   Aspect ratios the target API accepts.
   *
   * @return string|null
   *   The closest allowed ratio, or NULL when $size is not understood.
   */
  public static function aspectRatio(string $size, array $allowed = self::ASPECT_RATIOS): ?string {
    $ratio = static::ratio($size);
    if ($ratio === NULL) {
      return NULL;
    }
    $best = NULL;
    $best_distance = INF;
    foreach ($allowed as $candidate) {
      // Compare on a log scale so 2:1 and 1:2 are equally far from 1:1.
      $distance = abs(log($ratio) - log(static::ratio($candidate)));
      if ($distance < $best_distance) {
        $best = $candidate;
        $best_distance = $distance;
      }
    }
    return $best;
  }

  /**
   * Map a requested size to the closest pixel size from a fixed list.
   *
   * For APIs that only accept specific sizes, such as OpenAI image models.
   * A value already in the list is returned unchanged.
   *
   * @param string $size
   *   A pixel size or an aspect ratio.
   * @param array $allowed
   *   Pixel sizes the target API accepts, e.g. ['1024x1024', '1536x1024'].
   *
   * @return string
   *   The closest allowed size, or the first allowed size when $size is not
   *   understood.
   */
  public static function closestSize(string $size, array $allowed): string {
    if (in_array($size, $allowed, TRUE)) {
      return $size;
    }
    $ratio = static::ratio($size);
    if ($ratio === NULL) {
      return reset($allowed);
    }
    $best = reset($allowed);
    $best_distance = INF;
    foreach ($allowed as $candidate) {
      $distance = abs(log($ratio) - log(static::ratio($candidate)));
      if ($distance < $best_distance) {
        $best = $candidate;
        $best_distance = $distance;
      }
    }
    return $best;
  }

  /**
   * Map a requested size to one an OpenAI (or Azure OpenAI) model accepts.
   *
   * @param string $model
   *   The model ID, or an Azure deployment alias named after the model.
   * @param string $size
   *   A pixel size or an aspect ratio.
   */
  public static function openAiSize(string $model, string $size): string {
    if (stripos($model, 'dall-e-2') !== FALSE) {
      return static::closestSize($size, ['1024x1024', '512x512', '256x256']);
    }
    if (stripos($model, 'dall-e-3') !== FALSE) {
      return static::closestSize($size, ['1024x1024', '1792x1024', '1024x1792']);
    }
    // gpt-image models, and the default for models not known here.
    return static::closestSize($size, ['1024x1024', '1536x1024', '1024x1536']);
  }

}
