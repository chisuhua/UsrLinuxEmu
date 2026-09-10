# Proposal: ue-stage-1-4-2-1-extensions — UE 侧电源 + P2P 集成测试

> **状态**: 🔄 Proposed v1.0（2026-09-10）
> **工期**: 0.5-1 周
> **优先级**: P1（跟随 CppTLM stage-1-4-2-1 ship）
> **前置依赖**: CppTLM `2026-09-10-cpptlm-stage-1-4-2-1` ship

## Why

UE 侧电源管理 + P2P + Resizable BAR 跨仓集成测试。验证 CppTLM 1.4+2.1 实施后，从 UsrLinuxEmu 进程端到端调用电源状态切换 + P2P DMA 路由 + BAR resize。

## What Changes

实施 2 个跨仓集成测试：
1. `tests/integration/test_dgpu_power_mgmt_ue.cc`（1.4 电源）
2. `tests/integration/test_dgpu_p2p_ue.cc`（2.1 P2P + Resizable BAR）

## Oracle 复审

1 次轻量复审；验收标准：见 specs/ue-stage-1-4-2-1-extensions/spec.md

## refs

- 上游 `cpptlm-stage-1-4-2-1`
- 父 change `2026-09-09-5-5-8-cpptlm-kernel-dispatch-dma`（5.5.8 阶段 3 已 ship 后 5.5.9 启动前置）
