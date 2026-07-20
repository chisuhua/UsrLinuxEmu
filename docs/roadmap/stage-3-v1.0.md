# 阶段 3: v1.0 稳定

> **状态**: 📋 规划中
> **目标**: v1.0 stable release — production-ready quality
> **前置依赖**: 阶段 1 + 阶段 2 完成

---

## 涉及层（按 3 区分）

| 层 | 工作 |
|----|------|
| ① Linux 内核环境模拟 | 性能优化、错误处理完善、测试覆盖 |
| ② 可移植驱动代码 | 错误处理、文档完善、示例驱动补充 |
| ③ 硬件模拟 | CI/CD 集成、回归测试、跨平台验证 |

---

## 子任务

### 子任务 3.1 — CI/CD 全平台验证

**目标**: CI 在所有目标平台通过

**范围**:
- Linux x86_64（Ubuntu 20.04+, Debian 11+）
- Linux aarch64（ARM 服务器）
- macOS x86_64 + Apple Silicon（仅文档/Debug 验证，真机部署需 Linux）

**③ 硬件模拟**:
- 完善 CI 脚本（`.github/workflows/`）
- 跨平台构建测试
- 性能回归测试

**验收**:
- CI 全部绿
- 性能基准稳定（无回退）

### 子任务 3.2 — 性能优化

**目标**: 关键路径性能达到可用水平

**范围**:
- ioctl 派发延迟 < 100ns
- pushbuffer 提交吞吐量 ≥ 100K ops/sec
- mmap 共享开销 < 5%

**① 内核环境模拟**:
- VFS lookup 优化（hash 索引）
- ioctl 派发优化（去除虚函数）

**② 驱动**:
- BO 分配路径优化
- Queue 提交路径优化

**验收**:
- 性能基准达标
- 性能回归测试纳入 CI

### 子任务 3.3 — 错误处理完善

**目标**: 全路径错误处理符合 Linux 风格

**范围**:
- 所有 ioctl handler 返回正确的 Linux 错误码
- 错误日志结构化
- 用户态 panic recovery（避免进程崩溃）

**① 内核环境模拟**:
- 错误传播路径审计

**② 驱动**:
- DRM 错误处理完整化

**③ 硬件模拟**:
- sim 异常路径日志化

**验收**:
- 错误注入测试覆盖关键路径
- 日志结构化（JSON 输出）

### 子任务 3.4 — 文档完善

**目标**: v1.0 文档完整可用

**范围**:
- API 参考自动生成（Doxygen）
- 用户指南（quickstart + 高级主题）
- 开发者指南（添加新设备）
- 架构文档（SSOT + ROADMAP + ADR 全套）

**验收**:
- docs-audit 36/36 PASS
- 用户从 quickstart 到第一个示例 ≤ 15 分钟

---

## 涉及 ADR

| ADR | 角色 |
|-----|------|
| [ADR-012](../00_adr/adr-012-performance-optimization.md) | 性能优化策略 |
| [ADR-013](../00_adr/adr-013-error-handling-strategy.md) | 错误处理策略 |
| [ADR-014](../00_adr/adr-014-logging-enhancement.md) | 日志系统增强 |
| [ADR-036](../00_adr/adr-036-three-way-separation.md) | 3 区分架构原则 |

---

## 验收标准

- ✅ CI 全平台绿
- ✅ 性能基准达标
- ✅ docs-audit 36/36 PASS
- ✅ 错误注入测试通过
- ✅ 用户 quickstart ≤ 15 分钟
- ✅ SSOT + ROADMAP + ADR 全部最新

---

## v1.0 发布清单

- [ ] Release notes
- [ ] Migration guide（从 v0.x 升级）
- [ ] Binary release（GitHub Releases）
- [ ] Docker image（可选）
- [ ] 公告（GitHub Discussions）

---

## 下一步

[终态蓝图](blueprint.md)

---

## 当前进展（2026-07-20）

**Stage 3 进行中**：

| 子任务 | 状态 | 说明 |
|--------|------|------|
| **3.1 CI/CD 全平台验证** | ✅ 部分完成 (2026-07-08) | ubuntu-22.04 已入 CI matrix；ASan/UBSan/TSan 三 sanitizer 已落地（`ba48c79`）；macOS/aarch64 deferred |
| **3.2 性能优化** | 📋 规划中 | hotpath 优化 C-11 已归档（BO 2.1×, ioctl 11.6×, pushbuffer 1296×）；perf baseline 文档已建立 |
| **3.3 错误处理完善** | ✅ 部分完成 | `sim_graph_launch`/`sim_mem_pool_*_async` 的 `-1` → `-ENOMEM` 已修复（`fc6f854`）；全路径审计待做 |
| **3.4 文档完善** | ✅ 部分完成 | ADR-064 内存模型分阶段策略已归档；`gpu-real-memory-path.md` 已创建；Doxygen API 参考待生成 |
| **CUDA E2E real-path** | ✅ COMPLETED | Phase A-F 全部交付：BO 真实内存 + Puller MEMCPY HAL + fence 异步 + E2E 测试；104/104 + 14/14 ctest PASS |

**已落地的稳定性 commit**（Stage 3 期间）:
- `f180737` docs(openspec): archive sim-stream-primitive-support
- `3b2eeef` docs(openspec): add F.6 follow-up — sim_graph_launch real async impl
- `13477ff` refactor(gpu): use SIM_FENCE_ID_BASE macro (no magic number)
- `fc6f854` fix(sim): return -ENOMEM instead of -1 in graph/mem_pool async paths
- `ba48c79` feat(build): add ASan/UBSan/TSan sanitizer infrastructure
- `651c53b` feat(gpu): Phase A+B — real BO memory + Puller MEMCPY HAL path
- `e16e754` feat(puller): Phase D — explicit LAUNCH_KERNEL translator call
- `660eb2c` docs(openspec): mark cuda-e2e-real-path as COMPLETED (all 6 Phases)
- `9181384` docs(adr): ADR-064 memory model staging + ADR-023 HAL boundary rules
- `edb454d` docs(memory): GPU real memory path architecture document
