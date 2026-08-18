# ADR-090 实施任务清单

## Phase 1: 文档 SSOT 同步

- [x] 1.1 创建 ADR-090 草案 (PTXIR via H2D DMA, Supersedes ADR-076 v2)
- [x] 1.2 ADR-076 添加 🚫 Superseded 头注(指向 ADR-090)
- [x] 1.3 ADR-088 §C2 修订注记(取消"不被取代"条款)
- [x] 1.4 ADR-088 §D6.2 修订注记(SM executor +2~3 ABI 走 BREAKING 流程)
- [x] 1.5 `docs/00_adr/README.md` 索引更新(ADR-090 entry + ADR-076 状态 + 状态分布)
- [x] 1.6 `docs/README.md` ADR 计数 72→73 + ADR 编号范围同步

## Phase 2: 共享头文件 ABI 修订

- [x] 2.1 `gpu_ioctl.h` - `gpu_load_kernel_module_args` 重定义(移除 `kernel_name[256]`,新增 `out_vram_addr`)
- [x] 2.2 `gpu_ioctl.h` - `gpu_launch_kernel_module_args` 字段保留(ABI 卫生),handler -ENOSYS
- [x] 2.3 `gpu_ioctl.h` - `gpu_unload_kernel_module_args` 语义重定义(module_handle = vram_addr)
- [x] 2.4 `gpu_hal.h` - #66 fn-ptr 注释修正(H2D DMA + icache invalidate Mode A/B)
- [x] 2.5 `gpu_hal.h` - #67/#68 标 deprecated stub(struct 槽位保留 per ADR-023 §D4)

## Phase 3: 实现层修订

- [x] 3.1 `hal_user.cpp` - 移除 dlsym `libptxemu_device.so` (~150 行)
- [x] 3.2 `hal_user.cpp` - `user_kernel_module_load` 重写为 H2D DMA (user_mem_alloc + user_mem_write)
- [x] 3.3 `hal_user.cpp` - `user_kernel_module_execute/unload` 返回 -ENOSYS (deprecated stub)
- [x] 3.4 `hal_mock.cpp` - `kernel_module_load` mock 重写为 VRAM-write
- [x] 3.5 `hal_mock.cpp` - `kernel_module_execute/unload` 返回 -ENOSYS
- [x] 3.6 `hal_mock.cpp` - `hal_mock_inject_ptxemu_*` 4 个 helper 标 deprecated

## Phase 4: 驱动层修订

- [x] 4.1 `gpgpu_device.cpp` - `handleLoadKernelModule` 简化(移除 kernel_name 处理)
- [x] 4.2 `gpgpu_device.cpp` - `handleLaunchKernelModule` 转发 HAL deprecated stub
- [x] 4.3 `gpgpu_device.cpp` - `handleUnloadKernelModule` 转发 HAL deprecated stub

## Phase 4b: 测试更新

- [x] 4b.1 删除 6 个 ADR-076 PTX-EMU 错误注入 SECTIONs(迁出至 PTX-EMU/CppTLM 仓)
- [x] 4b.2 新增 5 个 ADR-090 SECTIONs(image_size 边界 / out_vram_addr / 0x28 stub / 0x29 stub / 并发 race)

## Phase 5: 构建验证

- [x] 5.1 cmake --build 全目标编译通过
- [x] 5.2 ctest -j8: 148/148 PASS, 0 failed
- [x] 5.3 docs-audit: 68/68 PASS(非 strict 模式)

## Phase 6: 跨仓协调文档

- [x] 6.1 创建 `docs/05-advanced/adr-090-cross-repo-coordination.md`
- [x] 6.2 PTX-EMU §D8 amendment spec (annex §A)
- [x] 6.3 TaskRunner tadr-308 spec (annex §B)
- [x] 6.4 CppTLM handoff spec v5.0 spec (annex §C)
- [x] 6.5 跨仓 commit 顺序 + 时间线 (annex §D)

## Phase 7: Oracle 复审

- [x] 7.1 Oracle 首次评审(Gate #6 PASS, 5 minor follow-ups) — session `ses_ff1f38c57ffevzUfohqp45av02`
- [x] 7.2 Follow-up #1: gpu_hal.h #66 icache invalidate Mode A/B 注释
- [x] 7.3 Follow-up #2: ADR-090 §Consequences 0x27 encoded value + §C3 Mode A 挂接说明
- [x] 7.4 Follow-up #4: docs/README.md ADR 计数 72→73
- [x] 7.5 Follow-up #5: ADR-090 §Migration M9 (deprecated hal_mock helper tracking)
- [x] 7.6 Cleanup: docs/README.md stale lines 删除
- [x] 7.7 Oracle 最终复审(Gate #6 PASS, APPROVED) — session `ses_ff1ecf07cffe5D3lwDqOmgFdyx`

## Phase 8: Cross-Repo Ack(待启动)

- [ ] 8.1 发送 cross-repo annex 到 PTX-EMU Architecture Team(Gate #3)
- [ ] 8.2 发送 cross-repo annex 到 TaskRunner owner(Gate #4)
- [ ] 8.3 发送 cross-repo annex 到 CppTLM maintainer(Gate #2)
- [ ] 8.4 收集三方 ack 反馈
- [ ] 8.5 ADR-090 状态从 🔄 Proposed 升 ✅ Accepted(待 6/6 Gate 全部 ✅)

## Phase M8: Mode B 切换(ADR-088 Phase 4 窗口)

- [ ] M8.1 CppTLM 完成 SM executor submodule 集成(Owner: CppTLM maintainer)
- [ ] M8.2 23+2~3 ABI ship(走 ADR-088 §D6.2 BREAKING 流程)
- [ ] M8.3 Mode A 路径保留为回退(Gate 4.7 双模式验证)
- [ ] M8.4 Mode B 切换测试(双模式同一测试集 PASS)

## Phase M9: 清理 deprecated helpers(后续 release)

- [ ] M9.1 移除 `hal_mock_inject_ptxemu_error(handle, cuda_error)`
- [ ] M9.2 移除 `hal_mock_clear_ptxemu_errors()`
- [ ] M9.3 移除 `hal_mock_get_ptxemu_unload_call_count(handle)`
- [ ] M9.4 移除 `hal_mock_reset_ptxemu_unload_counter(handle)`
- [ ] M9.5 跟踪 issue `#adr-090-cleanup-hal-mock-injection-helpers`

---

**总进度**: Phase 1-7 ✅ Complete / Phase 8 ⏳ 待跨仓 ack / Phase M8 ⏳ 待 ADR-088 Phase 4 / Phase M9 ⏳ 下一 release