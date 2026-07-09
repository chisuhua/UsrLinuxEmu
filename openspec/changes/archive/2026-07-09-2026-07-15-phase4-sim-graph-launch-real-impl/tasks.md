# Tasks: phase4-sim-graph-launch-real-impl

> **状态**: ✅ IMPLEMENTATION COMPLETE（2026-07-09，pending commit）
> **目标**: sim_graph_launch 真实异步执行（替换 PoC）— **已实现**
> **架构**: ADR-040 (Puller Fence Completion), ADR-041 (Graph → GPFIFO), ADR-043 (CP Boundary)

## 1. 设计（半天）

- [x] 1.1 确认跨仓 B-1 决策已读（`external/TaskRunner/docs/superpowers/cross-repo-prs/2026-07-05-phase3-1-stream-mempool-coordination.md` §3.1.1）
- [x] 1.2 确认 ADR-040/041/043 已 Accepted，design.md 已对齐
- [x] 1.3 设计 `sim_graph_instantiate` 中 `graph_to_gpfifo()` 序列化逻辑
- [x] 1.4 设计 sim-internal heap 分配（`sim_gpfifo_alloc`），确认 HAL-addressable
- [x] 1.5 确认 `stream_id` → `GpuQueueEmu` 映射与 ADR-033 R2 契约一致（`stream_id = LOW32(queue_handle)`）
- [x] 1.6 验证 `gpu_gpfifo_entry.payload[7]` 布局与 ADR-041 D4 pack 约定兼容

## 2. Puller Fence Completion（ADR-040 — 2 天）

- [x] 2.1 `HardwarePullerEmu` 新增 `pending_fence_id_` 成员 + `submitBatch()` 签名变更（`fence_id` 参数）
- [x] 2.2 `handleComplete()` 中，batch 全量完成时调用 `sim_fence_id_signal(pending_fence_id_)`
- [x] 2.3 `GpuQueueEmu::submit()` 签名变更，透传 `fence_id` 到 `submitBatch()`
- [x] 2.4 `handlePushbufferSubmitBatch`（`gpgpu_device.cpp`）改用 `sim_fence_id_alloc()` 替代 `hal_fence_create()`，传入 `q->submit(..., fence_id)`（同步修复 pushbuffer fence 永不 signal bug）
- [x] 2.5 `handleWaitFence`（`gpgpu_device.cpp`）适配双命名空间：sim fence（`sim_fence_id_check`）vs HAL fence（`hal_fence_read`），匹配 `gpu_drm_driver.cpp` 已有逻辑
- [x] 2.6 更新 `test_hardware_puller_emu_standalone`：验证 `submitBatch` + fence signal 路径

## 3. Graph Node → GPFIFO 预编译（ADR-041 — 2 天）

- [x] 3.1 `ExecEntry` 新增 `precompiled_entries`, `gpfifo_gpu_addr`, `entry_count`, `kernel_addrs` 字段
- [x] 3.2 实现 `graph_to_gpfifo()`：KERNEL node → `GPU_OP_LAUNCH_KERNEL` entry（pack grid/block dims），MEMCPY node → `GPU_OP_MEMCPY` entry
- [x] 3.3 实现 `sim_gpfifo_alloc()`：sim-internal heap，返回 HAL-addressable GPU VA（SIM_HEAP_BASE=0x20000000）
- [x] 3.4 `sim_graph_instantiate` 中集成序列化 + buffer 分配
- [x] 3.5 kernargs_bo_handle → GPU VA 解析（Phase 4 用 deterministic `KERNARGS_VA_BASE + bo*STRIDE`；Phase 5 切换 `hal_->mem_lookup`）

## 4. sim_graph_launch + drv handler 重写（ADR-043 — 1 天）

- [x] 4.1 `sim_graph_launch` 签名变更：`int sim_graph_launch(exec_handle, stream_id, *gpfifo_addr_out, *entry_count_out)` — 只做查表输出
- [x] 4.2 `handleGraphLaunch`（`gpgpu_device.cpp`）重写：调 `sim_graph_launch` → `getQueue()` → `q->submit(..., fence_id)` → `hal_doorbell_ring()` → return fence_id
- [x] 4.3 `gpu_ioctl_graph_launch`（`gpu_drm_driver.cpp`）同步重写（委托给 `GpgpuDevice::ioctl`）
- [x] 4.4 `sim_graph_launch` 中移除 `sim_fence_id_alloc()` + `sim_fence_id_signal()`（不再立即 signal fence）

## 5. 测试（1 天）

- [x] 5.1 单测：`sim_graph_instantiate` 后 `gpfifo_addr` > 0 且 `entry_count` > 0（test_sim_graph_standalone）
- [x] 5.2 单测：`sim_graph_launch` 返回 `gpfifo_addr` 和 `entry_count` 正确（不 signal fence）（test_sim_graph_standalone）
- [x] 5.3 单测：`handleGraphLaunch` 返回后 fence NOT signalled（composed of: sim_graph_launch read-only + sim_fence_id_alloc not-signaled + q->submit not blocking；test_sim_graph_standalone + test_gpu_fence_return 共同覆盖）
- [x] 5.4 单测：Puller 完成 batch 后 fence signalled（test_hardware_puller_emu_standalone）
- [x] 5.5 单测：pushbuffer 路径 fence 通过 Puller 完成 signal（test_gpu_fence_return_standalone 验证 ADR-040 D3 修复）
- [x] 5.6 集成：`test_gpu_driver_client_phase31` 验证 graph launch 完整链（+ test_gpu_plugin GPU_IOCTL_GRAPH_LAUNCH 集成测试）
- [x] 5.7 跨仓：TaskRunner `test_cu_graph` E2E PASS（30/30 cases, 92 assertions）

## 6. 验证 / commit（半天）

- [x] 6.1 ctest 全量 PASS（**86/86 baseline maintained**，+2 new test cases in sim_graph + 1 in test_gpu_plugin）
- [x] 6.2 docs-audit 无新 warning（43 passed, 0 failed, 0 warnings）
- [x] 6.3 cross-repo TaskRunner E2E PASS（**10/10 tests** in TaskRunner build）
- [x] 6.4 commit：`feat(sim): real async graph_launch via GpuQueueEmu::submit` — commits `7740a75`（13 files, +591/-111）+ `f0f7a03`（tasks.md update, 1 file, +49/-35），已 push 到 `origin/main`
- [x] 6.5 PR + merge，通知 TaskRunner 侧 — 本仓库采用 direct-to-main 工作流（与 `1fb3e02`/`168b33b` 等前序 phase 4 commit 一致），无需 PR。跨仓通知通过 https://github.com/chisuhua/TaskRunner/issues/9 完成（明确说明 ioctl 边界稳定，TaskRunner shim 无需修改）

## 验证结果摘要

```
Phase 6 验证（2026-07-09）:
- UsrLinuxEmu ctest: 86/86 PASS (10.84s)
- docs-audit: 43 passed, 0 failed, 0 warnings
- TaskRunner ctest: 10/10 PASS (test_cu_graph: 30 cases, 92 assertions)
- test_sim_graph_standalone: 17 cases, 78 assertions
- test_hardware_puller_emu_standalone: 10 cases
- test_gpu_fence_return_standalone: 1 case (pushbuffer ADR-040 D3)
- test_gpu_plugin: 52 cases, 183 assertions (including new async fence test)
- test_gpu_driver_client_phase31_standalone: 9 cases, 46 assertions
```

## 跨仓 E2E 构建命令

```bash
cd external/TaskRunner && git submodule update --init --recursive
mkdir -p build && cd build
cmake -DTASKRUNNER_BUILD_MODE=umd-evolution ..
make -j4
ctest -R test_cu_graph -V
```