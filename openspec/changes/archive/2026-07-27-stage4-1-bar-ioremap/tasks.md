# Tasks — Stage 4.1: BAR ioremap + DMA Coherent

> **状态**：所有任务通过 TDD 路径已提交（3 个原子 commit）。本 tasks.md 为事后追溯，所有条目标记 `[x]`（已完成），并附 commit SHA 作为证据。

## 1. Phase 1: ③ sim 基础落地（commit 571f9af）

- [x] 1.1 添加 VRAM backing store (`plugins/gpu_driver/sim/vram_store.{h,cpp}`)，per-device 256MB `mmap(MAP_ANONYMOUS)` — `571f9af feat(sim): add Stage 4.1 Phase 1 VRAM store`
- [x] 1.2 添加 BAR sim (`plugins/gpu_driver/sim/bar_sim.{h,cpp}`)，`sim_bar_ioremap/sim_bar_iounmap` + `sim_bar0_readl/writel` — `571f9af feat(sim): add Stage 4.1 Phase 1 ... BAR sim`
- [x] 1.3 添加 DMA coherent pool (`plugins/gpu_driver/sim/dma_coherent_pool.{h,cpp}`)，基址 `DMA_COHERENT_BASE=0x1_0000_0000` — `571f9af feat(sim): add Stage 4.1 Phase 1 ... DMA coherent pool (TDD)`
- [x] 1.4 TDD 测试 `tests/test_dma_coherent.cpp` PASS（地址池隔离 + 分配/释放）— `571f9af`

## 2. Phase 2: ① compat 接口（commit 556b647）

- [x] 2.1 添加 `include/linux_compat/io.h`：`ioremap`/`iounmap`/`readl`/`writel`/`ioread32`/`iowrite32`（签名与 Linux 6.12 LTS `include/linux/io.h` 一致）— `556b647 feat(compat): Phase 2 - ioremap/iounmap/readl/writel`
- [x] 2.2 添加 `include/linux_compat/dma-mapping.h`：`dma_alloc_coherent`/`dma_free_coherent`/`dma_map_single` — `556b647 feat(compat): ... + dma_alloc_coherent compat layer`
- [x] 2.3 添加 BAR0 BAR2 mmap 实现（独立 `MAP_ANONYMOUS`）到 compat 层 — `556b647`

## 3. Phase 3: BAR0 HQD 寄存器语义（commit 116ca8c）

- [x] 3.1 实现 `sim_bar0_readl/writel` 桥接到 `mqd_state`（KFD 队列元数据）— `116ca8c feat(bar0): add HQD register writel/readl with mqd_state hook`
- [x] 3.2 添加 `BAR0_HQD_BASE=0x4000` + `BAR0_HQD_STRIDE=64` 常量定义 — `116ca8c`

## 4. HAL 边界

- [x] 4.1 在 `plugins/gpu_driver/hal/gpu_hal.h` 添加第 15 个函数指针 `mem_map_bo(dev, bo_offset, size, va_out)` — 同 commit 571f9af
- [x] 4.2 实现 `hal_mock.cpp::mock_mem_map_bo`，桩返回 `0xA0000000 + offset` mock VA — 同 commit 571f9af
- [x] 4.3 实现 `hal_user.cpp::user_mem_map_bo`，调用 vram_store.map() — 同 commit 571f9af

## 5. 测试

- [x] 5.1 `tests/test_bar_ioremap.cpp` — `readl`/`writel` 往返 + HQD register 行为 PASS — `556b647` / `116ca8c`
- [x] 5.2 `tests/test_bar_ioremap_perf.cpp` — 延迟基准 ≤ Stage 3 baseline × 120% PASS — `571f9af`
- [x] 5.3 `tests/test_dma_coherent.cpp` — DMA 地址池隔离 + 分配/释放 PASS — `556b647`

## 6. HAL 边界静态检查（验收）

- [x] 6.1 `grep -r "#include.*sim/\|hal_user" plugins/gpu_driver/drv/` 输出为空 — ADR-023 D5 保持
- [x] 6.2 `grep -r "vram_store" plugins/gpu_driver/drv/` 输出为空 — ② 走 HAL 不直接 sim

## 7. Workflow 追溯（本提交）

- [x] 7.1 创建 `openspec/changes/stage4-1-bar-ioremap/{proposal,design,tasks}.md` + `specs/{bar-ioremap-compat,dma-coherent-emulation,hal-mem-map-bo}/spec.md`
- [x] 7.2 移动 `proposal-approved.md` 条目到 `已实施` 区
- [x] 7.3 归档 change 到 `openspec/changes/archive/` (本 tasks.md 完成后执行)
- [x] 7.4 更新 `.rddf/state/iteration.json` 中 change 状态为 archived

## Reference Commits

```
571f9af feat(sim): add Stage 4.1 Phase 1 VRAM store + BAR sim + DMA coherent pool (TDD)
556b647 feat(compat): Phase 2 - ioremap/iounmap/readl/writel + dma_alloc_coherent compat layer
116ca8c feat(bar0): add HQD register writel/readl with mqd_state hook
```
