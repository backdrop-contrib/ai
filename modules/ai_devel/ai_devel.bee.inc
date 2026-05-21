<?php

/**
 * Implements hook_bee_command().
 */
function ai_devel_bee_command() {
  return [
    'generate-content-gpt' => [
      'description' => 'Generate content using AI GPT models.',
      'options' => [
        'bundles' => 'Comma-separated list of content types to generate. Defaults to all content types.',
        'system' => 'AI system prompt for generating content. Defaults to a standard prompt.',
        'model' => 'AI model to use (e.g., gpt-3.5-turbo]. Defaults to gpt-3.5-turbo.',
        'temperature' => 'Sampling temperature (0-2, default: 0.7].',
        'max_tokens' => 'Maximum tokens for the response (default: 512].',
        'max_comments' => 'Maximum number of comments per node (default: 0].',
        'add_alias' => 'If set, add a URL alias to generated content.',
        'title_length' => 'Approximate number of words for the title (default: 4].',
        'num_nodes' => 'Number of nodes to generate (default: 1].',
      ],
      'callback' => 'ai_devel_generate_content_command',
    ],
  ];
}

/**
 * Command callback for `generate-content-gpt`.
 */
function ai_devel_generate_content_command($args, $options) {
  backdrop_bootstrap(BACKDROP_BOOTSTRAP_FULL);

  // Load the module.
  module_load_include('module', 'ai_devel');

  // Add default values for options.
  $options += [
    'node_types' => isset($options['bundles']) ? explode(',', $options['bundles']) : array_keys(node_type_get_types()),
    'system' => 'Your task is to generate content.',
    'model' => 'gpt-3.5-turbo',
    'temperature' => 0.7,
    'max_tokens' => 512,
    'add_alias' => FALSE,
    'max_comments' => 0,
    'title_length' => 4,
    'num_nodes' => 1, // Default to 1 node.
  ];

  // Call the shared content generation function.
  $total_created = ai_devel_generate_content($options);

  // Provide feedback to the user.
  bee_message(t('Created @count nodes.', ['@count' => $total_created]));
}
