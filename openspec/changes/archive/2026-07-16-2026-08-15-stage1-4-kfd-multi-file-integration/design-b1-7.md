# B.1.7 Design: kfd_priv.h Extension — Field Whitelist → Struct Mapping

> **Status**: 📋 DRAFT (2026-07-15)
> **Author**: Sisyphus (Architecture Team)
> **Parent**: C-12 `2026-08-15-stage1-4-kfd-multi-file-integration` Phase B.1.7
> **Source**: `docs/05-advanced/kfd-abi-comparison-report.md` §2 (APPROVED 2026-07-15)
> **Scope**: Extend `plugins/gpu_driver/drv/kfd/kfd_priv.h` per ADR-059 D4 decision
> **Excludes**: B.1.8 (kfd_topology.h) and B.1.9 (kfd_svm.h) — separate tasks

---

## 1. Scope

B.1.7 extends `kfd_priv.h` to add the 4 missing struct declarations identified in
the ABI comparison report §2. All field additions follow the **whitelist-only**
policy: fields not in §2.1-§2.4 are NOT declared (avoids transitive include trap
that caused Stage 1.4 PoC 5341c3f's 8 failures).

### Files modified

| File | Change | Lines added (est.) |
|------|--------|-------------------|
| `plugins/gpu_driver/drv/kfd/kfd_priv.h` | Add 4 structs + supporting types | ~120 |

### Files NOT modified (separate B.1.x tasks)

- `kfd_svm.h` — extended in B.1.9
- `kfd_topology.h` — extended in B.1.8
- `kfd_queue.c` — no change (already compiles against new structs via §2.5 self-check call sites)
- Any `kfd_*.c` file — no implementation in B.1.7 (header-only extension)

---

## 2. Field Whitelist → Struct Mapping

### 2.1 `struct kfd_dev` extension (13 fields → kfd_priv.h)

All fields from ABI report §2.1.1. C-12 stub types used where transitive include
risk exists.

| # | Field | C-12 Type | Source path | Stub rationale |
|---|-------|-----------|-------------|----------------|
| 1 | `id` | `unsigned int` | Linux 6.12 kfd_priv.h | direct |
| 2 | `xcc_mask` | `uint32_t` | Linux 6.12 kfd_priv.h | direct |
| 3 | `node` | `struct kfd_node *` | Linux 6.12 kfd_priv.h | kfd_node already stubbed (kfd_priv.h:51) |
| 4 | `dev` | `void *` | Linux 6.12 device.h | stub — `<linux/device.h>` transitive risk |
| 5 | `kfd_vm` | `void *` | Linux 6.12 kfd_priv.h | stub — full amdgpu_vm triggers 5341c3f #3 |
| 6 | `kfd2kgd` | `void *` | Linux 6.12 kfd2kgd.h | stub — HAL replaces (ADR-059 §D3) |
| 7 | `pci_vendor` | `u16` | Linux 6.12 pci.h | direct |
| 8 | `pci_device` | `u16` | Linux 6.12 pci.h | direct |
| 9 | `domain` | `void *` | Linux 6.12 iommu.h | stub — mm_shim doesn't hold real iommu_domain |
| 10 | `processes_lock` | `struct mutex` | local (pthread) | ADR-018 decision 3 (pthread_mutex_t wrapper) |
| 11 | `processes_list` | `struct list_head` | Linux 6.12 list.h | direct (linux_compat/list.h provides) |
| 12 | `init_complete` | `bool` | Linux 6.12 kfd_priv.h | direct (simple atomic flag) |
| 13 | `gpu_id` | `uint32_t` | Linux 6.12 kfd_priv.h | direct |

**Note**: `dbgdev` and `gws` from §2.1.1 are explicitly EXCLUDED (per §2.1.2
explicit exclusion list: "C-12 不使用 GWS, NULL stub 即可" — and "C-12 不调
dbgdev API"). These would be addable later if needed but are not in the 13-field
whitelist we are adding now.

### 2.2 `struct kfd_process` extension (10 fields → kfd_priv.h)

| # | Field | C-12 Type | Stub rationale |
|---|-------|-----------|----------------|
| 1 | `mm` | `struct mm_struct *` | local decl §2.4 (kfd_priv.h) |
| 2 | `lead_thread` | `void *` | stub — task_struct not needed (C-12) |
| 3 | `pid` | `pid_t` | direct (POSIX pid_t) |
| 4 | `pasid` | `u32` | direct |
| 5 | `n_queues` | `int` | simplified pqn (§2.2.1 note: no full DQM) |
| 6 | `queues_lock` | `struct mutex` | pthread wrapper |
| 7 | `queues_list` | `struct list_head` | direct |
| 8 | `doorbell_id` | `u32` | direct |
| 9 | `doorbell_kernel_addr` | `uint64_t` | stub — void __iomem not needed |
| 10 | `n_pdds` | `u32` | direct |
| 11 | `pdds` | `struct kfd_process_device *[8]` | fixed array; MAX_KFD_DEVICES=8 |

**Note**: `svms` field already exists at kfd_priv.h:64 — B.1.7 does NOT re-add
it. B.1.9 will extend the `svm_range_list` type itself.

### 2.3 `struct kfd_process_device_private_data` (NEW, 6 fields → kfd_priv.h)

This is a NEW struct (not an extension). Currently `kfd_process_device` exists
at kfd_priv.h:67-71 with `dev`, `process`, `drm_priv`. The new struct holds
per-PDD private data that C-12 needs for aperture queries.

| # | Field | C-12 Type | Source |
|---|-------|-----------|--------|
| 1 | `gpu_va_base` | `u64` | direct (ABI §2.3.1) |
| 2 | `gpu_va_limit` | `u64` | direct |
| 3 | `vm` | `void *` | stub — amdgpu_vm transitive risk |
| 4 | `process` | `struct kfd_process *` | back-reference |
| 5 | `dev` | `struct kfd_node *` | back-reference |
| 6 | `drm_priv` | `void *` | from existing stub |

**Decision**: Keep `kfd_process_device` (with 3 stub fields) and add
`kfd_process_device_private_data` as a separate struct. This matches upstream
Linux 6.12 LTS pattern (kfd_process_device holds the "public" interface;
kfd_process_device_private_data holds per-pdd internal state).

### 2.4 `struct mm_struct` local declaration (4 fields → kfd_priv.h)

Local declaration (no `<linux/mm_types.h>` include). Phase C.2.1 will use this
for PID + VMA tracking per kfd-multi-file.md §5.3.

| # | Field | C-12 Type | Stub rationale |
|---|-------|-----------|----------------|
| 1 | `mm_users` | `int` | simplified atomic_t (C-12 doesn't need atomic refcount for single-process sim) |
| 2 | `mm_count` | `int` | same |
| 3 | `pgd` | `void *` | stub — no real page table in sim |
| 4 | `mmap` | `void *` | stub — Phase C.2.1 will fill |

---

## 3. Migration to Real Linux Kernel

When porting `kfd_priv.h` to a real Linux kernel build:

1. **Remove** the local `struct mm_struct` (real one is in `<linux/mm_types.h>`)
2. **Remove** the `struct mutex` wrapper block (real one is in `<linux/mutex.h>`)
3. **Change** all `void *` stub types back to their upstream types:
   - `kfd_dev.dev` → `struct device *`
   - `kfd_dev.kfd_vm` → `struct amdgpu_vm *`
   - `kfd_dev.kfd2kgd` → `struct kfd2kgd_calls *`
   - `kfd_dev.domain` → `struct iommu_domain *`
   - `kfd_process.lead_thread` → `struct task_struct *`
   - `kfd_process.doorbell_kernel_addr` → `void __iomem *`
   - `kfd_process_device_private_data.vm` → `struct amdgpu_vm *`
   - `mm_struct.pgd` → `pgd_t *`
   - `mm_struct.mmap` → `struct vm_area_struct *`
4. **Add** the `<linux/...>` includes that were stubbed out
5. **Change** `int` for `mm_users`/`mm_count` to `atomic_t`

The `kfd_queue.c` source code does NOT need changes — it only references fields
that are already in the whitelist.

---

## 4. Verification

After B.1.7 commit:

- [ ] `kfd_queue.c` still compiles (no symbol changes)
- [ ] `make -j4` succeeds (88/88 ctest unchanged)
- [ ] `docs-audit.sh --strict` 43/43 PASS (no new ADR/doc changes)
- [ ] `nm libgpu_kfd.a | grep -E "kfd_(dev|process|process_device_private_data|mm_struct)"` shows new symbols

---

## 5. Risk Assessment

| Risk | Mitigation |
|------|------------|
| Stub type choices don't match future Phase B usage | Phase B.3 (mmu), B.4 (events) will validate; adjust if needed |
| `struct mutex` wrapper missing `pthread_mutex_destroy` in any path | All kfd_*.c files must call `mutex_init`/`mutex_destroy` symmetrically (B.1.1+ enforces) |
| `kfd_process_device_private_data` naming diverges from upstream | Per ABI §2.3 note: "C-12 用 kfd_process_device_private_data 作为内部 per-pdd 数据，与 kfd_process_device 配对" — matches upstream |
| `mm_struct` field count grows beyond 4 in Phase C.2 | ABI §2.4.2 explicit exclusion list is the contract; any new field needs §2.4.1 update + report §6.2 re-approval |
