# B.1.8 Design: kfd_topology.h Extension — Topology Stub

> **Status**: 📋 DRAFT (2026-07-15)
> **Author**: Sisyphus (Architecture Team)
> **Parent**: C-12 `2026-08-15-stage1-4-kfd-multi-file-integration` Phase B.1.8
> **Source**: `docs/05-advanced/kfd-abi-comparison-report.md` §2.1 + §4 (APPROVED 2026-07-15)
> **Scope**: Extend `plugins/gpu_driver/drv/kfd/kfd_topology.h` per ADR-059 D4 decision
> **Predecessor**: B.1.7 (kfd_priv.h extension, commit f9eb6c5)
> **Successor**: B.1.9 (kfd_svm.h)

---

## 1. Scope

B.1.8 provides the topology stub layer that `kfd_queue.c` calls at lines 227/329
via `kfd_topology_device_by_id(pdd->dev->id)`. Currently only a forward
declaration exists; the function has no implementation.

### Files modified

| File | Change | Lines added (est.) |
|------|--------|-------------------|
| `plugins/gpu_driver/drv/kfd/kfd_topology.h` | Verify struct fields + add init guard macro | ~10 |
| `plugins/gpu_driver/drv/kfd/kfd_topology.c` (new) | Stub implementation + static device array | ~60 |

### Files NOT modified

- `kfd_queue.c` — no change (already calls the function)
- `kfd_priv.h` — no change (B.1.7 already done)
- `kfd_svm.h` — separate task (B.1.9)

---

## 2. Struct Field Whitelist

### 2.1 `struct kfd_topology_device` (existing 3 fields → keep as-is)

| Field | Type | ABI §2.1.1 ref | Rationale |
|-------|------|----------------|-----------|
| `node_props` | `struct kfd_node_properties` | §2.1.1 #1 (id), #2 (xcc_mask) | kfd_queue.c:227/329 call site |
| `id` | `u32` | §2.1.1 #1 | kfd_topology index (returned by `by_id`) |
| `gpu` | `struct kfd_node *` | §2.1.1 #3 | kfd_node back-reference |

**Decision**: No new fields needed. Upstream Linux 6.12 LTS `kfd_topology_device`
has additional fields (e.g., `olp`, `add_capability`) that are EXCLUDED per
ABI §2.1.2 (C-12 does not implement offload processing or capability bitmap).

### 2.2 `struct kfd_node_properties` (existing 9 fields → keep as-is)

Already has: simd_count, simd_per_cu, simd_arrays_per_engine, array_count,
gfx_target_version, ctl_stack_size, cwsr_size, debug_memory_size, eop_buffer_size.

**Decision**: No changes. C-12 uses `gfx_target_version` and `ctl_stack_size`
per kfd_queue.c:286,333 (NUM_XCC macro call site).

---

## 3. Function Implementation

### 3.1 `kfd_topology_device_by_id(u32 gpu_id)` stub

Currently forward-declared at kfd_topology.h:34 with NO implementation.
kfd_queue.c:227,329 calls this function. Must provide implementation or the
linker will fail when kfd_queue.o is linked into an executable.

**C-12 stub strategy**: Single static topology device (GPU 0). For C-12
single-GPU simulation, `gpu_id == 0` returns the static device; all other
IDs return NULL.

```c
// kfd_topology.c
static struct kfd_node kfd_static_node = { .id = 0, .xcc_mask = 0x1 };
static struct kfd_node_properties kfd_static_props = {
  .simd_count = 4, .simd_per_cu = 4, .array_count = 1,
  .gfx_target_version = 0x90006,  /* GFX9 placeholder */
  .ctl_stack_size = 4096, .cwsr_size = 8192,
};
static struct kfd_topology_device kfd_static_topo = {
  .id = 0, .gpu = &kfd_static_node, .node_props = kfd_static_props,
};

struct kfd_topology_device *kfd_topology_device_by_id(u32 gpu_id) {
  return (gpu_id == 0) ? &kfd_static_topo : NULL;
}
```

### 3.2 Node discovery stub (per tasks.md B.1.8)

Tasks.md says: "补全 struct kfd_topology_device + 节点发现 stub".

C-12 stub: `kfd_topology_init(void)` that registers the single static
device. Called from `kfd_module_init()` (B.1.1 stub currently returns 0;
future: will call `kfd_topology_init()` first).

```c
int kfd_topology_init(void) {
  /* C-12: single GPU topology; future multi-GPU will populate array */
  return 0;
}
```

---

## 4. Migration to Real Linux Kernel

When porting to real Linux kernel:

1. **Delete** `kfd_topology.c` (real implementation is in upstream
   `drivers/gpu/drm/amd/amdkfd/kfd_topology.c` ~3000 lines)
2. **Add** `#include <linux/.../amdkfd/kfd_topology.h>` from linux_compat
3. **Remove** the static device stub (real topology discovery via PCI scan)
4. `kfd_module_init()` will call real `kfd_topology_init()` (not our stub)

---

## 5. Verification

After B.1.8 commit:

- [ ] `kfd_queue.c` links cleanly (no undefined reference to `kfd_topology_device_by_id`)
- [ ] `make -j4` succeeds (88/88 ctest unchanged)
- [ ] `docs-audit.sh --strict` 43/43 PASS
- [ ] `nm libgpu_kfd.a | grep kfd_topology` shows new symbol

---

## 6. Risk Assessment

| Risk | Mitigation |
|------|------------|
| Static device doesn't match real GPU 0 topology | kfd_queue.c only uses `id` and `node_props.gfx_target_version` — both are set |
| Multi-GPU scenarios (future) need different stub | Comment in source marks this as single-GPU placeholder |
| `kfd_topology_init` stub not called from B.1.1 stub yet | B.1.1 stub returns 0; future module impl will call topology/svm/process init in order |
