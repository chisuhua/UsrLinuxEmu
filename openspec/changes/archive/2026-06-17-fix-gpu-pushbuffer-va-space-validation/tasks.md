# Tasks: fix-gpu-pushbuffer-va-space-validation

> **依赖**: proposal ✅ / design ✅ / specs ✅
> **预估总工时**: 1-2 周（含 TaskRunner 同步 PR）

## 1. 协议扩展（前置依赖，跨仓库）

- [ ] 1.1 在 `plugins/gpu_driver/shared/gpu_ioctl.h` 的 `struct gpu_pushbuffer_args` 末尾追加 `u64 va_space_handle;` 字段（保持现有字段偏移）
- [ ] 1.2 在 `external/TaskRunner/` 创建同步 PR：
  - PR 标题：`fix(taskrunner): sync gpu_pushbuffer_args va_space_handle field`
  - 复用相同头（通过符号链接），但更新所有引用 `gpu_pushbuffer_args` 的使用方以确保零初始化行为不变
  - 验证 `external/TaskRunner` 自身构建通过
- [ ] 1.3 验证两仓库构建同步：项目根 `make -j4` + external/TaskRunner 自身构建均通过

## 2. 驱动实现

- [ ] 2.1 定位 `GpgpuDevice::vaSpaceExists(uint64_t)` 方法定义（推测在 `gpgpu_device.h:140` 附近），确认签名匹配
- [ ] 2.2 在 `handlePushbufferSubmitBatch`（gpgpu_device.cpp:247）的 `args->count` 校验**之后**插入校验块：
  ```cpp
  if (args->va_space_handle != 0) {
    if (!vaSpaceExists(args->va_space_handle)) {
      Logger::warn(...);
      return -EINVAL;
    }
    auto& va_space = va_spaces_[args->va_space_handle];
    if (std::find(va_space.attached_queues.begin(),
                  va_space.attached_queues.end(),
                  args->stream_id) == va_space.attached_queues.end()) {
      Logger::warn(...);
      return -EINVAL;
    }
  }
  ```
- [ ] 2.3 确认 `include/kernel/logger.h` 已 include（gpgpu_device.cpp 顶部）
- [ ] 2.4 确认 `<algorithm>` 已 include（用于 `std::find`）—— 必要时添加

## 3. 测试

- [ ] 3.1 新建 `tests/test_gpu_pushbuffer_validation.cpp`（Catch2 standalone）：
  - 3.1.1 Case A: 创建 VA Space → 创建 Queue → attach → submit pushbuffer → 期望成功
  - 3.1.2 Case B: 提交 pushbuffer 带 invalid va_space_handle → 期望 -EINVAL
  - 3.1.3 Case C: 创建 VA Space → 创建 Queue 但**不 attach** → submit pushbuffer 带 va_space_handle → 期望 -EINVAL
  - 3.1.4 Case D: submit pushbuffer 不带 va_space_handle（=0）→ 期望成功（向后兼容）
- [ ] 3.2 在 `tests/CMakeLists.txt` L88-91 之后追加：
  ```cmake
  add_executable(test_gpu_pushbuffer_validation_standalone test_gpu_pushbuffer_validation.cpp)
  target_link_libraries(test_gpu_pushbuffer_validation_standalone kernel gpu_drv)
  add_test(NAME test_gpu_pushbuffer_validation_standalone COMMAND test_gpu_pushbuffer_validation_standalone)
  ```
- [ ] 3.3 运行 `make test_gpu_pushbuffer_validation_standalone && ./bin/test_gpu_pushbuffer_validation_standalone` 验证

## 4. 文档同步

- [ ] 4.1 `docs/02_architecture/post-refactor-architecture.md` §1.3 v0.1.2 勘误段：删除"实际代码未实现这两个校验——handler 仅校验 count > 0"与"OpenSpec change [fix-gpu-pushbuffer-va-space-validation] 跟踪"指引。改写为：
  > Phase 2 校验（**已实现** — commit XXX，change fix-gpu-pushbuffer-va-space-validation）：
  > ├─→ validate VA Space exists（args->va_space_handle != 0 时）
  > ├─→ validate Queue belongs to VA Space（同上）
- [ ] 4.2 `docs/06-reference/ioctl-commands.md` 在 §GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH 表格加字段：`va_space_handle (u64, 0=向后兼容)`
- [ ] 4.3 `docs/02_architecture/post-refactor-architecture.md` 变更记录表新增一行 v0.1.3，引用本 change

## 5. 验证

- [ ] 5.1 `make -j4` 100% 通过
- [ ] 5.2 `cd build && ctest --output-on-failure` 33+1（新增）= 34/34 通过
- [ ] 5.3 `bash tools/docs-audit.sh --strict` 36/36 PASS（新增 1 个 spec 不应破坏 audit）
- [ ] 5.4 `external/TaskRunner` 自身 CI 通过

## 6. 提交与归档

- [ ] 6.1 本仓库 commit（按子任务拆分，建议 3 个 commit）：
  - `feat(gpu-ioctl): extend gpu_pushbuffer_args with va_space_handle field`
  - `feat(gpu-ioctl): add VA Space + Queue validation to PUSHBUFFER_SUBMIT_BATCH handler`
  - `test(gpu): add test_gpu_pushbuffer_validation covering 4 cases`
- [ ] 6.2 `openspec archive fix-gpu-pushbuffer-va-space-validation` 把 change 归档到 `openspec/changes/archive/`
- [ ] 6.3 SSOT v0.1.3 变更记录表追加 commit hash

## 回滚预案

任一 Phase 失败可独立 `git revert`；不需要数据库迁移。TaskRunner 子模块如果拒绝同步，回滚本仓库 `gpu_ioctl.h` 字段即可（TaskRunner 侧保持旧版不会构建失败，因为新字段是 0-初始化的）。
