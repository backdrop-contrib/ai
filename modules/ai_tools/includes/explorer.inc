<?php


/**
 * @file
 * Tool Explorer admin form — test tool calling against any configured provider.
 */

/**
 * Form builder: Tool Explorer.
 */
function ai_tools_explorer_form($form, &$form_state) {
  backdrop_add_css(backdrop_get_path('module', 'ai_tools') . '/css/ai_tools_explorer.css');
  backdrop_add_js(backdrop_get_path('module', 'ai_tools') . '/js/ai_tools_explorer.js');

  $tools = ai_tools_get_tools();

  // ai_get_all_chat_models() returns ['Provider Label' => ['provider/model' => 'label']]
  // Backdrop's Form API renders nested arrays as <optgroup> elements automatically.
  $models_grouped = ai_get_all_chat_models();

  if (empty($models_grouped)) {
    $form['no_models'] = [
      '#markup' => '<p class="messages error">' . t('No chat models available. Please <a href="@url">configure at least one provider</a> in AI settings.', ['@url' => url('admin/config/ai/settings')]) . '</p>',
    ];
    return $form;
  }

  $saved_model = $form_state['storage']['last_model'] ?? config_get('ai.settings', 'default_chat_model');
  $first_model = ai_tools_explorer_default_model($models_grouped);

  $form['model'] = [
    '#type'          => 'select',
    '#title'         => t('Model'),
    '#options'       => $models_grouped,
    '#default_value' => $saved_model ?: $first_model,
    '#description'   => t('Select a model from any enabled provider.'),
    '#required'      => TRUE,
  ];

  // Tool checkboxes flat-sorted by category then name.
  $flat_options = [];
  $by_category = [];
  foreach ($tools as $key => $tool) {
    $fn       = $tool['definition']['function'];
    $category = $tool['category'] ?? 'general';
    $by_category[$category][$key] = '<strong>' . check_plain($fn['name']) . '</strong> — ' . check_plain($fn['description'] ?? '');
  }
  ksort($by_category);
  foreach ($by_category as $items) {
    $flat_options += $items;
  }

  $form['tools_fieldset'] = [
    '#type'        => 'fieldset',
    '#title'       => t('Available tools'),
    '#collapsible' => TRUE,
    '#collapsed'   => FALSE,
  ];
  $form['tools_fieldset']['selection_controls'] = [
    '#type' => 'markup',
    '#markup' => '<div class="ai-tools-explorer-controls"><button type="button" class="ai-tools-select-all btn btn-sm">' . t('Select all') . '</button> <button type="button" class="ai-tools-clear-all btn btn-sm">' . t('Clear all') . '</button></div>',
  ];
  $form['tools_fieldset']['selected_tools'] = [
    '#type'          => 'checkboxes',
    '#title'         => t('Tools to expose to the model'),
    '#options'       => $flat_options,
    '#default_value' => isset($form_state['storage']['last_tools']) && is_array($form_state['storage']['last_tools']) ? $form_state['storage']['last_tools'] : array_keys($tools),
    '#description'   => t('Selected tools are sent to the model as callable functions.'),
    '#attributes'    => ['class' => ['ai-tools-explorer-grid']],
    '#prefix'        => '<div class="ai-tools-explorer-cards">',
    '#suffix'        => '</div>',
  ];

  $form['prompt'] = [
    '#type'          => 'textarea',
    '#title'         => t('Prompt'),
    '#rows'          => 3,
    '#default_value' => $form_state['storage']['last_prompt'] ?? 'What are the most recent posts on this site? Also tell me the site name.',
    '#description'   => t('Enter a message that may trigger one or more tool calls.'),
    '#required'      => TRUE,
  ];

  $form['temperature'] = [
    '#type' => 'number',
    '#title' => t('Temperature'),
    '#min' => 0,
    '#max' => 2,
    '#step' => 0.1,
    '#default_value' => $form_state['storage']['last_temperature'] ?? 0.3,
    '#description' => t('Controls randomness for tool selection and final answers.'),
  ];

  $form['max_tokens'] = [
    '#type' => 'number',
    '#title' => t('Max tokens'),
    '#min' => 1,
    '#step' => 1,
    '#default_value' => $form_state['storage']['last_max_tokens'] ?? 1024,
    '#description' => t('Requested completion budget. The adapter may automatically increase it when the model hits output limits.'),
  ];

  $form['actions']['#type'] = 'actions';
  $form['actions']['submit'] = [
    '#type'  => 'submit',
    '#value' => t('Run'),
  ];

  if (!empty($form_state['storage']['steps'])) {
    $form['results'] = [
      '#type'   => 'fieldset',
      '#title'  => t('Conversation trace'),
      '#weight' => 10,
    ];
    foreach ($form_state['storage']['steps'] as $i => $step) {
      $form['results']['step_' . $i] = [
        '#type'        => 'fieldset',
        '#title'       => check_plain($step['label']),
        '#collapsible' => TRUE,
        '#collapsed'   => FALSE,
      ];
      $form['results']['step_' . $i]['content'] = [
        '#markup' => '<pre>' . check_plain($step['body']) . '</pre>',
      ];
    }
  }

  return $form;
}

/**
 * Chooses a useful default model for tool-calling tests.
 */
function ai_tools_explorer_default_model(array $models_grouped) {
  $preferred = [
    'openai/gpt-4o-mini',
    'openai/gpt-4.1-mini',
    'openai/gpt-4o',
    'openai/gpt-4.1',
    'anthropic/claude-haiku-4-5-20251001',
    'google_gemini/models/gemini-2.5-flash',
    'groq/llama-3.3-70b-versatile',
    'openrouter/openai/gpt-4o-mini',
    'ollama/llama3.1:8b',
  ];

  foreach ($preferred as $model_id) {
    foreach ($models_grouped as $group) {
      if (is_array($group) && isset($group[$model_id])) {
        return $model_id;
      }
    }
  }

  foreach ($models_grouped as $provider_label => $group) {
    if ($provider_label === 'AWS Bedrock') {
      continue;
    }
    if (is_array($group) && !empty($group)) {
      return key($group);
    }
  }

  foreach ($models_grouped as $group) {
    if (is_array($group) && !empty($group)) {
      return key($group);
    }
  }

  return NULL;
}

/**
 * Form submit: run the full tool-calling loop.
 */
function ai_tools_explorer_form_submit($form, &$form_state) {
  $model_prefixed     = $form_state['values']['model'];
  $selected_tool_keys = array_filter($form_state['values']['selected_tools']);
  $prompt             = trim($form_state['values']['prompt']);
  $temperature        = isset($form_state['values']['temperature']) ? (float) $form_state['values']['temperature'] : 0.3;
  $max_tokens         = isset($form_state['values']['max_tokens']) ? (int) $form_state['values']['max_tokens'] : 1024;

  $form_state['storage']['last_model']  = $model_prefixed;
  $form_state['storage']['last_tools']  = array_keys($selected_tool_keys);
  $form_state['storage']['last_prompt'] = $prompt;
  $form_state['storage']['last_temperature'] = $temperature;
  $form_state['storage']['last_max_tokens'] = $max_tokens;
  $form_state['storage']['steps']       = [];

  try {
    $stripped_model = NULL;
    $api = ai_get_api_for_model($model_prefixed, $stripped_model);
    if (!$api) {
      throw new \Exception(t('Could not load API for model @m.', ['@m' => $model_prefixed]));
    }

    $all_tools = ai_tools_get_tools();
    $tool_definitions = [];
    foreach ($selected_tool_keys as $key) {
      if (isset($all_tools[$key])) {
        $tool_definitions[] = $all_tools[$key]['definition'];
      }
    }

    $messages = [
      ['role' => 'system', 'content' => 'You are a helpful assistant. Use the available tools when they can answer the user\'s question.'],
      ['role' => 'user',   'content' => $prompt],
    ];

    $context = ['operation' => 'tool_explorer'];
    $steps = [];
    for ($round = 1; $round <= 5; $round++) {
      $response = $api->chatWithTools($stripped_model, $messages, $tool_definitions, $temperature, $max_tokens, 'auto', $context);

      $step_body  = "finish_reason: {$response['finish_reason']}\n";
      $step_body .= "content: " . ($response['content'] ?: '(none)') . "\n";
      if (!empty($response['tool_calls'])) {
        $step_body .= "tool_calls:\n" . json_encode($response['tool_calls'], JSON_PRETTY_PRINT) . "\n";
      }
      $steps[] = ['label' => "Round $round — model ({$response['finish_reason']})", 'body' => $step_body];

      if ($response['finish_reason'] !== 'tool_calls' || empty($response['tool_calls'])) {
        break;
      }

      $messages[] = [
        'role'       => 'assistant',
        'content'    => $response['content'],
        'tool_calls' => array_map(function ($tc) {
          return ['id' => $tc['id'], 'type' => 'function', 'function' => ['name' => $tc['name'], 'arguments' => json_encode($tc['arguments'])]];
        }, $response['tool_calls']),
      ];

      foreach ($response['tool_calls'] as $tc) {
        $result_json = ai_tools_dispatch($tc['name'], $tc['arguments']);
        $steps[] = [
          'label' => "Tool: {$tc['name']}",
          'body'  => "Arguments: " . json_encode($tc['arguments'], JSON_PRETTY_PRINT) . "\nResult: $result_json",
        ];
        $messages[] = ['role' => 'tool', 'tool_call_id' => $tc['id'], 'content' => $result_json];
      }
    }

    $form_state['storage']['steps'] = $steps;
  }
  catch (\Exception $e) {
    $form_state['storage']['steps'] = [['label' => 'Error', 'body' => $e->getMessage()]];
  }

  $form_state['rebuild'] = TRUE;
}
