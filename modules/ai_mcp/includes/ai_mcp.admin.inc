<?php

/**
 * @file
 * Admin UI for AI MCP server configuration.
 *
 * Provides listing, add, edit, and delete pages for MCP server configs.
 */

/**
 * Admin listing page: displays all configured MCP servers.
 *
 * @return array|string
 *   Render array or markup.
 */
function ai_mcp_admin_page() {
  $servers = ai_mcp_server_load_all();
  $all_tools = ai_tools_get_tools();

  $header = [
    t('Label'),
    t('Machine name'),
    t('Allowed tools'),
    t('Operations'),
  ];

  $rows = [];
  foreach ($servers as $id => $server) {
    $label = isset($server['label']) ? check_plain($server['label']) : check_plain($id);

    $tool_config = isset($server['tools']) ? array_filter((array) $server['tools']) : [];
    if (empty($tool_config)) {
      $tool_count = t('All (@count)', ['@count' => count($all_tools)]);
    }
    else {
      $tool_count = t('@selected of @total', [
        '@selected' => count($tool_config),
        '@total'    => count($all_tools),
      ]);
    }

    $edit_link   = l(t('Edit'),   'admin/config/services/mcp/' . $id);
    $delete_link = l(t('Delete'), 'admin/config/services/mcp/' . $id . '/delete');

    $rows[] = [
      $label,
      check_plain($id),
      $tool_count,
      $edit_link . ' | ' . $delete_link,
    ];
  }

  if (empty($rows)) {
    $empty = '<p>' . t('No MCP servers configured yet. <a href="@add">Add a server</a>.', [
      '@add' => url('admin/config/services/mcp/add'),
    ]) . '</p>';
    return $empty;
  }

  $build = [];
  $build['servers_table'] = [
    '#theme'  => 'table',
    '#header' => $header,
    '#rows'   => $rows,
  ];

  return $build;
}

/**
 * Form: add or edit an MCP server config.
 *
 * @param array $form
 *   Form array.
 * @param array $form_state
 *   Form state array.
 * @param array|FALSE $server
 *   Existing server config (from wildcard loader) or FALSE/NULL for new.
 *
 * @return array
 *   Form render array.
 */
function ai_mcp_server_form(array $form, array &$form_state, $server = NULL) {
  $is_new = empty($server);

  $form['#server'] = $server;

  $form['label'] = [
    '#type'          => 'textfield',
    '#title'         => t('Label'),
    '#default_value' => $is_new ? '' : $server['label'],
    '#required'      => TRUE,
    '#description'   => t('Human-readable name for this MCP server instance.'),
  ];

  $form['id'] = [
    '#type'          => 'machine_name',
    '#title'         => t('Machine name'),
    '#default_value' => $is_new ? '' : $server['id'],
    '#required'      => TRUE,
    '#disabled'      => !$is_new,
    '#machine_name'  => [
      'exists'   => 'ai_mcp_server_load',
      'source'   => ['label'],
    ],
    '#description'   => t('Unique identifier (cannot be changed after creation).'),
  ];

  $form['auth_key'] = [
    '#type'          => 'textfield',
    '#title'         => t('Auth key (Bearer token)'),
    '#default_value' => $is_new ? '' : $server['auth_key'],
    '#required'      => TRUE,
    '#description'   => t('Clients must send this token in the <code>Authorization: Bearer &lt;token&gt;</code> header.'),
    '#attributes'    => ['autocomplete' => 'off'],
  ];

  $form['uid'] = [
    '#type'          => 'number',
    '#title'         => t('Run tool calls as user ID'),
    '#default_value' => $is_new ? 0 : (isset($server['uid']) ? (int) $server['uid'] : 0),
    '#min'           => 0,
    '#description'   => t('Tool permission checks run as this user. Use a dedicated low-privilege service account; 0 runs as anonymous.'),
  ];

  // Build tool checkboxes from the registry.
  $all_tools  = ai_tools_get_tools();
  $tool_options = [];
  foreach ($all_tools as $key => $tool) {
    $fn = isset($tool['definition']['function']) ? $tool['definition']['function'] : [];
    $name = isset($fn['name']) ? $fn['name'] : $key;
    $desc = isset($fn['description']) ? $fn['description'] : '';
    $tool_options[$key] = '<strong>' . check_plain($name) . '</strong>'
      . (empty($desc) ? '' : ' — ' . check_plain(truncate_utf8($desc, 80, TRUE, TRUE)));
  }

  $current_tools = !$is_new && isset($server['tools']) ? array_filter((array) $server['tools']) : [];

  $form['tools'] = [
    '#type'          => 'checkboxes',
    '#title'         => t('Allowed tools'),
    '#options'       => $tool_options,
    '#default_value' => array_keys(array_filter($current_tools, function($v) { return (bool) $v; })),
    '#description'   => t('Leave all unchecked to allow every tool in the registry. Check specific tools to restrict this server to that subset.'),
  ];

  $form['actions'] = ['#type' => 'actions'];
  $form['actions']['submit'] = [
    '#type'  => 'submit',
    '#value' => $is_new ? t('Create server') : t('Save server'),
  ];
  $form['actions']['cancel'] = [
    '#type'  => 'link',
    '#title' => t('Cancel'),
    '#href'  => 'admin/config/services/mcp',
  ];

  return $form;
}

/**
 * Validates the server form.
 */
function ai_mcp_server_form_validate(array $form, array &$form_state) {
  $auth_key = trim($form_state['values']['auth_key']);
  if (empty($auth_key)) {
    form_set_error('auth_key', t('Auth key is required.'));
  }

  $uid_raw = isset($form_state['values']['uid']) ? trim((string) $form_state['values']['uid']) : '';
  if ($uid_raw !== '') {
    if (!ctype_digit($uid_raw)) {
      form_set_error('uid', t('User ID must be a non-negative integer.'));
    }
    else {
      $uid = (int) $uid_raw;
      if ($uid > 0) {
        if ($uid == 1) {
          form_set_error('uid', t('User ID 1 (the site superuser) bypasses all permission checks and cannot be used as the execution user. Use a dedicated low-privilege account.'));
        }
        $account = user_load($uid);
        if (!$account) {
          form_set_error('uid', t('The selected user ID does not exist.'));
        }
        elseif (empty($account->status)) {
          form_set_error('uid', t('The selected user account is blocked and cannot be used as the execution user.'));
        }
      }
      // Normalize so submit sees the cast value, not the raw string.
      $form_state['values']['uid'] = $uid;
    }
  }
}

/**
 * Submits the server form — saves config and redirects.
 */
function ai_mcp_server_form_submit(array $form, array &$form_state) {
  $values  = $form_state['values'];
  $is_new  = empty($form['#server']);
  $uid_raw = isset($values['uid']) ? trim((string) $values['uid']) : '';

  // Collect selected tools (checkboxes returns key => 0|key).
  $raw_tools   = isset($values['tools']) ? $values['tools'] : [];
  $tools_clean = array_values(array_filter($raw_tools));

  $server = [
    'id'           => $values['id'],
    'label'        => $values['label'],
    'auth_key'     => trim($values['auth_key']),
    'uid'          => $uid_raw === '' ? 0 : (int) $uid_raw,
    'tools'        => $tools_clean,
    'oauth_enabled'=> FALSE,
  ];

  ai_mcp_server_save($server);

  backdrop_set_message($is_new
    ? t('MCP server %label created.', ['%label' => $server['label']])
    : t('MCP server %label updated.', ['%label' => $server['label']])
  );

  $form_state['redirect'] = 'admin/config/services/mcp';
}

/**
 * Confirm form: delete an MCP server config.
 *
 * @param array $form
 *   Form array.
 * @param array $form_state
 *   Form state array.
 * @param array $server
 *   Server config array from wildcard loader.
 *
 * @return array
 *   Confirm form render array.
 */
function ai_mcp_server_delete_form(array $form, array &$form_state, array $server) {
  $form['#server'] = $server;

  return confirm_form(
    $form,
    t('Are you sure you want to delete the MCP server %label?', ['%label' => $server['label']]),
    'admin/config/services/mcp',
    t('This action cannot be undone. Any clients using this server\'s auth key will immediately lose access.'),
    t('Delete'),
    t('Cancel')
  );
}

/**
 * Submits the delete confirm form.
 */
function ai_mcp_server_delete_form_submit(array $form, array &$form_state) {
  $server = $form['#server'];
  ai_mcp_server_delete($server['id']);
  backdrop_set_message(t('MCP server %label deleted.', ['%label' => $server['label']]));
  $form_state['redirect'] = 'admin/config/services/mcp';
}
