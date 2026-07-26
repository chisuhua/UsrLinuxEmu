# Tasks: Stage 4.1 — 真实 BAR + ioremap + DMA Coherent 仿真

## 实施顺序: ③ SIM → ① COMPAT → ② DRIVER → 验收

### Phase 1: ③ 硬件模拟层 (foundation)

- [x] 1.1 Create `plugins/gpu_driver/sim/vram_store.h` — GpuVramStore class: 256MB mmap backing + BAR array[6]
- [x] 1.2 Create `plugins/gpu_driver/sim/vram_store.cpp` — mmap(MAP_ANONYMOUS|MAP_SHARED) implementation + BAR phys→offset mapping
- [x] 1.3 Create `plugins/gpu_driver/sim/bar_sim.h` — sim_bar_ioremap/sim_bar_iounmap API (per ADR-063 sim_proxy pattern)
- [x] 1.4 Create `plugins/gpu_driver/sim/bar_sim.cpp` — BAR 0-5 phys_base→mmap 映射 + readl/writel backing store access
- [x] 1.5 Create `plugins/gpu_driver/sim/dma_coherent_pool.h` — DmaCoherentPool class: independent mmap + bump allocator (DMA_COHERENT_BASE=0x1_0000_0000)
- [x] 1.6 Create `plugins/gpu_driver/sim/dma_coherent_pool.cpp` — allocate: mmap(MAP_ANONYMOUS|MAP_SHARED) → dma_addr bump; free: munmap + pool cleanup
- [x] 1.7 Wire GpuVramStore + DmaCoherentPool into GpgpuDevice init (device creation time)
- [x] 1.8 Update `plugins/gpu_driver/sim/CMakeLists.txt` — add bar_sim, vram_store, dma_coherent_pool sources

### Phase 2: ① 内核环境模拟层 (compat API)

- [x] 2.1 Create `include/linux_compat/io.h` — ioremap/iounmap/readl/writel/ioread32/iowrite32 (inline volatile, no HAL)
- [x] 2.2 Implement `ioremap` → sim_proxy → sim_bar_ioremap → mmap BAR backing
- [x] 2.3 Implement `iounmap` → munmap BAR mapping
- [x] 2.4 Create `include/linux_compat/dma-mapping.h` — dma_alloc_coherent/dma_free_coherent/dma_map_single stubs
- [x] 2.5 Implement `dma_alloc_coherent` → mmap(MAP_ANONYMOUS|MAP_SHARED) → DmaCoherentPool allocation → return cpu_addr + dma_addr
- [x] 2.6 Implement `dma_free_coherent` → munmap + DmaCoherentPool release
- [x] 2.7 Implement `dma_map_single` stub (streaming deferred per ADR-073 D6, condition 2/3 not triggered)
- [x] 2.8 Verify API signatures match Linux 6.12 LTS (include/linux/io.h + include/linux/dma-mapping.h)

### Phase 3: ② 可移植驱动代码 (HAL + driver)

- [x] 3.1 Extend `plugins/gpu_driver/hal/gpu_hal.h` — add `mem_map_bo` as 15th fn-ptr: `int (*mem_map_bo)(struct gpgpu_device*, uint64_t bo_offset, size_t size, void **user_map)`
- [x] 3.2 Implement `hal_mock.cpp::mem_map_bo` — BAR2 offset → VRAM backing store mmap
- [x] 3.3 Add `hal_user.cpp::mem_map_bo` stub (real hardware, deferred)
- [x] 3.4 Add `plugins/gpu_driver/shared/gpu_types.h` BAR2_OFFSET_BASE + BAR2_OFFSET_SIZE macros
- [x] 3.5 Implement `GpgpuDevice::mmap` BAR2 path: `HAL.mem_map_bo(this, bo_offset, size, &mapping)` per ADR-069 D4
- [x] 3.6 Migrate drv/ heap offset dereferences to ioremap/readl/writel pattern (BAR0 register access only; BAR2 stays mmap)
- [x] 3.7 Verify `drv/` directory: `grep -r '#include.*sim/\|hal_user'` output empty (HAL boundary, ADR-023 D5)

### Phase 4: 测试

- [x] 4.1 Create `tests/test_bar_ioremap.cpp` (Catch2) — TEST_CASE "ioremap/writel/readl roundtrip": verify writel(0xDEADBEEF) → readl returns 0xDEADBEEF
- [x] 4.2 Create `tests/test_dma_coherent.cpp` (Catch2) — TEST_CASE "dma_alloc_coherent allocates and returns unique dma_addr": verify cpu_addr != NULL, dma_addr ∈ [0x1_0000_0000, 0x1_0FFF_FFFF]
- [x] 4.3 Add `test_bar_ioremap_standalone` + `test_dma_coherent_standalone` to `tests/CMakeLists.txt`
- [x] 4.4 Run full ctest: `cd build && ctest --output-on-failure` — expect all existing + 2 new tests PASS
- [x] 4.5 Run docs-audit: `tools/docs-audit.sh --strict` — expect PASS
- [x] 4.6 Verify portability: compile ② driver code with only #include path adjustments as Linux 6.12 LTS kernel module (ADR-072 L2)
- [x] 4.7 Performance benchmark: `readl`/`writel` roundtrip latency ≤ Stage 3 heap offset × 120%

## 依赖关系

```
1.1-1.6 (③ sim classes) → 1.7 (wire into device init)
1.7-1.8 → 2.1-2.8 (① compat API depends on ③ sim)
2.1-2.8 → 3.1-3.7 (② driver depends on ① compat + ③ sim)
3.1-3.7 → 4.1-4.7 (tests depend on all layers)
```

## 预估工作量

| Phase | 任务 | 预估 |
|-------|------|------|
| 1 | ③ SIM 层 | 4-6h |
| 2 | ① COMPAT 层 | 3-4h |
| 3 | ② DRIVER 层 | 2-3h |
| 4 | 测试 + 验收 | 2-3h |
| **总计** | | **11-16h** |