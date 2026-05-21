# Example Provider Module Updates

## Summary

I've reviewed the example provider module (`ai_example_provider`) and updated it to match the current patterns used in `ai_ollama` and `ai_openrouter`.

## Changes Made

### 1. Updated `OpenAiExampleAdapter.php`

**Added:**
- `getModelsByCapability($capability)` method - A helper method that filters models by capability (text, vision, image, embeddings, moderation)
- `getModerationModels()` method - Now part of the standard adapter interface
- Support for `backdrop_alter('ai_model_capabilities')` hook - Allows site admins to override capability detection

**Updated:**
- `getChatModels()` - Now uses `getModelsByCapability('text')`
- `getImageModels()` - Now uses `getModelsByCapability('image')`
- `getVisionModels()` - Now uses `getModelsByCapability('vision')`
- `getEmbeddingModels()` - Now uses `getModelsByCapability('embeddings')`
- `getModels()` - Now returns a more complete set of example models including vision and moderation

### 2. Updated `PROVIDER_MODULE_GUIDE.md`

**Added:**
- Complete documentation of the `getModelsByCapability()` pattern
- Documentation of `getModerationModels()` method
- New section "Model Capability Detection" explaining how different providers handle capabilities
- Best practices for implementing capability detection
- Examples showing the `backdrop_alter()` hook integration

**Key Patterns Documented:**
- AI: Uses API metadata for reliable capability detection
- Ollama: Uses API metadata + pattern matching + alter hook
- OpenRouter: Uses pattern matching + alter hook (no reliable API metadata)

## Why These Changes Were Needed

Both `ai_ollama` and `ai_openrouter` have evolved to use a more sophisticated capability detection system:

1. **Centralized Logic**: The `getModelsByCapability()` method centralizes all capability detection logic, making it easier to maintain and extend.

2. **Flexibility**: The `backdrop_alter()` hook allows site administrators to override capability detection in custom modules without modifying provider code.

3. **Consistency**: All capability-specific methods (`getChatModels()`, `getVisionModels()`, etc.) now follow the same pattern.

4. **Completeness**: The `getModerationModels()` method was missing from the example but is now part of the standard interface.

## Files Modified

1. `modules/contrib/ai/examples/includes/AiExampleAdapter.php`
2. `modules/contrib/ai/examples/PROVIDER_MODULE_GUIDE.md`

## Files That Are Still Good

The following files didn't need changes:
- `ai_example_provider.module` - Still follows current patterns
- `ai_example_provider.info` - Still valid
- `README.md` - Still accurate
- `model_capability_override.php` - Already demonstrates the alter hook pattern

## Testing Recommendation

If you want to test the example provider:

1. Copy the example to a top-level module folder:
   ```bash
   cp -r modules/contrib/ai/examples/ai_example_provider modules/contrib/ai_example_provider
   ```

2. Enable it:
   ```bash
   ddev bee en ai_example_provider -y
   ddev bee cc all
   ```

3. Configure it at Admin → Configuration → AI → Settings

The example provider will now demonstrate all the current best practices for building provider modules.
