<?php

/**
 * @file
 * Example of how to override model capabilities for Ollama/OpenRouter models.
 *
 * Since Ollama and OpenRouter don't provide reliable capability metadata via
 * their APIs, site administrators can use this hook to manually tag models
 * with capabilities.
 *
 * Place this code in a custom module (e.g., mysite_ai_overrides.module).
 */

/**
 * Implements hook_ai_model_capabilities_alter().
 *
 * Allow site-specific model capability overrides.
 *
 * @param array &$filtered
 *   Array of model IDs that were filtered for the capability.
 * @param string &$capability
 *   The capability being queried ('text', 'vision', 'image', 'embeddings').
 * @param string &$provider_id
 *   The provider ID ('ollama', 'openrouter', etc.).
 */
function mysite_ai_overrides_ai_model_capabilities_alter(&$filtered, &$capability, &$provider_id) {
  // Example 1: Add a vision model that wasn't detected
  if ($provider_id === 'ollama' && $capability === 'vision') {
    // If you have a vision model that isn't being detected, add it here
    $filtered['llava3:latest'] = 'llava3:latest';
    $filtered['minicpm-v:latest'] = 'minicpm-v:latest';
  }

  // Example 2: Remove a model that was incorrectly detected
  if ($provider_id === 'ollama' && $capability === 'vision') {
    // If a model is incorrectly showing up as a vision model, remove it
    unset($filtered['some-wrong-model']);
  }

  // Example 3: Add custom embedding models
  if ($provider_id === 'ollama' && $capability === 'embeddings') {
    $filtered['my-custom-embed:latest'] = 'my-custom-embed:latest';
  }

  // Example 4: OpenRouter model overrides
  if ($provider_id === 'openrouter' && $capability === 'vision') {
    // Add a model that should support vision but isn't detected
    $filtered['anthropic/claude-3-opus'] = 'anthropic/claude-3-opus';
  }
}

/**
 * Example: Override embedding dimensions for custom models.
 *
 * Implements hook_ai_embedding_dimension_alter().
 *
 * @param int &$dimension
 *   The detected or default dimension.
 * @param string &$model_id
 *   The model ID.
 * @param string &$provider_id
 *   The provider ID.
 */
function mysite_ai_overrides_ai_embedding_dimension_alter(&$dimension, &$model_id, &$provider_id) {
  // Example: Set dimension for a custom Ollama embedding model
  if ($provider_id === 'ollama' && $model_id === 'my-custom-embed:latest') {
    $dimension = 768;
  }

  // Example: Override for a specific OpenRouter model
  if ($provider_id === 'openrouter' && $model_id === 'some-provider/some-embed-model') {
    $dimension = 1024;
  }
}
