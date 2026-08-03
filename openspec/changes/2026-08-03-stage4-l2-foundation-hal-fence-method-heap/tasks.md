# Tasks: stage4-l2-foundation-hal-fence-method-heap

## 1. HAL extension (gpu_hal.h)

- [ ] 1.1 Add 5 fn-ptr declarations to `struct gpu_hal_ops` (append-only, after `hal_pdl_signal_completion`):
  - `int64_t (*fence_id_alloc)(void *ctx);`
  - `void (*fence_id_signal)(void *ctx, uint64_t fence_id);`
  - `int (*fence_id_check)(void *ctx, uint64_t fence_id, bool *signaled);`
  - `int (*method_codec_encode)(void *ctx, /* TODO: confirm exact signature */);`
  - `void* (*heap_ptr)(void *ctx, uint64_t gpu_va);`
- [ ] 1.2 Add 5 inline wrapper functions in `gpu_hal.h`:
  - `static inline int64_t hal_fence_id_alloc(struct gpu_hal_ops *hal) { return hal->fence_id_alloc(hal->ctx); }`
  - `static inline void hal_fence_id_signal(struct gpu_hal_ops *hal, uint64_t fence_id) { hal->fence_id_signal(hal->ctx, fence_id); }`
  - `static inline int hal_fence_id_check(struct gpu_hal_ops *hal, uint64_t fence_id, bool *signaled) { return hal->fence_id_check(hal->ctx, fence_id, signaled); }`
  - `static inline int hal_method_codec_encode(struct gpu_hal_ops *hal /*, ...args... */) { return hal->method_codec_encode(hal->ctx /*, ...args... */); }`
  - `static inline void* hal_heap_ptr(struct gpu_hal_ops *hal, uint64_t gpu_va) { return hal->heap_ptr(hal->ctx, gpu_va); }`

## 2. Production implementation (hal_user.cpp)

- [ ] 2.1 In `hal_user_init()` (or equivalent), after existing fn-ptr assignments:
  - Assign `hal->fence_id_alloc = lambda { return sim_fence_id_alloc(); };`
  - Assign `hal->fence_id_signal = lambda { sim_fence_id_signal(fence_id); };`
  - Assign `hal->fence_id_check = lambda { return sim_fence_id_check(fence_id, signaled); };`
  - Assign `hal->method_codec_encode = lambda { return method_codec_encode(/* ... */); };`
  - Assign `hal->heap_ptr = lambda { auto* hc = static_cast<hal_user_context*>(ctx); return hc->heap + (gpu_va - HAL_HEAP_BASE); };`
- [ ] 2.2 Verify includes: `sim/fence_id.h` + `sim/hardware/method_codec.h` + `hal/hal_user.h` already present in hal_user.cpp

## 3. Mock implementation (hal_mock.cpp)

- [ ] 3.1 In `hal_mock_init()` (or equivalent), after existing fn-ptr assignments:
  - Assign `hal->fence_id_alloc = lambda { static std::atomic<uint64_t> next{0x1000}; return ++next; };`
  - Assign `hal->fence_id_signal = lambda { /* no-op */ };`
  - Assign `hal->fence_id_check = lambda { if (signaled) *signaled = true; return 0; };`
  - Assign `hal->method_codec_encode = lambda { return 0; };`
  - Assign `hal->heap_ptr = lambda { return nullptr; };`
- [ ] 3.2 Verify includes: `<atomic>` (likely already present for `std::atomic`)

## 4. Verification

- [ ] 4.1 `make kernel gpu_hal hal_mock hal_user gpu_sim plugin_gpu_driver -j4` — clean build
- [ ] 4.2 `make test` — all existing tests still PASS (0 regression)
- [ ] 4.3 Confirm new fn-ptrs are initialized in BOTH `hal_user_init()` and `hal_mock_init()` (no null fn-ptrs)
- [ ] 4.4 L2 violation count: still 12 (this change doesn't remove any — that is follow-up)
- [ ] 4.5 `docs-audit.sh --strict` — clean (except Doxygen-not-installed env limitation)

## 5. Documentation

- [ ] 5.1 (Not in scope for this change) Update `docs/00_adr/adr-023-hal-interface.md` Decision 4 spec to list 5 new fn-ptrs
- [ ] 5.2 (Not in scope) Update `docs/00_adr/adr-072-portability-validation.md` — note that 5 fn-ptrs are available, 3 removal changes pending

## 6. Archive

- [ ] 6.1 `openspec archive 2026-08-03-stage4-l2-foundation-hal-fence-method-heap --yes --skip-specs`
- [ ] 6.2 (Not in scope) `openspec change 2026-08-03-stage4-l2-foundation-removal-fence-id --yes` (separate change)
- [ ] 6.3 (Not in scope) `openspec change 2026-08-03-stage4-l2-foundation-removal-method-codec --yes` (separate change)
- [ ] 6.4 (Not in scope) `openspec change 2026-08-03-stage4-l2-foundation-removal-hal-user --yes` (separate change)
