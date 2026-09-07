# Tasks: sim-hardware-mock-test-completion

## 1. Platform 模块测试（新二进制）

- [x] 1.1 创建 `tests/sim_hardware/test_platform_standalone.cpp`：P1.1 load→get 反映 / P1.2 默认值（默认 topology_path 非空勿断言空）/ P1.3 三态 PlatformType / P1.4 覆盖写 / P1.5 长 topology_path round-trip（std::string 无截断）/ P1.6 双线程 thread_local 隔离——**观察线程**断言默认值，勿用主线程（P1.4 已污染，per design D2；线程内 atomic 计错）/ P1.7 显式 load enable_phy_digital=true 后 get 反映 true（per design D2+Metis 修订，characterization 立即绿）
- [x] 1.2 在 `tests/CMakeLists.txt` Stage 5.5.2 块注册 `sim_hardware/test_platform_standalone.cpp`（链接 `sim_hardware_mock`，与现有 4 个同款）

## 2. Bridge 测试扩展（E2.x）

- [x] 2.1 扩展 `tests/sim_hardware/test_cpptlm_bridge_mock_standalone.cpp`：E2.1 attach 存储成功 / E2.2 attach null → -EINVAL / E2.3 attach 未 init → -ENODEV / E2.4 重复 attach 替换 / E2.5 endpoint destroy 后 bridge mmio 惰性（characterization，per design D5）
- [x] 2.2 同文件：E2.6 mmio 并发 4R×4W×1000，writer i 写固定常量 VAL_i 于 offset i*8，线程内 atomic 计错、主线程断言 ∈{0,VAL_i}（per design D3 + 线程断言纪律）/ E2.7 config offset=4092 合法（最后合法 dword）/ E2.8 config offset=4093 → -EINVAL（首个拒绝偏移；可加测 4094）/ E2.9 destroy 后 `CpptlmBridge_get()` 为 nullptr

## 3. host_bridge/topology 测试扩展（HB.5-HB.8）

- [x] 3.1 扩展 `tests/sim_hardware/test_pcie_host_bridge_mock_standalone.cpp`：HB.5 三设备 topology 枚举（BDF 0x08/0x10/0x18，临时 fixture per design D4）/ HB.6 max_devices=2 截断（devices 数组先 memset 0xEE 哨兵，断言 out_count=3、仅 copy 2、devices[2] 哨兵未动）
- [x] 3.2 扩展 `tests/sim_hardware/test_topology_standalone.cpp`：HB.7 非法 default_mode "Invalid" → -EINVAL / HB.8 非法 drain policy → -EINVAL / HB.9 合法枚举全集中通过 + Topology 字段映射正确（characterization：loader 已有校验 topology.cpp:170-175）

## 4. 验证与收尾

- [x] 4.1 构建（`make -j4`）+ 运行 3 个受影响测试二进制全部通过（characterization 确认）
- [x] 4.2 全量回归 `ctest --output-on-failure` 零回归（98 → 新增后全绿）
- [x] 4.3 ASan 快速验证新二进制（E2.6 并发用例重点）：`SANITIZER=asan` 下新测试通过
- [x] 4.4 TSan 验证 E2.6 并发用例；若报警则按 design Risks 升级处理（预期不触发）
- [x] 4.5 勾选本 tasks.md 全部条目，记录验证证据（测试输出摘要）
