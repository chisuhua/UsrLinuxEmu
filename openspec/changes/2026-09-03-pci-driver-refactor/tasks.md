# pci-driver-refactor: Tasks（Stage 5.5.1）

> **状态**: 🔄 Proposed v0.3（2026-09-03，Oracle v0.2 复审 INCONCLUSIVE 修复完成，待 v0.3 复审）
> **关联**: [proposal.md](proposal.md) + [design.md](design.md) + [ADR-091](../00_adr/adr-091-pci-driver-architecture-and-four-quadrant.md) ✅ Accepted v0.2
> **工期**: 4-6 周
> **TDD 纪律**: 每个 task 顺序 = Write test → Verify fail → Implement → Verify pass → Commit

---

## §1 任务总览

| Wave | 内容 | 工期 | 依赖 |
|------|------|-----:|------|
| **1A** | PCI driver 迁移 | 1-2 周 | 无 |
| **1B** | IOMMU driver 迁移 | 2-3 周 | Wave 1A |
| **1C** | sim_hardware 骨架 | 0.5 周 | 无（与 1A/1B 并行） |
| **1D** | ModuleLoader 升级 | 1 周 | 无（与 1A/1B 并行） |
| **1E** | 现有插件适配 + 文档升级 | 1 周 | Wave 1A+1B+1D |

**总计**：4-6 周

---

## §2 Wave 1A：PCI driver 迁移（1-2 周）

### 任务 1.1：建立 `plugins/pci_driver/` 目录结构

- [ ] 创建 `plugins/pci_driver/` 目录
- [ ] 创建 `plugins/pci_driver/include/` 子目录
- [ ] 创建 `plugins/pci_driver/CMakeLists.txt`（见 design.md §D4.1）
- [ ] 创建 `plugins/pci_driver/plugin.cpp` 骨架（含 module mod 导出）

### 任务 1.2：迁移公共头文件

- [ ] **Write test**: `tests/plugins/test_pci_driver_standalone.cpp::test_header_exists` 验证公共头文件存在
- [ ] **Verify fail**: 测试失败（公共头未迁移）
- [ ] **Implement**:
  - [ ] `cp include/kernel/pcie/pcie_emu.h plugins/pci_driver/include/pcie_emu.h`
  - [ ] `cp include/kernel/pcie_device.h plugins/pci_driver/include/pci_device.h`
  - [ ] 验证内容相同（`diff`）
- [ ] **Verify pass**: 测试通过
- [ ] **Commit**: `feat(pci-driver): migrate public headers from include/kernel/pcie/`

### 任务 1.3：迁移私有头文件

- [ ] **Write test**: `tests/plugins/test_pci_driver_standalone.cpp::test_impl_header_exists`
- [ ] **Verify fail**: 测试失败
- [ ] **Implement**:
  - [ ] `cp src/kernel/pcie/pcie_emu_impl.h plugins/pci_driver/include/pcie_emu_impl.h`
- [ ] **Verify pass**: 测试通过
- [ ] **Commit**: `feat(pci-driver): migrate private header pcie_emu_impl.h`

### 任务 1.4：迁移 4 个实现源文件

- [ ] **Write test**: `tests/plugins/test_pci_driver_standalone.cpp::test_basic_pcie_emu` 验证 PcieEmu 基本功能
- [ ] **Verify fail**: 测试失败（实现未迁移）
- [ ] **Implement**:
  - [ ] `cp src/kernel/pcie/pcie_emu.cpp plugins/pci_driver/probe.cpp`（v0.3 修订：保留 .cpp 扩展名）
  - [ ] `cp src/kernel/pcie/config_space.cpp plugins/pci_driver/pci_access.cpp`
  - [ ] `cp src/kernel/pcie/msi_x.cpp plugins/pci_driver/pci_msi.cpp`
  - [ ] `cp src/kernel/pcie/capability_walk.cpp plugins/pci_driver/pci_cap.cpp`
- [ ] **Verify pass**: 测试通过（编译 + 运行）
- [ ] **Commit**: `feat(pci-driver): migrate 4 implementation files from src/kernel/pcie/`

### 任务 1.5：重命名命名空间

- [ ] **Write test**: `tests/plugins/test_pci_driver_standalone.cpp::test_namespace_pci` 验证 `usr_linux_emu::pci` 命名空间
- [ ] **Verify fail**: 测试失败（命名空间未改）
- [ ] **Implement**:
  - [ ] 在 `plugins/pci_driver/` 全部文件中将 `pcie_internal` → `pci`
  - [ ] 更新 `include/linux_compat/pci/` 中的引用
- [ ] **Verify pass**: 测试通过 + 现有 test_pcie_emu_standalone 仍 PASS（API 兼容）
- [ ] **Commit**: `refactor(pci-driver): rename namespace pcie_internal -> pci`

### 任务 1.6：移除 src/kernel/pcie/ 旧文件

- [ ] **Write test**: `grep -r "src/kernel/pcie/" --include="*.cpp" --include="*.h"` 应返回 0 匹配（除归档注释）
- [ ] **Verify fail**: 测试失败（仍有引用）
- [ ] **Implement**:
  - [ ] `rm -rf src/kernel/pcie/`
  - [ ] 修改 `src/kernel/CMakeLists.txt`：移除 pcie 引用
- [ ] **Verify pass**: 全仓编译通过 + 全部现有测试 PASS
- [ ] **Commit**: `refactor(kernel): remove src/kernel/pcie/ after migration`

---

## §3 Wave 1B：IOMMU driver 迁移（2-3 周）

### 任务 2.1：建立 `plugins/iommu_driver/` 目录结构

- [ ] 创建 `plugins/iommu_driver/` 目录
- [ ] 创建 `plugins/iommu_driver/include/` 子目录
- [ ] 创建 `plugins/iommu_driver/CMakeLists.txt`（见 design.md §D4.2）
- [ ] 创建 `plugins/iommu_driver/plugin.cpp` 骨架

### 任务 2.2：迁移 6 个 iommu framework 源文件

- [ ] **Write test**: `tests/plugins/test_iommu_driver_standalone.cpp::test_iommu_init` 验证 iommu_emu_init 工作
- [ ] **Verify fail**: 测试失败
- [ ] **Implement**:
  - [ ] `cp src/kernel/iommu/iommu_emu_state.cpp plugins/iommu_driver/iommu.cpp`
  - [ ] `cp src/kernel/iommu/iommu_group.cpp plugins/iommu_driver/iommu_group.cpp`
  - [ ] `cp src/kernel/iommu/iommu_domain.cpp plugins/iommu_driver/iommu_domain.cpp`
  - [ ] `cp src/kernel/iommu/ats_protocol.cpp plugins/iommu_driver/ats_protocol.cpp`
  - [ ] `cp src/kernel/iommu/dma_remap.cpp plugins/iommu_driver/dma_remap.cpp`
  - [ ] `cp src/kernel/iommu/ioasid.cpp plugins/iommu_driver/ioasid.cpp`
- [ ] **Verify pass**: 测试通过
- [ ] **Commit**: `feat(iommu-driver): migrate 6 framework files from src/kernel/iommu/`

### 任务 2.3：迁移 vfio_bridge（OQ5 决议 A）

- [ ] **Write test**: `tests/plugins/test_iommu_driver_standalone.cpp::test_vfio_bridge_init`
- [ ] **Verify fail**: 测试失败
- [ ] **Implement**:
  - [ ] `cp src/kernel/iommu/vfio_bridge.cpp plugins/iommu_driver/vfio_bridge.cpp`
  - [ ] `cp src/kernel/iommu/vfio_bridge.h plugins/iommu_driver/include/vfio_bridge.h`
- [ ] **Verify pass**: 测试通过
- [ ] **Commit**: `feat(iommu-driver): migrate vfio_bridge.cpp/h`

### 任务 2.4：迁移 pcie_integration（跨 driver 协作）

- [ ] **Write test**: `tests/plugins/test_iommu_driver_standalone.cpp::test_pci_iommu_integration`
- [ ] **Verify fail**: 测试失败
- [ ] **Implement**:
  - [ ] `cp src/kernel/iommu/pcie_integration.cpp plugins/pci_driver/pci_iommu_integration.cpp`
  - [ ] 更新 #include：指向 `plugins/iommu_driver/include/`
- [ ] **Verify pass**: 测试通过
- [ ] **Commit**: `feat(pci-driver): add pci_iommu_integration from src/kernel/iommu/`

### 任务 2.5：迁移 iommu_internal.h（OQ6 决议 A）

- [ ] **Write test**: `tests/plugins/test_iommu_driver_standalone.cpp::test_internal_header_exists`
- [ ] **Verify fail**: 测试失败
- [ ] **Implement**:
  - [ ] `cp src/kernel/iommu/iommu_internal.h plugins/iommu_driver/include/iommu_internal.h`
  - [ ] 修改 `tests/test_iommu_priv_contract_regression_standalone.cpp` 引用新路径
- [ ] **Verify pass**: 测试通过 + 现有 4 个 test_iommu_* 全 PASS
- [ ] **Commit**: `feat(iommu-driver): migrate iommu_internal.h + fix test refs`

### 任务 2.6：invalidate.cpp 暂留 Q2（v0.3 修订 — ORACL-Block-4 fix）

> **v0.3 重要修订**：原 v0.1/v0.2 计划将 `invalidate.cpp` 迁入 `sim_hardware/dma/iommu_hw.cpp`。但 Oracle 实测发现 `invalidate.cpp` include `kernel/uvm/mmu_notifier_internal.h` + `kernel/uvm/mm_shim.h`（Q1 kernel sim），是 registration stub + dispatch 桥，不是纯硬件模型；移入 Q3 会引入 Q3→Q1 反向依赖，且破坏 `sim_hardware` 不依赖 `Q1` 的设计。**Q2/Q3 拆分决策推迟到 Change-2 显式裁决**。

- [ ] **Write test**: `tests/plugins/test_iommu_driver_standalone.cpp::test_invalidate_stays_in_q2`
- [ ] **Verify fail**: 测试失败（invalidate.cpp 尚未迁移）
- [ ] **Implement**:
  - [ ] `cp src/kernel/iommu/invalidate.cpp plugins/iommu_driver/invalidate.cpp`（**暂留 Q2**，不迁入 sim_hardware）
  - [ ] iommu.cpp 内部直接调用 invalidate.cpp（无需 #include sim_hardware）
  - [ ] 在 Change-1 内**不**创建 `sim_hardware/dma/`
- [ ] **Verify pass**: 测试通过 + iommu.cpp 编译通过
- [ ] **Commit**: `feat(iommu-driver): migrate invalidate.cpp (stay in Q2, defer Q2/Q3 split to Change-2)`

### 任务 2.7：移除 src/kernel/iommu/ 旧文件

- [ ] **Write test**: `grep -r "src/kernel/iommu/" --include="*.cpp" --include="*.h"` 应返回 0 匹配
- [ ] **Verify fail**: 测试失败
- [ ] **Implement**:
  - [ ] `rm -rf src/kernel/iommu/`
  - [ ] 修改 `src/kernel/CMakeLists.txt`：移除 iommu 引用
- [ ] **Verify pass**: 全仓编译通过 + 全部现有测试 PASS
- [ ] **Commit**: `refactor(kernel): remove src/kernel/iommu/ after migration`

---

## §4 Wave 1C：sim_hardware 骨架（0.5 周，v0.3 修订 — ORACL-CRITICAL fix）

> **v0.3 重要修订**：原 v0.1/v0.2 在 `src/sim_hardware/` + `include/sim_hardware/` 拆分目录，但 **ADR-091 §D2.3 + four-quadrant-architecture.md §4.2 SSOT 定义为顶级目录模型** `sim_hardware/{include,src,topology}/`。**统一为顶级目录**。

### 任务 3.1：建立 sim_hardware 顶级目录结构

- [ ] 创建 `sim_hardware/` 顶级目录（不在 src/ 或 include/ 下）
- [ ] 创建 `sim_hardware/include/{pcie,cpptlm,chipset}/` 子目录（**不**创建 dma/，因 invalidate.cpp 暂留 Q2）
- [ ] 创建 `sim_hardware/src/` 目录
- [ ] 创建 `sim_hardware/topology/` 目录
- [ ] 创建 `sim_hardware/CMakeLists.txt`（见 design.md §D4.3）
- [ ] 创建 `sim_hardware/include/platform.h` 占位头文件

### 任务 3.2：占位头文件（Stage 5.5.1 仅骨架）

- [ ] 创建 `sim_hardware/include/pcie/bypass.h` 占位（仅 enum-only，不含类方法以避免半成品 API）
- [ ] 创建 `sim_hardware/include/pcie/host_bridge.h` 占位
- [ ] 创建 `sim_hardware/include/cpptlm/bridge.h` 占位
- [ ] **不创建** `sim_hardware/include/dma/iommu_hw.h`（invalidate.cpp 暂留 Q2，详见任务 2.6）

### 任务 3.3：顶层 CMakeLists 集成

- [ ] 修改顶层 `CMakeLists.txt`：添加 `add_subdirectory(sim_hardware)`（顶级目录路径，无 `src/` 前缀）
- [ ] 验证编译通过（即使占位）

---

## §5 Wave 1D：ModuleLoader 升级（1 周）

### 任务 4.1：struct module ABI 变更

- [ ] **Write test**: `tests/test_moduleloader_toposort_standalone.cpp::test_module_struct_load_priority`
- [ ] **Verify fail**: 测试失败（无 load_priority 字段）
- [ ] **Implement**:
  - [ ] 修改 `include/kernel/module_loader.h`：struct module 加 `uint32_t load_priority`
- [ ] **Verify pass**: 测试通过
- [ ] **Commit**: `feat(moduleloader): add load_priority field to struct module (ABI change)`

### 任务 4.2：拓扑排序算法

- [ ] **Write test**: 5 个测试用例（见 design.md §D5.2）
- [ ] **Verify fail**: 测试失败（无拓扑排序）
- [ ] **Implement**:
  - [ ] 修改 `src/kernel/module_loader.cpp`：添加 topological_sort + cycle detection
  - [ ] 修改 `load_plugins()` 使用拓扑排序
- [ ] **Verify pass**: 全部 5 个测试用例通过
- [ ] **Commit**: `feat(moduleloader): implement topological sort + cycle detection`

### 任务 4.3：现有 3 个 plugin + 2 个 sample driver 加 load_priority + depends 字段（v0.3 修订 — ORACL-Block-2 fix）

> **v0.3 路径实测修正**（`find plugins drivers -name plugin.cpp` 实测）：
> - `plugins/gpu_driver/plugin.cpp`（**无** drv/ 子目录 — 与 AGENTS.md CODE MAP 一致）
> - `plugins/net_driver/drv/plugin.cpp`（有 drv/）
> - `plugins/storage_driver/drv/plugin.cpp`（有 drv/）
> - sample_memory / sample_serial **plugin.cpp 不存在**（仅 `drivers/sample_memory.cpp` / `drivers/sample_serial.cpp` 示例源文件，无 module mod 导出）

- [ ] **Write test**: `tests/test_moduleloader_toposort_standalone.cpp::test_existing_plugins_have_load_priority` — 验证 3 个 plugin 的 module mod 包含 `load_priority` 字段且值非 0
- [ ] **Verify fail**: 测试失败（plugin 缺 load_priority）
- [ ] **Implement**（**v0.3 实测路径**）：
  - [ ] 修改 `plugins/gpu_driver/plugin.cpp`（**无** drv/）：加 `load_priority = 300` + `.depends = (const char*[]){"pci_driver", "iommu_driver", nullptr}`
  - [ ] 修改 `plugins/net_driver/drv/plugin.cpp`：加 `load_priority = 400`（无 plugin 依赖）
  - [ ] 修改 `plugins/storage_driver/drv/plugin.cpp`：加 `load_priority = 400`（无 plugin 依赖）
  - [ ] **skip sample_memory / sample_serial**（无 module mod 导出，无 module struct 需加 load_priority）
- [ ] **Verify pass**: 测试通过 + 3 个 plugin 编译通过
- [ ] **Commit**: `feat(plugin): add load_priority field to 3 existing plugins + gpu_driver deps`

### 任务 4.4：pci_driver + iommu_driver plugin.cpp 加 load_priority + depends

- [ ] `plugins/pci_driver/plugin.cpp`：
  - [ ] `.load_priority = 100`
  - [ ] `.depends = (const char*[]){"sim_hardware", nullptr}`（注意：sim_hardware 不是 plugin，是静态库）
  - [ ] ⚠️ 注：sim_hardware 是 STATIC 库不是 plugin，所以 depends 实际为空数组
  - [ ] 修改：`.depends = (const char*[]){nullptr}` (无 plugin 依赖)
- [ ] `plugins/iommu_driver/plugin.cpp`：
  - [ ] `.load_priority = 200`
  - [ ] `.depends = (const char*[]){nullptr}` (无 plugin 依赖，独立于 pci_driver)

---

## §6 Wave 1E：现有插件适配 + 文档升级（1 周）

### 任务 5.1：现有 6 个 test_*_standalone 路径适配

- [ ] **Verify**: `tests/test_pcie_emu_standalone.cpp` 仍编译（路径不变）
- [ ] **Verify**: `tests/test_pcie_gpu.cpp` 仍编译
- [ ] **Verify**: `tests/test_iommu_emu_standalone.cpp` 仍编译
- [ ] **Verify**: `tests/test_iommu_invalidate_runtime_standalone.cpp` 仍编译
- [ ] **Verify**: `tests/test_iommu_notifier_standalone.cpp` 仍编译
- [ ] **Verify**: `tests/test_iommu_priv_contract_regression_standalone.cpp` 仍编译（OQ6 决议 A：改 #include）
- [ ] **Verify**: 全部 6 个测试 PASS

### 任务 5.2：ADR-036 升级 v0.2

- [ ] 修改 `docs/00_adr/adr-036-three-way-separation.md`：3 区分 → 4 象限
- [ ] 在 ADR 顶部记录 v0.2 changelog
- [ ] Status: 🔄 Proposed → ✅ Accepted（由 ADR-091 v0.2 评审合并批准）

### 任务 5.3：ADR-089 升级 v0.6（location 更新）

- [ ] 修改 `docs/00_adr/adr-089-v55-system-hw-simulation.md`：location `src/system_hw/` → `sim_hardware/`
- [ ] Status: ✅ Accepted → ✅ Accepted v0.6

### 任务 5.4：ADR-072 升级 v0.2（L2 build 扩展）

- [ ] 修改 `docs/00_adr/adr-072-portability-validation.md`：L2 build 目标集扩展
- [ ] Status: ✅ Accepted → ✅ Accepted v0.2

### 任务 5.5：post-refactor-architecture.md §1.10 更新

- [ ] 添加 Stage 5.5.1 完成状态标注
- [ ] 列出 4 象限迁移后的目录结构（与 design.md §D1.1 对齐）

---

## §7 Gate 验证

### Gate 5.5.1-A：现有 ctest 全 PASS（v0.2 修订 — ORACL-Block-5 fix）

- [ ] **测试链接方式**：**现有 6 个 test_*_standalone 通过 dlopen 加载 plugin .so 解析符号**（而非链接期 link 插件）
- [ ] **新增 task**：每个 test CMakeLists.txt 加 `dlopen` helper 调 `module mod` + 解析符号
- [ ] 执行：`ctest --output-on-failure`
- [ ] 期望：所有现有测试 PASS（0 regression）
- [ ] 基线数字：`ctest -N | wc -l`（建立 baseline 数字）

### Gate 5.5.1-B：ModuleLoader 拓扑排序工作

- [ ] 执行：`./build/bin/test_moduleloader_toposort_standalone`
- [ ] 期望：5 个测试场景全部 PASS

### Gate 5.5.1-C：L2 build 通过（v0.2 修订 — ORACL-Block-3 fix）

- [ ] Stage 5.5.1 内**新建** `tools/l2-build/build_pci_driver.sh` + `build_iommu_driver.sh`（脚本本身在 Change-1 内实施）
- [ ] 执行：`./tools/l2-build/build_pci_driver.sh`
- [ ] 期望：编译通过（仅 #include sed 替换 + stub 真机编译）
- [ ] 执行：`./tools/l2-build/build_iommu_driver.sh`
- [ ] 期望：编译通过

### Gate 5.5.1-D：Oracle 实施后复审（v0.2 新增 — METIS-Q1.3 闭环）

- [ ] Change-1 全部 Wave 完成后，**Oracle 重新审查**
- [ ] Oracle 评估：(a) 实施 vs ADR-091 v0.2 + design.md 目标态的偏差
- [ ] Oracle 决定：(b) 是否调整 ADR-091 / 4 象限架构文档 / 实施路线图
- [ ] Oracle 决定：(c) 是否调整 Change-2（Stage 5.5.2）的内容或时间线
- [ ] 输出：Oracle 复审报告（含调整建议列表 + 优先级）
- [ ] **前置评审**：ADR-036/089/072 升档目标态在 Change-1 实施**前**由本次 Oracle+Metis 双审查一并审

---

## §8 实施顺序

**严格按以下顺序执行（前序依赖）**：

```
Wave 1A（PCI） + Wave 1C（sim_hardware 骨架）+ Wave 1D（ModuleLoader）
                    ↓
        Wave 1B（IOMMU，含 pci_iommu_integration）
                    ↓
        Wave 1E（现有插件适配 + 文档升级）
                    ↓
            Gate 5.5.1-A / B / C 验证
                    ↓
            Change-2 (Stage 5.5.2) 启动
```

**Wave 1A、1C、1D 可并行**（修改不同文件集）
**Wave 1B 必须在 1A+1D 完成后**（pci_iommu_integration 依赖 pci_driver）
**Wave 1E 必须在 1A+1B+1D 完成后**（现有插件适配）

---

## §9 风险监控

| # | 风险 | 监控指标 | 缓解 |
|---|------|----------|------|
| 1 | 迁移映射错误 | `git diff --stat`（应仅显示迁移，无功能变更） | ADR-091 v0.2 已 100% 实测验证 |
| 2 | 现有 plugin 重编译失败 | `make -j$(nproc)` 输出 | task 4.3 加 load_priority 字段 |
| 3 | test_*_standalone 回归 | `ctest --output-on-failure` | task 5.1 路径适配 + OQ6 决议 A |
| 4 | L2 build 失败 | `tools/l2-build/build_*.sh` 输出 | task 5.5 sed 脚本批量改 #include |
| 5 | **残留旧 ABI .so 静默损坏**（v0.2 新增 — METIS）| `find plugins -name "*.so"` 输出 | task 4.3 前加"清理 `plugins/*.so` + 全量重建"步骤 + ModuleLoader 导出 `MODULE_ABI_VERSION` 拒绝旧插件 |
| 6 | **测试 5 个 #include 未 tasked**（v0.2 新增 — METIS）| `grep "include.*kernel/pcie" tests/` 输出 | 任务 5.1 显式列出每个测试的 #include 旧→新映射 |
| 7 | **构建接线缺失**（v0.2 新增 — METIS）| `make` 输出 | 新增 task 修改 `plugins/CMakeLists.txt` + `tests/CMakeLists.txt` |
| 8 | **sim_hardware 依赖模型分歧**（v0.3 修订 — invalidate.cpp 暂留 Q2）| iommu.cpp → invalidate.cpp 内部调用链 | 5.5.1 INTERFACE 库占位，Q2/Q3 拆分决策推迟 Change-2 |

---

## §10 关联文档

- [proposal.md](proposal.md) — Change 提案
- [design.md](design.md) — 详细设计
- [ADR-091](../00_adr/adr-091-pci-driver-architecture-and-four-quadrant.md) — SSOT
- [4 象限架构文档](../02_architecture/four-quadrant-architecture.md) — 4 象限详细
- [实施路线图](../roadmap/pcie-bus-bridge-roadmap.md) — Stage 5.5.1-5.5.5

---

**状态**: 🔄 Proposed v0.3（2026-09-03，Oracle v0.2 复审 INCONCLUSIVE 修复完成，待 v0.3 复审）

---

## 修订记录

- **v0.3** (2026-09-03)：Oracle v0.2 复审 INCONCLUSIVE 修复
  - 任务 1.4/2.2/2.3/2.4：所有 `.c` → `.cpp`
  - 任务 2.6：invalidate.cpp 暂留 Q2（不迁入 sim_hardware）
  - §4 Wave 1C：sim_hardware 顶级目录模型
  - 任务 4.3：5 plugin → 3 plugin（sample_* 无 module mod）
  - 任务 4.3：gpu_driver 加 depends
  - §7 Gate D 新增（Oracle 实施后复审）
  - §9 风险监控 4 项 → 8 项
- **v0.2** (2026-09-03)：Oracle + Metis 双审查 v0.1 INCONCLUSIVE 修复
- **v0.1** (2026-09-03)：初版
