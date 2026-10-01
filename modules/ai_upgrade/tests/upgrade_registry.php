<?php

/**
 * Offline regression checks for the AI Upgrade registry and bundled projects.
 *
 * Run with:
 *   php modules/contrib/ai/modules/ai_upgrade/tests/upgrade_registry.php
 */

$GLOBALS['ai_upgrade_test_enabled'] = [];
$GLOBALS['ai_upgrade_test_disabled'] = [];

function module_exists($module) {
  return !empty($GLOBALS['ai_upgrade_test_enabled'][$module]);
}

function module_disable(array $modules) {
  $GLOBALS['ai_upgrade_test_disabled'] = array_values($modules);
}

function system_rebuild_module_data() {
  return [];
}

require_once dirname(__DIR__) . '/ai_upgrade.install';
require_once dirname(__DIR__) . '/ai_upgrade.module';

function ai_upgrade_test_same($expected, $actual, $message) {
  if ($expected !== $actual) {
    fwrite(STDERR, "FAIL: $message\nExpected: " . var_export($expected, TRUE) . "\nActual: " . var_export($actual, TRUE) . "\n");
    exit(1);
  }
}

$registry = ai_upgrade_registry();
$bundles = ai_upgrade_bundle_map();

foreach (['ai_alt_bulk', 'ai_alt_tools', 'ai_devel', 'ai_tools', 'ai_webform_tools'] as $module) {
  ai_upgrade_test_same(TRUE, isset($registry[$module]), "$module is registered");
  ai_upgrade_test_same('ai', $bundles[$module], "$module resolves to the ai project");
}

foreach (['ai_search_explorer', 'ai_search_header', 'ai_search_search_block', 'ai_search_search_block_log', 'ai_search_simple_chatbot'] as $module) {
  ai_upgrade_test_same('ai_search', $bundles[$module], "$module resolves to the ai_search project");
}

ai_upgrade_test_same('ai_agents.settings', $registry['ai_agent']['config_map']['openai_agent.settings']['new'], 'agent config uses the plural replacement name');
ai_upgrade_test_same('ai_assistants.settings', $registry['ai_assistant']['config_map']['openai_assistant.settings']['new'], 'assistant config uses the plural replacement name');
foreach ([$registry['ai_agent'], $registry['ai_assistant']] as $info) {
  $transform = reset($info['config_map'])['transform'];
  ai_upgrade_test_same(TRUE, function_exists($transform), "$transform exists");
}

ai_upgrade_test_same('openai/gpt-4o', _ai_upgrade_qualify_model('gpt-4o'), 'unprefixed legacy model routes to openai');
ai_upgrade_test_same('anthropic/claude-x', _ai_upgrade_qualify_model('anthropic/claude-x'), 'prefixed model is untouched');
ai_upgrade_test_same('openai/gpt-4o', _ai_upgrade_qualify_model('openai/gpt-4o'), 'openai prefix is kept, not stripped');
ai_upgrade_test_same('', _ai_upgrade_qualify_model(''), 'empty model stays empty');
$assistant = _ai_transform_assistant_config(['assistants' => ['a' => ['model' => 'gpt-4o-mini'], 'b' => ['model' => '']]], 'openai_assistant.settings', 'ai_assistants.settings');
ai_upgrade_test_same(['a' => ['model' => 'openai/gpt-4o-mini'], 'b' => ['model' => '']], $assistant['assistants'], 'assistant transform qualifies models');
ai_upgrade_test_same('ai', ai_upgrade_project_name('ai_alt_bulk'), 'bundled AI submodule uses its parent project');
ai_upgrade_test_same('ai_search', ai_upgrade_project_name('ai_search_simple_chatbot'), 'bundled search submodule uses its parent project');
ai_upgrade_test_same('ai', ai_upgrade_project_name('openai_chatgpt'), 'consolidated explorer module uses the parent AI project');

$missing_projects = ai_upgrade_get_missing_projects(['ai_alt_bulk', 'ai_alt_tools', 'ai_tools']);
ai_upgrade_test_same(['ai'], array_keys($missing_projects), 'bundled modules are queued as one project');

$GLOBALS['ai_upgrade_test_enabled'] = [
  'ai_agents' => TRUE,
  'openai_agent' => TRUE,
  'ai_assistants' => TRUE,
  'openai_assistant' => TRUE,
  'ai_tools' => TRUE,
  'openai_tools' => TRUE,
  'openai_prompt' => TRUE,
];
_ai_disable_legacy_suite_modules();
$disabled = $GLOBALS['ai_upgrade_test_disabled'];
sort($disabled);
ai_upgrade_test_same(
  ['openai_agent', 'openai_assistant', 'openai_tools'],
  $disabled,
  'bulk disable follows registry replacements and skips deprecated modules'
);

fwrite(STDOUT, "AI upgrade registry checks passed.\n");
