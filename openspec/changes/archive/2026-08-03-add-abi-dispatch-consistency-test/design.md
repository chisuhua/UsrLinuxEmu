## Context

`add-abi-dispatch-consistency-test` 修复 `System C IOCTL 端到端测试完备性审计（2026-08-02）` 识别的能力缺口。

- **ADR-036 + ADR-018 + ADR-023** — `GpgpuDevice::ioctl` 派发表是 ② 驱动层对外契约的唯一权威；ABI 头（`gpu_ioctl.h`）定义 38 个命令（实际命令）+ `GPU_IOCTL_BASE`（helper），活跃派发表是 36 项（kNumIoctls）。两者必须严格同步
- **openspec `2026-07-07-stage3-ioctl-dispatch-completeness/proposal.md`** — 记录 PR #20 失败模式：DRM 表更新了 18 项 Phase 3 IOCTL，运行时派发表漏接（后又补上，但 0x02/0x03 残留至今即 P0 提案内容）。无自动化测试拦截此类漂移
- **现状（已审计验证）**：
  - 头文件 38 个 `#define GPU_IOCTL_*` 命令（行 40-635），加 `GPU_IOCTL_BASE` 共 39 个宏
  - `GpgpuDevice::getIoctlTablePtr()` 36 项（`gpgpu_device.cpp:97-135`），缺 0x02/0x03（P0 待修）
  - DRM 表 38 项（`gpu_drm_driver.cpp:700-741`），但无运行时消费者
  - 三个派发表数（36/38/38）相互间无编译期或测试期一致性保证
- **rdd-workflow TDD 纪律** — 漂移检测应作为硬性 CI 门禁，而非人工 code review 兜底
- **AGENTS.md 风格** — Catch2 + `REQUIRE` + 失败信息含具体值

## Goals / Non-Goals

**Goals:**

- 新增 `tests/test_ioctl_abi_dispatch_consistency_standalone.cpp`（Catch2）
- 测试内硬编码 `constexpr std::array<uint32_t, 38> ABI_IOCTL_REQUESTS`，包含全部 38 个 `GPU_IOCTL_*` 命令的 request 值（按数值排序）
- 数值取自 `plugins/gpu_driver/shared/gpu_ioctl.h`（`#define` 后的 `_IOWR(...)` 编码结果）
- 注释行引用每个值对应的 ABI 名（如 `{0x01, "PUSHBUFFER_SUBMIT_BATCH"}`）
- 测试逻辑：
- CMake 注册：`add_standalone_test` + `add_test`，WORKING_DIRECTORY = PROJECT_SOURCE_DIR
- 测试需链接 `gpu_drv`（仅访问 `GpgpuDevice::getIoctlTablePtr()`，不需 sim/hal 链接）
- 测试成功 FAIL 时输出：缺失 request 列表 + 期望但未派发的 ABI 名 + 派发表多余项
- AGENTS.md "关键架构决策"中增加一条"ABI—派发表漂移检测"说明

**Non-Goals:**

- - 重构 `gpu_ioctl.h` 为 X-macro 列表（另案，保持本提案最小变更）
  - 校验 DRM 表（`gpu_drm_driver.cpp:700-741`）与 ABI 一致（DRM 表是死代码，P0 提案会决定其去留；本提案不重复校验）
  - 校验 handler 函数指针非空（仅校验 request 值集合；handler 存在性由既有 stub 测试覆盖）
  - 自动更新期望集（每次 ABI 变更需手动更新 `ABI_IOCTL_REQUESTS`，这是预期 TDD 行为）

## Decisions

**架构决策**:
- **决策 1**: P2 优先级 — 修改活跃派发表而非 ABI 头（保持 ABI 兼容）
**技术约束（继承自 improvement）**:
- **MUST**:
  - `ABI_IOCTL_REQUESTS` 数组使用 `constexpr std::array<uint32_t, 38>`，**必须按数值升序**
  - 每个值后跟 `// <NAME>` 注释引用 ABI 名（如 `0x10, // ALLOC_BO`），便于未来 PR review 时一眼对照
  - 测试用 `REQUIRE(std::is_sorted(...))` 断言数组已排序（CI 拦截未排序提交）
  - 派发表枚举通过直接调用 `GpgpuDevice::getIoctlTablePtr()` + `dispatchCount()`，不依赖 VFS / plugin / HAL
  - 失败信息必须列出具体 request 值 + 缺/多对应的 ABI 名（`std::set_difference` + 映射表）
  - 测试不需要 sim 链接，**不**走 `add_catch_sim_test`，仅 `add_standalone_test` 链接 kernel + gpu_drv
  - 测试在 `git diff` 中应仅含：1 个新测试文件 + 1 个新增 add_test 调用 + AGENTS.md 一行说明
- **MUST NOT**:
  - 不修改 `plugins/gpu_driver/shared/gpu_ioctl.h` ABI 头
  - 不修改 `plugins/gpu_driver/drv/gpgpu_device.cpp` 派发表（除非本次先 P0 同步合并；本提案假设 P0 已合或并行合）
  - 不重构 `gpu_ioctl.h` 为 X-macro
  - 不在测试中 `#include` `sim/` 内部头
  - 不依赖 DRM 表（DRM 表是死代码，不应作为派发表权威）
  - 不引入新 HAL fn-ptr
- **SHOULD**:
  - 数组值命名清晰：`ABI_IOCTL_REQUESTS`，副标题 `// Auto-generated from gpu_ioctl.h; keep sorted`
  - 缺失/多余值用 `INFO(...)` 输出，让 Catch2 报告带上下文
  - 测试加 tag `[abi][dispatch][consistency]` 便于按 tag 过滤
  - commit message 引用 openspec 事故 ID `2026-07-07-stage3-ioctl-dispatch-completeness`，并说明"P0 与本提案协同合入：先 P0 修复派发表到 38 项，本测试才能通过"

## Risks / Trade-offs

- [Risk] 活跃派发表扩展可能冲击既有 36 项 handler → [Mitigation] 既有 ctest 全 PASS + spot-check 测试独立验证
- [Risk] handler 实现桥接到 sim 层（违反分层 ADR-023）→ [Mitigation] 走 `kfd_sim_bridge` 公开 API，不直接 `#include "sim/"`
- [Risk] 测试覆盖深度不足 → [Mitigation] 验收标准明确列出 CTest 断言，包含 sim 侧回调计数与入参可观察状态

## Migration Plan

1. 修改活跃派发表 + handler 实现
2. 编写/强化测试（standalone binary）
3. CTest registered: `cd build && ctest --output-on-failure`
4. ABI 头 `git diff` 必须为空（兼容性回归保护）

## Open Questions

- 无 — 此 change 是增量修复，独立后续提案处理遗留问题（DRM 表 / render node）
