# Tasks: strengthen-semantic-assertions-for-destroy-va-space-and-query-queue

## 1. Implementation

- [ ] 1.1 `tests/test_va_space.cpp`：
- [ ] 1.2 新增/强化 `DESTROY_VA_SPACE` 语义断言：destroy 后对同一 handle 二次 destroy 返回 `< 0`；destroy 后用同一 handle 创建 queue 返回 `< 0`；destroy 重复调用不引起崩溃
- [ ] 1.3 不破坏既有 `CREATE_VA_SPACE` / `CREATE_QUEUE` / `DESTROY_QUEUE` 测试用例
- [ ] 1.4 `tests/test_gpu_plugin.cpp`：
- [ ] 1.5 新增 `TEST_CASE "GPU_IOCTL_QUERY_QUEUE E2E semantic"`：
- [ ] 1.6 前置：`CREATE_VA_SPACE` + `CREATE_QUEUE` 携带特定 `queue_type`（如 `GPU_QUEUE_TYPE_COMPUTE=0`）和 `ring_buffer_size`
- [ ] 1.7 `QUERY_QUEUE` 后断言：
- [ ] 1.8 `ret == 0`
- [ ] 1.9 `args.queue_type == 创建时设定值`
- [ ] 1.10 `args.queue_id != 0`（非零 ID）
- [ ] 1.11 `args.doorbell_offset ∈ [DOORBELL_ALLOC_BASE, ...)`（合理范围）
- [ ] 1.12 `args.ring_buffer_size == 创建时设定值`
- [ ] 1.13 负路径：`QUERY_QUEUE` 携带不存在的 `queue_handle` → `< 0`

## 2. Verification

- [ ] 2.1 `tests/test_va_space.cpp` 新增 DESTROY_VA_SPACE 语义 TEST_CASE PASS：
- [ ] 2.2 `tests/test_gpu_plugin.cpp` 新增 `GPU_IOCTL_QUERY_QUEUE E2E semantic` TEST_CASE PASS：
- [ ] 2.3 既有 `tests/test_va_space.cpp` 全部既有 TEST_CASE 仍 PASS（不修改既有断言）
- [ ] 2.4 既有 `tests/test_gpu_plugin.cpp` 36 项既有 TEST_CASE 仍 PASS
- [ ] 2.5 既有 `test_stub_handlers_tier2_standalone` / `test_kfd_l1_l2_bridge_standalone` / `test_gpu_mempool_export_standalone` / `test_register_cb_ioctl_standalone`（若 P0 已合并）全 PASS，0 regression
- [ ] 2.6 `handleDestroyVASpace` / `handleQueryQueue` 实现未变
- [ ] 2.7 `plugins/gpu_driver/shared/gpu_ioctl.h` / `gpu_queue.h` ABI 头未变
- [ ] 2.8 `docs/00_adr/` 中无新增 ADR
- [ ] 2.9 若测试暴露实现 bug：记录到 `docs/roadmap/` follow-up，不在本提案修复

## 3. Process / Documentation

- [ ] 3.1 AGENTS.md / README 中 IOCTL 列表同步（如适用）
- [ ] 3.2 提交信息包含 openspec change 名（`strengthen-semantic-assertions-for-destroy-va-space-and-query-queue`）+ 引用改进提案路径
- [ ] 3.3 CTest 全 PASS（`cd build && ctest --output-on-failure`），0 regression
