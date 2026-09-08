# 驱动栈图谱实施路径图（post-Stage 5.5.2 Roadmap）

> **目的**：本文档是 [stage-5-5-2-driver-stack-flow.md](../02_architecture/stage-5-5-2-driver-stack-flow.md) 修订工作的实施计划，使该文档成为 PF 阶段（基于 CppTLM 仿真）驱动开发的**真相源**，并为未来 SR-IOV VF 演进预留路径。
>
> **状态**: Draft（待评审后转 Approved）
> **最后更新**: 2026-09-08

## 0. 概览

### 0.1 当前文档成熟度（前三轮审查结论）

| 维度 | 评分 | 主要缺口 |
|------|------|----------|
| 当前阶段 PF 架构完整性 | 7.5 / 10 | 缺模块级状态矩阵、占比口径不齐、验收链路未落图、71 fn-ptrs 无分组视图 |
| 未来 HAL 演进（SR-IOV VF） | 3 / 10 | 原则充分、机制空白（后端挂载规则、PF→VF 扩展面、hal_vfio 接入均未写） |
| 代码事实准确性 | 高 | §3 API 与 header 不符（HIGH）、ioctl 名拼写、22/23 ABI、37/41 op、§9 空行 |

### 0.2 路径图阶段总览

| 阶段 | 名称 | 性质 | 预计工期 | 产出 |
|------|------|------|---------|------|
| P0 | 阻塞性修正 | 文档纠错（必须先做） | 0.5-1 人日 | 文档 v0.1.3 |
| P1 | 阶段一 PF 真相源化 | 文档增补 | 1-2 人日 | 文档 v0.2.0 |
| P2 | SR-IOV VF 演进锚点 | 文档增补 | 0.5-1 人日 | 文档 v0.2.1 |
| P3 | 跨文档对账与同步 | 文档/ADR/头注释同步 | 1-1.5 人日 | 4 份对账差异清单 + 各文件修订 |
| P4 | 代码变更（按需） | 代码 + 测试 | 1-3 人日 | header/测试修正（可选） |

**合计**：4-8.5 人日（纯文档 3-5.5 人日）。

### 0.3 涉及的其他文档（需对齐）

- `four-quadrant-architecture.md`（L5 SR-IOV tier、Wave 3/5、§4.5 BypassMode）
- `post-refactor-architecture.md`（SSOT，L560-561 双后端、L617 ADR-092、L700 后端共存）
- `AGENTS.md`（68→71 fn-ptrs、39→41 ioctls）
- `adr-092-hal-adapter-and-bypass-binding.md`（Proposed v0.1 → 推动升 Accepted v0.2）
- `adr-091 / adr-023 / adr-055 / adr-088`

### 0.4 涉及的代码变更列表

- `plugins/gpu_driver/hal/gpu_hal.h:4,7-8`（头注释 "65+3=68" 陈旧）⚠️ 必须
- `sim_hardware/include/cpptlm/backdoor_endpoint.h`（与 ADR-092 §D3 偏差，二选一）⚠️ 按决策
- `plugins/gpu_driver/hal/hal_cpptlm.cpp`（Phase 2.2 真实实现）🔜
- `sim_hardware/src/cpptlm/backdoor_endpoint_stub.cpp`（补 -ENOSYS 行为测试）🔜 可选

## 1. 阶段划分

### P0：阻塞性修正（必须先做）

**目标**：文档与代码事实零偏差——消灭所有 HIGH 与低级错误，让文档"每一句都可以被核对"。

- 任务 P0.1：**§3 API 对齐已提交 header**
  将 §3 的 ule_dgpu_* API 列表改为 `sim_hardware/include/cpptlm/backdoor_endpoint.h` 实际 5 函数（`ule_dgpu_acquire(dev_id, *out_handle)` / `ule_dgpu_get_adapter_info(handle, *out_info)` / `ule_dgpu_read` / `ule_dgpu_write` / `ule_dgpu_release`）+ `enum class ule_dgpu_space : uint32_t { kConfig=0, kBarMmio=1, kBarVram=2, kAxiDirect=3 }`；同时把"ADR-092 §D3 规范（`ule_dgpu_get_device_count` / `_by_id` / `kConfigSpace`）与 header 偏差"写入 §11 遗留表第 6 项。
- 任务 P0.2：**§2 设计/实现态标注**
  CpptlmBridge 段 3 处（so 加载、handle 映射表、锁外 destroy）加 `(设计, Phase 2.2 实现)`；§5 加"hal_cpptlm 三 op 当前 -ENOSYS（hal_cpptlm.cpp:47-63）"状态条。
- 任务 P0.3：**数字修正**
  ① §-1.3.9 "37 个 op"→"**41 个 op**"，分布改 7/12/22；② §9 "CppTLM 23 ABI"→"**22**"，与 §1/§2/§10 统一（或脚注说明 23 的来源口径）。
- 任务 P0.4：**§2 ioctl 名拼写**
  `GPU_IOCTL_SUBMIT_BATCH (Push)` → `GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH`（`gpu_ioctl.h:40`）。
- 任务 P0.5：**§9 畸形空行清理**
  删除 L620 ` | | | |`。

**验收标准**：上一轮所有 HIGH 项清零；对文档每个代码引用做一次行号复核，偏差为零；§9 表格可正常渲染。

### P1：阶段一 PF 真相源化（当前开发对齐）

**目标**：PF 开发者仅凭本文档即可启动 Stage 5.5.2+ 开发，无需平行参考其他架构文档。

- 任务 P1.1：**新增 §-1.2.1 8 模块 × 实现状态矩阵**
  8 行 × {模块, 真机占比, 本项目实现状态（统一枚举）, HAL 覆盖入口, 代码事实}；状态初稿：PCIe ⚠️ / 固件 ❌ / 中断 ⚠️ / 显存 ⚠️ / 命令 ⚠️ / 同步 ⚠️ / 错误 ⛔ ADR-055 / 电源 ❌。
- 任务 P1.2：**新增 §-1.6 HAL 后端演进机制章节**
  三小节：挂载机制规则（`hal_<name>_init(struct gpu_hal_ops*, ctx)` 填同一 `gpu_hal_ops`）、PF→VF 扩展面 4 方向、VFIO 接入路径（不进入接口面）。
- 任务 P1.3：**§-1.5.2 验收链路落图**
  6 步真机链路 ↔ §1-§7 节点交叉引用表 + "本项目等价验收"段（仿真栈 E2E）。
- 任务 P1.4：**§-1.3.9 op 基数修正**（与 P0.3① 合并执行，此处确认口径与 7 组归类表同步落位）。
- 任务 P1.5：**占比口径对齐（采纳 8 模块口径）**
  §-1.1/§-1.2 表与 ASCII 图改 8 模块占比 15/20/15/20/25/10/10/低；更新 115% 重叠口径注释；错误处理由"附属"改"主体"。
- 任务 P1.6：**71 fn-ptrs 7 组归类视图**
  §-1.3.9 后新增分组表：7 组映射 + 专属扩展组（graph/mem_pool/stream_capture/queue/puller/KFD/green_context/PDL/adapter）。

**验收标准**：PF 阶段开发可单文档启动（自测问题：仅凭本文档能否回答"显存管理现在做到什么程度、缺什么、找哪个 fn-ptr"——能，则达标）。

### P2：SR-IOV VF 演进锚点预留（架构未来性）

**目标**：机制层成熟度 3/10 → 8+/10；未来 VF 开发有明确入口，当前开发不被超前设计干扰。

- 任务 P2.1：**新增 §13 未来 SR-IOV VF 演进锚点**
  4 扩展点（BAR 分区 / MSI-X 子集 / doorbell 重映射 / SR-IOV Capability 只读）各配"现状锚点 → 未来接口形态（append-only）"；明确本节点到锚点预留为止。
- 任务 P2.2：**§-1.6 新增 PF→VF 扩展面小节**（与 P1.2 同步，属该章节第 2 小节）。
- 任务 P2.3：**跨文档 SR-IOV 口径对齐**
  §-1.4 排除表第 1 行加注（真机 VF 驱动 ❌ vs sim 侧 SR-IOV tier four-quadrant L5/Wave3/Wave5）；§13 跨文档锚点表。
- 任务 P2.4：**ADR-023 append-only 增补 VF 组的预案**
  在 §-1.6 写"增补机制 = 追加到 `gpu_hal_ops` 末尾，零修改既有 71（以 `gpu_hal.h:382-384` 为范式）"；不定义具体 VF 接口签名（避免超前设计）。

**验收标准**：文档明确支持"未来 VF 模式"扩展路径；对"当前 dGPU 开发是否需要 VF 接口"答案为"否，仅锚点"；无架构阻塞点。

### P3：跨文档对账与同步

**目标**：所有引用本文档或与其冲突的文件完成同步，消除指针漂移。

- 任务 P3.1：**与 post-refactor-architecture.md 对齐**
  比对 L560-561（双后端）、L617（ADR-092 增补）、L700（后端共存）与本文档 §12 新章节；差异最小化（post-refactor 为 SSOT，本文档补充细节，不重复主文）。
- 任务 P3.2：**与 four-quadrant-architecture.md 对齐**
  ① §6 BypassMode 与四象限 §4.5 逐字核对（已在两轮中确认一致）；② §-1.4/§13 的 SR-IOV 口径互链（P2.3）。
- 任务 P3.3：**与 AGENTS.md 同步**
  AGENTS.md 中 "68 函数指针"→"71"、"39 个 IOCTL"→"41 个命令"；引用 CODE MAP 表同步。
- 任务 P3.4：**与 gpu_hal.h 头注释同步**
  `gpu_hal.h:4` "65 + 3 = 68" → "68 + 3 = 71（ADR-090 后 68，ADR-092 追加 3）"；L7-8 的 "现存 65" 段同步。
- 任务 P3.5：**推动 ADR-092 Proposed v0.1 → Accepted v0.2**
  依据 §10 Gate D 4/4 evidence + 本文档修订完成后复审；ADR-092 frontmatter 状态、docs/README ADR 统计、ADR-092 中"23 ABI"口径三处联动更新。

**验收标准**：`grep` 复核——所有引用本文档的文件中 "68"/"39 ioctls"/"23 ABI" 不再出现（或被脚注显式说明）；ADR-092 状态在 3 处一致。

### P4：代码变更路径（如适用）

**仅当文档修订暴露出需要代码变更时触发**。两种可选路线（二选一，决策见 §3 风险）：

- 任务 P4.1：**路线 A（推荐）：改 ADR 不改代码**
  Oracle 复审建议：BackdoorEndpoint header 为已 ship 事实，ADR-092 §D3 的 `get_device_count` / `_by_id` / `kConfigSpace` 为笔误源头；因此修改 ADR 不修改代码（成本最低、无跨仓风险）。
- 任务 P4.2：**补 backdoor_endpoint weak stub 测试**
  无论选 A/B，为 `backdoor_endpoint_stub.cpp` 的 5 个 -ENOSYS weak 符号补 Catch2 行为测试（返回 -ENOSYS、参数校验），锁定契约。
- 任务 P4.3：**gpu_hal.h 头注释修正**（若 P3.4 未随文档修订一起改则此处执行）。

**验收标准**：所有"文档声称 ✅ 对应代码 commit X"100% 准确；新增/修改代码过 ASan 测试（参照 AGENTS.md sanitizer 流程）；测试计数更新。

## 2. 任务依赖关系图

```mermaid
flowchart LR
    subgraph P0[P0 阻塞性修正]
        P0_1[P0.1 §3 API 对齐 header]
        P0_2[P0.2 §2 设计/实现态标注]
        P0_3[P0.3 数字修正 23→22 / 37→41]
        P0_4[P0.4 ioctl 名拼写]
        P0_5[P0.5 §9 空行清理]
        P0_5 -.独立.-> P0_1
        P0_1 --> P0_2
        P0_3 -.独立.-> P0_4
    end

    subgraph P1[P1 PF 真相源化]
        P1_1[P1.1 §-1.2.1 模块状态矩阵]
        P1_2[P1.2 §-1.6 HAL 演进机制]
        P1_3[P1.3 验收链路落图]
        P1_4[P1.4 op 基数确认]
        P1_5[P1.5 占比口径 8 模块]
        P1_6[P1.6 71 fn-ptrs 分组视图]
        P1_1 --> P1_5
        P1_4 --> P1_6
        P1_2 -.与 P1.1 并行.-> P1_3
    end

    subgraph P2[P2 SR-IOV 锚点]
        P2_1[P2.1 §13 演进锚点]
        P2_2[P2.2 §-1.6 扩展面小节]
        P2_3[P2.3 跨文档口径对齐]
        P2_4[P2.4 append-only 预案]
        P2_1 --> P2_3
        P2_2 --> P2_4
    end

    subgraph P3[P3 跨文档对账]
        P3_1[P3.1 post-refactor 对齐]
        P3_2[P3.2 four-quadrant 对齐]
        P3_3[P3.3 AGENTS.md 同步]
        P3_4[P3.4 gpu_hal.h 头注释]
        P3_5[P3.5 ADR-092 升 Accepted]
    end

    subgraph P4[P4 代码变更]
        P4_1[P4.1 ADR-092 §D3 笔误修正]
        P4_2[P4.2 weak stub 测试]
        P4_3[P4.3 头注释修正]
    end

    P0_1 --> P1_1
    P0_2 --> P1_3
    P0_3 --> P1_4
    P1_2 --> P2_2
    P1_6 --> P2_1
    P2_3 --> P3_2
    P1_2 --> P3_5
    P3_4 --> P4_3
    P2_1 -.GATE: Oracle 决策.-> P4_1
    P4_1 --> P4_2
    P3_1 & P3_2 & P3_3 & P3_4 & P3_5 --> FINAL[验收: grep 复核 + 成熟度复评]
```

**依赖要点**：
- P0.1/P0.2 是 P1 前置（错误基线先清）；
- P1.2 同时喂给 P2.2 与 P3.5（HAL 机制章节是 ADR-092 复审输入）；
- P2.1 完成前 P4.1 有一个 **Oracle Gate 决策点**（已建议：改 ADR 不改 header）；
- P1.2/P1.6/P2.1 与 P3.x 无强依赖，可与 P0 并行起草。

## 3. 资源与人力估算

### 3.1 每阶段人日估算

| 阶段 | 人日 | 依赖技能 | 主要风险 |
|------|------|---------|---------|
| P0 | 0.5-1 | 文档 + 代码对照 | 低 |
| P1 | 1-2 | 架构分析 + HAL 领域知识 | 中（占比口径争议） |
| P2 | 0.5-1 | SR-IOV/VFIO 领域知识 | 中（超前设计诱惑） |
| P3 | 1-1.5 | 跨文档一致性 + ADR 治理 | 中（ADR-092 升档需 Oracle 复审） |
| P4 | 1-3 | C++ / Catch2 / CMake | 中（改 ADR 笔误 + 补测试） |

### 3.2 每阶段产出物清单

- P0：`stage-5-5-2-driver-stack-flow.md` v0.1.3（纯纠错 diff）
- P1：本文档 v0.2.0（+§-1.2.1、§-1.6、§12；§-1.2/§-1.3.9 修订）
- P2：本文档 v0.2.1（+§13；§-1.4 加注）
- P3：四象限/post-refactor/AGENTS.md/gpu_hal.h/ADR-092 各一份修订 diff + 对账差异清单
- P4：ADR-092 §D3 笔误修正 + stub 测试 + hal_cpptlm.cpp 更新 + gpu_hal.h 注释修正

### 3.3 风险点与回退方案

| 风险 | 等级 | 缓解 / 回退 |
|------|------|------------|
| §3 偏差的"哪边是对的"争议（ADR-092 §D3 vs header） | 中 | Oracle 已建议：改 ADR 不改 header（成本最低、无跨仓风险）；P4.1 任务已据此设计 |
| 占比口径（8 模块 115% vs 六主体 105%）与既有 ADR-088 表述冲突 | 中 | §-1.2 保留旧口径注释作为脚注，主表采用 8 模块口径；若 ADR-088 有正式占比，以其为准并反向修订本文 |
| "真相源"与 post-refactor-architecture.md（SSOT）定位冲突 | 中 | §0 明确分工：post-refactor = 全局 SSOT；本文 = PF 阶段（Stage 5.5.2+）驱动栈专项真相源；冲突时 post-refactor 优先，本文负责细节与演进锚点 |
| P4 ADR 笔误修订波及 ADR-092 治理流程 | 中 | ADR-092 升 Accepted v0.2 时一并合并笔误修订；如不可行则回退为"仅文档标注偏差" |
| 修订幅度过大拖长评审 | 低 | 分 4 个 PR（P0/P1/P2+P3 文档/P4），每个独立可合并 |

## 4. 验收清单

### 4.1 文档质量验收

- [ ] 代码事实准确性：对文档每个 `文件:行号` 引用逐一复核，**命中率 100%**（抽查 20 个以上）。
- [ ] HIGH 项清零：第 1-3 轮 HIGH 项（HIGH-1 §3 API、HIGH-2 §2 混写、HIGH-3 模块矩阵、HIGH-4 演进机制）全部关闭。
- [ ] 成熟度复评：PF 架构完整性 ≥ 9/10；HAL 演进机制 ≥ 8/10。
- [ ] §9 表格正常渲染（无畸形行）；全文状态枚举只用 5 种统一标记。

### 4.2 跨文档一致性验收

- [ ] `grep -rn "68 fn\|68 函数指\|39 个 IOCTL\|23 ABI"` 在 AGENTS.md/docs/ 命中数为 0（或全为显式脚注）。
- [ ] BypassMode 枚举在本文档 §6、four-quadrant §4.5、bypass.h:8-12、ADR-091 §D4 四处逐字一致。
- [ ] ADR-092 状态在 ADR frontmatter、docs/README、本文档 §10/§11 三处一致（同一日期）。

### 4.3 代码事实准确性验收

- [ ] 行号引用抽查 20 处 == 实际行号（gpu_hal.h 段标注逐行核对）。
- [ ] "对应代码 commit" 声明可追溯：`6d2ea90`（UsrLinuxEmu）、`bab64dd5`（CppTLM）存在且内容匹配（外部仓经符号对拍，§10③ 证据链完整）。
- [ ] 文档中每个 ✅ 状态有代码依据（fn-ptr 存在 + 非 -ENOSYS）；每个 ⚠️/❌/⛔ 有明确理由（mock/缺失/ADR 排除+编号）。

### 4.4 PF 开发可用性验收

- [ ] 最小验证路径（§-1.5.2 修订后）能逐节点映射到 §2/§5/§7 图，新人按图走通"提交→WAIT_FENCE"链路。
- [ ] 仅凭本文档可回答：8 模块各做到什么程度、缺什么、找哪个 HAL fn-ptr、何时用哪个后端（§12）。
- [ ] §13 锚点表可在 10 分钟内定位未来 VF 任一扩展点的入口。

## 5. 变更追踪

| 版本 | 日期 | 变更摘要 | 关联 |
|------|------|---------|------|
| v0.1.3 | 2026-09-08 | P0：§3 API 对齐 header、§2 态标注、数字修正（41 op / 22 ABI）、ioctl 名、§9 空行；§11 增补 API 偏差遗留 | P0 |
| v0.2.0 | 2026-09-08 | P1：新增 §-1.2.1 模块矩阵、§-1.6 后端演进机制、§12 后端契约；§-1.2 占比 8 模块化、§-1.3.9 分组视图；验收链路落图 | P1 |
| v0.2.1 | 2026-09-08 | P2：新增 §13 SR-IOV VF 锚点；§-1.4 口径加注；append-only 预案 | P2 |
| v0.3.0 | 待 P3 | 跨文档对账 diff 合入（post-refactor/四象限/AGENTS.md/gpu_hal.h/ADR-092 升档） | P3 |
| v1.0.0 | 待 P4 | ADR-092 §D3 笔误修正 + stub 测试 + 头注释修正 | P4 |

**重大决策记录**（本节为决策锚点，正式决议走 ADR 流程）：

- **决策候选 1（P4 Gate）**：BackdoorEndpoint API 以哪边为准——ADR-092 §D3 规范 or 已提交 header？→ **Oracle 建议**：以已提交 header 为准、ADR-092 §D3 为笔误源头（成本最低、无跨仓风险）。
- **决策候选 2（P3）**：ADR-092 升 Accepted v0.2 的 Gate D 复审输入 = 本文档 v0.2.x + §10 evidence 复核。
- **决策候选 3（P1）**：占比口径采纳 8 模块（115% 重叠注释）；若与 ADR-088 冲突，以 ADR 为准反向修订。

## 6. 引用代码事实索引（核心摘录）

### HAL（plugins/gpu_driver/hal/）

| 引用 | 内容 | 用途 |
|------|------|------|
| `gpu_hal.h:4` | 头注释 "65 + 3 = 68"（**陈旧**） | P3.4 / P4.3 修正 |
| `gpu_hal.h:30` | `struct gpu_hal_ops {` | 后端挂载点规则 |
| `gpu_hal.h:382-384` | adapter #69/70/71（append-only 范式） | §-1.6.2 引用 |
| `hal_user.cpp:303` | `hal_user_init` | 后端实例 1 |
| `hal_mock.cpp:322` | `hal_mock_init` | 后端实例 2 |
| `hal_cpptlm.cpp:67` | `hal_cpptlm_init` | 后端实例 3 |
| `hal_cpptlm.cpp:47-63` | 3 op -ENOSYS stub | §5/§9 状态依据 |

### drv（plugins/gpu_driver/）

| 引用 | 内容 | 用途 |
|------|------|------|
| `gpu_ioctl.h:40` | `GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH` | §2 ioctl 修正 |
| `gpgpu_device.cpp:95-96` | PUSHBUFFER_SUBMIT_BATCH 派发 | §2 对照 |
| `gpgpu_device.cpp:446-488` | WAIT_FENCE 轮询 | 验收链路 E2E |

### sim_hardware/

| 引用 | 内容 | 用途 |
|------|------|------|
| `bypass.h:8-12` | BypassMode {kFull=0, kBypass=1, kPartial=2} | §6 表核对 |
| `bridge.cpp:12-24` | Impl 字段（**无 handle map**） | §2 态标注 |
| `bridge.cpp:35-48` | valid_mmio（bar<6 校验） | §13 BAR 分区锚点 |
| `backdoor_endpoint.h` | 5 函数 + `kConfig=0` 枚举 | §3 对齐对象 |

### ADR

| ADR | 内容 | 用途 |
|------|------|------|
| ADR-092 §D1-D5 | adapter / BypassMode / BackdoorEndpoint / VFIO / 跨仓锁 | 多处对照 |
| ADR-091 §D3.2 | SR-IOV Capability 只读 | §-1.1/§13 |
| ADR-023 §D4 | HAL append-only | 原则 3 |
| ADR-055 | 错误处理 Deferred-Never | ⛔ 状态依据 |
| ADR-088 | dGPU 完整仿真（23 ABI 口径来源） | §9 脚注 |
