# sim-hardware-mock-test-coverage Delta

## ADDED Requirements

### Requirement: Platform 模块回归测试覆盖
`sim_hardware` 的 platform 模块（`platform.h`/`platform.cpp`）MUST 拥有独立测试二进制，覆盖 `platform_load`/`platform_get` 的 thread_local 持有语义、`PlatformType` 全部三态（kPcX86Mock/kAmdNavi/kNvidiaAda）、`PlatformConfig` 默认值与覆盖写行为。

#### Scenario: load 后 get 反映配置
- **WHEN** 以任一 `PlatformType` 调用 `platform_load(cfg)`
- **THEN** `platform_get()` 返回与 cfg 一致的 type 与 topology_path，返回码 0

#### Scenario: 三态 PlatformType 全部可加载
- **WHEN** 依次以 kPcX86Mock、kAmdNavi、kNvidiaAda 调用 `platform_load`
- **THEN** 每次返回 0 且 `platform_get().type` 等于最近一次加载值

#### Scenario: 未加载时使用默认值
- **WHEN** 线程从未调用 `platform_load` 即调用 `platform_get`
- **THEN** 返回 `PlatformConfig{}` 默认值（type=kPcX86Mock，enable_pcie_root_complex=true，enable_link_layer=true，enable_phy_digital=false）

#### Scenario: 后写覆盖前写
- **WHEN** 同一线程先后 `platform_load` 两个不同配置
- **THEN** `platform_get` 反映第二次配置

#### Scenario: 长 topology_path round-trip 保留
- **WHEN** 以含超长（>255 字符）及含空格的 topology_path 调用 `platform_load`（PlatformConfig.topology_path 为 std::string，无截断）
- **THEN** `platform_get().topology_path` 与输入逐字符一致

#### Scenario: 线程间隔离
- **WHEN** 两个工作线程各自 `platform_load` 不同的 `PlatformType` 并 join；随后另起一个**从未调用过 `platform_load` 的观察线程**断言默认值（不得复用主线程——主线程可能已被其他用例的 load 污染，thread_local 随主线程存活整个进程）
- **THEN** 两工作线程 `platform_get` 各自返回本线程加载值（线程内以 atomic 捕获、主线程断言，Catch2 断言禁止在 std::thread 内使用），观察线程 `platform_get` 返回 `PlatformConfig{}` 默认值（thread_local 语义）

### Requirement: CpptlmBridge attach_endpoint 覆盖
`CpptlmBridge::attach_endpoint` 公开 API MUST 被测试覆盖：null handle 拒绝、未初始化拒绝、合法存储、重复 attach 替换、endpoint destroy 后 bridge 操作不受影响（mock handle 惰性语义）。

#### Scenario: attach null handle
- **WHEN** 对已 init 的 bridge 调用 `attach_endpoint(nullptr)`
- **THEN** 返回 -EINVAL

#### Scenario: attach 在 init 之前
- **WHEN** 对未 init 的 bridge 调用 `attach_endpoint(合法 handle)`
- **THEN** 返回 -ENODEV

#### Scenario: 合法 attach 成功且重复 attach 不破坏 bridge
- **WHEN** 对已 init bridge 先后 attach 两个不同 mock endpoint（均 `pcie_endpoint_create` 合法创建；「替换语义」因 impl_->endpoint 私有且无 getter 黑盒不可观察，仅断言返回码）
- **THEN** 两次均返回 0，且后续 mmio roundtrip 仍正常

#### Scenario: endpoint destroy 后 bridge 惰性
- **WHEN** mock endpoint 经 `pcie_endpoint_destroy` 释放后，bridge 继续 `mmio_write`/`mmio_read` roundtrip
- **THEN** 操作返回 0 且数据正确（mock 后端不解引用 endpoint handle；此契约在 kCpptlm backend 引入解引用时必须显式升级）

### Requirement: CpptlmBridge 并发与边界覆盖
`CpptlmBridge` MUST 被测试覆盖：mmio 并发读写互斥安全、config space 边界（末 dword 偏移 4093 合法 / 4094 越界拒绝）、`destroy()` 清除 active singleton。

#### Scenario: mmio 并发读写安全
- **WHEN** 4 reader 线程与 4 writer 线程各执行 1000 次 mmio 操作，writer i 对 offset i*8（4 字节对齐）每轮写**同一常量** VAL_i（≠0，各 writer 互不相同、不与他线程 offset 重叠），**reader i 读 offset i*8（与 writer i 同 offset）**
- **THEN** 无崩溃、无撕裂读——线程内仅以 `std::atomic` 计错（ Catch2 断言禁止在 std::thread 内使用，异常会致 std::terminate），join 后主线程断言每个读值 ∈ {0, VAL_i}（初始零值或该 writer 常量，绝无混合值；前提：该 offset 在本 MockBridgeScope 内此前无其他写入——bars 仅在 init 时清零）

#### Scenario: config space 末 dword 合法
- **WHEN** `config_read(4092, &v)` / `config_write(4092, v)`（4092 为最后合法 dword 偏移，4092+4=4096 恰在边界内；per bridge.cpp:119/129 `offset > 4092` 拒绝逻辑与现有测试 :97 已断言 4093 拒绝）
- **THEN** 返回 0

#### Scenario: config space 越界拒绝
- **WHEN** `config_read(4093, &v)`、`config_write(4093, v)`（首个被拒绝偏移，4093+4 越过 4096 边界）以及 `config_read(4094, &v)`
- **THEN** 均返回 -EINVAL

#### Scenario: destroy 清除 active singleton
- **WHEN** bridge 经 `CpptlmBridge_set_active(&b)` 设为 active 后调用 `b.destroy()`
- **THEN** `CpptlmBridge_get()` 返回 nullptr

### Requirement: host_bridge 多设备枚举与截断覆盖
`host_bridge_enumerate` MUST 被测试覆盖多设备场景：N=3 topology 全量枚举（packed BDF 0x08/0x10/0x18，per topology.cpp pack_bdf 约定）、`max_devices` 小于实际数量时返回实际总数且仅复制 max 个。

#### Scenario: 三设备 topology 全量枚举
- **WHEN** 以含 3 台设备（BDF 0000:01/02/03:00.0）的临时 topology JSON 调用 `host_bridge_enumerate`
- **THEN** 返回 0、out_count=3、三个 packed BDF 依次为 0x08/0x10/0x18，vendor/device id 与 fixture 一致

#### Scenario: max_devices 截断
- **WHEN** 同一 3 设备 topology 以 `max_devices=2` 枚举，且调用前将 devices 数组整体预填充哨兵值（如 0xEE）
- **THEN** 返回 0、`out_count=3`（实际总数而非复制数）、devices[0..1] 已填充、devices[2] 哨兵值保持原样（证明未被写入）

### Requirement: topology 枚举值校验测试固定
`topology_load_json` 对 `bypass_mux.default_mode` 与 `default_drain_policy` 的枚举值校验（实现已存在于 topology.cpp:170-175）MUST 被 characterization 测试固定：非法值返回 -EINVAL，合法值全集中通过。

#### Scenario: 非法 default_mode 拒绝
- **WHEN** topology JSON 的 `bypass_mux.default_mode` 为 "Invalid"（合法集 {Full,Bypass,Partial} 之外）
- **THEN** `topology_load_json` 返回 -EINVAL

#### Scenario: 非法 drain policy 拒绝
- **WHEN** topology JSON 的 `bypass_mux.default_drain_policy` 为 "Invalid"（合法集 {GracefulDrain,ImmediateAbort} 之外）
- **THEN** `topology_load_json` 返回 -EINVAL

#### Scenario: 合法枚举值全集中通过
- **WHEN** default_mode ∈ {Full,Bypass,Partial} 且 default_drain_policy ∈ {GracefulDrain,ImmediateAbort} 分别加载
- **THEN** 均返回 0 且 Topology 字段映射正确
