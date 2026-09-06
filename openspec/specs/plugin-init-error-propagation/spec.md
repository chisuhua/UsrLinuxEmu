# plugin-init-error-propagation Specification

## Purpose
TBD - created by archiving change add-plugin-negative-path-test. Update Purpose after archive.
## Requirements
### Requirement: gpu_driver plugin init failure leaves no /dev/gpgpu* registered

When gpu_driver's `plugin_init_internal` fails (e.g. `pci_probe_enumerate_from_sim_hardware` returns an error because the topology file is missing relative to the process CWD), the failure MUST leave `VFS::instance()` with no `/dev/gpgpu*` device registered. The failure MUST be logged via `std::cerr` with a recognizable marker (`ENOENT` / `topology` / `No such file`).

Note on return codes: `ModuleLoader::load_plugin` returns `-1` on init failure (generic, not `-ENOENT`-specific). `ModuleLoader::load_plugins` does NOT propagate per-plugin init failures — it logs and continues, returning 0 unless `scan_candidates` or `topo_sort` fail. Therefore the testable observable is the side effects (VFS device registration + stderr log), NOT the return value.

#### Scenario: missing topology file prevents /dev/gpgpu0 registration

- **WHEN** the test binary captures the absolute path to the `plugins` directory before `chdir`, then calls `ModuleLoader::load_plugins(<absolute plugins path>)` from a temp directory that does not contain a `sim_hardware/topology/default_topology.json` path
- **THEN** `VFS::instance().open("/dev/gpgpu0", 0)` returns `nullptr`
- **AND** `std::cerr` contains at least one of: "ENOENT", "topology", "No such file"

