# pci-driver-refactor: 4 象限重构设计文档（Stage 5.5.1）

> **状态**: 🔄 Proposed v0.3（2026-09-03，Oracle v0.2 复审 INCONCLUSIVE 修复完成，待 v0.3 复审）
> **关联**: [proposal.md](proposal.md) + [ADR-091](../00_adr/adr-091-pci-driver-architecture-and-four-quadrant.md) ✅ Accepted v0.2

---

## D1: 模块依赖图（4 象限视图）

### D1.1: 迁移后目录结构

```
/workspace/project/UsrLinuxEmu/
├── AGENTS.md                              # 更新：4 象限布局
├── README.md                              # 更新：架构图
├── CMakeLists.txt                         # add_subdirectory(sim_hardware)
│
├── include/                               # 不变
│   ├── kernel/                            # Q1: ONLY Linux kernel env sim
│   │   ├── vfs.h
│   │   ├── module_loader.h                # ⚠️ 修改：struct module 加 load_priority
│   │   ├── device/
│   │   ├── service_registry.h
│   │   └── ...
│   ├── linux_compat/                      # 不变（API header）
│   │   ├── pci/                           # 保留
│   │   ├── iommu/                         # 保留
│   │   ├── drm/
│   │   └── ...
│   # v0.3 修订：sim_hardware 不放在 include/ 下，见顶级 sim_hardware/
│
├── src/
│   ├── kernel/                            # Q1: ONLY Linux kernel env sim impl
│   │   ├── vfs.cpp
│   │   ├── module_loader.cpp              # ⚠️ 修改：拓扑排序算法
│   │   └── ...
│   │   # ❌ 移除 src/kernel/pcie/ 全部文件
│   │   # ❌ 移除 src/kernel/iommu/ 全部文件
│   # v0.3 修订：sim_hardware 不放在 src/ 下，见顶级 sim_hardware/
│
├── sim_hardware/                          # v0.3 顶级目录（per ADR-091 §D2.3 SSOT）
│   ├── CMakeLists.txt                     # INTERFACE 库占位
│   ├── include/                            # Q3 公共 API 头
│   │   ├── platform.h
│   │   ├── pcie/bypass.h                  # enum-only 占位
│   │   ├── pcie/host_bridge.h             # 占位
│   │   └── cpptlm/bridge.h                # 占位
│   ├── src/                                # 5.5.1 空（Stage 5.5.2 填充）
│   └── topology/                           # PC 拓扑配置
│
├── plugins/
│
├── plugins/
│   ├── pci_driver/                        # 🆕 Q2: PCI subsystem driver
│   │   ├── CMakeLists.txt                 # add_library(pci_driver SHARED)
│   │   ├── plugin.cpp                     # 导出 module mod
│   │   ├── include/
│   │   │   ├── pcie_emu.h                 # 迁移自 include/kernel/pcie/pcie_emu.h
│   │   │   ├── pcie_emu_impl.h            # 迁移自 src/kernel/pcie/pcie_emu_impl.h
│   │   │   └── pci_device.h               # 迁移自 include/kernel/pcie_device.h
│   │   ├── probe.cpp                       # 迁移自 src/kernel/pcie/pcie_emu.cpp（v0.3 保留 .cpp）
│   │   ├── pci_access.cpp                  # 迁移自 config_space.cpp
│   │   ├── pci_msi.cpp                     # 迁移自 msi_x.cpp
│   │   └── pci_cap.cpp                     # 迁移自 capability_walk.cpp
│   │
│   ├── iommu_driver/                      # 🆕 Q2: IOMMU subsystem driver
│   │   ├── CMakeLists.txt
│   │   ├── plugin.cpp
│   │   ├── include/
│   │   │   ├── iommu_internal.h           # 迁移自 src/kernel/iommu/iommu_internal.h
│   │   │   └── vfio_bridge.h              # 迁移自 src/kernel/iommu/vfio_bridge.h
│   │   ├── iommu.cpp                       # 迁移自 iommu_emu_state.cpp（v0.3 保留 .cpp）
│   │   ├── iommu_group.cpp                 # 迁移自 iommu_group.cpp
│   │   ├── iommu_domain.cpp                # 迁移自 iommu_domain.cpp
│   │   ├── ats_protocol.cpp                # 迁移自 ats_protocol.cpp
│   │   ├── dma_remap.cpp                   # 迁移自 dma_remap.cpp
│   │   ├── ioasid.cpp                      # 迁移自 ioasid.cpp
│   │   ├── vfio_bridge.cpp                 # 迁移自 vfio_bridge.cpp
│   │   ├── invalidate.cpp                  # 迁移自 src/kernel/iommu/invalidate.cpp（v0.3 暂留 Q2，**不**迁入 sim_hardware）
│   │   ├── pci_iommu_integration.cpp       # 迁移自 src/kernel/iommu/pcie_integration.cpp
│   │   └── (iommu_internal.h 引用测试需修)
│   │
│   ├── gpu_driver/                        # ✅ 不变
│   ├── net_driver/                        # ✅ 不变
│   ├── storage_driver/                    # ✅ 不变
│   ├── drivers/sample_memory.cpp          # ✅ v0.3 实测：源文件，无 plugin.cpp（无 module mod 导出，无需加 load_priority）
│   └── drivers/sample_serial.cpp          # ✅ 同上
│
├── tests/
│   ├── (新增) plugins/
│   │   ├── test_pci_driver_standalone.cpp         # 新增
│   │   └── test_iommu_driver_standalone.cpp       # 新增
│   ├── test_pcie_emu_standalone.cpp               # ✅ 路径不变（API 兼容）
│   ├── test_iommu_emu_standalone.cpp              # ✅ 路径不变
│   ├── test_iommu_invalidate_runtime_standalone.cpp # ✅ 路径不变
│   ├── test_iommu_notifier_standalone.cpp         # ✅ 路径不变
│   ├── test_iommu_priv_contract_regression_standalone.cpp # ✅ 路径不变（可能需改头文件引用）
│   ├── test_pcie_gpu.cpp                          # ✅ 路径不变
│   └── test_moduleloader_toposort_standalone.cpp   # 🆕 新增
│
└── docs/
    ├── 00_adr/
    │   ├── adr-091-pci-driver-architecture-and-four-quadrant.md  # ✅ Accepted v0.2
    │   ├── adr-036-three-way-separation.md                       # ⚠️ 升级 v0.2 (3→4 象限)
    │   ├── adr-089-v55-system-hw-simulation.md                   # ⚠️ 升级 v0.6 (location)
    │   └── adr-072-portability-validation.md                     # ⚠️ 升级 v0.2 (L2 build 扩展)
    ├── 02_architecture/
    │   ├── four-quadrant-architecture.md         # ✅ Accepted v0.2
    │   └── post-refactor-architecture.md         # ⚠️ 更新 §1.10（Stage 5.5.1 状态）
    └── roadmap/
        └── pcie-bus-bridge-roadmap.md            # ✅ Accepted v0.2
```

### D1.2: 4 象限依赖图

```
┌─────────────────────────────────────────────────────────────────────────┐
│ Q1: src/kernel/ (Linux Kernel Env Sim)                                  │
│   - vfs / module_loader (升级) / iocontroller                           │
│   - 不依赖 Q2/Q3/Q4                                                     │
└─────────────────────────────────────────────────────────────────────────┘
                            ▲ (kernel API)
                            │
┌─────────────────────────────────────────────────────────────────────────┐
│ Q2: plugins/*_driver/ (Portable Driver Code)                            │
│   - pci_driver (新) — 依赖 Q1 + sim_hardware API (Stage 5.5.2+)        │
│   - iommu_driver (新) — 依赖 Q1                                         │
│   - gpu_driver / net_driver / storage_driver (不变)                     │
└─────────────────────────────────────────────────────────────────────────┘
                            ▲ (调 platform API)
                            │
┌─────────────────────────────────────────────────────────────────────────┐
│ Q3: sim_hardware/ (PC System Hardware Sim, v0.3 顶级目录)            │
│   - 5.5.1 仅骨架（include/src/topology 占位，无实现）                    │
│   - pcie/ + cpptlm/ + chipset/ (Stage 5.5.1 仅骨架占位)                 │
│   - 不依赖 Q1/Q2/Q4                                                    │
└─────────────────────────────────────────────────────────────────────────┘

Q4: plugins/gpu_driver/sim/ (GPU HW Sim) — 不变
HAL: plugins/gpu_driver/hal/ (68 fn-ptrs) — 不变（per ADR-023 append-only）
```

## D2: ModuleLoader struct module ABI 变更

### D2.1: 现有 struct module（`include/kernel/module_loader.h:10`）

```c
typedef struct module {
  const char* name;       // 插件名称
  const char** depends;   // 依赖项列表（NULL 结尾）
  int (*init)(void);      // 初始化函数
  void (*exit)(void);     // 卸载函数
} module;
```

### D2.2: Stage 5.5.1 后目标 struct module

```c
typedef struct module {
  const char* name;            // 插件名称（不变）
  uint32_t    load_priority;   // 🆕 新增：0=默认，按权重拓扑排序
  const char** depends;       // 依赖项列表（NULL 结尾，不变）
  int (*init)(void);           // 初始化函数（不变）
  void (*exit)(void);          // 卸载函数（不变）
} module;
```

### D2.3: 字段顺序与 designated initializer 兼容性

- 现有 5 个 plugin 使用 designated initializer（`module mod = {.name = ..., .depends = ..., .init = ..., .exit = ...}`）
- designated initializer **容忍字段重排序**（按字段名匹配，与位置无关）
- **但** ABI 变更要求所有现有 plugin **重编译**（struct size 变化）

### D2.4: 拓扑排序算法

```cpp
// src/kernel/module_loader.cpp 新增算法
class ModuleLoader {
public:
    static int load_plugins(const std::string& dir_path);  // 升级为拓扑排序
    static void unload_plugins();
    
private:
    // 拓扑排序：DAG 检测 + 优先级权重
    static std::vector<std::string> topological_sort(
        std::unordered_map<std::string, PluginInfo>& plugins);
    
    // 循环依赖检测：DFS 三色标记
    static bool has_cycle(const std::unordered_map<std::string, std::vector<std::string>>& graph);
    
    // 解析依赖：递归 + memoization
    static std::vector<std::string> resolve_dependencies(
        const std::string& plugin_name,
        const std::unordered_map<std::string, PluginInfo>& plugins);
};
```

### D2.5: 加载顺序

```
1. 读取所有 plugin → 构建依赖图
2. 检测循环依赖 → 失败则报错（exit -ELOOP）
3. 拓扑排序 → 按 load_priority 平局时序
4. 依次 dlopen + 调 init()
```

## D3: 命名空间迁移

### D3.1: 命名空间映射

| 现有 | 新 |
|------|---|
| `usr_linux_emu::pcie_internal` | `usr_linux_emu::pci` |
| `usr_linux_emu::iommu_emu_global_state` | `usr_linux_emu::iommu_driver::global_state` |
| `usr_linux_emu::pcie_internal::PcieEmuImpl` | `usr_linux_emu::pci::PcieEmuImpl` |

### D3.2: API 兼容保证

- `include/kernel/pcie/pcie_emu.h` → `plugins/pci_driver/include/pcie_emu.h`（内容相同）
- `include/kernel/pcie_device.h` → `plugins/pci_driver/include/pci_device.h`（内容相同）
- 现有 6 个 test_* 路径不变，#include 路径调整（如 `include/kernel/pcie/pcie_emu.h` → `pci_driver/include/pcie_emu.h`）

## D4: 构建系统变更

### D4.1: plugins/pci_driver/CMakeLists.txt

```cmake
add_library(pci_driver SHARED
    plugin.cpp
    probe.cpp
    pci_access.cpp
    pci_msi.cpp
    pci_cap.cpp
    pci_iommu_integration.cpp
)
target_include_directories(pci_driver PUBLIC include)
target_link_libraries(pci_driver PUBLIC kernel)
add_dependencies(pci_driver kernel)
set_target_properties(pci_driver PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins)
install(TARGETS pci_driver DESTINATION plugins)
```

### D4.2: plugins/iommu_driver/CMakeLists.txt

```cmake
add_library(iommu_driver SHARED
    plugin.cpp
    iommu.cpp iommu_group.cpp iommu_domain.cpp
    ats_protocol.cpp dma_remap.cpp ioasid.cpp
    invalidate.cpp                  # 暂留 Q2（v0.3）
    vfio_bridge.cpp
)
target_include_directories(iommu_driver PUBLIC include)
target_link_libraries(iommu_driver PUBLIC kernel)
add_dependencies(iommu_driver kernel)
set_target_properties(iommu_driver PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins)
install(TARGETS iommu_driver DESTINATION plugins)
```

### D4.3: sim_hardware/CMakeLists.txt（v0.3 顶级目录）

```cmake
# v0.3 修订：sim_hardware 是顶级目录，无 src/ 前缀
# 5.5.1 仅占位，src/ 目录为空（Stage 5.5.2 填充）
add_library(sim_hardware INTERFACE)  # INTERFACE 库（仅头文件占位）
target_include_directories(sim_hardware INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
# v0.3：invalidate.cpp 暂留 Q2，不在此处
```

### D4.4: 顶层 CMakeLists.txt 修改

```cmake
# Stage 5.5.1 之前：
# add_subdirectory(src/kernel)
# add_subdirectory(plugins)

# Stage 5.5.1 之后：
add_subdirectory(src/kernel)
add_subdirectory(sim_hardware)       # 🆕 v0.3 顶级目录路径
add_subdirectory(plugins/pci_driver)   # 🆕
add_subdirectory(plugins/iommu_driver) # 🆕
add_subdirectory(plugins/gpu_driver)
# ... 其他 plugin
```

### D4.5: 移除原 src/kernel/pcie/ 和 src/kernel/iommu/ 的 CMakeLists.txt 引用

```cmake
# Stage 5.5.1 之前（src/kernel/CMakeLists.txt）：
target_sources(kernel PRIVATE
    pcie/pcie_emu.cpp
    pcie/config_space.cpp
    pcie/msi_x.cpp
    pcie/capability_walk.cpp
    iommu/iommu_emu_state.cpp
    iommu/iommu_group.cpp
    # ... 其他 iommu 文件
)

# Stage 5.5.1 之后：移除上述所有 iommu 行 + 全部 pcie 行（invalidate.cpp 留在 iommu_driver，v0.3 修订）
target_sources(kernel PRIVATE
    # 仅 kernel core sim（移除 pcie + iommu）
)
```

## D5: ModuleLoader 拓扑排序实现细节

### D5.1: 数据结构

```cpp
struct PluginInfo {
    std::string path;
    void* handle = nullptr;
    module* mod;
    int ref_count = 0;
    uint32_t load_priority = 0;
    std::vector<std::string> depends;  // 解析后的依赖列表
};
```

### D5.2: 算法流程

```
1. dlopen 所有 plugin → 读取 module struct（name, depends, init, exit, load_priority）
2. 构建依赖图：plugin_name → [depends array]
3. 检测循环依赖：DFS 三色标记（white/gray/black）
4. 拓扑排序：Kahn 算法（BFS）→ 按 load_priority 升序（同 load_priority 时按名字字典序）
5. 按序 dlopen + init
```

### D5.3: 错误处理

| 错误 | 返回码 | 说明 |
|------|--------|------|
| 循环依赖 | -ELOOP | 打印循环路径 |
| 依赖缺失 | -ENOENT | 打印缺失插件名 |
| dlopen 失败 | -ELIBACC | 打印 dlerror() |
| init 失败 | -ECOMM | 清理已加载 plugins |

## D6: 测试策略

### D6.1: Gate 5.5.1-A 验证

```bash
cd /workspace/project/UsrLinuxEmu
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)
ctest --output-on-failure

# 期望：所有现有测试 PASS（0 regression）
# 基线数字 = ctest -N | wc -l
```

### D6.2: Gate 5.5.1-B 验证

```bash
# test_moduleloader_toposort_standalone 测试场景：
./build/bin/test_moduleloader_toposort_standalone
# - 单 plugin 无依赖 → 按 load_priority 排序
# - 链式依赖：A → B → C → C 先加载
# - 循环依赖检测：返回 -ELOOP
# - 5 个现有 plugin 重编译后仍可加载
```

### D6.3: Gate 5.5.1-C 验证（v0.2 修订 — ORACL-Block-3 fix）

```bash
# v0.2 修订：Stage 5.5.1 创建 tools/l2-build/ 脚本（之前不存在）
cd /workspace/project/UsrLinuxEmu
mkdir -p tools/l2-build

# scripts/build_pci_driver.sh（v0.2 新建）
# 验证：sed 替换 #include 路径 + Linux 6.12 真机 stub 编译
KERNEL_SRC=${KERNEL_SRC:-/usr/src/linux-headers-$(uname -r)}
sed 's|#include "kernel/pcie/pcie_emu.h"|#include "pci_driver/pcie_emu.h"|g' \
    plugins/pci_driver/probe.cpp > /tmp/l2_test.cpp
g++ -c /tmp/l2_test.cpp -I$KERNEL_SRC/include -fsyntax-only

# 期望：编译通过（仅 #include 替换 + syntax check）
# 注意：5.5.1 仅验证 #include 路径替换可工作，
#       完整 L2 build（含所有 .cpp 真机编译）推迟到 Change-2
```

### D6.4: Gate 5.5.1-D 验证（v0.2 新增 — METIS-Q1.3 闭环）

```bash
# Change-1 全部 Wave 完成后，Oracle 重新审查
# Oracle 输出：实施 vs ADR-091 v0.2 + design.md 目标态偏差报告
# Oracle 决定：是否调整 ADR-091 / 4 象限架构 / 实施路线图 / Change-2
```

## D7: 风险与缓解

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|:---:|:---:|------|
| 1 | 迁移映射表错误 | 低 | 高 | ADR-091 v0.2 已 100% 实测验证 |
| 2 | struct module ABI 变更 → 5 个现有 plugin 重编译失败 | 中 | 中 | 修改每个 plugin.cpp 加 load_priority 字段 + designated initializer 兼容 |
| 3 | 测试引用 `iommu_internal.h` 内部头 | 高 | 低 | Stage 5.5.1 内改测试，引用 plugin 公共 API |
| 4 | L2 build 失败（#include 路径调整） | 中 | 中 | 提供 sed 脚本批量调整 #include |
| 5 | 循环依赖检测误报 | 低 | 中 | 测试覆盖各种 cycle 模式 |
| 6 | 现有 6 个 test_*_standalone API 兼容失败 | 低 | 高 | 公共头文件名不变 + 命名空间迁移通过 alias 兼容 |

---

**状态**: 🔄 Proposed v0.3（2026-09-03，Oracle v0.2 复审 INCONCLUSIVE 修复完成，待 v0.3 复审）

---

## 修订记录

- **v0.3** (2026-09-03)：Oracle v0.2 复审 INCONCLUSIVE 修复
  - D1.1 + D4.1 + D4.2：所有 `.c` → `.cpp`
  - D1.1 + D4.5：sample_memory/sample_serial 路径实测修正
  - D4.3：sim_hardware 顶级目录模型（INTERFACE 库占位）
  - D4.4：`add_subdirectory(sim_hardware)` 无 src/ 前缀
  - invalidate.cpp 暂留 Q2（不动 Q2/Q3 拆分）
- **v0.2** (2026-09-03)：Oracle + Metis 双审查 v0.1 INCONCLUSIVE 修复（仅 proposal.md）
- **v0.1** (2026-09-03)：初版
