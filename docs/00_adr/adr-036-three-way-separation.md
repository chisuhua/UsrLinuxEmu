# ADR-036: 3-Way Architectural Separation (3 区分架构原则)

**状态**: ✅ Accepted
**日期**: 2026-06-23
**提案人**: UsrLinuxEmu Architecture Team
**评审者**: UsrLinuxEmu Architecture Team + TaskRunner owner
**修订记录**:
- 2026-06-23 v1: 初始版本（🔄 Proposed）
- 2026-06-23 v1: 状态升 ✅ Accepted（迁移步骤全部完成：SSOT §1.10、ADR-018/023 交叉引用、README INDEX 同步、README 架构图）
**关联 ADR**: ADR-001, ADR-006, ADR-008, ADR-018, ADR-023, ADR-035
**关联 Change**: 无（governance cleanup 阶段同步推进）

---

## Context

UsrLinuxEmu 的核心目标（见 [ADR-001](adr-001-user-mode-emulation.md)）是让驱动开发者在无需 root 权限或内核编译的情况下开发与测试 GPGPU 驱动，最终目标是让验证过的驱动无痛迁移到真实 Linux 内核。这一目标被 ROADMAP Stage 1 显式列为验收指标：驱动代码必须可移植。

当前架构（[ADR-018](adr-018-driver-sim-separation.md) + [ADR-023](adr-023-hal-interface.md)）已经规定 drv/hal/sim/shared 四目录物理分离，但仍有歧义：

1. **shared 是 ABI 契约**（既不属于驱动也不属于仿真），剩下 drv/hal/sim 三块容易让人误以为只是"3 个目录"，而非"3 个职责层"。
2. **HAL 容易被误读为第 4 层**：当前 README 的"四层架构"图把 HAL 单独画成一层，但 HAL 的本质是 `drv/` 调 `sim/` 的反向依赖桥（见 [ADR-023](adr-023-hal-interface.md) §背景 line 20 + §决策 2 line 71-79 + line 80-82）。
3. **后续 ROADMAP 阶段**（Stage 1/2/3）需要一个统一的判定原则，让"驱动是否真的可移植"成为可度量、可 review 的属性，而不是各人各自的解读。

本 ADR 显式确立**3 区分原则**：kernel 环境模拟 / 可移植驱动实现 / 硬件仿真，并显式声明 HAL 是桥（bridge）而非第 4 层。该原则是 ROADMAP 各 Stage 描述、跨仓架构变更、PR review 的共同判定基准。

---

## Decision

### 核心原则：3 区分 + 1 桥

代码按职责分为 3 个层次，HAL 作为层次之间的桥接机制：

| 编号 | 层次 | 物理路径 | 职责 | 与 HAL 的关系 |
|------|------|---------|------|--------------|
| ① | **Linux 内核环境模拟** | `src/kernel/`, `include/kernel/`, `include/linux_compat/` | 在用户态提供 Linux 内核 API：VFS、ModuleLoader、Device 类、`linux_compat/` 类型/宏/DRM 子集 | 通过 `dlopen("plugins/*.so")` 加载驱动的"宿主"；为驱动提供系统调用兼容层 |
| ② | **可移植的驱动代码实现** | `plugins/gpu_driver/drv/` | GpgpuDevice（ioctl 派发表）、BO/VA Space/Queue/Fence 管理；遵循 linux kernel idioms | 通过 HAL 接口调用 ③ 的能力，不直接调 ③ |
| ③ | **硬件仿真** | `plugins/gpu_driver/sim/`（含 `libgpu_core/`） | BuddyAllocator、HardwarePullerEmu 状态机、GlobalScheduler、`gpu_queue_emu`、纯 C buddy 算法 | 被 HAL 通过 ctx 指针反向调用；通过 ctx 暴露状态 |
| HAL | **桥（bridge）** | `plugins/gpu_driver/hal/`（`gpu_hal.h`, `hal_user.cpp`, `hal_mock.cpp`） | 11 个函数指针表（[ADR-023](adr-023-hal-interface.md) 决策 1）+ `static inline` 包装 + 构造注入 | ② 调 HAL，HAL 调 ③。HAL 不是独立层，是 ② 与 ③ 之间的依赖反向桥 |
| shared | **ABI 契约** | `plugins/gpu_driver/shared/`（`gpu_ioctl.h`, `gpu_types.h`, `gpu_queue.h`） | TaskRunner 与 UsrLinuxEmu 共享头文件 | 既不属于 ② 也不属于 ③，仅作为可移植驱动与外部 consumer 之间的契约 |

### 关键判定：HAL 是桥，不是第 4 层

把 HAL 视为"桥"而非"独立层"，有 3 个理由：

1. **依赖方向单一**：仅 `② → HAL → ③`。HAL 自身不持有独立职责，只是 ② 调 ③ 的中间站。如果 HAL 是层，则应同时被 ① 或其他上层调用，但实际并不存在这种调用。
2. **接口形式与 Linux 内核 `struct xxx_ops` 一致**（[ADR-023](adr-023-hal-interface.md) 决策 2）：是"反向依赖 + 抽象"机制，符合 `struct file_operations`、`struct pci_driver_ops`、`struct drm_driver` 等 Linux 内核惯例，不构成新层。
3. **可替换性**：HAL 的 `hal_user.cpp`（用户态实现）可被 `hal_mock.cpp`（单元测试 mock）替换；如果 HAL 是层，则不会有可替换的实现变体。

### 依赖规则（与 [ADR-018](adr-018-driver-sim-separation.md) 决策 2 + [ADR-023](adr-023-hal-interface.md) 决策 2 一致）

```
① (kernel env sim) ── dlopen ──> ② (drv/) ── 调用 ──> HAL (ops 表) ── 反向调用 ──> ③ (sim/)
                                                              │
                                                          ctx 指针
                                                              │
                                                              ▼
                                                       ③ 内部状态
```

强制约束：

- ② **不直接调** ③（所有硬件访问必须经 HAL）
- ③ **不依赖** ②（可独立编译与测试）
- ① **不直接包含** ②/③ 的头
- shared 不属于任何一边，只是 ABI 契约

---

## Consequences

### 正面后果

- **可移植性可度量**：ROADMAP Stage 1 把"驱动代码仅经 HAL 调硬件"作为可移植验收基线，违反此规则的代码在 PR review 时直接被打回，标准客观。
- **职责清晰**：新人 onboarding 时，通过目录结构就能立刻看出"哪些代码进内核（②），哪些仅仿真（③），哪些是桥（HAL），哪些是契约（shared）"。
- **治理锚点**：所有跨仓架构变更（H-2.5、H-3）必须自检是否符合 3 区分原则，见 [ADR-032](adr-032-h2-5-igpu-driver-abstraction.md) 与 [ADR-033](adr-033-h3-phase2-lifecycle.md)。
- **文档统一**：原 README "四层架构"表述统一改为"3 区分 + HAL 桥"，消除 HAL 误读。
- **PR review 标准化**：code review checklist 加上"是否经 HAL 调用"这一项，让违规无处藏身。

### 负面后果

- **Code review 成本上升**：每个新 ioctl / 新 PR 都需检查 `drv/` 是否经 HAL 调用 `sim/`，违反即拒绝。短期 review 时间会增加。
- **HAL 接口扩展需谨慎**：新增 HAL 函数会影响 ABI（见 [ADR-023](adr-023-hal-interface.md) 决策 2 line 73-78 的注释），不能随意增减，需要走 ADR 流程。
- **跨层耦合禁止**：① 不能直接包含 ②/③ 的头；② 不能 `#include` ③ 的 cpp；③ 不能 import ② 的任何符号。这些约束需要 build 系统（CMake `target_link_libraries`）配合显式声明。
- **shared 边界模糊风险**：TaskRunner 与 UsrLinuxEmu 共用头文件，如果任一方改动结构体字段，另一方必须同步（已有符号链接机制，但仍需双方维护人同步）。

### 风险与缓解

| 风险 | 缓解措施 |
|------|---------|
| HAL 接口数不够驱动演化 | 预留 Phase 2 扩展点（见 [ADR-023](adr-023-hal-interface.md) 实施步骤 §风险表），新增走 ADR 流程 |
| 驱动代码无意调 `sim/`（绕过 HAL） | Code review checklist + `tools/docs-audit.sh` 静态扫描（`#include` 来源检查） |
| HAL 误用为第 4 层 | 在 PR template 加 "HAL 是桥，非层" 自检项；README 架构图同步更新 |
| shared 头文件双方不同步 | TaskRunner 与 UsrLinuxEmu 维护人在每次 shared 改动后需双向 ack（已有符号链接，diff 即触发） |

---

## Migration

### 文档同步（本 ADR 被接受后由后续 change 执行）

1. **SSOT 更新**：`docs/02_architecture/post-refactor-architecture.md` 新增 §1.10 章节，标题"3 区分架构原则"，把原"四层架构"图统一改为"3 区分 + HAL 桥"。本任务范围内不修改 SSOT，由后续 change 落地。
2. **ADR 交叉引用更新**：
   - [ADR-018](adr-018-driver-sim-separation.md) line 11「关联 ADR」追加 `, ADR-036 (3-way Architectural Separation)`
   - [ADR-023](adr-023-hal-interface.md) line 11「关联 ADR」追加 `, ADR-036 (3-way Architectural Separation: HAL as bridge)`
   - [ADR-018](adr-018-driver-sim-separation.md) 决策 2 段补充"与 ADR-036 的 3 区分模型一致：drv 是 ②，sim 是 ③，HAL 是桥"
   - [ADR-023](adr-023-hal-interface.md) 决策 2 段补充"HAL 是 `drv/` 与 `sim/` 之间的桥接机制，不是独立第 4 层，详见 ADR-036"
3. **README 索引更新**：`docs/00_adr/README.md` 索引表追加 ADR-036 行；状态分布表更新为 28 Accepted + 6 Deferred + 2 Proposed = 36 total；ADR 关系图新增 ADR-036 节点。
4. **顶层 README 架构图**：根目录 `README.md` §"架构概览"的"四层架构"图改为"3 区分 + HAL 桥"，与 SSOT 保持一致。

### 后续 ROADMAP 对齐（原则级）

- 所有 ROADMAP 文件（`docs/ROADMAP*`）的 Stage 描述应显式标注"是否符合 3 区分原则"作为架构判定项。
- `ROADMAP/blueprint.md` 描述的"成熟态"以本原则为评判基准：成熟态意味着 ② 内代码仅依赖 HAL + shared + ①，零直接依赖 ③。

### 不绑定具体 OpenSpec change

本 ADR 是元决策（meta-decision），描述跨所有 change 生效的架构原则；不绑定任何具体 OpenSpec change 编号或 ROADMAP 工作项。具体 change 在引用时按需 link 即可。

---

**维护者**: UsrLinuxEmu Architecture Team
**最后更新**: 2026-06-23