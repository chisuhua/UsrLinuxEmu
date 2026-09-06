## ADDED Requirements

### Requirement: Device/FileOperations shared_ptrs from VFS::open() MUST NOT outlive ModuleLoader::unload_plugins()

`VFS::instance().open(path, flags)` SHALL return a `std::shared_ptr<Device>` whose internal `FileOperations` member points to a `GpgpuDevice`-derived object that lives in the loaded plugin's `.so` file. When `ModuleLoader::unload_plugins()` is called, the loaded plugins are `dlclose()`d, which unmaps the `.so` code segment. Any `shared_ptr<Device>` still alive at that point becomes a use-after-free: its destructor dispatches virtual calls through a vtable pointer that now points into unmapped memory.

The caller MUST release the `shared_ptr<Device>` (via `reset()`, scope end, or explicit destruction) BEFORE calling `ModuleLoader::unload_plugins()`. Violation produces SIGSEGV at the destructor of the still-held `Device`, not at the `unload_plugins()` call site. This invariant is documented at the `unload_plugins()` declaration in `include/kernel/module_loader.h`.

The kernel idiom that this mirrors is `try_module_get` / `module_put`: a module cannot be unloaded while users hold references. The current codebase does NOT implement this refcounting at the framework level — the convention is enforced at the test/caller level instead. See proposal.md for the upgrade path.

#### Scenario: unload_plugins after open + reset does not crash

- **WHEN** a test or caller calls `VFS::instance().open("/dev/gpgpu0", O_RDWR)` to obtain a `shared_ptr<Device>`, then explicitly calls `dev.reset()` (or otherwise drops the reference) before invoking `ModuleLoader::unload_plugins()`
- **THEN** the process does NOT crash
- **AND** `unload_plugins()` returns normally (all plugins' `mod->exit()` callbacks complete; all loaded plugins removed from `ModuleLoader::loaded_plugins_`)

#### Scenario: unload_plugins with held Device shared_ptr crashes (regression guard)

- **WHEN** a test or caller calls `VFS::instance().open("/dev/gpgpu0", O_RDWR)` and holds the resulting `shared_ptr<Device>` while `ModuleLoader::unload_plugins()` runs (i.e. violates the invariant above)
- **THEN** the process receives SIGSEGV during destruction of the held `Device` after the `unload_plugins()` call returns
- **AND** the SIGSEGV stack frame is `Device::~Device()` → `~shared_ptr<FileOperations>()` → `_M_release()` (verified via gdb on the committed reproducer `tests/test_plugin_exit_cleanup_standalone.cpp`)

This scenario documents the bug; it exists in the change set so any future regression that re-introduces the load-while-held pattern (e.g. by removing the `dev.reset()` calls in the reproducer) is caught at SIGSEGV time and not silently masked.