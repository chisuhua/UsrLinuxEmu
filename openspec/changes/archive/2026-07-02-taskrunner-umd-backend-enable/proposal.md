# Change: taskrunner-umd-backend-enable

> **状态**: ⚠️ DRAFT（2026-07-02，等待 TaskRunner Phase 1.5 Stretch 完成后激活）
> **创建**: 2026-07-02
> **来源**: TaskRunner umd-evolution roadmap Phase 1.5 Stretch
> **关联 TaskRunner Spec**: `external/TaskRunner/docs/superpowers/specs/2026-07-02-phase1.5-cudastub-dynamic-cast-fix.md`
> **关联 TaskRunner Plan**: `external/TaskRunner/docs/superpowers/plans/2026-07-02-phase1.5-cudastub-dynamic-cast-fix.md`

## Why

### 现状

TaskRunner 的 `libcuda_taskrunner.so`（Phase 2，79 cu\* 符号）目前**只能通过 CudaStub 假后端**运行。CudaScheduler 中有 5 个 `dynamic_cast<CudaStub*>(driver_)` 硬绑定导致注入 GpuDriverClient（UsrLinuxEmu 真实 GPU 后端）时返回 `-ENOSYS`，阻塞了真实 GPU 内存分配和数据传输路径：

```
libcuda_taskrunner.so → CudaRuntimeApi → CudaScheduler ──❌ dynamic_cast ──→ GpuDriverClient
                                                          │
                                                          └──✅ CudaStub (假)
```

### Gap

TaskRunner 的 CudaScheduler 已使用 IGpuDriver 虚接口（`alloc_bo`, `submit_memcpy`, `submit_launch` 等）处理部分操作，但 5 个 CUDA Driver API 路径（`submitMemAlloc`/`submitMemFree`/`submitMemcpyH2D`/`submitMemcpyD2H`/`submitLaunch`）仍硬绑定 CudaStub。

### Why Now

1. Phase 0-2 UMD evolution 已全部完成（76/76 tests, TaskRunner `main` @ `83ef131`）
2. 这是 TaskRunner ↔ UsrLinuxEmu **真实后端连接的唯一剩余障碍**
3. Phase 1.5 Stretch 估工仅 0.5-1 天，属于低风险、高价值的打通工作
4. 修复后 UsrLinuxEmu 的 GpuDriverClient 可以参与端到端 CUDA 程序测试（LD_PRELOAD 路径），为未来的 Stage 1.4 KFD 集成验证铺路

## What Changes

### UsrLinuxEmu 侧（本 change — 验证为主，代码改动为零）

**本 change 不包含任何 UsrLinuxEmu 代码改动**。GpuDriverClient 已实现 IGpuDriver 31 个方法，TaskRunner 修复后即可直接调用。

验证工作：

1. **Submodule bump**：TaskRunner Phase 1.5 Stretch 完成后，bump UsrLinuxEmu 的 `external/TaskRunner` submodule 指针
2. **GpuDriverClient 后端 smoke test**：验证 TaskRunner CudaScheduler 可通过 GpuDriverClient 完成 `submitMemAlloc` → `submitMemcpyH2D` → `submitLaunch` → `submitMemcpyD2H` → `submitMemFree` 完整链路
3. **Regression guard**：确认 UsrLinuxEmu 现有 GPU 测试（test_gpu_ioctl, test_va_space 等）不受影响
4. **TADR mirror 更新**：如需，更新 `docs/00_adr/README.md` 中 TaskRunner TADR mirror 表

### TaskRunner 侧（由关联 Plan 完成）

参见 `external/TaskRunner/docs/superpowers/plans/2026-07-02-phase1.5-cudastub-dynamic-cast-fix.md`：
- MemoryDescriptor 扩展（添加 `bo_handle` 字段）
- 5 个 `dynamic_cast<CudaStub*>` 替换为 IGpuDriver 虚接口调用
- Stub mode fence 处理迁移

## Capabilities

### New Capabilities

- `taskrunner-umd-backend-verification`：验证 TaskRunner libcuda_taskrunner.so 可通过 GpuDriverClient 后端完成真实 GPU 内存操作

### Modified Capabilities

- NONE（GpuDriverClient 代码不变）

## Impact

| 影响项 | 范围 | 风险 |
|--------|------|------|
| 代码 | 0 行 UsrLinuxEmu 改动 | 无 |
| Submodule | `external/TaskRunner` pointer bump | 低 |
| 测试 | 运行现有 GPU 测试 + smoke test | 低 |
| 文档 | 可能需要更新 TADR mirror 表 | 极低 |
| 跨仓 | TaskRunner 先 commit → UsrLinuxEmu 再 bump | 低 |

**风险缓解**:
- 本 change 不包含代码改动，无回归风险
- TaskRunner 侧 76/76 测试必须全过才 bump submodule
- Smoke test 覆盖 GpuDriverClient 后端完整链路

## 关联 Changes

- TaskRunner: `docs/superpowers/plans/2026-07-02-phase1.5-cudastub-dynamic-cast-fix.md`
- 前置依赖: TaskRunner Phase 2 (LD_PRELOAD shim) ✅ COMPLETE
- 后续依赖: 无（本 change 完成后，为 Stage 1.4 铺路）
- 关联 ADR: [ADR-032](../00_adr/adr-032-h2-5-igpu-driver-abstraction.md), [ADR-033](../00_adr/adr-033-h3-phase2-lifecycle.md), [ADR-035](../00_adr/adr-035-governance-policy.md)