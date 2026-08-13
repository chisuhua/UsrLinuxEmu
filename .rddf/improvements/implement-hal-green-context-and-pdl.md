# implement-hal-green-context-and-pdl

**优先级**: P1 | **来源**: 项目健康报告 §3.A + HAL 深度分析 Group 7+8
**阶段**: stage-4.6 | **分类**: core-impl
**类型**: functional (ADR-056 完整实施)

## 架构依据

ADR-056 规定 Green Context（低优先级可抢占 CUDA 上下文）和 Programmatic Dependent Launch (PDL) 两个特性，对应 4 个 HAL fn-ptrs：

```c
int (*hal_green_context_create)(void *ctx, uint64_t tsg_id, uint64_t *out_handle);
int (*hal_green_context_destroy)(void *ctx, uint64_t handle);
int (*hal_pdl_launch)(void *ctx, uint64_t kernel_addr, uint64_t kernargs_va,
                       uint32_t grid_x, uint32_t block_x,
                       uint64_t *out_signal_handle);
int (*hal_pdl_signal_completion)(void *ctx, uint64_t signal_handle, uint64_t value);
```

**当前状态**（HAL 深度分析 Group 7+8）：

| fn-ptr | user 实现 (line) | mock 实现 (line) | 真实状态 |
|--------|------------------|------------------|---------|
| `hal_green_context_create` | 返回 -ENOSYS (308-310) | 分配递增 handle (322-326) | 完全 stub |
| `hal_green_context_destroy` | 返回 -ENOSYS (311-313) | no-op (327-330) | 完全 stub |
| `hal_pdl_launch` | 返回 -ENOSYS (314-317) | 分配 sem handle (331-336) | 完全 stub |
| `hal_pdl_signal_completion` | 返回 -ENOSYS (318-320) | no-op (337-339) | 完全 stub |

**关键前置调查**（实施前必做）：

- 是否存在 `plugins/gpu_driver/sim/green_context.{h,cpp}` 提供 Green Context 实体？
- 是否存在 `plugins/gpu_driver/sim/pdl.{h,cpp}` 或 `sim/pdl_launcher.{h,cpp}` 提供 PDL 调度？
- 若 sim 层缺失 → 此 change 需分两阶段：(a) 创建 sim 实体；(b) HAL 对接

参考 ADR-056 和已存在的 `archive/2026-08-03-stage4-6-green-context-pdl-closeout/` change 中的实施记录（如果存在）。

## 范围

- **In Scope**:
  - `plugins/gpu_driver/hal/hal_user.cpp` — 4 个 lambda 从 stub 升级为真实委托
  - 验证/创建 sim 层支持：
    - `plugins/gpu_driver/sim/green_context.{h,cpp}`（如缺失则创建最小骨架）
    - `plugins/gpu_driver/sim/pdl.{h,cpp}`（如缺失则创建最小骨架）
  - `struct hal_user_context` 新增 green context / pdl 实例存储
  - 完整 ctest PASS（基线 130/130）
  - 新增 green-context + PDL 单元测试
- **Out Scope**:
  - Preemption / Semaphore（独立 proposal）
  - 高级 Green Context 调度策略（仅实现基础 API）
  - PDL 多 kernel 链式依赖（仅实现基础 launch + signal）

## 关键场景

- GIVEN drv/ 调用 `hal_green_context_create(tsg_id, &handle)`
  - WHEN **修复前** THEN 返回 -ENOSYS，drv 调用失败
  - WHEN **修复后** THEN 真实创建 green context 绑定 TSG，返回 handle
- GIVEN drv/ 调用 `hal_pdl_launch(kernel_addr, kernargs, grid_x, block_x, &sem_handle)`
  - WHEN **修复前** THEN 返回 -ENOSYS
  - WHEN **修复后** THEN 真实启动 PDL kernel，返回 signal semaphore handle
- GIVEN PDL kernel 执行完成
  - WHEN drv/ 调用 `hal_pdl_signal_completion(sem_handle, value)` THEN 真实信号 semaphore，触发任何 waiter
- GIVEN green context 不再需要 WHEN drv/ 调用 `hal_green_context_destroy(handle)` THEN 真实释放

## 技术约束

- MUST 调查 sim 层是否存在 green context + PDL 实体（如缺失需在范围内创建）
- MUST 复用 sim 层的硬件 puller（如 `HardwarePullerEmu` 已能处理 kernel dispatch）
- MUST 复用 `SemaphoreManager` 实现 signal/wait（与 Proposal 2 协同）
- MUST NOT 修改 `struct gpu_hal_ops` 签名
- MUST NOT 修改 `hal_mock` 实现
- MUST 保持 TSG 绑定语义（tsg_id 是 timestamp queue group 标识符）
- SHOULD 复用现有 GPU queue 提交路径（PDL 是 kernel launch 的特例）
- SHOULD 处理 grid/block dim 验证（合理性检查）

## 验收标准

- `hal_user.cpp` 中 4 个 lambda 真实化
- sim 层（如缺失）创建 `green_context.{h,cpp}` 和 `pdl.{h,cpp}` 最小骨架
- `struct hal_user_context` 新增必要字段
- 新增单元测试：
  - `test_green_context_create_destroy`
  - `test_pdl_launch_signal_completion`
- `make -j4` 编译通过，无 warning
- `ctest --output-on-failure` 全部 PASS
- 端到端验证：drv/ 创建 green context → 在其上 PDL launch → kernel 完成 → signal → 资源释放
- Sanitizer run (ASan/UBSan) PASS
- `lsp_diagnostics` 无 error
- 文档同步：`openspec/specs/green-context/spec.md` 和 `pdl-launch/spec.md` 中 `TBD Purpose` 更新为完整描述（Reference [link to spec]）