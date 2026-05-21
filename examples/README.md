AI Example Provider (ai_example_provider)

This folder contains an example provider module packaged inside the `ai` module at `ai/examples/ai_example_provider/`.

Important: Backdrop will not enable modules located inside another module's directory. To test and enable this example as a real module, copy the entire `ai_example_provider` folder to `modules/contrib/ai_example_provider/` (parallel to `ai/`).

Files included:
- ai_example_provider.info - module info
- ai_example_provider.module - hooks to register provider and add settings
- includes/OpenExampleAdapter.php - minimal adapter skeleton

Usage:
1. Copy the example folder to a top-level module folder, e.g.:

```cmd
cp -r modules/contrib/ai/examples/ai_example_provider modules/contrib/ai_example_provider
```

2. Enable the module via the Backdrop UI (Admin → Modules) or via Bee:

```cmd
# from Windows cmd.exe
ddev exec "bee en ai_example_provider -y"
ddev exec "bee cc all"
```

3. Configure the provider at Admin → Configuration → AI → Settings and enable the provider.

Replace the simulated adapter calls inside `includes/AiExampleAdapter.php` with real HTTP calls to your provider and adapt return shapes to match the expectations of the `ai` core.

