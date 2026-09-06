## ADDED Requirements

### Requirement: ModuleLoader::load_plugin skips init when plugin already loaded

`src/kernel/module_loader.cpp` `load_plugin` SHALL check `loaded_plugins_.count(mod->name)` after `dlsym` resolves the `mod` symbol. If a PluginInfo already exists for `mod->name`, `load_plugin` SHALL increment the existing PluginInfo's `ref_count`, `dlclose` the redundant handle, and return 0 without calling `mod->init()` and without overwriting the existing PluginInfo.

#### Scenario: repeated load_plugins of the same plugin does not re-invoke init

- **WHEN** `ModuleLoader::load_plugins("plugins")` is called twice in the same process
- **THEN** each plugin's `mod->init()` is called at most once across both invocations
- **AND** `loaded_plugins_[mod->name]->ref_count` equals 2 after the second call
- **AND** `mod->init()` is not invoked on the second call

#### Scenario: unload_plugins decrements ref_count correctly

- **WHEN** a plugin was loaded twice and then `unload_plugins` is called once
- **THEN** the plugin's library is fully unmapped (no glibc refcount residue of 1)
- **AND** the plugin's `mod->exit()` is called exactly once