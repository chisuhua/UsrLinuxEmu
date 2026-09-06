## Why

During Plan 1 framework-guard implementation (commit `252dfb4`), a SIGSEGV was observed in `test_gpu_plugin_init_idempotent_standalone` when calling `ModuleLoader::unload_plugins()`. The bug was deferred with the note "out of scope; document and defer" but a real reproducer (Oracle session `ses_f87662888ffevZbNReMW9dK7MS`) confirmed it. Root cause:

1. Test holds a `shared_ptr<Device>` returned by `VFS::instance().open("/dev/gpgpu0", ...)`.
2. `Device` contains a `shared_ptr<FileOperations>` that actually points to a `GpgpuDevice` instance living in `plugin_gpu_driver.so` (GpgpuDevice inherits from FileOperations, has a vtable).
3. `ModuleLoader::unload_plugins()` calls `mod->exit()` (which unregisters the device from VFS but does NOT destroy the Device shared_ptr held by the test), then `dlclose(plugin_gpu_driver.so)`.
4. `dlclose` unmaps the .so; GpgpuDevice's vtable pointer is now invalid.
5. Test scope ends; the local `shared_ptr<Device>` is destroyed; `Device::~Device()` calls `~FileOperations()`; virtual dispatch through the unmapped vtable → SIGSEGV.

gdb stack trace (from the reproducer `tests/test_plugin_exit_cleanup_standalone.cpp`):
```
Thread 1 received signal SIGSEGV
#0  std::_Sp_counted_base::_M_release()
#1  std::__shared_count::~__shared_count()
#2  std::__shared_ptr<FileOperations>::~__shared_ptr()  from libkernel.so
#3  std::shared_ptr<FileOperations>::~shared_ptr()       from libkernel.so
#4  usr_linux_emu::Device::~Device()                      from libkernel.so
#5  std::_Destroy<Device>(Device*)
#6  std::_Sp_counted_ptr_inplace<Device>::_M_dispose()
...
#11 CATCH2_INTERNAL_TEST_2()
```

Oracle strategy (`ses_f87662888ffevZbNReMW9dK7MS`): **Option A — test-side fix + central invariant declaration**. Not architectural:
- CLI is batch-mode (`tools/cli/main.cpp`); `unload` doesn't hold Device references in the same process — no production exposure.
- ~20 existing tests already follow `dev.reset(); unload_plugins()`; the new test was the outlier.
- Option D (pImpl) cannot fix: `dlclose` unmaps code segments too, not just data; a captured deleter pointer would itself be unmapped.
- Option E (don't dlclose) would re-introduce the glibc-refcount bug fixed in commit `d2c6d4b`.
- Architectural fix (Option B-lite: VFS open→increase_ref / Device dtor→decrease_ref) is the right upgrade path IF a production caller ever holds Device across unload. None exists today.

## What Changes

- `include/kernel/module_loader.h`: add 2-line invariant comment on `unload_plugins()` declaration ("dlcloses plugin .so files; Device/FileOperations shared_ptrs from VFS::open() MUST be reset before this call (SIGSEGV otherwise)"). Centralizes a contract that previously existed only in scattered test comments.
- `tests/test_plugin_exit_cleanup_standalone.cpp` (new): regression test verifying `load + open-dev + dev.reset() + unload` cycle does not crash. Includes 4 TEST_CASEs (1/2/20 cycles + F4-pattern). Non-tautology verified by removing `dev.reset()` and observing the SIGSEGV return.
- `tests/CMakeLists.txt`: register test in `CATCH2_SIM_TESTS` list.

## Non-goals

- Architectural fix (VFS open→refcount / Device dtor→refcount → defer dlclose while refs > 0). Defer until a production caller actually holds Device across unload (none today).
- PImpl pattern for Device. Cannot work — `dlclose` unmaps code segments.
- PluginManager cleanup (legacy `plugins.json` path). Out of scope; not used by current callers.

## References

- Oracle strategy session: `ses_f87662888ffevZbNReMW9dK7MS`
- Original SIGSEGV: commit `252dfb4` Task #1, deferred as out-of-scope
- Plan 1 framework fix (precedent for ref_count semantics): commit `d2c6d4b`
- Existing convention (~20 tests use `dev.reset()` before unload; e.g. `tests/test_poll.cpp`, `tests/test_serial_ioctl.cpp`)