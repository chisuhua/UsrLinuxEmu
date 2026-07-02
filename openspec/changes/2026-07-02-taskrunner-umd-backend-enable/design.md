# Design: taskrunner-umd-backend-enable

> **Change 类型**: 验证型（verification-only）— 零 UsrLinuxEmu 代码改动
> **依赖**: TaskRunner Phase 1.5 Stretch（`external/TaskRunner/docs/superpowers/plans/2026-07-02-phase1.5-cudastub-dynamic-cast-fix.md`）

## 数据流

```
TaskRunner UMD App
    ↓ LD_PRELOAD=libcuda_taskrunner.so
TaskRunner CudaScheduler
    ↓ IGpuDriver virtual interface (after Phase 1.5 fix)
    ↓ alloc_bo / submit_memcpy / submit_launch / free_bo
GpuDriverClient (UsrLinuxEmu)
    ↓ GPU_IOCTL_* System C
UsrLinuxEmu GpgpuDevice (drv/)
    ↓ HAL (gpu_hal_ops)
UsrLinuxEmu Hardware Sim (sim/)
```

## 关键决策

### D1: UsrLinuxEmu 侧无需代码改动

**理由**: GpuDriverClient 已实现 IGpuDriver 31 个方法（tadr-301 + tadr-109）。TaskRunner Phase 1.5 Stretch 修复 CudaScheduler 中 5 个 `dynamic_cast<CudaStub*>` 硬绑定后，IGpuDriver 虚接口路径自然启用，无需 UsrLinuxEmu 侧配合改动。

**验证**: 本 change 的 tasks.md 中无任何 `.cpp/.h` 编辑步骤。

### D2: 验证策略 = 现有 GPU 测试回归 + GpuDriverClient 后端 smoke test

**理由**: 现有 4 个 GPU 测试（test_gpu_ioctl / test_va_space / test_gpu_ringbuffer / test_gpu_plugin）覆盖了 IOCTL / VA Space / Queue / Plugin 加载的完整链路。bump submodule 后重跑确认零回归。Smoke test 补充 GpuDriverClient 后端路径——这是本次 change 的核心增量。

## 验证矩阵

| 步骤 | 测试/命令 | 预期结果 | 负责方 |
|------|----------|---------|--------|
| 前置 Gate | TaskRunner 76/76 tests PASS | 全部绿 | TaskRunner change |
| 前置 Gate | 79 cu\* 符号不变 | `nm -D` 输出 79 | TaskRunner change |
| 基线 | UsrLinuxEmu 4 GPU tests | 全部 PASS | 本 change |
| Submodule bump | `git pull origin main` in external/TaskRunner | submodule commit hash 更新 | 本 change |
| 回归 | UsrLinuxEmu 4 GPU tests (post-bump) | 全部 PASS | 本 change |
| Smoke | cudaMalloc → memcpy → launch → memcpy → free | 不返回 -ENOSYS | 本 change |

## 风险缓解

- **Submodule bump 回滚**: `git checkout HEAD~1 -- external/TaskRunner`（task 7.1 已定义）
- **零代码改动**: 本 change 无编译风险
- **Smoke test 失败**: 回滚 submodule，通知 TaskRunner 侧修复；UsrLinuxEmu 侧不受影响