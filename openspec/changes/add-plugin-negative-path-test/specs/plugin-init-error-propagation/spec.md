## ADDED Requirements

### Requirement: gpu_driver plugin init propagates -ENOENT when topology file is missing

When `pci_probe_enumerate_from_sim_hardware("sim_hardware/topology/default_topology.json")` returns `-ENOENT` because the topology file does not exist relative to the process CWD, `plugin_init_internal` SHALL return `-ENOENT` to the caller. `VFS::instance()` SHALL NOT register any `/dev/gpgpu*` device in this case.

#### Scenario: missing topology file causes plugin init failure with no /dev/gpgpu0

- **WHEN** the process CWD does not contain a `sim_hardware/topology/default_topology.json` path (test injects this via `chdir` to an empty temp dir before `load_plugins`)
- **THEN** `VFS::instance().open("/dev/gpgpu0", 0)` returns `nullptr`
- **AND** the gpu_driver plugin's `init()` returns non-zero
- **AND** no other plugin's failure masks the gpu_driver failure (gpu_driver is first by load_priority 50; ModuleLoader logs the failure)