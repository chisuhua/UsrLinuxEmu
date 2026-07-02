# Capability: taskrunner-umd-backend-verification

验证 TaskRunner `libcuda_taskrunner.so` 可通过 GpuDriverClient 后端与 UsrLinuxEmu 真实 GPU 后端完成 E2E 交互。

## ADDED Requirements

### Requirement: TaskRunner CudaScheduler 可通过 GpuDriverClient 调用 IGpuDriver 内存操作

TaskRunner Phase 1.5 Stretch 完成后，CudaScheduler 中的 `cudaMalloc` / `cudaMemcpy(H2D)` / `cudaLaunchKernel` / `cudaMemcpy(D2H)` / `cudaFree` 路径 MUST 通过 IGpuDriver 虚接口调用 GpuDriverClient，不再返回 `-ENOSYS`（由旧的 `dynamic_cast<CudaStub*>` 硬绑定导致）。

#### Scenario: Submodule bump 后 UsrLinuxEmu 现有 GPU 测试全部 PASS

- **WHEN** UsrLinuxEmu 将 `external/TaskRunner` submodule pointer bump 到 TaskRunner Phase 1.5 Stretch 完成后的 commit
- **AND** 从项目根目录运行 `cd build && make -j4` 构建成功
- **THEN** `test_gpu_ioctl_standalone`, `test_va_space_standalone`, `test_gpu_ringbuffer_standalone`, `test_gpu_plugin` MUST 全部 PASS
- **AND** 无回归错误（对比 bump 前基线）

#### Scenario: GpuDriverClient 后端 smoke test 通过完整内存链路

- **WHEN** 使用 GpuDriverClient 作为 IGpuDriver 后端运行 smoke test
- **THEN** `cudaMalloc` → `cudaMemcpy(H2D)` → `cudaLaunchKernel` → `cudaMemcpy(D2H)` → `cudaFree` 完整链路 MUST 不返回 `-ENOSYS`
- **AND** allocated BO handle 非零，fence_id 可被 `GPU_IOCTL_WAIT_FENCE` 等待