## Why

Oracle post-ship audit on Change-3 (`add-gpu-driver-sim-hardware-bridge`, session `ses_f8b8c32e5ffeGgFiPdo4hE0jiu`) identified a deeper ModuleLoader-level bug (Q3) that Change-3 partially worked around via a plugin-side guard.

The bug at `src/kernel/module_loader.cpp:177-214` (`load_plugin`):
- Re-invokes `mod->init()` unconditionally on every `dlopen` of the same plugin path
- No `loaded_plugins_.count(mod->name)` check before init
- `loaded_plugins_[mod->name] = info` at line 211 **overwrites** the existing PluginInfo and **resets `ref_count = 0`**, breaking the loader's own refcount accounting — subsequent `unload_plugins` then `dlclose`s once (refcount → 1 in glibc), so the plugin **never fully unloads** (glibc refcount stuck at 1).

Today the gpu_driver plugin masks this via its own `g_plugin_initialized` guard (commit `31dd5e1`), but every other plugin shipped in `plugins/` has the same exposure. The fix belongs at the framework layer.

## What Changes

- `src/kernel/module_loader.cpp` `load_plugin`: insert `loaded_plugins_.count(mod->name)` check after `dlsym` and before `resolve_dependencies`. If present, increment the existing PluginInfo's `ref_count`, dlclose the redundant handle, and return 0 (no init call, no PluginInfo overwrite).

## Non-goals (deferred)

- Adding idempotency to individual plugins' `init()` functions (Change-3 gpu_driver already has the guard; other plugins should add their own if they allocate per-init resources).
- ModuleLoader unit tests (no existing ModuleLoader test infrastructure; bootstrap pattern to be defined if/when the project's first ModuleLoader test is written).

## References

- Oracle audit session `ses_f8b8c32e5ffeGgFiPdo4hE0jiu` (post-ship, Change-3)
- Oracle deep-dive session `ses_f8b7a31c2ffejLzL1Cwq9Ep8tA` (Q3 interplay analysis)
- `plugins/gpu_driver/plugin.cpp` guard precedent (commit `31dd5e1`) — plugin-side workaround until framework fix lands
- `src/kernel/module_loader.cpp:177-214` (`load_plugin`)

## Notes

29 test files call `ModuleLoader::load_plugins("plugins")` — any future plugin that wants `init()` called fresh on every `dlopen` would need to opt out (e.g., via a flag in the `module` struct); no such plugin exists today.