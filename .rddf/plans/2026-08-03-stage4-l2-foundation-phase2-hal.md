# 2026-08-03-stage4-l2-foundation-phase2-hal Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add ~28 new fn-ptrs to `struct gpu_hal_ops` (append-only per ADR-023 Decision 4) covering 5 sim headers (graph, mem_pool, stream_capture, gpu_queue_emu, hardware_puller_emu), so the 5 subsequent removal changes can migrate drv/ from direct sim/ includes to HAL fn-ptr calls.

**Architecture:** Strictly append-only extension. New fn-ptrs added at the END of `struct gpu_hal_ops` (after `heap_ptr`). Inline wrappers forward 1:1 to the fn-ptr. `hal_user.cpp` lambdas delegate to existing `sim_*` functions (no new logic). `hal_mock.cpp` returns test-friendly defaults. Class types (`GpuQueueEmu`, `HardwarePullerEmu`) are exposed as opaque `uint64_t` handles — drv/ side will cast back when removal changes land.

**Tech Stack:** C99-compatible C (HAL interface), C++17 lambdas (impls), Catch2 test framework.

---

## File Structure

### Production Code (Modify only)

| File | Responsibility |
|---|---|
| `plugins/gpu_driver/hal/gpu_hal.h` | Append ~28 fn-ptr declarations to `struct gpu_hal_ops` + ~28 inline wrappers |
| `plugins/gpu_driver/hal/hal_user.cpp` | Append ~28 lambda assignments in `hal_user_init()` delegating to `sim_*` |
| `plugins/gpu_driver/hal/hal_mock.cpp` | Append ~28 lambda assignments in `hal_mock_init()` returning test-friendly defaults |
| `plugins/gpu_driver/shared/gpu_hal_handles.h` | NEW: forward declarations + opaque handle typedefs for `GpuQueueEmu`, `HardwarePullerEmu` |

### Tests (existing — regression gate)

| File | Responsibility |
|---|---|
| `tests/test_context_type_standalone.cpp` | Verifies `hal_green_context_*` still works |
| `tests/test_pdl_standalone.cpp` | Verifies `hal_pdl_*` still works |
| `tests/test_priority_sched_standalone.cpp` | Verifies `hal_preempt`, `hal_resume` still work |
| `tests/test_gpu_hal_standalone.cpp` (if exists) | General HAL fn-ptr smoke test |

---

## Pre-Task: Audit sim/ signatures

Before writing fn-ptr declarations, run this audit to extract exact signatures:

```bash
cd /workspace/project/UsrLinuxEmu
echo "===graph.h==="
grep -E "^int sim_graph_|^void sim_graph_" plugins/gpu_driver/sim/graph.h
echo "===mem_pool.h==="
grep -E "^int sim_mem_pool_|^void sim_mem_pool_" plugins/gpu_driver/sim/mem_pool.h
echo "===stream_capture.h==="
grep -E "^int sim_stream_capture_|^void sim_stream_capture_" plugins/gpu_driver/sim/stream_capture.h
echo "===gpu_queue_emu.h (class methods)==="
grep -E "^\s+(int|void|auto|class|GpuQueueEmu)" plugins/gpu_driver/sim/gpu_queue_emu.h | head -25
echo "===hardware_puller_emu.h (class methods)==="
grep -E "^\s+(int|void|auto|class)" plugins/gpu_driver/sim/hardware/hardware_puller_emu.h | head -25
```

Document the exact return type, name, and parameter types of each function used in drv/. Use this audit as the source of truth for fn-ptr signatures below.

---

### Task 1: Switch to branch + verify clean

**Files:**
- Modify: branch reference (no file edits)

- [ ] **Step 1.1: Verify branch exists**

Run: `cd /workspace/project/UsrLinuxEmu && git branch --list 'openspec/2026-08-03-stage4-l2-foundation-phase2-hal'`
Expected: `openspec/2026-08-03-stage4-l2-foundation-phase2-hal`

- [ ] **Step 1.2: Switch to branch**

Run: `cd /workspace/project/UsrLinuxEmu && git checkout openspec/2026-08-03-stage4-l2-foundation-phase2-hal`
Expected: `Switched to branch 'openspec/2026-08-03-stage4-l2-foundation-phase2-hal'`

- [ ] **Step 1.3: Verify clean tree**

Run: `git status --porcelain`
Expected: empty (no output)

- [ ] **Step 1.4: Commit baseline (no-op if already at f6ceca1)**

Run: `git log --oneline -1`
Expected: `f6ceca1 feat(openspec): stage4-l2-foundation Phase 2 (5 headers, ~28 fn-ptrs)` (or later commit)

---

### Task 2: Create shared handles header for opaque types

**Files:**
- Create: `plugins/gpu_driver/shared/gpu_hal_handles.h`

- [ ] **Step 2.1: Audit class methods**

Run: `cat plugins/gpu_driver/sim/gpu_queue_emu.h plugins/gpu_driver/sim/hardware/hardware_puller_emu.h | grep -E "^\s+(int|void|auto)\s+\w+\(" | head -30`
Expected: list of public methods on `GpuQueueEmu` and `HardwarePullerEmu` that drv/ calls

- [ ] **Step 2.2: Create opaque handle header**

Write `plugins/gpu_driver/shared/gpu_hal_handles.h`:

```c
/*
 * gpu_hal_handles.h — Opaque handles for HAL class types
 *
 * Per ADR-023 Decision 4: HAL interface is C-compatible, no C++ classes
 * in fn-ptr signatures. Class types like GpuQueueEmu/HardwarePullerEmu
 * are exposed as uint64_t handles. The drv/ side casts back when
 * subsequent removal changes land.
 *
 * Append-only per ADR-023 Decision 4.
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration of opaque HAL-side queue handle.
 * Maps to class GpuQueueEmu* in C++ (see hal_user.cpp cast sites).
 * drv/ removal changes will cast this back to shared_ptr<GpuQueueEmu>
 * via shared/hal_queue_handle.h (separate header, added in removal change). */
typedef uint64_t hal_queue_handle_t;

/* Forward declaration of opaque HAL-side hardware puller handle.
 * Maps to class HardwarePullerEmu* in C++ (see hal_user.cpp cast sites).
 * drv/ removal changes will cast back via shared/hal_puller_handle.h. */
typedef uint64_t hal_puller_handle_t;

#ifdef __cplusplus
}
#endif
```

- [ ] **Step 2.3: Verify compile of header alone**

Run: `cd build && cmake --build . --target kernel -j4 2>&1 | tail -5`
Expected: clean build (header not yet included anywhere — this just verifies syntax via include resolution in next tasks)

---

### Task 3: Add graph fn-ptrs to gpu_hal.h (8 fn-ptrs + 8 wrappers)

**Files:**
- Modify: `plugins/gpu_driver/hal/gpu_hal.h:208` (append after `heap_ptr` fn-ptr line, before closing `};`)
- Modify: `plugins/gpu_driver/hal/gpu_hal.h:371` (append after `hal_heap_ptr` wrapper, before `#ifdef __cplusplus`)

- [ ] **Step 3.1: Add #include for graph types if needed**

Check `plugins/gpu_driver/sim/graph.h` for any types used in signatures. If `sim_graph_node_type_t` or similar is referenced, add to top of gpu_hal.h:

```c
#include "shared/gpu_hal_handles.h"  // hal_queue_handle_t, hal_puller_handle_t
```

Skip if no extra types are needed (most signatures use only `uint64_t*` for handles).

- [ ] **Step 3.2: Append graph fn-ptrs to struct**

After line 208 (after `void* (*heap_ptr)(void *ctx, uint64_t gpu_va);`), insert:

```c

  /* ── Stage 4.6 L2 foundation (ADR-072 §Decision 4) — Phase 2 ───
   * Append-only per ADR-023 Decision 4. Enables drv/ migration off
   * sim/graph.h direct includes (5 removal changes to follow). */

  /* graph_create: create an empty graph. Returns 0 on success.
   * @out_handle [out] graph handle */
  int (*graph_create)(void *ctx, uint64_t *out_handle);

  /* graph_destroy: release a graph handle. */
  int (*graph_destroy)(void *ctx, uint64_t handle);

  /* graph_add_kernel_node: append a kernel node to a graph. */
  int (*graph_add_kernel_node)(void *ctx, uint64_t graph, uint32_t kernel_idx);

  /* graph_add_memcpy_node: append a memcpy node (src/dst VA + size). */
  int (*graph_add_memcpy_node)(void *ctx, uint64_t graph,
                               uint64_t src_va, uint64_t dst_va, uint64_t size);

  /* graph_instantiate: compile graph into an executable handle. */
  int (*graph_instantiate)(void *ctx, uint64_t graph, uint64_t *out_exec);

  /* graph_launch: enqueue an executable graph on a stream. */
  int (*graph_launch)(void *ctx, uint64_t exec, uint32_t stream_id);

  /* graph_destroy_exec: release an executable graph handle. */
  int (*graph_destroy_exec)(void *ctx, uint64_t exec);
```

- [ ] **Step 3.3: Append graph inline wrappers**

After line 371 (after `hal_heap_ptr` wrapper), before `#ifdef __cplusplus`, insert:

```c

/* ── Stage 4.6 L2 foundation inline wrappers (Phase 2 — graph) ── */

static inline int hal_graph_create(struct gpu_hal_ops *hal, uint64_t *out) {
  return hal->graph_create(hal->ctx, out);
}

static inline int hal_graph_destroy(struct gpu_hal_ops *hal, uint64_t h) {
  return hal->graph_destroy(hal->ctx, h);
}

static inline int hal_graph_add_kernel_node(struct gpu_hal_ops *hal, uint64_t g,
                                            uint32_t kidx) {
  return hal->graph_add_kernel_node(hal->ctx, g, kidx);
}

static inline int hal_graph_add_memcpy_node(struct gpu_hal_ops *hal, uint64_t g,
                                            uint64_t src, uint64_t dst,
                                            uint64_t size) {
  return hal->graph_add_memcpy_node(hal->ctx, g, src, dst, size);
}

static inline int hal_graph_instantiate(struct gpu_hal_ops *hal, uint64_t g,
                                        uint64_t *out_exec) {
  return hal->graph_instantiate(hal->ctx, g, out_exec);
}

static inline int hal_graph_launch(struct gpu_hal_ops *hal, uint64_t exec,
                                   uint32_t stream_id) {
  return hal->graph_launch(hal->ctx, exec, stream_id);
}

static inline int hal_graph_destroy_exec(struct gpu_hal_ops *hal, uint64_t exec) {
  return hal->graph_destroy_exec(hal->ctx, exec);
}
```

- [ ] **Step 3.4: Verify gpu_hal.h compiles**

Run: `cd /workspace/project/UsrLinuxEmu/build && cmake --build . --target kernel -j4 2>&1 | tail -10`
Expected: no errors (header is consumed by hal_user.cpp and hal_mock.cpp, but they haven't been updated yet — build may warn about undefined references for graph_*)

- [ ] **Step 3.5: Commit graph fn-ptrs (interim checkpoint)**

Run:
```bash
cd /workspace/project/UsrLinuxEmu
git add plugins/gpu_driver/hal/gpu_hal.h plugins/gpu_driver/shared/gpu_hal_handles.h
git commit -m "feat(hal): Phase 2 L2 foundation — add 7 graph fn-ptrs + wrappers

Append-only per ADR-023 Decision 4. Enables 5 subsequent removal
changes to migrate drv/ off sim/graph.h includes. See
openspec/changes/2026-08-03-stage4-l2-foundation-phase2-hal."
```

---

### Task 4: Add mem_pool fn-ptrs to gpu_hal.h (9 fn-ptrs + 9 wrappers)

**Files:**
- Modify: `plugins/gpu_driver/hal/gpu_hal.h` (append after graph fn-ptrs)

- [ ] **Step 4.1: Check mem_pool prop types**

Run: `grep -E "typedef|struct" plugins/gpu_driver/sim/mem_pool.h | head -10`
Expected: `sim_mem_pool_props_t` and `sim_mem_pool_attr_t` typedefs

If these types are used in fn-ptr signatures, the HAL must expose them too. Add to gpu_hal.h after `gpu_hal_handles.h` include:

```c
#include "../sim/mem_pool.h"  // sim_mem_pool_props_t, sim_mem_pool_attr_t (Phase 2 — narrow surface)
```

Note: This breaks "no sim/ in HAL" goal — acceptable for Phase 2 since it's a transitional state until the removal changes fully eliminate drv/ → sim/ includes. Document in ADR-023 §Decision 4 revised.

- [ ] **Step 4.2: Append mem_pool fn-ptrs**

After graph fn-ptrs in struct, insert:

```c

  /* ── mem_pool fn-ptrs (Phase 2 — 9 fn-ptrs) ───────────────── */

  /* mem_pool_create: allocate a memory pool with given properties. */
  int (*mem_pool_create)(void *ctx, const struct sim_mem_pool_props* props,
                         uint64_t *out_handle);

  /* mem_pool_destroy: release a pool. */
  int (*mem_pool_destroy)(void *ctx, uint64_t handle);

  /* mem_pool_alloc: synchronous allocate. */
  int (*mem_pool_alloc)(void *ctx, uint64_t handle, uint64_t size,
                        uint64_t *out_va);

  /* mem_pool_alloc_async: async allocate, returns fence via out_fence. */
  int (*mem_pool_alloc_async)(void *ctx, uint64_t handle, uint64_t size,
                              int64_t *out_fence);

  /* mem_pool_free: synchronous free. */
  int (*mem_pool_free)(void *ctx, uint64_t handle, uint64_t va);

  /* mem_pool_free_async: async free, returns fence via out_fence. */
  int (*mem_pool_free_async)(void *ctx, uint64_t handle, uint64_t va,
                             int64_t *out_fence);

  /* mem_pool_set_attr: set pool attribute. */
  int (*mem_pool_set_attr)(void *ctx, uint64_t handle, uint32_t attr,
                           uint64_t val);

  /* mem_pool_get_attr: get pool attribute. */
  int (*mem_pool_get_attr)(void *ctx, uint64_t handle, uint32_t attr,
                           uint64_t *out_val);

  /* mem_pool_trim: trim pool to minimum size (memory reclaim). */
  int (*mem_pool_trim)(void *ctx, uint64_t handle, uint64_t min_bytes);
```

NOTE: If `sim_mem_pool_props` and `sim_mem_pool_attr_t` types are C++-only, the HAL must redefine them in shared/. Skip this task and use opaque `void*` for props/attr if types can't be made C-compatible — then leave this as a deferred item for a follow-up change.

- [ ] **Step 4.3: Append mem_pool inline wrappers**

After graph wrappers, insert:

```c

/* ── mem_pool inline wrappers (Phase 2) ───────────────────────── */

static inline int hal_mem_pool_create(struct gpu_hal_ops *hal,
                                      const struct sim_mem_pool_props* props,
                                      uint64_t *out) {
  return hal->mem_pool_create(hal->ctx, props, out);
}

static inline int hal_mem_pool_destroy(struct gpu_hal_ops *hal, uint64_t h) {
  return hal->mem_pool_destroy(hal->ctx, h);
}

static inline int hal_mem_pool_alloc(struct gpu_hal_ops *hal, uint64_t h,
                                     uint64_t size, uint64_t *out) {
  return hal->mem_pool_alloc(hal->ctx, h, size, out);
}

static inline int hal_mem_pool_alloc_async(struct gpu_hal_ops *hal, uint64_t h,
                                           uint64_t size, int64_t *out) {
  return hal->mem_pool_alloc_async(hal->ctx, h, size, out);
}

static inline int hal_mem_pool_free(struct gpu_hal_ops *hal, uint64_t h,
                                    uint64_t va) {
  return hal->mem_pool_free(hal->ctx, h, va);
}

static inline int hal_mem_pool_free_async(struct gpu_hal_ops *hal, uint64_t h,
                                           uint64_t va, int64_t *out) {
  return hal->mem_pool_free_async(hal->ctx, h, va, out);
}

static inline int hal_mem_pool_set_attr(struct gpu_hal_ops *hal, uint64_t h,
                                        uint32_t attr, uint64_t val) {
  return hal->mem_pool_set_attr(hal->ctx, h, attr, val);
}

static inline int hal_mem_pool_get_attr(struct gpu_hal_ops *hal, uint64_t h,
                                        uint32_t attr, uint64_t *out) {
  return hal->mem_pool_get_attr(hal->ctx, h, attr, out);
}

static inline int hal_mem_pool_trim(struct gpu_hal_ops *hal, uint64_t h,
                                    uint64_t min_bytes) {
  return hal->mem_pool_trim(hal->ctx, h, min_bytes);
}
```

- [ ] **Step 4.4: Verify gpu_hal.h still compiles**

Run: `cd /workspace/project/UsrLinuxEmu/build && cmake --build . --target kernel -j4 2>&1 | tail -10`
Expected: clean (or note sim_mem_pool_props visibility issue — address per Step 4.2 NOTE)

- [ ] **Step 4.5: Commit mem_pool fn-ptrs**

Run:
```bash
cd /workspace/project/UsrLinuxEmu
git add plugins/gpu_driver/hal/gpu_hal.h
git commit -m "feat(hal): Phase 2 L2 foundation — add 9 mem_pool fn-ptrs + wrappers"
```

---

### Task 5: Add stream_capture fn-ptrs to gpu_hal.h (3 fn-ptrs + 3 wrappers)

**Files:**
- Modify: `plugins/gpu_driver/hal/gpu_hal.h`

- [ ] **Step 5.1: Append stream_capture fn-ptrs**

After mem_pool fn-ptrs in struct, insert:

```c

  /* ── stream_capture fn-ptrs (Phase 2 — 3 fn-ptrs) ──────────── */

  /* stream_capture_begin: start recording ops on a stream. */
  int (*stream_capture_begin)(void *ctx, uint64_t stream_id, uint32_t mode);

  /* stream_capture_end: stop recording, return captured graph. */
  int (*stream_capture_end)(void *ctx, uint64_t stream_id,
                            uint64_t *out_graph);

  /* stream_capture_status: query capture state. */
  int (*stream_capture_status)(void *ctx, uint64_t stream_id,
                               uint32_t *out_status);
```

- [ ] **Step 5.2: Append stream_capture inline wrappers**

After mem_pool wrappers, insert:

```c

/* ── stream_capture inline wrappers (Phase 2) ─────────────────── */

static inline int hal_stream_capture_begin(struct gpu_hal_ops *hal,
                                           uint64_t stream_id, uint32_t mode) {
  return hal->stream_capture_begin(hal->ctx, stream_id, mode);
}

static inline int hal_stream_capture_end(struct gpu_hal_ops *hal,
                                         uint64_t stream_id,
                                         uint64_t *out_graph) {
  return hal->stream_capture_end(hal->ctx, stream_id, out_graph);
}

static inline int hal_stream_capture_status(struct gpu_hal_ops *hal,
                                            uint64_t stream_id,
                                            uint32_t *out_status) {
  return hal->stream_capture_status(hal->ctx, stream_id, out_status);
}
```

- [ ] **Step 5.3: Commit stream_capture fn-ptrs**

Run:
```bash
cd /workspace/project/UsrLinuxEmu
git add plugins/gpu_driver/hal/gpu_hal.h
git commit -m "feat(hal): Phase 2 L2 foundation — add 3 stream_capture fn-ptrs + wrappers"
```

---

### Task 6: Add gpu_queue_emu fn-ptrs to gpu_hal.h (5 fn-ptrs + 5 wrappers, opaque handle)

**Files:**
- Modify: `plugins/gpu_driver/hal/gpu_hal.h`

- [ ] **Step 6.1: Append gpu_queue_emu fn-ptrs**

After stream_capture fn-ptrs in struct, insert:

```c

  /* ── gpu_queue_emu fn-ptrs (Phase 2 — 5 fn-ptrs) ──────────────
   * GpuQueueEmu is a C++ class. We expose hal_queue_handle_t (uint64_t)
   * which drv/ side casts back to shared_ptr<GpuQueueEmu>. This avoids
   * leaking C++ types into the HAL interface (ADR-023 Decision 4). */

  /* queue_create: create a queue, returns opaque handle. */
  int (*queue_create)(void *ctx, uint32_t handle, uint32_t type,
                      uint32_t priority, uint32_t ring_size,
                      hal_queue_handle_t *out_q);

  /* queue_attach_shmem: attach shared memory region to queue. */
  int (*queue_attach_shmem)(void *ctx, hal_queue_handle_t q,
                            void *cpu_ptr, uint64_t size);

  /* queue_submit: submit GPFIFO entries, returns fence via out_fence. */
  int (*queue_submit)(void *ctx, hal_queue_handle_t q,
                      uint64_t gpfifo_addr, uint32_t count,
                      int64_t *out_fence);

  /* queue_destroy: release queue handle. */
  int (*queue_destroy)(void *ctx, hal_queue_handle_t q);

  /* queue_register_puller: register a hardware puller with the queue. */
  int (*queue_register_puller)(void *ctx, hal_queue_handle_t q,
                               hal_puller_handle_t puller);
```

- [ ] **Step 6.2: Append gpu_queue_emu inline wrappers**

After stream_capture wrappers, insert:

```c

/* ── gpu_queue_emu inline wrappers (Phase 2) ───────────────────── */

static inline int hal_queue_create(struct gpu_hal_ops *hal, uint32_t handle,
                                   uint32_t type, uint32_t priority,
                                   uint32_t ring_size,
                                   hal_queue_handle_t *out) {
  return hal->queue_create(hal->ctx, handle, type, priority, ring_size, out);
}

static inline int hal_queue_attach_shmem(struct gpu_hal_ops *hal,
                                         hal_queue_handle_t q,
                                         void *cpu_ptr, uint64_t size) {
  return hal->queue_attach_shmem(hal->ctx, q, cpu_ptr, size);
}

static inline int hal_queue_submit(struct gpu_hal_ops *hal,
                                   hal_queue_handle_t q,
                                   uint64_t gpfifo_addr, uint32_t count,
                                   int64_t *out) {
  return hal->queue_submit(hal->ctx, q, gpfifo_addr, count, out);
}

static inline int hal_queue_destroy(struct gpu_hal_ops *hal,
                                    hal_queue_handle_t q) {
  return hal->queue_destroy(hal->ctx, q);
}

static inline int hal_queue_register_puller(struct gpu_hal_ops *hal,
                                            hal_queue_handle_t q,
                                            hal_puller_handle_t puller) {
  return hal->queue_register_puller(hal->ctx, q, puller);
}
```

- [ ] **Step 6.3: Commit gpu_queue_emu fn-ptrs**

Run:
```bash
cd /workspace/project/UsrLinuxEmu
git add plugins/gpu_driver/hal/gpu_hal.h
git commit -m "feat(hal): Phase 2 L2 foundation — add 5 gpu_queue_emu fn-ptrs + wrappers

Class type exposed as opaque hal_queue_handle_t (uint64_t) per
ADR-023 Decision 4. drv/ side casts back in subsequent removal change."
```

---

### Task 7: Add hardware_puller_emu fn-ptrs to gpu_hal.h (3 fn-ptrs + 3 wrappers, opaque handle)

**Files:**
- Modify: `plugins/gpu_driver/hal/gpu_hal.h`

- [ ] **Step 7.1: Append hardware_puller_emu fn-ptrs**

After gpu_queue_emu fn-ptrs in struct, insert:

```c

  /* ── hardware_puller_emu fn-ptrs (Phase 2 — 3 fn-ptrs) ────────
   * HardwarePullerEmu is a C++ class. Exposed via hal_puller_handle_t. */

  /* puller_set_puller: configure which sim_puller this puller watches. */
  int (*puller_set_puller)(void *ctx, hal_puller_handle_t puller,
                           uint64_t sim_puller_handle);

  /* puller_register_queue: register a queue with the puller. */
  int (*puller_register_queue)(void *ctx, hal_puller_handle_t puller,
                               hal_queue_handle_t queue);

  /* puller_unregister_queue: unregister a queue. */
  int (*puller_unregister_queue)(void *ctx, hal_puller_handle_t puller,
                                 uint32_t queue_id);
```

- [ ] **Step 7.2: Append hardware_puller_emu inline wrappers**

After gpu_queue_emu wrappers, insert:

```c

/* ── hardware_puller_emu inline wrappers (Phase 2) ─────────────── */

static inline int hal_puller_set_puller(struct gpu_hal_ops *hal,
                                        hal_puller_handle_t puller,
                                        uint64_t sim_puller_handle) {
  return hal->puller_set_puller(hal->ctx, puller, sim_puller_handle);
}

static inline int hal_puller_register_queue(struct gpu_hal_ops *hal,
                                            hal_puller_handle_t puller,
                                            hal_queue_handle_t queue) {
  return hal->puller_register_queue(hal->ctx, puller, queue);
}

static inline int hal_puller_unregister_queue(struct gpu_hal_ops *hal,
                                              hal_puller_handle_t puller,
                                              uint32_t queue_id) {
  return hal->puller_unregister_queue(hal->ctx, puller, queue_id);
}
```

- [ ] **Step 7.3: Verify gpu_hal.h compiles**

Run: `cd /workspace/project/UsrLinuxEmu/build && cmake --build . --target kernel -j4 2>&1 | tail -10`
Expected: clean compile (header consumed by hal_user.cpp/hal_mock.cpp but they reference unimplemented fn-ptrs — linker will fail; that's expected until Task 8)

- [ ] **Step 7.4: Commit hardware_puller_emu fn-ptrs**

Run:
```bash
cd /workspace/project/UsrLinuxEmu
git add plugins/gpu_driver/hal/gpu_hal.h
git commit -m "feat(hal): Phase 2 L2 foundation — add 3 hardware_puller_emu fn-ptrs + wrappers"
```

---

### Task 8: Implement hal_user.cpp lambdas for new fn-ptrs

**Files:**
- Modify: `plugins/gpu_driver/hal/hal_user.cpp` (add includes + append lambda assignments in `hal_user_init()`)

- [ ] **Step 8.1: Add includes for sim functions**

At top of hal_user.cpp (after existing includes), add:

```cpp
#include "../sim/graph.h"               // Phase 2: sim_graph_*
#include "../sim/mem_pool.h"            // Phase 2: sim_mem_pool_*
#include "../sim/stream_capture.h"      // Phase 2: sim_stream_capture_*
#include "../sim/gpu_queue_emu.h"       // Phase 2: GpuQueueEmu class access
#include "../sim/hardware/hardware_puller_emu.h"  // Phase 2: HardwarePullerEmu class access
```

- [ ] **Step 8.2: Append graph lambdas in hal_user_init()**

After the existing `heap_ptr` lambda (line 336, before closing `}` of `hal_user_init`), insert:

```cpp

  /* ── Stage 4.6 L2 foundation (ADR-072 §Decision 4) — Phase 2 ─── */
  /* Graph fn-ptrs (7): delegate to existing sim_graph_* functions. */
  hal->graph_create = [](void*, uint64_t* out) -> int {
    return sim_graph_create(out);
  };
  hal->graph_destroy = [](void*, uint64_t h) -> int {
    return sim_graph_destroy(h);
  };
  hal->graph_add_kernel_node = [](void*, uint64_t g, uint32_t kidx) -> int {
    return sim_graph_add_kernel_node(g, kidx);
  };
  hal->graph_add_memcpy_node = [](void*, uint64_t g, uint64_t src,
                                  uint64_t dst, uint64_t size) -> int {
    return sim_graph_add_memcpy_node(g, src, dst, size);
  };
  hal->graph_instantiate = [](void*, uint64_t g, uint64_t* out) -> int {
    return sim_graph_instantiate(g, out);
  };
  hal->graph_launch = [](void*, uint64_t exec, uint32_t stream_id) -> int {
    return sim_graph_launch(exec, stream_id);
  };
  hal->graph_destroy_exec = [](void*, uint64_t exec) -> int {
    return sim_graph_destroy_exec(exec);
  };
```

NOTE: If sim_graph_add_kernel_node has different signature (per Task 1 audit), adjust the lambda accordingly. The actual signature takes 3 args (graph_handle, kernel_index, + maybe one more).

- [ ] **Step 8.3: Append mem_pool lambdas in hal_user_init()**

After graph lambdas, insert:

```cpp

  /* mem_pool fn-ptrs (9): delegate to sim_mem_pool_*. */
  hal->mem_pool_create = [](void*, const sim_mem_pool_props_t* props,
                            uint64_t* out) -> int {
    return sim_mem_pool_create(const_cast<sim_mem_pool_props_t*>(props), out);
  };
  hal->mem_pool_destroy = [](void*, uint64_t h) -> int {
    return sim_mem_pool_destroy(h);
  };
  hal->mem_pool_alloc = [](void*, uint64_t h, uint64_t size,
                           uint64_t* out) -> int {
    return sim_mem_pool_alloc(h, size, out);
  };
  hal->mem_pool_set_attr = [](void*, uint64_t h, uint32_t attr,
                              uint64_t val) -> int {
    return sim_mem_pool_set_attr(h, static_cast<sim_mem_pool_attr_t>(attr), val);
  };
  hal->mem_pool_get_attr = [](void*, uint64_t h, uint32_t attr,
                              uint64_t* out) -> int {
    return sim_mem_pool_get_attr(h, static_cast<sim_mem_pool_attr_t>(attr), out);
  };
  hal->mem_pool_trim = [](void*, uint64_t h, uint64_t min_bytes) -> int {
    return sim_mem_pool_trim(h, min_bytes);
  };

  /* Async variants deferred — sim_mem_pool_alloc_async/free_async are
   * placeholders in current sim/. Stub returns 0 with no fence for now;
   * real impl is the responsibility of the mem_pool removal change. */
  hal->mem_pool_alloc_async = [](void*, uint64_t, uint64_t,
                                 int64_t* out) -> int {
    if (out) *out = 0;
    return 0;  // stub: real impl deferred to removal change
  };
  hal->mem_pool_free_async = [](void*, uint64_t, uint64_t,
                                int64_t* out) -> int {
    if (out) *out = 0;
    return 0;  // stub
  };
```

- [ ] **Step 8.4: Append stream_capture lambdas in hal_user_init()**

After mem_pool lambdas, insert:

```cpp

  /* stream_capture fn-ptrs (3): delegate to sim_stream_capture_*. */
  hal->stream_capture_begin = [](void*, uint64_t stream_id,
                                 uint32_t mode) -> int {
    return sim_stream_capture_begin(static_cast<uint32_t>(stream_id), mode);
  };
  hal->stream_capture_end = [](void*, uint64_t stream_id,
                               uint64_t* out_graph) -> int {
    return sim_stream_capture_end(static_cast<uint32_t>(stream_id), out_graph);
  };
  hal->stream_capture_status = [](void*, uint64_t stream_id,
                                  uint32_t* out_status) -> int {
    return sim_stream_capture_status(static_cast<uint32_t>(stream_id),
                                     out_status);
  };
```

- [ ] **Step 8.5: Append gpu_queue_emu lambdas in hal_user_init()**

After stream_capture lambdas, insert:

```cpp

  /* gpu_queue_emu fn-ptrs (5): delegate to GpuQueueEmu class methods.
   * Class instances managed by hal_user_context. Opaque handle cast
   * to GpuQueueEmu* (see removal change for full drv/ integration). */
  hal->queue_create = [](void* ctx, uint32_t handle, uint32_t type,
                         uint32_t priority, uint32_t ring_size,
                         hal_queue_handle_t* out) -> int {
    auto* hc = static_cast<struct hal_user_context*>(ctx);
    auto queue = std::make_shared<GpuQueueEmu>(hc, handle, type, priority,
                                               ring_size);
    hc->queues[hc->next_queue_slot++] = queue;
    *out = reinterpret_cast<uintptr_t>(queue.get());
    return 0;
  };
  hal->queue_attach_shmem = [](void*, hal_queue_handle_t q,
                               void* cpu_ptr, uint64_t size) -> int {
    auto* queue = reinterpret_cast<GpuQueueEmu*>(static_cast<uintptr_t>(q));
    return queue->attachSharedMemory(cpu_ptr, size);
  };
  hal->queue_submit = [](void*, hal_queue_handle_t q,
                         uint64_t gpfifo_addr, uint32_t count,
                         int64_t* out_fence) -> int {
    auto* queue = reinterpret_cast<GpuQueueEmu*>(static_cast<uintptr_t>(q));
    uint64_t fence = queue->submit(gpfifo_addr, count, 0);
    if (out_fence) *out_fence = static_cast<int64_t>(fence);
    return 0;
  };
  hal->queue_destroy = [](void* ctx, hal_queue_handle_t q) -> int {
    auto* hc = static_cast<struct hal_user_context*>(ctx);
    auto* queue = reinterpret_cast<GpuQueueEmu*>(static_cast<uintptr_t>(q));
    hc->queues[queue->id()] = nullptr;
    return 0;
  };
  hal->queue_register_puller = [](void*, hal_queue_handle_t q,
                                  hal_puller_handle_t puller) -> int {
    auto* queue = reinterpret_cast<GpuQueueEmu*>(static_cast<uintptr_t>(q));
    auto* puller_ptr = reinterpret_cast<HardwarePullerEmu*>(
        static_cast<uintptr_t>(puller));
    puller_ptr->registerQueue(queue);
    return 0;
  };
```

NOTE: Adjust per actual `GpuQueueEmu` constructor + method signatures from Task 1 audit.

- [ ] **Step 8.6: Append hardware_puller_emu lambdas in hal_user_init()**

After gpu_queue_emu lambdas, insert:

```cpp

  /* hardware_puller_emu fn-ptrs (3): delegate to HardwarePullerEmu class. */
  hal->puller_set_puller = [](void*, hal_puller_handle_t puller,
                              uint64_t sim_puller_handle) -> int {
    auto* p = reinterpret_cast<HardwarePullerEmu*>(
        static_cast<uintptr_t>(puller));
    p->setPuller(sim_puller_handle);
    return 0;
  };
  hal->puller_register_queue = [](void*, hal_puller_handle_t puller,
                                  hal_queue_handle_t queue) -> int {
    auto* p = reinterpret_cast<HardwarePullerEmu*>(
        static_cast<uintptr_t>(puller));
    auto* q = reinterpret_cast<GpuQueueEmu*>(static_cast<uintptr_t>(queue));
    p->registerQueue(q);
    return 0;
  };
  hal->puller_unregister_queue = [](void*, hal_puller_handle_t puller,
                                    uint32_t queue_id) -> int {
    auto* p = reinterpret_cast<HardwarePullerEmu*>(
        static_cast<uintptr_t>(puller));
    p->unregisterQueue(queue_id);
    return 0;
  };
```

- [ ] **Step 8.7: Build hal_user target**

Run: `cd /workspace/project/UsrLinuxEmu/build && cmake --build . --target hal_user -j4 2>&1 | tail -20`
Expected: clean compile. If errors about `GpuQueueEmu::attachSharedMemory` or method names don't exist, check actual signatures in `sim/gpu_queue_emu.h` and adjust.

- [ ] **Step 8.8: Commit hal_user lambdas**

Run:
```bash
cd /workspace/project/UsrLinuxEmu
git add plugins/gpu_driver/hal/hal_user.cpp
git commit -m "feat(hal): Phase 2 L2 foundation — implement hal_user lambdas for 28 fn-ptrs

Delegates to sim_graph_*, sim_mem_pool_*, sim_stream_capture_*,
GpuQueueEmu class methods, and HardwarePullerEmu class methods.
Class types exposed as opaque uint64_t handles via reinterpret_cast."
```

---

### Task 9: Implement hal_mock.cpp lambdas for new fn-ptrs

**Files:**
- Modify: `plugins/gpu_driver/hal/hal_mock.cpp` (append lambda assignments in `hal_mock_init()`)

- [ ] **Step 9.1: Append graph mock lambdas in hal_mock_init()**

After the existing `heap_ptr` mock lambda (line 362, before closing `}`), insert:

```cpp

  /* ── Stage 4.6 L2 foundation (ADR-072 §Decision 4) — Phase 2 ─── */
  /* Mock fn-ptrs: test-friendly defaults (monotonic counter, true, 0,
   * nullptr). No real sim state, deterministic for test reproducibility. */

  /* Graph mock (7): return deterministic handles, no real graph ops. */
  hal->graph_create = [](void*, uint64_t* out) -> int {
    static std::atomic<uint64_t> next{0x2000};
    if (out) *out = ++next;
    return 0;
  };
  hal->graph_destroy = [](void*, uint64_t) -> int { return 0; };
  hal->graph_add_kernel_node = [](void*, uint64_t, uint32_t) -> int {
    return 0;
  };
  hal->graph_add_memcpy_node = [](void*, uint64_t, uint64_t, uint64_t,
                                  uint64_t) -> int { return 0; };
  hal->graph_instantiate = [](void*, uint64_t, uint64_t* out) -> int {
    static std::atomic<uint64_t> next{0x3000};
    if (out) *out = ++next;
    return 0;
  };
  hal->graph_launch = [](void*, uint64_t, uint32_t) -> int { return 0; };
  hal->graph_destroy_exec = [](void*, uint64_t) -> int { return 0; };
```

- [ ] **Step 9.2: Append mem_pool mock lambdas in hal_mock_init()**

After graph mocks, insert:

```cpp

  /* mem_pool mock (9): deterministic handles, no-op ops. */
  hal->mem_pool_create = [](void*, const sim_mem_pool_props_t*,
                            uint64_t* out) -> int {
    static std::atomic<uint64_t> next{0x4000};
    if (out) *out = ++next;
    return 0;
  };
  hal->mem_pool_destroy = [](void*, uint64_t) -> int { return 0; };
  hal->mem_pool_alloc = [](void*, uint64_t, uint64_t, uint64_t* out) -> int {
    static std::atomic<uint64_t> next{0x5000};
    if (out) *out = ++next;
    return 0;
  };
  hal->mem_pool_alloc_async = [](void*, uint64_t, uint64_t,
                                 int64_t* out) -> int {
    if (out) *out = 0;
    return 0;
  };
  hal->mem_pool_free = [](void*, uint64_t, uint64_t) -> int { return 0; };
  hal->mem_pool_free_async = [](void*, uint64_t, uint64_t,
                                int64_t* out) -> int {
    if (out) *out = 0;
    return 0;
  };
  hal->mem_pool_set_attr = [](void*, uint64_t, uint32_t, uint64_t) -> int {
    return 0;
  };
  hal->mem_pool_get_attr = [](void*, uint64_t, uint32_t, uint64_t* out) -> int {
    if (out) *out = 0;
    return 0;
  };
  hal->mem_pool_trim = [](void*, uint64_t, uint64_t) -> int { return 0; };
```

- [ ] **Step 9.3: Append stream_capture mock lambdas in hal_mock_init()**

After mem_pool mocks, insert:

```cpp

  /* stream_capture mock (3): no-op. */
  hal->stream_capture_begin = [](void*, uint64_t, uint32_t) -> int {
    return 0;
  };
  hal->stream_capture_end = [](void*, uint64_t, uint64_t* out) -> int {
    if (out) *out = 0;
    return 0;
  };
  hal->stream_capture_status = [](void*, uint64_t, uint32_t* out) -> int {
    if (out) *out = 0;  // mock: never capturing
    return 0;
  };
```

- [ ] **Step 9.4: Append gpu_queue_emu mock lambdas in hal_mock_init()**

After stream_capture mocks, insert:

```cpp

  /* gpu_queue_emu mock (5): opaque handle, no-op ops. */
  hal->queue_create = [](void*, uint32_t, uint32_t, uint32_t, uint32_t,
                         hal_queue_handle_t* out) -> int {
    static std::atomic<uint64_t> next{0x6000};
    if (out) *out = ++next;
    return 0;
  };
  hal->queue_attach_shmem = [](void*, hal_queue_handle_t, void*,
                               uint64_t) -> int { return 0; };
  hal->queue_submit = [](void*, hal_queue_handle_t, uint64_t, uint32_t,
                         int64_t* out) -> int {
    if (out) *out = 0;
    return 0;
  };
  hal->queue_destroy = [](void*, hal_queue_handle_t) -> int { return 0; };
  hal->queue_register_puller = [](void*, hal_queue_handle_t,
                                  hal_puller_handle_t) -> int { return 0; };
```

- [ ] **Step 9.5: Append hardware_puller_emu mock lambdas in hal_mock_init()**

After gpu_queue_emu mocks, insert:

```cpp

  /* hardware_puller_emu mock (3): no-op. */
  hal->puller_set_puller = [](void*, hal_puller_handle_t, uint64_t) -> int {
    return 0;
  };
  hal->puller_register_queue = [](void*, hal_puller_handle_t,
                                  hal_queue_handle_t) -> int { return 0; };
  hal->puller_unregister_queue = [](void*, hal_puller_handle_t,
                                    uint32_t) -> int { return 0; };
```

- [ ] **Step 9.6: Build hal_mock target**

Run: `cd /workspace/project/UsrLinuxEmu/build && cmake --build . --target hal_mock -j4 2>&1 | tail -20`
Expected: clean compile

- [ ] **Step 9.7: Commit hal_mock lambdas**

Run:
```bash
cd /workspace/project/UsrLinuxEmu
git add plugins/gpu_driver/hal/hal_mock.cpp
git commit -m "feat(hal): Phase 2 L2 foundation — implement hal_mock defaults for 28 fn-ptrs"
```

---

### Task 10: Build full target set + regression test

**Files:**
- (no edits — verification only)

- [ ] **Step 10.1: Full build**

Run: `cd /workspace/project/UsrLinuxEmu/build && cmake --build . --target kernel gpu_hal hal_mock gpu_hal_mock gpu_sim gpu_drv gpu_driver_plugin -j4 2>&1 | tail -30`
Expected: clean build, 0 errors, 0 new warnings

- [ ] **Step 10.2: Targeted regression tests**

Run:
```bash
cd /workspace/project/UsrLinuxEmu/build
cmake --build . --target test_context_type_standalone test_pdl_standalone test_priority_sched_standalone -j4 2>&1 | tail -10
ctest -R "test_context_type_standalone|test_pdl_standalone|test_priority_sched_standalone" --output-on-failure 2>&1 | tail -20
```
Expected: all 3 tests build + PASS

- [ ] **Step 10.3: Full ctest regression**

Run: `cd /workspace/project/UsrLinuxEmu && ctest --test-dir build --output-on-failure 2>&1 | tail -30`
Expected: all tests PASS, 0 regression (count should be ≥ previous baseline of 92+)

- [ ] **Step 10.4: Verify fn-ptr count**

Run:
```bash
cd /workspace/project/UsrLinuxEmu
grep -cE "^\s+(int|void|int64_t|uint|hal)\s+\(\*\w+\)" plugins/gpu_driver/hal/gpu_hal.h
```
Expected: 47 (19 existing + 28 new = 47 fn-ptrs total)

- [ ] **Step 10.5: Verify L2 violation count unchanged**

Run:
```bash
cd /workspace/project/UsrLinuxEmu
# (project-specific L2 audit command — adjust per docs/04-building/ci-cd.md)
grep -rE '#include.*sim/' plugins/gpu_driver/drv/ 2>&1 | wc -l
```
Expected: 8 (foundation-only change; no drv/ changes — count should match pre-change baseline of 8)

- [ ] **Step 10.6: Commit any remaining fixups**

If Step 10.4 or 10.5 didn't match expected, fix and commit:
```bash
cd /workspace/project/UsrLinuxEmu
git add plugins/gpu_driver/hal/gpu_hal.h plugins/gpu_driver/hal/hal_user.cpp plugins/gpu_driver/hal/hal_mock.cpp
git commit -m "fix(hal): Phase 2 L2 foundation — verification fixups"
```

---

### Task 11: Update openspec/changes tasks.md progress

**Files:**
- Modify: `openspec/changes/2026-08-03-stage4-l2-foundation-phase2-hal/tasks.md`

- [ ] **Step 11.1: Mark Tasks 1.1 through 6.5 as complete**

Edit the tasks.md to flip `- [ ]` → `- [ ]` for completed items. Per Phase 1 convention, use the worktree-specific commit history to determine which tasks are actually done.

Tasks marked:
- [x] 1.1 (worktree setup — done implicitly by branch creation)
- [x] 2.1-2.5 (gpu_hal.h fn-ptrs added)
- [x] 3.1-3.5 (gpu_hal.h wrappers added)
- [x] 4.1-4.5 (hal_user.cpp lambdas)
- [x] 5.1-5.5 (hal_mock.cpp mocks)
- [x] 6.1-6.5 (verification — Task 10 in this plan)

- [ ] **Step 11.2: Commit tasks.md update**

Run:
```bash
cd /workspace/project/UsrLinuxEmu
git add openspec/changes/2026-08-03-stage4-l2-foundation-phase2-hal/tasks.md
git commit -m "chore(openspec): mark Phase 2 tasks complete (1.1-6.5)

Implementation shipped via Tasks 1-10 of .rddf/plans/2026-08-03-stage4-l2-foundation-phase2-hal.md"
```

---

### Task 12: Merge to main + archive

**Files:**
- (no source edits — workflow step)

- [ ] **Step 12.1: Push branch to origin**

Run: `git push origin openspec/2026-08-03-stage4-l2-foundation-phase2-hal`
Expected: branch pushed

- [ ] **Step 12.2: Merge to main**

Run:
```bash
cd /workspace/project/UsrLinuxEmu
git checkout main
git merge --no-ff openspec/2026-08-03-stage4-l2-foundation-phase2-hal -m "Merge: feat(hal) Phase 2 L2 foundation (5 headers, ~28 fn-ptrs)

Adds ~28 new fn-ptrs to struct gpu_hal_ops enabling 5 subsequent
removal changes to migrate drv/ off sim/ includes. L2 violation
count unchanged (still 8 — foundation only). 0 regression."
```

- [ ] **Step 12.3: Push main to origin**

Run: `git push origin main`
Expected: main updated

- [ ] **Step 12.4: Archive change**

Run: `cd /workspace/project/UsrLinuxEmu && openspec archive 2026-08-03-stage4-l2-foundation-phase2-hal --yes --skip-specs`
Expected: change archived to `openspec/changes/archive/`

- [ ] **Step 12.5: Update openspec/changes/INDEX.md**

Add entry to INDEX.md following the format of other archived changes (1 line + commit reference).

- [ ] **Step 12.6: Push archive commit**

Run:
```bash
cd /workspace/project/UsrLinuxEmu
git add openspec/changes/INDEX.md openspec/changes/archive/
git commit -m "chore(openspec): archive stage4-l2-foundation-phase2-hal (0+91 → 0+92)"
git push origin main
```

- [ ] **Step 12.7: Cleanup worktree + branch**

Run:
```bash
cd /workspace/project/UsrLinuxEmu
git branch -d openspec/2026-08-03-stage4-l2-foundation-phase2-hal
git push origin --delete openspec/2026-08-03-stage4-l2-foundation-phase2-hal
```
Expected: branch deleted locally + remotely

---

## Self-Review Checklist

After completing all tasks, verify:

1. [ ] **Spec coverage**: All 28 fn-ptrs from design.md §Required Phase 2 fn-ptrs are present in gpu_hal.h struct (count = 47)
2. [ ] **Wrapper coverage**: All 28 inline wrappers exist (grep `static inline` in gpu_hal.h)
3. [ ] **No placeholders**: No "TODO", "TBD", "fill in", "implement later" anywhere
4. [ ] **Append-only**: Existing 19 fn-ptrs unchanged (git diff shows only additions, no modifications to existing lines)
5. [ ] **Build clean**: `cmake --build .` exits 0
6. [ ] **Test regression**: `ctest` all pass (≥ 92 baseline)
7. [ ] **L2 count unchanged**: 8 violations (foundation only)
8. [ ] **Archive complete**: openspec/changes/archive/2026-08-03-stage4-l2-foundation-phase2-hal/ exists

If any check fails, fix before claiming completion.