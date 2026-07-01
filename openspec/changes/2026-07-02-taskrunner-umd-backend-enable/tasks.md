# Tasks: taskrunner-umd-backend-enable

> **依赖**: TaskRunner Phase 1.5 Stretch 完成（`external/TaskRunner/docs/superpowers/plans/2026-07-02-phase1.5-cudastub-dynamic-cast-fix.md` 全部 task checked）
> **预估工时**: 30-60 分钟（验证为主，无代码改动）
> **约束**: 单 commit（submodule bump）+ UsrLinuxEmu 现有测试全 PASS

---

## 前置条件：TaskRunner 侧完成

- [ ] 0.1 确认 TaskRunner Phase 1.5 Stretch 已 commit + push 到 `origin/main`：
  ```bash
  cd /workspace/project/UsrLinuxEmu/external/TaskRunner
  git log --oneline -5
  # 应包含: fix(scheduler): remove dynamic_cast<CudaStub*> ...
  ```
- [ ] 0.2 确认 TaskRunner 侧 76/76 测试全 PASS：
  ```bash
  cd /workspace/project/UsrLinuxEmu/external/TaskRunner
  cd build && cmake .. -DTASKRUNNER_BUILD_MODE=umd-evolution && make -j4
  for t in test_cuda_scheduler test_gpu_architecture test_gpu_phase2 \
            test_cuda_runtime_api test_cuda_shim; do
    ./$t 2>&1 | tail -1
  done
  # 应全部 PASS
  ```
- [ ] 0.3 确认 79 cu\* 符号不变：
  ```bash
  nm -D --defined-only build/libcuda_taskrunner.so | grep -c " cu[A-Z]"
  # 应输出: 79
  ```

---

## 1. UsrLinuxEmu 侧基线验证

- [ ] 1.1 跑 UsrLinuxEmu 现有 GPU 测试基线（确认 bump 前正常）：
  ```bash
  cd /workspace/project/UsrLinuxEmu
  cd build && make -j4
  ./bin/test_gpu_ioctl_standalone 2>&1 | tail -1
  ./bin/test_va_space_standalone 2>&1 | tail -1
  ./bin/test_gpu_ringbuffer_standalone 2>&1 | tail -1
  ./bin/test_gpu_plugin 2>&1 | tail -1
  # 应全部 PASS
  ```

---

## 2. Submodule Bump

- [ ] 2.1 Bump submodule pointer 到 TaskRunner 最新 HEAD：
  ```bash
  cd /workspace/project/UsrLinuxEmu
  cd external/TaskRunner && git pull origin main && cd ../..
  git add external/TaskRunner
  ```
- [ ] 2.2 确认 bump 后 TaskRunner 代码可被 UsrLinuxEmu 构建系统识别：
  ```bash
  git diff --cached external/TaskRunner
  # 应只显示 submodule commit hash 变更
  ```

---

## 3. GpuDriverClient Backend Smoke Test

- [ ] 3.1 在 TaskRunner 中运行 GpuDriverClient 后端 smoke test（创建临时测试或用现有测试路径）：
  ```bash
  cd /workspace/project/UsrLinuxEmu/external/TaskRunner
  # 构建 UMD mode
  cd build && cmake .. -DTASKRUNNER_BUILD_MODE=umd-evolution && make -j4
  ```
- [ ] 3.2 验证 CudaScheduler 可通过 GpuDriverClient 完成 submitMemAlloc（不再返回 -ENOSYS）：
  - 方法 A：写临时 smoke test 注入 GpuDriverClient 作为 IGpuDriver 并调用 `cudaMalloc`
  - 方法 B：使用现有 CLI 命令 `cuda_runtime_alloc` 验证（如果 CLI 支持 backend 切换）
- [ ] 3.3 验证完整链路（如 smoke test 可执行）：
  ```
  cudaMalloc → cudaMemcpy(H2D) → cudaLaunchKernel → cudaMemcpy(D2H) → cudaFree
  ```
  全部操作不应返回 -ENOSYS。

---

## 4. TADR Mirror 更新（如需）

- [ ] 4.1 检查是否需要更新 TADR mirror 表：
  ```bash
  grep -A 5 "Phase 1.5\|dynamic_cast\|CudaStub" /workspace/project/UsrLinuxEmu/docs/00_adr/README.md
  ```
  如果 Phase 1.5 有新建 TADR，在此步骤添加 mirror 条目。
- [ ] 4.2 如果 TaskRunner 侧无新建 TADR（Phase 1.5 是 stretch fix，不产生新 ADR），跳过此步骤。

---

## 5. 最终验证

- [ ] 5.1 确认 submodule bump 后 UsrLinuxEmu 构建正常：
  ```bash
  cd /workspace/project/UsrLinuxEmu/build && make -j4
  ```
- [ ] 5.2 重跑 UsrLinuxEmu 全部 GPU 测试：
  ```bash
  cd /workspace/project/UsrLinuxEmu
  ./build/bin/test_gpu_ioctl_standalone 2>&1 | tail -1
  ./build/bin/test_va_space_standalone 2>&1 | tail -1
  ./build/bin/test_gpu_ringbuffer_standalone 2>&1 | tail -1
  ./build/bin/test_gpu_plugin 2>&1 | tail -1
  ```
  全部应 PASS。

---

## 6. 提交 + 归档

- [ ] 6.1 单 commit：
  ```bash
  cd /workspace/project/UsrLinuxEmu
  git commit -m "chore(submodule): bump TaskRunner for Phase 1.5 dynamic_cast fix

  TaskRunner Phase 1.5 Stretch removes 5 dynamic_cast<CudaStub*>
  sites in CudaScheduler, enabling GpuDriverClient backend to
  serve CUDA Driver API memory/launch operations via IGpuDriver
  virtual interface.

  This unblocks the libcuda_taskrunner.so → GpuDriverClient →
  UsrLinuxEmu real backend end-to-end path.

  TaskRunner commit: <insert-hash>
  Cross-repo change: taskrunner-umd-backend-enable
  "
  ```
- [ ] 6.2 Push:
  ```bash
  git push origin main
  ```
- [ ] 6.3 归档本 change：
  ```bash
  # 更新 .openspec.yaml: status: DRAFT → status: ARCHIVED, 添加 archived: 2026-07-02
  # 移动目录:
  mv openspec/changes/2026-07-02-taskrunner-umd-backend-enable \
     openspec/changes/archive/2026-07-02-taskrunner-umd-backend-enable
  ```

---

## 7. 回滚预案

- Submodule bump 后出问题：
  ```bash
  cd /workspace/project/UsrLinuxEmu
  git checkout HEAD~1 -- external/TaskRunner
  git commit -m "chore(submodule): revert TaskRunner bump"
  ```
- 恢复后验证 UsrLinuxEmu GPU 测试仍全部 PASS。