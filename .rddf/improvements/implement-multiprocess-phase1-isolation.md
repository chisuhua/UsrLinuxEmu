# implement-multiprocess-phase1-isolation

**优先级**: P2 | **来源**: ADR-011 §Phase 1 + gap-analysis §2.1
**阶段**: stage-3 (trigger-gated) | **分类**: arch-design
**类型**: functional (process isolation)

## 架构依据

[ADR-011](docs/00_adr/adr-011-multiprocess-support.md) 🔄 提议中（2026-03 起草，2026-07-16 C-12 mini-gate C.0.3 降级决策批准）。

Phase C.0.3 决策：
> Phase C.2.3 使用 multi-thread single-process 方案，不改动本 ADR 的 Phase 1/2 完整多进程支持方向；本 ADR 继续等待 Phase 3 触发条件

[stage4-gpu-cp-completion-gap-analysis.md §2.1](docs/architecture/stage4-gpu-cp-completion-gap-analysis.md) 显式列为剩余差距：

> 多进程支持：单进程（per ADR-011 决策：Phase C.2.3 用 multi-thread single-process 方案）→ 多进程 → ADR-011（🔄 提议中，等待 Phase 3 触发）

**Phase 1 范围**（per ADR-011）：
- 共享内存 + 进程内隔离
- 设备级进程隔离（Linux namespaces）
- 资源隔离（Cgroups）
- 故障隔离（per-device process crash 不影响其他）

**Phase 1 不包含**：
- 跨进程 GPU 设备共享
- IPC 机制（socket / pipe / shmem）
- 分布式 GPU 调度

## 范围

- **In Scope**:
  - `IsolatedDevice` 类封装 PID namespace + Cgroup 资源限制
  - `SharedMemoryRegion` 跨进程 GPU ring buffer / event page 共享
  - `/proc/<pid>/devices.json` 暴露设备 → PID 映射
  - CLI 子命令 `cli devices --isolated` 列出隔离设备
  - Process crash 检测（SIGCHLD + reap）→ 清理 GPU 资源
  - 配套测试 `test_multiprocess_isolation_standalone.cpp`
- **Out Scope**:
  - 跨进程 GPU buffer 共享（Phase 2）
  - 多设备 IPC（Phase 2）
  - 真实 Linux namespace API（仅模拟 namespace 行为）

## 关键场景

- GIVEN 用户启动 `cli run --isolated /dev/gpgpu0 ./my_app`
  - WHEN 执行
  - THEN 创建独立 PID namespace + Cgroup，my_app 看到独立 /dev/gpgpu0
- GIVEN my_app 崩溃（SIGSEGV）
  - WHEN 退出
  - THEN GPU 资源被自动清理（fence / BO / VA space），不影响其他进程
- GIVEN 两个隔离设备 GPGPU0 + GPGPU1
  - WHEN 两个进程分别使用
  - THEN 互不影响，单个崩溃不传播
- GIVEN 测试套件执行 WHEN Phase 1 完成 THEN ctest 全部 PASS

## 技术约束

- MUST 保持现有 single-process API 向后兼容
- MUST 复用 `kernel_thread_base`（per ADR-060）+ `kernel_workqueue`
- MUST NOT 引入新的全局单例（per Issue #11）
- SHOULD 使用 `clone(CLONE_NEWPID)` 而非真实 `unshare()`
- SHOULD 提供 fallback：未启用 namespace 时仍能工作（单进程模式）

## 验收标准

- `IsolatedDevice` 类实现 namespace + Cgroup 模拟
- `cli run --isolated` 子命令可用
- 进程崩溃后 GPU 资源自动清理（fence / BO / VA space）
- 新增 `test_multiprocess_isolation_standalone.cpp`，至少 5 个 test case 覆盖：
  - IsolatedDevice 创建 + cleanup
  - 进程崩溃 → 资源回收
  - 多进程并发访问不同设备
  - Cgroup 资源限制生效
  - SharedMemoryRegion 共享 ring buffer
- `make -j4` 编译通过，无 warning
- `ctest --output-on-failure` 全部 PASS
- 修改的代码行通过 `lsp_diagnostics` 检查
