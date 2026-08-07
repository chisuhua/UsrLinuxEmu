# implement-pm4-microcode-parsing

**优先级**: P1 | **来源**: ADR-052 D3 显式延后 + gap-analysis §2.1
**阶段**: stage-5 | **分类**: core-impl
**类型**: functional (new format support)

## 架构依据

[ADR-052](docs/00_adr/adr-052-aql-pm4-packet-native-support.md) 定义 UsrLinuxEmu pushbuffer 三种格式：
- `0 = UsrNative`（默认，已交付，stage4-3-cp-phase5-method-hyperqueue）
- `1 = AQL`（已交付，stage4-5-cp-phase6-predication-aql，commit `89e9ee1` 关联）
- `2 = PM4`（NVIDIA GF100+ format，**显式延后至 Phase 6.5 per ADR-052 D3**）

[stage4-gpu-cp-completion-gap-analysis.md §2.1](docs/architecture/stage4-gpu-cp-completion-gap-analysis.md) 显式列为剩余差距：

> PM4 microcode：AQL 解析已交付（`gpfifo_translator.cpp:37-46`，`parseAqlPacket`），PM4 仍为 stub（`FORMAT_PM4` → `translate()` 返回 `false`；`gpu_types.h:73` 注释标 stub）

当前代码（`plugins/gpu_driver/sim/gpfifo_translator.cpp`）的 `translate()` 函数对 `FORMAT_PM4` 直接返回 false，gpfifo entry 被丢弃。

**触发条件**: TaskRunner CUDA 路径需要 PM4 解析（NVIDIA pushbuffer 格式），目前仅 ROCm/HIP AQL 路径可用。

## 范围

- **In Scope**:
  - 实现 PM4 method format 解析（32-bit word packing）
  - `method_addr` + `subchannel` + `NI/INC` + variable `data_count` 解码
  - `plugins/gpu_driver/sim/gpfifo_translator.cpp::translate()` 中 `FORMAT_PM4` 分支
  - subchannel 路由（最多 8 个 subchannel，每个独立寄存器空间）
  - NI（non-incrementing）与 INC（incrementing）模式区分
  - 配套 Catch2 测试 `test_pm4_parse_standalone.cpp`
- **Out Scope**:
  - TaskRunner CUDA 路径集成（独立 change）
  - PM4 opcode 类型扩展（只支持 base method write）
  - H/W 寄存器语义（仅解析 + 写入 HAL 抽象层）

## 关键场景

- GIVEN `format = FORMAT_PM4` 的 gpfifo entry
  - WHEN `translate()` 解析
  - THEN 提取 method_addr + subchannel + data_words，按 NI/INC 决定写入策略
- GIVEN PM4 packet header 含 INC flag
  - WHEN 解析连续 method
  - THEN method_addr 自动递增，无需每个 method 携带地址
- GIVEN subchannel=2 的 PM4 packet
  - WHEN 路由
  - THEN 写入 `hal_registers[subchannel=2][method_addr]`，不影响 subchannel=0/1
- GIVEN 测试套件执行 WHEN 解析完成 THEN ctest 全部 PASS，新增 PM4 测试覆盖 NI/INC/subchannel 三类场景

## 技术约束

- MUST 遵循 ADR-052 §D3 PM4 格式规范
- MUST 保持 `struct gpu_hal_ops` 签名不变（HAL 接口 append-only per ADR-023 §D4）
- MUST NOT 修改 AQL 解析路径
- SHOULD 复用现有 `parseAqlPacket` 的 helper 函数（如有）
- SHOULD 输出 per-PM4-packet 的 debug 日志（`Logger::debug` level）

## 验收标准

- `gpfifo_translator.cpp::translate()` 中 `FORMAT_PM4` 分支不再返回 false
- 新增 `parsePm4Packet()` 函数处理 32-bit word packing
- 新增 `test_pm4_parse_standalone.cpp`，至少 8 个 test case 覆盖：
  - 基础 method write (subchannel 0/1/2)
  - NI vs INC 模式
  - 跨 packet 的 method_addr 连续性
  - 边界：单 packet 0 data_words（仅 header）
- `make -j4` 编译通过，无 warning
- `ctest --output-on-failure` 全部 PASS
- 修改的代码行通过 `lsp_diagnostics` 检查
