<?php

/**
 * @file
 * Request/response mapping for native decision endpoints.
 */

/**
 * Converts decide() question envelopes to and from typed-question requests.
 *
 * Used by adapters whose provider evaluates decisions natively (TypeSafe AI,
 * and Ollama models with the "decision" capability). Adapters without a
 * native endpoint keep using the emulation in AIAdapterBase::decide().
 */
class AIDecisionHelper {

  /**
   * Build the typed-question map used by native decision endpoints.
   *
   * TypeSafe AI and Ollama's /v1/systemone take a state plus named questions,
   * each typed "noul", "choice" or "score" with instructions and criteria.
   * Choice questions take 'options' (a list, or option => description).
   * Score questions take optional 'levels' (2-10 descriptions, low to high);
   * without them a five-level scale is used and the answer is scaled to
   * 0-100, as emulated decisions return.
   *
   * @return array
   *   [0] Questions keyed by ID. [1] Per-question metadata for
   *   parseAnswers(), in question order.
   */
  public static function buildQuestions(array $questions): array {
    $map = [];
    $meta = [];
    foreach (array_values($questions) as $idx => $q) {
      $id = isset($q['id']) && $q['id'] !== '' ? (string) $q['id'] : 'q' . $idx;
      $type = isset($q['type']) ? (string) $q['type'] : 'boolean';
      $prompt = trim((string) ($q['prompt'] ?? $q['question'] ?? ''));
      $item = ['instructions' => $prompt];
      $info = ['id' => $id, 'type' => $type];

      if ($type === 'choice') {
        $criteria = [];
        foreach ((array) ($q['options'] ?? []) as $key => $value) {
          $criteria[is_int($key) ? (string) $value : (string) $key] = (string) $value;
        }
        if (count($criteria) < 2) {
          throw new \InvalidArgumentException(sprintf('Choice question "%s" needs at least two options.', $id));
        }
        $item += ['type' => 'choice', 'criteria' => $criteria];
      }
      elseif ($type === 'score') {
        $levels = array_values(array_map('strval', (array) ($q['levels'] ?? [])));
        $info['scaled'] = !$levels;
        $levels = $levels ?: ['None', 'Low', 'Moderate', 'High', 'Severe'];
        if (count($levels) < 2 || count($levels) > 10) {
          throw new \InvalidArgumentException(sprintf('Score question "%s" needs between 2 and 10 levels.', $id));
        }
        $info['levels'] = $levels;
        $item += ['type' => 'score', 'criteria' => $levels];
      }
      else {
        $info['type'] = 'boolean';
        $item += [
          'type' => 'noul',
          'criteria' => ['true' => 'Yes. ' . $prompt, 'false' => 'No. The opposite is true.'],
        ];
      }

      $map[$id] = $item;
      $meta[] = $info;
    }
    return [$map, $meta];
  }

  /**
   * Convert a native decision response into normalized decisions.
   *
   * @param array $response
   *   Decoded response with an "answers" map keyed by question ID.
   * @param array $meta
   *   Metadata from buildQuestions().
   *
   * @return array
   *   Decisions in question order: id, type, answer, probability (of the
   *   returned answer), probabilities and, when supplied, confidence.
   *
   * @throws \RuntimeException
   *   When an answer is missing, so callers can fall back.
   */
  public static function parseAnswers(array $response, array $meta): array {
    $unit = function ($value): float {
      return is_numeric($value) && is_finite((float) $value) ? max(0.0, min(1.0, (float) $value)) : 0.0;
    };
    $answers = (array) ($response['answers'] ?? []);
    $decisions = [];
    foreach ($meta as $info) {
      $raw = $answers[$info['id']] ?? NULL;
      if (!is_array($raw)) {
        throw new \RuntimeException(sprintf('The decision response has no answer for "%s".', $info['id']));
      }

      if ($info['type'] === 'boolean') {
        $p_true = $unit($raw['noul'] ?? NULL);
        $answer = $p_true >= 0.5;
        $decisions[] = [
          'id' => $info['id'],
          'type' => 'boolean',
          'answer' => $answer,
          'probability' => $answer ? $p_true : 1.0 - $p_true,
          'probabilities' => ['true' => $p_true, 'false' => 1.0 - $p_true],
        ];
      }
      elseif ($info['type'] === 'choice') {
        $choice = (string) ($raw['choice'] ?? '');
        $probabilities = array_map($unit, (array) ($raw['probabilities'] ?? []));
        $decisions[] = [
          'id' => $info['id'],
          'type' => 'choice',
          'answer' => $choice,
          'probability' => $probabilities[$choice] ?? 0.0,
          'probabilities' => $probabilities,
          'confidence' => $unit($raw['confidence'] ?? NULL),
        ];
      }
      else {
        $score = $raw['score'] ?? NULL;
        if (!is_numeric($score) || !is_finite((float) $score)) {
          throw new \RuntimeException(sprintf('The decision response has no score for "%s".', $info['id']));
        }
        $levels = $info['levels'];
        // Key the distribution by level description rather than index.
        $probabilities = [];
        foreach ((array) ($raw['probabilities'] ?? []) as $index => $p) {
          $probabilities[$levels[(int) $index] ?? (string) $index] = $unit($p);
        }
        $decisions[] = [
          'id' => $info['id'],
          'type' => 'score',
          'answer' => $info['scaled'] ? round((float) $score / (count($levels) - 1) * 100, 2) : (float) $score,
          'probability' => $probabilities ? max($probabilities) : 0.0,
          'probabilities' => $probabilities,
          'confidence' => $unit($raw['confidence'] ?? NULL),
        ];
      }
    }
    return $decisions;
  }

}
