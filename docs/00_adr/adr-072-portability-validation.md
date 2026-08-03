# ADR-072: 驱动代码可移植性验证框架

**状态**: ✅ 已采纳 (Accepted)

**日期**: 2026-07-25

**提案人**: Sisyphus（基于 Oracle ADR 议题审查 — ADR-036 执行层闭环缺失）

**评审者**: 待定

**关联 ADR**: ADR-036（3 区分架构原则）§依赖规则、ADR-023（HAL 接口契约）§Decision 5 边界禁止规则、ADR-043（CP 可移植性边界）§D5 已知技术债清单

**关联 Change**: 无直接绑定 change；验证框架本身是跨 changes 的架构基础设施

---

## 背景

### 问题：有规则，无执行

ADR-036 §依赖规则明确定义了 3 区分边界：

```
② drv/ ── #include ──> sim/  ← 禁止
② drv/ ── #include ──> hal/hal_user.h, hal/hal_mock.h  ← 禁止
② drv/ ── 访问 ──> ③ 的内部类型/符号  ← 禁止
```

**当前违规现状**（`grep -rn '#include.*"sim/\|#include.*"hal/hal_user' plugins/gpu_driver/drv/`）：

```
plugins/gpu_driver/drv/gpgpu_device.cpp:15:#include "sim/graph.h"
plugins/gpu_driver/drv/gpgpu_device.cpp:16:#include "sim/hardware/hardware_puller_emu.h"
plugins/gpu_driver/drv/gpgpu_device.cpp:17:#include "sim/hardware/method_codec.h"
plugins/gpu_driver/drv/gpgpu_device.cpp:18:#include "sim/fence_id.h"
plugins/gpu_driver/drv/gpgpu_device.cpp:19:#include "sim/gpu_queue_emu.h"
plugins/gpu_driver/drv/gpgpu_device.cpp:20:#include "sim/mem_pool.h"
plugins/gpu_driver/drv/gpgpu_device.cpp:21:#include "sim/stream_capture.h"
plugins/gpu_driver/drv/gpu_drm_driver.cpp:26:#include "sim/fence_id.h"
plugins/gpu_driver/drv/gpu_drm_driver.cpp:27:#include "sim/stream_capture.h"
plugins/gpu_driver/drv/gpu_drm_driver.cpp:28:#include "sim/graph.h"
plugins/gpu_driver/drv/gpu_drm_driver.cpp:29:#include "sim/mem_pool.h"
plugins/gpu_driver/drv/gpgpu_device.cpp:26:#include "hal/hal_user.h"
```

**12 处已知违规**（2026-08-03 复审：在 ADR-043 §D5 原始 11 处基础上新增 `gpgpu_device.cpp:17` 的 `sim/hardware/method_codec.h`）。这些代码如果拷贝到 `drivers/gpu/xxx/`（真实 Linux 内核树），会编译失败——`sim/` 和 `hal/hal_user.h` 不存在于真实内核中。这违反了项目的核心目标："逻辑零修改可移植"。

### 现状：无检测，无阻断

- `docs-audit.sh` §5.3 仅检查 `CMakeLists.txt` 中的 `include_directories(simulator)` 路径——它是一个过时的检查（`simulator/` 目录已清空），不检查源代码级别的 include 违规
- 没有 CI gate 阻止新的违规引入
- ADR-043 §D5 记录了已知技术债，但无到期日或触发条件
- 新开发者可能不知道这些规则，无意中引入违规

### 为什么需要架构决策

可移植性验证不是工具配置问题——它是架构问题：

1. **验证机制决定代码结构**：如果自动检测阻断违规，团队会被迫在 merge 前重构（推动架构合规）
2. **已知债务的分类策略**：哪些违规是"合理暂时容忍"（如 ADR-043 D5 的技术债），哪些是"必须立即修复"——需要架构层面的判定
3. **验证层级设计**：静态分析（快速）+ 编译测试（准确）+ 文档审计（可追溯）——三层验证的职责划分是架构设计

---

## 决策

### Decision 1: 三层验证架构

| 层级 | 机制 | 速度 | 精度 | 阻断级别 | 实现 |
|------|------|------|------|---------|------|
| **L1: 静态分析** | `#include` 白名单检查（grep/脚本） | 毫秒级 | 高（可漏报） | **Error**（阻断 merge） | `tools/check-portability.sh` |
| **L2: 编译测试** | Docker 容器内 Linux 6.12 LTS 内核头文件编译 | 分钟级 | 最高（零漏报） | **Error**（阻断 merge） | `tools/compile-check-kernel.sh` |
| **L3: 文档审计** | `docs-audit.sh` 新增 §5.4 | 秒级 | 中（依赖 L1 结果） | **Warning**（不阻断） | 追加到现有 `docs-audit.sh` |

**设计理由**：

- L1 作为 pre-commit hook（毫秒级，零摩擦）：开发者在 `git commit` 时立即知道违规
- L2 作为 CI gate（准确但慢）：PR merge 前的终极验证
- L3 作为追溯审计：确保 L1/L2 本身在运行（自检）

### Decision 2: 违规分类与处理策略

并非所有 `#include "sim/"` 都是等价的。需要分类处理：

| 违规类别 | 检测方式 | 示例 | 处理 |
|---------|---------|------|------|
| **A: 类型/常量引用** | 仅使用 typedef/常量 | `gpu_queue_handle_t`, `GPU_MAX_QUEUES` | 迁移到 `shared/` 公共头（低风险，高收益） |
| **B: 接口调用 + 字段访问** | `#include "sim/gpu_queue_emu.h"` — 调用类方法或访问 struct 字段 | `q->submit()`, `q->get_ring_buffer()`, `hc->heap` | 通过 HAL 函数指针重新暴露（中等风险） |
| **C: 直接内存/结构体访问** | `#include "sim/mem_pool.h"` — 访问内部数据结构 | `pool->alloc()`, `pool->base_va` | 通过 HAL + `mem_map_bo` 抽象（高风险，需架构变更） |

> **2026-08-03 修订**（A/B/C 实际分布审计）：
> - **A-class 实测 0 个**：ADR-043 §D5 列表中所有 12 个违规均涉及函数调用或字段访问
> - ADR-043 §D5 历史示例 `sim/fence_id.h`（A-class）实为 B-class — drv/ 调用 `sim_fence_id_alloc()`
> - 原 `hal/hal_user.h`（A-class 候选）实为 B-class — drv/ 访问 `hc->heap` 字段
> - 修订后 **B-class = 12 个**，**A-class = 0 个**，**C-class = 0 个**
> - B-class 修复路径：扩展 `struct gpu_hal_ops`（per ADR-023 Decision 4 spec-driven "追加不改" 原则），drv/ 通过 fn-ptr 间接调用 sim 符号
> - 修复策略变更：1 个 foundation change（HAL fn-ptr 扩展 pattern） + N 个 removal change（每移除一个 sim include = 1 个）

**分类执行规则**：

- 新代码中的 A/B/C 类违规 → **全部阻断**（L1 Error）
- 已有代码中的 A 类 → 文件迁移到 `shared/`，优先级 P2（不阻塞 Stage 4）
- 已有代码中的 B 类 → 走 HAL 扩展流程（per ADR-023 D4），创建独立 change
- 已有代码中的 C 类 → 走完整架构变更（per ADR-064/069），创建独立 change

### Decision 3: 白名单机制——允许的跨边界引用

不是所有 `drv/` 引用 `sim/` 都是违规。以下情况是**显式允许**的：

| 允许的引用 | 条件 | 理由 |
|-----------|------|------|
| `drv/ → shared/` | 无限制 | `shared/` 是 ABI 契约层（per ADR-036），属于公共接口 |
| `drv/ → ① compat/` | 无限制 | Linux 内核 API（`linux_compat/`）是可移植的基础设施 |
| `drv/ → sim/proxy/` | 仅 `sim_proxy.h`（①→③ forward-prototype，per ADR-063） | ① 向 ③ 暴露的 API 声明，不是 ② 直接依赖 ③ |
| `drv/ → hal/*.h` | 仅 `gpu_hal.h`（公开接口头） | HAL 公开函数指针表（per ADR-023） |

**不在白名单上的一切 `drv/ → sim/` 引用 = 违规**。

### Decision 4: 已知技术债处置——不追溯，不新增

**当前 12 处违规**按以下策略处理：

```
  11 sim/ includes + 1 hal_user.h include = 12 known violations (2026-08-03 复审)
  ├── A 类（类型/常量）: 0 (实测 — 见 Decision 2 修订注)
  │     → 暂无需创建 change
  ├── B 类（接口调用 + 字段访问）: 所有 12 个
  │     ├─ 简单: fence_id.h, stream_capture.h, method_codec.h
  │     ├─ 中等: graph.h, gpu_queue_emu.h, mem_pool.h, hardware_puller_emu.h
  │     └─ hal/hal_user.h (drv 访问 hc->heap 字段)
  │     → 走 HAL 扩展（per ADR-023 D4）— 1 foundation change + N removal changes
  └── C 类（内存结构体）: 0
        → 全部归入 B 类（via HAL fn-ptr 间接调用即可，无须架构级重构）
```

**硬性规则**：本 ADR 生效后（状态升级为 Accepted），**任何新的跨边界 include 不得合并到 main**。L1 检查在 pre-commit hook 中执行，L2 编译测试在 CI 中执行。

---

## 后果

### 正面后果

- ✅ 项目核心目标"逻辑零修改可移植"从**口头承诺变为可验证属性**
- ✅ 自动阻断新违规——开发者不需要记住规则，工具替你记住
- ✅ L1 毫秒级 pre-commit hook 零摩擦——不影响开发体验
- ✅ L2 真机内核头文件编译——唯一真值（Linux 6.12 LTS headers 不会说谎）
- ✅ 已知债务分类处置——不是粗暴"全部禁止"，而是"分类渐进修复"

### 负面后果

- ⚠️ L2 编译测试需要 Docker 环境（Linux 6.12 LTS 内核头文件）——本地开发和 CI 都需要额外配置
- ⚠️ 已知 12 处违规需要 B-class 修复路径（HAL fn-ptr 扩展）——1 foundation + N removal changes
- ⚠️ pre-commit hook 的误报风险——需要在白名单配置中维护准确列表

### 风险

| 风险 | 缓解 |
|------|------|
| L1 白名单被绕过（开发者添加新的 sim/ include 路径） | L2 编译测试作为终极防线（真机内核头文件不存在 sim/） |
| Docker 不可用时 L2 无法执行 | CI 中作为 hard requirement；本地开发依赖 L1（pre-commit） |
| 已知债务修复被无限延期 | 每个违规类别绑定到 roadmap stage（A 类 Stage 4 前, B/C 类触发时） |

---

## 被拒绝的替代方案

### 方案 A: 仅 L1 静态分析（无 L2 编译测试）

**拒绝理由**: grep 是语法检查，可能被绕过（文件重命名、间接 include）。真机内核编译是唯一零漏报方案。L1 + L2 组合提供"快速反馈 + 终极验证"。

### 方案 B: 立即修复所有 12 处违规（不分阶段）

**拒绝理由**（2026-08-03 修订）：12 处违规全部归入 B-class（per Decision 2 修订注），修复路径是 HAL fn-ptr 扩展。一次性提交 12 个 removal change 风险过高（每个需 1-2 小时，HAL 扩展需 review），分批渐进（1 foundation + N removal）更安全。

### 方案 C: 不做任何事情（继续口头规范）

**拒绝理由**: 无检测 = 无执行。12 处违规的存在证明口头规范无效。项目目标"逻辑零修改可移植"必须从宣示变为可验证。

---

## 实施计划

1. **L1：`tools/check-portability.sh`** — 新增脚本
   - 定义 `DRV_DIR="plugins/gpu_driver/drv"` 和 `WHITELIST="shared/ hal/gpu_hal.h sim/proxy/"`（相对于 `DRV_DIR` 的路径前缀）
   - `grep -rn '#include.*"sim/' "$DRV_DIR" | grep -vFf whitelist_patterns.txt`
   - 退出码 0 = clean, 1 = violation found
   - 集成到 `scripts/pre-commit` hook

2. **L2：`tools/compile-check-kernel.sh`** — 新增脚本
   - Docker 容器拉取 `kernel-headers:6.12` 镜像
   - 挂载 `drv/` + `shared/` + `hal/gpu_hal.h` + `include/linux_compat/`
   - `make -C /usr/src/linux-headers-6.12 M=$PWD/drv modules`
   - 失败 = 违规（真机内核头文件中 `sim/` 不存在）

3. **L3：`docs-audit.sh` §5.4** — 追加检查
   - 运行 L1 脚本 → 输出结果到 `docs-audit-report.txt`
   - 违规数 > 0 → Warning（不阻断 CI，仅文档审计警告）

4. **pre-commit hook** — `scripts/pre-commit` 追加
   - 调用 `tools/check-portability.sh`
   - 违规退出码 1 → 阻止 commit
   - `SKIP_PORTABILITY_CHECK=1 git commit` 可跳过（紧急 hotfix）

5. **文件分类与迁移** — 已知违规分 3 个 change
   - `migrate-sim-types-to-shared`：fence_id.h, gpu_queue_emu.h 部分（A 类）
   - `hal-cp-ops-extension`：graph.h, hardware_puller_emu.h, gpu_queue_emu.h 部分（B 类）
   - `sim-mem-pool-hal-abstraction`：mem_pool.h，stream_capture.h（C 类）

---

## 关联文档

- [ADR-036](adr-036-three-way-separation.md) §依赖规则 — 本 ADR 的执行机制
- [ADR-023](adr-023-hal-interface.md) §Decision 5 — HAL 边界禁止规则
- [ADR-043](adr-043-cp-portability-boundary.md) §D5 — 已知技术债清单
- [ADR-064](adr-064-memory-model-staging.md) — mem_map_bo HAL 扩展（C 类违规修复路径）
- [docs-audit.sh](../../tools/docs-audit.sh) — L3 文档审计集成点