# B.1.9 Design: kfd_svm.h Extension — SVM Range Stub

> **Status**: 📋 DRAFT (2026-07-15)
> **Author**: Sisyphus (Architecture Team)
> **Parent**: C-12 `2026-08-15-stage1-4-kfd-multi-file-integration` Phase B.1.9
> **Source**: `docs/05-advanced/kfd-abi-comparison-report.md` §2.2 + §4 (APPROVED 2026-07-15)
> **Predecessor**: B.1.8 (kfd_topology.h, commit pending)
> **Successor**: B.1.3 (kfd_pasid.c) — now unblocked

---

## 1. Scope

B.1.9 provides the SVM (Shared Virtual Memory) range lookup stub that
`kfd_queue.c` calls at line 97 via `svm_range_from_addr(&p->svms, addr, NULL)`.
Currently only a forward declaration exists; the function has no implementation.

### Files modified

| File | Change | Lines added (est.) |
|------|--------|-------------------|
| `plugins/gpu_driver/drv/kfd/kfd_svm.h` | Verify struct fields + add init guard macro | ~10 |
| `plugins/gpu_driver/drv/kfd/kfd_svm.c` (new) | Stub implementation | ~40 |

### Files NOT modified

- `kfd_queue.c` — no change (already calls the function)
- `kfd_priv.h` / `kfd_topology.h` — already extended in B.1.7 / B.1.8

---

## 2. Struct Field Whitelist

### 2.1 `struct svm_range_list` (existing 5 fields → keep as-is)

| Field | Type | ABI §2.2 ref | Rationale |
|-------|------|----------------|-----------|
| `lock` | `struct mutex` | §2.2.1 #8 (svms.lock) | kfd_queue.c:88/134/148/168 call site |
| `objects` | `struct rb_root` | §2.2.1 #8 (svms.objects) | kfd_queue.c:150 call site |
| `list` | `struct list_head` | (reserved) | kfd_svm.h:30 — for future kfd_process.c / kfd_mmu.c |
| `deferred_range_list` | `struct list_head` | (reserved) | kfd_svm.h:31 — for future kfd_mmu.c invalidation queue |

**Decision**: No new fields. ABI §2.5 self-check confirms existing fields cover
all C-12 call sites.

### 2.2 `struct svm_range` (existing 11 fields → keep as-is)

Already has: it_node, update_list, child_list, start, last, bitmap_access,
bitmap_aip, flags, mapped_to_gpu, queue_refcount.

**Decision**: No changes. Upstream Linux 6.12 LTS `svm_range` has additional
fields (`svms`, `notifier`, `mr`, `mm`, `evicting_buffers_count`, etc.) that
are EXCLUDED per ABI §2.2.2 (C-12 does not implement mmu_notifier, range
eviction, or hmm_range).

---

## 3. Function Implementation

### 3.1 `svm_range_from_addr(svms, addr, unused)` stub

Currently forward-declared at kfd_svm.h:52 with NO implementation.
kfd_queue.c:97 calls this function. Must provide implementation or the
linker will fail when kfd_queue.o is linked into an executable.

**C-12 stub strategy**: Return NULL. C-12 sim does not maintain SVM range
trees (no real GPU memory mapping). kfd_queue.c:97 is in the `kfd_cleanup_bo_vas`
path which is only exercised on error/cleanup; returning NULL is safe.

```c
// kfd_svm.c
struct svm_range *svm_range_from_addr(struct svm_range_list *svms,
                                       u64 addr, void *unused) {
  (void)svms;
  (void)addr;
  (void)unused;
  return NULL;  /* C-12: no SVM range tree in sim */
}
```

### 3.2 Range tree stub (per tasks.md B.1.9)

Tasks.md says: "补全 struct kfd_svm + range tree stub（与 B.3.1 协同）".

C-12 stub: `svm_range_list_init(svms)` that initializes the lock and list
heads. Called from `kfd_process_create()` (B.1.5 future module).

```c
void svm_range_list_init(struct svm_range_list *svms) {
  mutex_init(&svms->lock);
  svms->objects.rb_node = NULL;
  INIT_LIST_HEAD(&svms->list);
  INIT_LIST_HEAD(&svms->deferred_range_list);
}
```

---

## 4. Migration to Real Linux Kernel

When porting to real Linux kernel:

1. **Delete** `kfd_svm.c` (real implementation is in upstream
   `drivers/gpu/drm/amd/amdkfd/kfd_svm.c` ~4000 lines)
2. **Add** `#include <linux/.../amdkfd/kfd_svm.h>` from linux_compat
3. **Extend** `struct svm_range` with upstream fields (svms, notifier, mr,
   mm, evicting_buffers_count) per ABI §2.2.2 exclusion reversal
4. `svm_range_from_addr` becomes a real interval tree lookup (rb-tree)

---

## 5. Verification

After B.1.9 commit:

- [ ] `kfd_queue.c` links cleanly (no undefined reference to `svm_range_from_addr`)
- [ ] `make -j4` succeeds (88/88 ctest unchanged)
- [ ] `docs-audit.sh --strict` 43/43 PASS
- [ ] `nm libgpu_kfd.a | grep svm_range` shows new symbols

---

## 6. Risk Assessment

| Risk | Mitigation |
|------|------------|
| Stub returning NULL breaks B.3.1 SVM range tree usage | B.3.1 is BLOCKED on B.1.9 + B.1.3; B.1.9 establishes the API contract; B.3.1 will replace NULL with real lookup |
| `INIT_LIST_HEAD` macro not available in linux_compat | linux_compat/list.h provides this (verified in kfd_svm.h existing code) |
| `svm_range_list_init` not called from any kfd_*.c yet | B.1.5 (kfd_process.c) will call it; for now it's a public API ready for use |
