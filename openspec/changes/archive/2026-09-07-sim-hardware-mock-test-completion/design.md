# Design: sim-hardware-mock-test-completion

## Context

Stage 5.5.2 交付了 sim_hardware mock 后端（`sim_hardware/`，ADR-091 v0.2 Q3 象限）与 4 个 standalone 测试 + 1 个跨边界集成测试（`test_gpu_sim_hardware_bridge_standalone.cpp`，REQ-BRIDGE-TEST-001）。对照解读 B 测试基线（`plugins/gpu_driver/sim/hardware/` 路径，含并发先例 `bypass: concurrent get_mode`、perf 先例 `test_bar_ioremap_perf.cpp`）发现三个缺口：platform 模块 0 测试、bridge `attach_endpoint`/并发 0 测试、host_bridge 多设备 0 测试。

实施前调查修正：topology loader **已实现** bypass_mux 枚举值校验（`topology.cpp:170-175`），相关用例定性为 characterization。mock 后端按设计自包含（`sim_hardware/CMakeLists.txt:43`），**本 change 无 CppTLM 端实施需求**。

## Goals / Non-Goals

**Goals:**
- platform 模块获得回归测试（thread_local 语义 + 三态 PlatformType + 默认值 + 覆盖写）
- bridge 公开 API（attach_endpoint / config 边界 / 并发 / destroy 单例清理）获得测试固定
- host_bridge 多设备枚举 + max_devices 截断 + topology 枚举值校验获得测试固定
- 全部用例立即绿（characterization），CI 零红灯

**Non-Goals:**
- 不改 `sim_hardware/src/**` 产品代码
- 不实施 kCpptlm 真实 backend（属 Stage 5.5.3 / Change-3）
- 不在 CppTLM 仓库做任何改动（ABI 已存在，见 Decision 6）
- 不做 perf 基线测试（P3 项，独立后续提案）
- 不测试 `pcie_emu.h`（`include/kernel/pcie/`，属另一 API 层，与 sim_hardware mock 无关）

## Decisions

### D1: 全部 characterization，无 TDD 红灯
本 change 补的是**既有实现的覆盖缺口**，不是新功能。若引入红灯（先写失败测试再改实现），会违反"测试补全不动产品代码"的提案边界，且阻塞 CI。备选方案——对 attach_endpoint 等做 TDD 式实现改进——被否：范围膨胀，属 Change-3。
影响：tasks.md 不含"verify fail"步骤，改为"run → confirm pass（characterization）"。

### D2: platform 双线程 isolation 测试
`platform.cpp` 用 `thread_local PlatformConfig g_cfg`——**thread_local 随线程存活整个进程**，主线程会跨 TEST_CASE 持久（Metis 审查修正：P1.4 覆盖写在主线程 load 后，P1.6 不能再用"主线程从未加载"断言）。测试结构：
- 两个 `std::thread` 各自 `platform_load` 不同 `PlatformType`，线程内以 `std::atomic` 捕获本线程 `platform_get` 结果（**Catch2 断言禁止在 std::thread 内**，失败异常逃逸致 `std::terminate`），join 后主线程断言；
- **另起一个从未 load 的观察线程**断言 `platform_get` 返回默认值——这才是 thread_local 语义的回归哨兵（若实现被误改成全局 static，worker 互染 + 观察线程非默认，双重抓红）。
理由：误改 thread_local→static 正是 Change-3 最可能的回归点。

### D3: 并发测试风格对齐解读 B
解读 B 先例：`bypass: concurrent get_mode is safe (4 readers x 1000 reads)`。bridge mmio 并发扩展为 **4 reader × 4 writer × 各 1000 iters**：
- **offset 分配**：writer i / reader i 共用 offset `i*8`（len=4，天然 4 字节对齐，满足 `valid_mmio` 对齐检查），各线程不重叠。
- **写值策略（关键）**：writer i 每轮写**同一常量** `VAL_i`（≠0 且各 writer 互不相同）。这是"读值 ∈ {旧值, 新值}"断言可表达的前提——初始 bars 全零（init 清零），故 reader 读 offset i*8 的合法值集合恰为 `{0, VAL_i}`。若写递增值则该断言必错。
- **线程断言纪律**：本仓库 Catch2 下 std::thread 内 REQUIRE/CHECK 失败会异常逃逸导致 `std::terminate`（进程崩溃而非干净红灯）。必须照现有先例（`test_pcie_bypass_mock_standalone.cpp:177-195`）：线程内仅以 `std::atomic<int>` 计错，join 后主线程 `REQUIRE(cnt == 0)`。P1.6 platform 双线程测试同此纪律。
- 理由：共享 offset 会制造假数据竞争；独占 offset + 常量值只测 mutex 互斥与内存可见性，正是实现承诺（`impl_->mutex`）。torn read 实际不可能发生（memcpy 全程在 mutex 下），本用例本质是 mutex 互斥 characterization——可接受。

### D4: 多设备 topology 用测试内临时 fixture
`write_temp()` 生成 3 设备 topology（BDF 0x08/0x10/0x18，非标准 pack 约定：bus 从 chars[0:4] 读，per `topology.cpp:27-49` + REQ-BRIDGE-TEST-001 注释）。不复用/修改 `default_topology.json`（共享 fixture，改它会级联影响 T2.1/T2.2 与 bridge 集成测试）。截断用例：3 设备 + `max_devices=2` → `out_count=3`（返回实际总数）+ 仅 copy 2 个（per `host_bridge.cpp:63-80` 与 design §5.3）。

### D5: endpoint destroy 惰性语义（characterization）
mock 中 `PcieEndpointIP` 的 4 个 opaque handle 自引用且**从不被解引用**（`bridge.cpp` 全文验证：`impl_->endpoint` 仅存储）。测试固定：destroy 后 bridge mmio/config 仍正常工作。UAF 检测交给既有 sanitizer 构建（ASan/UBSan，`build.sh`），不在单测内做崩溃测试。此测试是"惰性契约"的哨兵——未来 kCpptlm backend 引入解引用时，该 characterization 需随变更同步升级。

### D6: CppTLM 端零改动 + ABI 映射表（记录性）
CppTLM 已有 C ABI（`include/abi/cpptlm_emulator.h`，19 函数，`v1.0-dgpu-v0`）。映射关系供 Change-3/Stage 5.5.3 参考，**本 change 不执行绑定**：

| UsrLinuxEmu（CpptlmBridge） | CppTLM C ABI | 备注 |
|---|---|---|
| `init(params)` | `cpptlm_emulator_create(profile_path)` | `topology_path` ↔ `profile_path` 概念映射，Change-3 决策 |
| `mmio_read/write` | `cpptlm_emulator_mmio_read/write` | 签名一致 |
| `config_read/write` | `cpptlm_emulator_pcie_config_read/write` | CppTLM 带 width 参数 |
| `register_msix_callback` | `register_callbacks(intr_cb,...)` + `msix_init` | CppTLM 回调带 trans_id，UsrLinuxEmu 无 |
| `inject_msix_for_test` | （无对应） | mock 专用，保留 UsrLinuxEmu 侧 |
| `host_bridge_enumerate` | （无对应） | 枚举是 UsrLinuxEmu topology loader 职责，CppTLM 单设备 emulator，无缺口 |

结论：**无需 CppTLM 端提案**（Metis 审查确认，采用"跳过 + 文档化"立场）。**显式延后条款**：CppTLM 侧 ABI 契约测试（19 函数当前零测试覆盖）触发于 Change-3 规划期，在 CppTLM 仓单独提 change；「ABI 契约测试覆盖」列入 Change-3 入口条件——ABI 冻结于 v1.0-dgpu-v0，绑定前无消费者，此刻跨仓测试契约无调用方、双重维护且违反本 change 单仓边界。若 Change-3 发现 ABI 缺口（如 bypass mode 下发），届时在 CppTLM 仓库单独提 change。

## Risks / Trade-offs

- [characterization 测试固化了占位实现行为（platform thread_local）] → Change-3 若改为真实平台注册表，需同步改测试；risk 接受，因测试本身就是回归哨兵。
- [并发测试在 TSan 下可能暴露现有 mutex 覆盖不足] → 实施后立即跑 `SANITIZER=tsan`；若 TSan 报警则属真 bug，升级为 fix（超出本 change 则另立 change 记录）。
- [bridge 测试扩展后单二进制用例数翻倍，测试间全局状态（bypass in_flight、active bridge）耦合] → 所有新用例沿用 `MockBridgeScope` RAII；mmio 经 TlpGuard 对称 enter/exit，不残留计数。
- [多设备 fixture BDF pack 非标准约定易写错] → 断言值直接引用 `topology.cpp:27-49` pack 逻辑推算（0x08/0x10/0x18），并在测试注释中写明推导，与 REQ-BRIDGE-TEST-001 注释风格一致。
- [HB.6 "devices[2] 未被写入" 无哨兵则断言无意义（未初始化内存）] → 调用前将 devices 数组整体预填充哨兵（如 0xEE），枚举后断言 devices[2] 哨兵保持。
- [E2.9 执行后 g_active_bridge == nullptr] → 良性：后续测试依赖 MockBridgeScope 构造函数强制 set_active 恢复；新用例追加到现有文件末尾（Catch2 默认按定义顺序执行），无跨测试污染。

### 实施纪律（Metis 审查 AI 失败点清单，实施者必读）
- **include 路径**：`sim_hardware/include` 已在 include path（tests/CMakeLists.txt:50-54），写 `#include "platform.h"` / `"topology.h"`（对照现有 `"cpptlm/bridge.h"`），勿写 `"sim_hardware/include/platform.h"`。
- **CMake 注册**：向 `CATCH2_TESTS` 列表 Stage 5.5.2 块（:224-228）**追加一行**，勿新建 add_executable 块；target 由现有 foreach 统一创建并链 `sim_hardware_mock`。
- **Catch2 tag**：沿用双 tag 约定 `[stage_5_5_2][<subtopic>]`（platform / bridge_* / host_bridge / topology）。
- **E2.3 顺序陷阱**：`bridge.cpp:145` 先查 handle 再查 initialized——传 nullptr 到未 init bridge 得 -EINVAL 而非 -ENODEV；E2.3 必须用**合法 handle** 才命中 -ENODEV 路径。
- **E2.6 初值前提**：bars 仅在 init 清零（bridge.cpp:75），`{0, VAL_i}` 断言仅在单个 MockBridgeScope 生命周期内且 offset 无历史写入时成立。
- **HB.6 哨兵断言**：**逐字段**断言（bdf/vendor_id/device_id/class_code == 0xEEEE…），勿对整个 `DiscoveredDevice` 做 memcmp——结构体 padding 字节不确定，会 flaky。
- **P1.2 默认 path 非空**：`platform.h:17` 默认 topology_path 是 `"sim_hardware/topology/default_topology.json"`，勿断言其为空。

## Migration Plan

纯测试新增：构建 → ctest 全绿即完成；无部署/回滚 concerns。回滚 = revert 测试文件 + CMakeLists 行。

## Open Questions

（无——P1.6 线程语义已由 `platform.cpp:8-9` 实现确认；HB.7/HB.8 已确认 loader 有校验，characterization 定性。）
