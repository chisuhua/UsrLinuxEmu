# Tasks: phase4-sim-graph-launch-test-gaps

> **状态**: ✅ COMPLETE
> **目标**: Close 4 coverage gaps identified in `bg_01583a02` audit (P1, P2, H1, plus null guards)
> **范围**: UsrLinuxEmu-internal test additions only — no production code changes
> **最后验证**: 2026-07-09 (commit `6610b4c`)
> **结果**: 86 → 91 用例（52→54 gpu_plugin + 10→12 puller + 17→19 sim_graph + 86 ctest PASS, 0 regressions）

## 1. drv 层错误路径测试（test_gpu_plugin.cpp）

- [x] 1.1 添加 `GPU_IOCTL_GRAPH_LAUNCH empty executable returns -EINVAL` 测试用例
  - 创建空图（无节点），instantiate 成功（entry_count=0）
  - 调用 GRAPH_LAUNCH
  - 断言返回 -EINVAL（drv 层 `gpgpu_device.cpp:854-859`）
- [x] 1.2 添加 `GPU_IOCTL_GRAPH_LAUNCH missing queue returns -ENOENT` 测试用例
  - 创建 VA Space + Queue → 创建图 → instantiate（成功）
  - DESTROY_QUEUE（删除队列）
  - 调用 GRAPH_LAUNCH，stream_id 指向已删除队列
  - 断言返回 -ENOENT（drv 层 `gpgpu_device.cpp:864-869`）
- [x] 1.3 运行 `test_gpu_plugin_standalone` 验证两个新用例通过
- [x] 1.4 运行完整 ctest 验证 86 基线 + 新用例全部通过

## 2. Puller fence 多条目信号测试（test_hardware_puller_emu.cpp）

- [x] 2.1 添加 `test_puller_fence_signal_multi_entry` 测试函数
  - 创建 mock_hal + doorbell + puller
  - 分配 sim fence_id
  - 调用 `submitBatch(0x1000, 3, fence_id)`（3 个条目）
  - 调用 `doorbell.write(0)` 触发
  - 使用现有 `wait_for_state` drain 模式等待 IDLE
  - 断言 fence 已被 signal（`sim_fence_id_check` → signaled=true）
- [x] 2.2 添加 `test_puller_fence_not_signaled_at_intermediate_entry` 测试函数（防御性）
  - 验证 fence 在第 2 个条目完成时**未**被 signal
  - 这确保 `current_index_ + 1 >= total_entries_` 边界正确
  - 通过跨 batch fence 隔离（pending_fence_id_ 不能跨 batch 泄漏）验证
- [x] 2.3 运行 `test_hardware_puller_emu_standalone` 验证两个新用例通过
- [x] 2.4 运行完整 ctest 验证基线 + 新用例全部通过

## 3. sim_graph null guard 测试（test_sim_graph_standalone.cpp）

- [x] 3.1 添加 `graph — create with NULL handle returns -EINVAL` 测试用例
  - 调用 `sim_graph_create(nullptr)`
  - 断言返回 -EINVAL（`graph.cpp:173-174`）
- [x] 3.2 添加 `graph — instantiate with NULL exec handle returns -EINVAL` 测试用例
  - 创建有效图
  - 调用 `sim_graph_instantiate(graph_handle, nullptr)`
  - 断言返回 -EINVAL（`graph.cpp:236-237`）
- [x] 3.3 运行 `test_sim_graph_standalone` 验证两个新用例通过

## 4. 验证 / commit（半天）

- [x] 4.1 完整 ctest 验证（86 → 91 用例，全部通过）
- [x] 4.2 docs-audit 无新 warning
- [x] 4.3 commit：`test(gpu): close phase4-sim-graph-launch drv error path gaps`
- [x] 4.4 推送到 origin/main

## 预期结果

| 指标 | 前 | 后 |
|------|------|------|
| test_gpu_plugin 用例数 | 52 | 54 |
| test_hardware_puller_emu 用例数 | 10 | 12 |
| test_sim_graph_standalone 用例数 | 17 | 19 |
| 总 ctest 用例数 | 86 | 91 |
| drv 错误路径覆盖 | 2/4 HIGH gaps | 4/4 HIGH gaps |
| 多条目 fence 信号覆盖 | 0 (1 entry only) | ✅ multi-entry tested |
| sim null guard 覆盖 | 0 | ✅ NULL paths tested |
