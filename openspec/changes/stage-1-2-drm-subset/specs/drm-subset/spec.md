## MODIFIED Requirements

### Requirement: linux_compat/drm 头文件扩展

The system MUST add/extend the following headers under `include/linux_compat/drm/` aligned with Linux 6.12 LTS API signatures (per ADR-027 decision 3, ABI consistency not guaranteed):

- `drm_gem.h` MUST expose complete `struct drm_gem_object` with init/handle_create/refcount/release fields
- `drm_prime.h` MUST expose **`dma_buf_dynamic_attach`** / **`dma_buf_detach`** / **`dma_buf_map_attachment`** / **`dma_buf_unmap_attachment`** / **`dma_buf_pin`** / **`dma_buf_unpin`** function signatures AND `struct dma_buf_attach_ops` (with `allow_peer2peer` + `move_notify` fields)
- `drm_file_operations.h` MUST expose complete `struct drm_file` abstraction
- `drm_mode_config.h` MUST provide basic structs (placeholder, stage 1.4+ will extend per KFD need)
- `drm_ioctl.h` MUST add `drm_ioctl_permit` + `errno_to_linux` errno mapping helper

#### Scenario: drm_prime.h exposes corrected dma_buf API set

- **WHEN** a translation unit includes `linux_compat/drm/drm_prime.h`
- **THEN** the 6 dma_buf_* function signatures AND `struct dma_buf_attach_ops` MUST be declared
- **AND** `dma_buf_attach()` MUST NOT be declared (amdgpu does not call it; see design.md Decision 6)

#### Scenario: drm_gem.h supports full lifecycle

- **WHEN** a translation unit includes `linux_compat/drm/drm_gem.h` and instantiates `struct drm_gem_object`
- **THEN** the struct MUST contain fields for `refcount` + `handle_count` + `dev` pointer
- **AND** `drm_gem_handle_create()` + `drm_gem_object_release()` declarations MUST be visible

### Requirement: src/kernel/drm/ 实现

The system MUST provide the following implementations:

- `src/kernel/drm/drm_gem.cpp` — GEM object full lifecycle (init / handle_create / refcount tracking / release)
- `src/kernel/drm/drm_file.cpp` — `struct drm_file` lifecycle (one instance per fd open/close)
- `src/kernel/drm/drm_prime.cpp` — dma_buf API implementations honoring `unmap → detach → put` release order (design.md Decision 5/contract)
- `src/kernel/drm/render_node.cpp` — `/dev/dri/renderD128` (mode=0666, ownership 0:0) + `/dev/dri/card0` registration per ADR-037
- `src/kernel/drm/errno_to_linux.cpp` — errno mapping helper for `-EACCES`/`-EFAULT`/`-ENOMEM`/`-EREMOTEIO`/`-ENOSPC`

#### Scenario: GEM object has no reference leak

- **WHEN** driver code allocates a GEM object via `drm_gem_object_init`
- **AND** performs N handle_create + release cycles
- **THEN** ASan MUST report zero leaks
- **AND** the object's refcount MUST drop to zero after final release

#### Scenario: render node registers with correct mode

- **WHEN** `render_node.cpp` module-load runs
- **THEN** exactly two devices MUST register with VFS: `/dev/dri/renderD128` and `/dev/dri/card0`
- **AND** both MUST have `mode = 0666`, `uid = 0`, `gid = 0` per ADR-037 / Linux udev default
- **AND** `VFS::open("/dev/dri/renderD128", O_RDWR)` MUST succeed

### Requirement: GpgpuDevice 保留 FileOperations 入口（design.md Decision 1）

The system MUST keep the existing `FileOperations` entry point for `GpgpuDevice` while internally redirecting to `drm_ioctl()`. The `ioctl()` stub on `FileOperations` MUST delegate to `drm_ioctl()` via the `drm_ioctl_desc[]` table.

#### Scenario: existing TaskRunner IOCTL path preserved

- **WHEN** external caller invokes `dev->fops->ioctl(fd, GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH, &args)`
- **THEN** the call MUST reach `gpu_ioctl_pushbuffer_handler` via `drm_ioctl()` table dispatch
- **AND** the existing system C ioctl numbers MUST remain stable (`scripts/check_gpu_ioctl_sync.sh` reports 0 diff)

### Requirement: drm_ioctl_desc[] 表扩展至 ≥15 个 IOCTL

The system MUST extend `drm_ioctl_desc[]` to cover:

- All 7 existing GPU_IOCTL_* (PUSHBUFFER_SUBMIT_BATCH, REGISTER_MMU_EVENT_CB, REGISTER_FIRMWARE_CB, ALLOC_BO, FREE_BO, MAP_BO, WAIT_FENCE)
- All 4 VA Space IOCTL (CREATE_VA_SPACE, DESTROY_VA_SPACE, REGISTER_GPU, GET_DEVICE_INFO)
- All 4 Queue IOCTL (CREATE_QUEUE, DESTROY_QUEUE, MAP_QUEUE_RING, QUERY_QUEUE)
- 5 new KFD-compat IOCTL (GET_PROCESS_APERTURE 0x44, UPDATE_QUEUE 0x45, MAP_MEMORY 0x46, UNMAP_MEMORY 0x47) + CREATE_QUEUE field extension (already done by Option B, 2026-07-02)

Total: ≥20 IOCTL entries (with duplicates via natural numbering).

#### Scenario: each IOCTL maps to one handler

- **WHEN** `drm_ioctl()` dispatches any registered IOCTL command
- **THEN** a unique handler function MUST be invoked per command
- **AND** the `drm_ioctl_desc[]` array MUST have one entry per `GPU_IOCTL_*` constant defined in `gpu_ioctl.h`

### Requirement: 1.2/1.3 边界契约 G1-G4（design.md Decision 5）

The system MUST lock the following interface contracts between stage 1.2 and 1.3 to prevent premature coupling:

| Contract | 1.2 Guarantee | 1.3 Expectation |
|----------|---------------|------------------|
| `struct drm_device` lifetime | Same as `GpgpuDevice` (init on create, shutdown before dtor) | uvm module holds `drm_device*` valid for device lifetime |
| BO refcount | `drm_gem_object` refcount drops to 0 on close(fd) | hmm_range never references released BO |
| prime import buffer release order | `dma_buf_unmap` → `dma_buf_detach` → `dma_buf_put` (Linux 6.12) | mmu_notifier invalidate completes before `dma_buf_detach` |
| fence timing | All fences signal before GEM object release | hmm_range fault completes before triggering GEM release |

#### Scenario: 1.2 leaves skeleton for 1.3 lifecycle test (G1)

- **WHEN** stage 1.2 implementation completes
- **THEN** `tests/test_uvm_drm_lifecycle_standalone.cpp` MUST exist as a skeleton with at least one passing test case covering basic BO release order
- **AND** the test MUST carry an explicit `// 1.3 will extend this` marker

#### Scenario: 1.2 does not pre-implement 1.3 dependencies (G4)

- **WHEN** `git grep` searches stage 1.2 codebase for `mmu_notifier` / `hmm_range_fault` / `hmm_migrate`
- **THEN** zero implementations MUST appear (these belong to stage 1.3)
- **AND** interfaces in `drm_prime.h` MAY declare (forward declarations) but MUST NOT define bodies

#### Scenario: dma_buf API signatures match Linux 6.12 ABI (G2)

- **WHEN** `linux_compat/drm/drm_prime.h` is inspected
- **THEN** `dma_buf_dynamic_attach` / `dma_buf_detach` / `dma_buf_map_attachment` / `dma_buf_unmap_attachment` / `dma_buf_pin` / `dma_buf_unmap` MUST have Linux 6.12 ABI signatures
- **AND** `struct dma_buf_attach_ops` MUST contain `allow_peer2peer` and `move_notify` fields
- **AND** KFD compiles with errors=0 against these signatures (validated via Option C PoC, 2026-07-02)

### Requirement: errno mapping 端到端一致性（Blind Spot 3）

The system MUST route all DRM IOCTL handler errno returns through `errno_to_linux()` helper. Error codes MUST match Linux 6.12 ABI byte-exactly. Documented in `docs/05-advanced/drm-error-semantics.md` with at least 5 rows.

#### Scenario: errno mapping table exists

- **WHEN** `cat docs/05-advanced/drm-error-semantics.md` runs
- **THEN** the file MUST exist
- **AND** it MUST contain rows for at least `-EACCES`, `-EFAULT`, `-ENOMEM`, `-EREMOTEIO`, `-ENOSPC` each with code value (e.g., `-EFAULT = -14`)

#### Scenario: test_drm_ioctl_dispatch_standalone covers errno

- **WHEN** `tests/test_drm_ioctl_dispatch_standalone.cpp` runs
- **THEN** it MUST contain at least one assertion that calls a DRM IOCTL with invalid arguments
- **AND** the returned value MUST equal the Linux 6.12 ABI value exactly

### Requirement: DRM 兼容矩阵文档（Blind Spot 5，Linux 6.6 ↔ 6.12）

The system MUST produce `docs/05-advanced/drm-compat-matrix.md` documenting differences between Linux 6.6 LTS and 6.12 LTS in the DRM subset. The document MUST include at least 3 categories of differences:

#### Scenario: compat matrix exists with required categories

- **WHEN** `cat docs/05-advanced/drm-compat-matrix.md` runs
- **THEN** the file MUST exist and contain sections covering:
  - struct layout changes (must include `struct dma_buf.list_node` conditional on `CONFIG_DEBUG_FS`)
  - function signature changes (must note that dma_buf_attach() is unchanged but unused by amdgpu)
  - new required ops (must note none added)
- **AND** each row MUST state UsrLinuxEmu's simulation strategy

### Requirement: render node + primary node 权限分离（ADR-037 + Blind Spot 4）

The system MUST register both `/dev/dri/renderD128` (render node) and `/dev/dri/card0` (primary node) with Linux udev-default permissions. Mode bits MUST be 0666 with uid:gid = 0:0. The implementation MUST be backed by ADR-037 (which is already created, 2026-07-02). The system MUST use the VFS infrastructure extended in VFS-1~VFS-4 (also 2026-07-02).

#### Scenario: both nodes register and open successfully

- **WHEN** `render_node.cpp` runs at module load
- **THEN** both `/dev/dri/renderD128` and `/dev/dri/card0` MUST be discoverable via VFS
- **AND** `VFS::access("/dev/dri/renderD128", 6)` MUST return 0 (R_OK|W_OK with mode 0666)
- **AND** `VFS::chmod("/dev/dri/card0", 0666)` MUST succeed

### Requirement: 测试交付（4 个 Catch2 standalone）

The system MUST provide 4 Catch2 standalone test executables:

| Test | Range |
|------|-------|
| `test_drm_gem_standalone` | GEM object lifecycle + ASan validation |
| `test_drm_ioctl_dispatch_standalone` | ≥20 IOCTL dispatch + errno mapping |
| `test_render_node_standalone` | render node registration + permission verification |
| `test_uvm_drm_lifecycle_standalone` | 1.2/1.3 boundary contract G1 skeleton |

All MUST pass with `ctest --output-on-failure` from project root.

#### Scenario: all 4 tests pass after stage 1.2

- **WHEN** stage 1.2 implementation completes and `ctest --output-on-failure` runs
- **THEN** the 4 new standalone tests MUST exit 0
- **AND** all 41 pre-existing tests MUST remain green (zero regression)

### Requirement: 不引入 HAL 接口扩展（design.md Decision 4 / ADR-035 guardrail）

This change MUST NOT add any `hal_drm_*` entries to `struct gpu_hal_ops` (the 11-function-pointer table defined per ADR-023). HAL extensions are FORBIDDEN per ADR-035 unless KFD driver code in stage 1.4 demonstrably requires them. This requirement acts as a guardrail and is captured in Oracle's Launch Condition C4.

#### Scenario: gpu_hal_ops unchanged by stage 1.2

- **WHEN** `git diff plugins/gpu_driver/hal/include/hal_ops.h include/linux_compat/drm/` is computed
- **THEN** `struct gpu_hal_ops` MUST have zero additions or modifications
- **AND** `openspec/changes/stage-1-2-drm-subset/specs/hal-drm-ops-audit.md` MUST exist (even if 0 ops added) recording the decision rationale

### Requirement: KFD IOCTL 编号已在 System C 预留（Option B, 2026-07-02 已完成）

The system MUST have reserved 4 new GPU_IOCTL_* numbers (0x44-0x47) and extended CREATE_QUEUE (0x40) with KFD-compat fields. The TaskRunner mirror MUST stay in sync via `scripts/check_gpu_ioctl_sync.sh`. This requirement is already satisfied (completed 2026-07-02).

#### Scenario: dual-side IOCTL sync maintained

- **WHEN** `scripts/check_gpu_ioctl_sync.sh` runs
- **THEN** it MUST exit 0 reporting `OK: 15 GPU_IOCTL_* entries in sync`
- **AND** `plugins/gpu_driver/shared/gpu_ioctl.h` MUST contain 4 new IOCTL define entries for `0x44` through `0x47`

## REMOVED Requirements

None.

## RENAMED Requirements

None.