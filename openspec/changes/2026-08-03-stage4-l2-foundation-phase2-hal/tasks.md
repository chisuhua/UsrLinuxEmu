# Tasks: stage4-l2-foundation-phase2-hal

## 1. Worktree setup

- [x] 1.1 Create worktree `openspec/2026-08-03-stage4-l2-foundation-phase2-hal` from main

## 2. gpu_hal.h — Add ~28 new fn-ptr declarations

- [x] 2.1 Add 8 graph fn-ptrs to `struct gpu_hal_ops` (graph_create/destroy/add_kernel_node/add_memcpy_node/instantiate/launch/destroy_exec/1-more)
- [x] 2.2 Add 9 mem_pool fn-ptrs to `struct gpu_hal_ops` (create/destroy/alloc/alloc_async/free/free_async/set_attr/get_attr/trim)
- [x] 2.3 Add 3 stream_capture fn-ptrs to `struct gpu_hal_ops` (begin/end/status)
- [x] 2.4 Add ~5 gpu_queue_emu fn-ptrs to `struct gpu_hal_ops` (queue_create/attach_shmem/submit/2-more)
- [x] 2.5 Add ~3 hardware_puller_emu fn-ptrs to `struct gpu_hal_ops` (set_puller/register_queue/1-more)

## 3. gpu_hal.h — Add ~28 new inline wrapper functions

- [x] 3.1 Add 8 inline wrappers for graph fn-ptrs (zero-overhead call forwarding)
- [x] 3.2 Add 9 inline wrappers for mem_pool fn-ptrs
- [x] 3.3 Add 3 inline wrappers for stream_capture fn-ptrs
- [x] 3.4 Add ~5 inline wrappers for gpu_queue_emu fn-ptrs
- [x] 3.5 Add ~3 inline wrappers for hardware_puller_emu fn-ptrs

## 4. hal_user.cpp — Add ~28 new lambda assignments in hal_user_init()

- [x] 4.1 Add 8 graph fn-ptr lambda assignments (delegate to sim_graph_*)
- [x] 4.2 Add 9 mem_pool fn-ptr lambda assignments (delegate to sim_mem_pool_*)
- [x] 4.3 Add 3 stream_capture fn-ptr lambda assignments (delegate to sim_stream_capture_*)
- [x] 4.4 Add ~5 gpu_queue_emu fn-ptr lambda assignments (delegate to GpuQueueEmu methods or sim_queue_* helpers)
- [x] 4.5 Add ~3 hardware_puller_emu fn-ptr lambda assignments (delegate to HardwarePullerEmu methods or sim_hardware_puller_* helpers)

## 5. hal_mock.cpp — Add ~28 new mock fn-ptr impls in hal_mock_init()

- [x] 5.1 Add 8 graph fn-ptr mock impls (return test-friendly defaults)
- [x] 5.2 Add 9 mem_pool fn-ptr mock impls
- [x] 5.3 Add 3 stream_capture fn-ptr mock impls
- [x] 5.4 Add ~5 gpu_queue_emu fn-ptr mock impls
- [x] 5.5 Add ~3 hardware_puller_emu fn-ptr mock impls

## 6. Verification

- [x] 6.1 `make kernel gpu_hal hal_mock gpu_hal_mock gpu_sim gpu_drv gpu_driver_plugin -j4` — clean build
- [x] 6.2 `make test_context_type_standalone test_pdl_standalone test_priority_sched_standalone -j4` — build + run, all PASS
- [x] 6.3 `make test` — full ctest suite, all PASS (0 regression)
- [x] 6.4 L2 violation count: still 8 (this change is foundation only; no drv/ changes)
- [x] 6.5 `struct gpu_hal_ops` total fn-ptr count: 19 → ~47 (verified via grep)

## 7. Documentation

- [ ] 7.1 (Not in scope) Update `docs/00_adr/adr-023-hal-interface.md` Decision 4 spec to list all 47 fn-ptrs

## 8. Archive

- [ ] 8.1 Commit on openspec branch with detailed message
- [ ] 8.2 Push branch to origin
- [ ] 8.3 Merge to main
- [ ] 8.4 `openspec archive 2026-08-03-stage4-l2-foundation-phase2-hal --yes --skip-specs`
- [ ] 8.5 Update `openspec/changes/INDEX.md` (0 active + 91 archived → 0 active + 92 archived)
- [ ] 8.6 Push archive commit to origin
- [ ] 8.7 Cleanup worktree + delete openspec branch

## Notes

- This is a LARGE change (~28 new fn-ptrs + wrappers + impls + mocks)
- Estimated time: 2-3 hours for full implementation
- Phase 2 foundation enables 5 subsequent removal changes (much simpler)
- After Phase 2 foundation: L2 violations can be removed 1-at-a-time via dedicated removal changes
