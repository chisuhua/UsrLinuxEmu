## ADDED Requirements

### Requirement: ModuleLoader::load_plugin skips init when plugin already loaded

`src/kernel/module_loader.cpp` `load_plugin` SHALL check `loaded_plugins_.count(mod->name)` after `dlsym` resolves the `mod` symbol. If a PluginInfo already exists for `mod->name`, `load_plugin` SHALL increment the existing PluginInfo's `ref_count`, `dlclose` the redundant handle, and return 0 without calling `mod->init()` and without overwriting the existing PluginInfo.

Note on `ref_count` semantics: the first successful load sets `ref_count = 0` (`module_loader.cpp:208`). Each subsequent load of the same plugin name (caught by the new branch) increments to `1`. A `decrease_ref` then drives `ref_count` from 1 → 0; the `--ref_count <= 0` check in `decrease_ref` (`:168`) triggers `mod->exit()` + `dlclose` + erase. So a plugin loaded twice + `unload_plugins` called once yields: ref_count 1→0, exit once, dlclose once, fully unmapped.

#### Scenario: repeated load_plugins of the same plugin does not re-invoke init

- **WHEN** `ModuleLoader::load_plugins("plugins")` is called twice in the same process
- **THEN** each plugin's `mod->init()` is called at most once across both invocations
- **AND** `loaded_plugins_[mod->name]->ref_count` equals 1 after the second call (0 after first, +1 after second)
- **AND** `mod->init()` is not invoked on the second call

#### Scenario: unload_plugins decrements ref_count correctly

- **WHEN** a plugin was loaded twice and then `unload_plugins` is called once
- **THEN** the plugin's library is fully unmapped (no glibc refcount residue of 1)
- **AND** the plugin's `mod->exit()` is called exactly once

### Requirement: dependency ref_count is NOT incremented on duplicate load (asymmetry)

When `load_plugin` takes the already-loaded branch, it SHALL increment only the loaded plugin's own `ref_count`, NOT the `ref_count` of that plugin's dependencies. The duplicate load therefore holds one reference on the plugin but no extra reference on its dependencies.

This is harmless today because no plugin in the repository is both depended-on and double-loaded. But it is an intentional scope limit of this change: a future scenario where a dependent plugin is loaded twice would not see symmetric dependency refcounts. Documented here so a future implementer does not "fix" it by calling `resolve_dependencies` on the already-loaded branch — that would change the semantic from "skip everything" to "skip init but still resolve deps", which has its own refcount implications.

#### Scenario: duplicate load does not increment dependency ref_count

- **WHEN** plugin A (which depends on plugin B) is loaded once, then loaded again via `load_plugins("plugins")`
- **THEN** `loaded_plugins_[A]->ref_count` equals 1 (incremented from 0)
- **AND** `loaded_plugins_[B]->ref_count` is unchanged from the value set by the first load's `resolve_dependencies`
- **AND** `unload_plugins` called once triggers exactly one `decrease_ref` on B (not two), and B's `ref_count` drops by 1