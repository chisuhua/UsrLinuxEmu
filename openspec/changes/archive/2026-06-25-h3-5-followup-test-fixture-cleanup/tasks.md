# Tasks: H-3.5 TaskRunner test-fixture 范畴清理

> **依赖**: proposal ✅, design ✅, specs ✅
> **预估总工时**: 1-2 周（Phase A+B+C+D）
> **前置条件**: H-3 phase2-management 已 archived 2026-06-22, H-5 taskrunner-scope-clarification 已 archived 2026-06-24
> **后续约束**: H-3.5 shippable 后可启动 umd-evolution PoC（独立 change #2）

## Phase A：先写失败测试（TDD，1-2 天）

### A.1 更新 test_gpu_architecture 回归测试

- [ ] A.1.1 阅读 `tests/test_fixture/test_gpu_architecture.cpp:207-214` 当前测试内容
- [ ] A.1.2 改名：`TEST_CASE("H-2.5 Bonus: GpuDriverClient H-3 placeholders throw")` → `TEST_CASE("H-3.5: GpuDriverClient guards return 0/-1, no throw")`
- [ ] A.1.3 改写测试体（5 个 CHECK 改为 `CHECK_NOTHROW` + 验证返回值 0/-1）
- [ ] A.1.4 验证：`cmake --build build && ./build/test_gpu_architecture` 应 11/11 PASS（之前 10/11 FAIL）

### A.2 新增 CudaScheduler 抽象泄漏测试

- [ ] A.2.1 新建 `tests/test_fixture/test_cuda_scheduler_universal.cpp`（基于 test_cuda_scheduler.cpp 模板）
- [ ] A.2.2 把 `CudaStub` 替换为 `MockGpuDriver`（DI 注入测试）
- [ ] A.2.3 复制 8 个原测试 case 但用 `MockGpuDriver` 而非 `CudaStub`
- [ ] A.2.4 验证：`cmake --build build && ./build/test_cuda_scheduler_universal` 预期 0/8 FAIL（因 dynamic_cast 失败）

### A.3 更新 MockGpuDriver guard 偏差测试（T6-T9）

- [ ] A.3.1 阅读 `tests/test_fixture/test_gpu_phase2.cpp:115-195` T6-T9 当前内容
- [ ] A.3.2 删除注释：`// ⚠️ Mock-behavior deviation T6-T9: ...`
- [ ] A.3.3 改写测试断言：从 `last_call().empty()` 改为 `result == 0 || result == -1`
- [ ] A.3.4 验证：`./build/test_gpu_phase2` 预期 T6-T9 FAIL（4 cases 失败）

### A.10 验证 Phase A

- [ ] A.10.1 `cmake .. && make -j4` 编译通过
- [ ] A.10.2 3 个测试套件运行状态：
  - test_cuda_scheduler 8/8 PASS（保持）
  - test_gpu_phase2 8/12 PASS（T6-T9 FAIL）
  - test_gpu_architecture 11/11 PASS（更新后）

---

## Phase B：实施修复（1 周）

### B.1 IGpuDriver 接口扩展 3 个新方法

- [ ] B.1.1 阅读 `include/shared/igpu_driver.hpp` 当前 28 个方法定义
- [ ] B.1.2 添加 `virtual void set_stub_mode(bool stub_mode) {}`（默认 no-op）
- [ ] B.1.3 添加 `virtual int initialize() { return 0; }`（默认 success）
- [ ] B.1.4 添加 `virtual void shutdown() {}`（默认 no-op）
- [ ] B.1.5 验证：`cmake .. && make -j4` 编译通过（IGpuDriver 接口扩展不应破坏既有 28 方法的 override）

### B.2 CudaStub override 3 个新方法

- [ ] B.2.1 阅读 `include/test_fixture/cuda_stub.hpp` 当前 `set_stub_mode` / `initialize` / `shutdown` 声明
- [ ] B.2.2 添加 `override` 关键字
- [ ] B.2.3 验证 `src/test_fixture/cuda_stub.cpp` 实现无修改（已存在同名方法）

### B.3 GpuDriverClient override 3 个新方法

- [ ] B.3.1 阅读 `include/test_fixture/gpu_driver_client.h` 当前类
- [ ] B.3.2 添加 `void set_stub_mode(bool) override {}`（no-op）
- [ ] B.3.3 添加 `int initialize() override`：维护 `is_initialized_` flag + 返回 0
- [ ] B.3.4 添加 `void shutdown() override`：清除 `is_initialized_` flag
- [ ] B.3.5 验证：`cmake .. && make -j4` 编译通过

### B.4 MockGpuDriver override 3 个新方法

- [ ] B.4.1 阅读 `tests/test_fixture/mock_gpu_driver.hpp` 当前类
- [ ] B.4.2 添加 `void set_stub_mode(bool mode) override { stub_mode_ = mode; }`
- [ ] B.4.3 添加 `int initialize() override { initialized_ = true; return 0; }`
- [ ] B.4.4 添加 `void shutdown() override { initialized_ = false; }`
- [ ] B.4.5 添加成员变量 `bool stub_mode_` + `bool initialized_`

### B.5 MockGpuDriver 5 个 Phase 2 guard 添加

- [ ] B.5.1 修改 `create_va_space(uint32_t)`：添加 `if (va_space_handle == 0) return 0;`（与 GpuDriverClient 一致）
- [ ] B.5.2 修改 `create_queue(uint64_t, ...)`：添加 `if (va_space_handle == 0) return 0;`
- [ ] B.5.3 修改 `register_gpu(uint64_t, ...)`：添加 `if (va_space_handle == 0) return -1;`
- [ ] B.5.4 修改 `destroy_va_space(uint64_t)`：添加 `if (handle == 0) return -1;`
- [ ] B.5.5 修改 `destroy_queue(uint64_t)`：添加 `if (handle == 0) return -1;`

### B.6 CudaScheduler 重构 — 删除 6 处 dynamic_cast

- [ ] B.6.1 阅读 `src/test_fixture/cuda_scheduler.cpp:45`（initialize）
- [ ] B.6.2 替换：`if (auto* stub = dynamic_cast<...>) { ... }` → `driver_->set_stub_mode(stub_mode); int result = driver_->initialize();`
- [ ] B.6.3 阅读 `src/test_fixture/cuda_scheduler.cpp:65`（shutdown）
- [ ] B.6.4 替换：`if (auto* stub = dynamic_cast<...>) { stub->shutdown(); }` → `driver_->shutdown();`
- [ ] B.6.5 阅读 `src/test_fixture/cuda_scheduler.cpp:101`（submit_mem_alloc）
- [ ] B.6.6 替换：`dynamic_cast<...>->mem_alloc(...)` → 通过 IGpuDriver 抽象调用（待设计：是否新增 legacy 接口？）
- [ ] B.6.7 阅读 `src/test_fixture/cuda_scheduler.cpp:147, 188, 227, 269`（submit_memcpy_* + submit_launch）
- [ ] B.6.8 同 B.6.6 处理
- [ ] B.6.9 验证：`cmake .. && make -j4` 编译通过

### B.7 CudaScheduler 依赖 IGpuDriver 抽象扩展（如果需要）

- [ ] B.7.1 如果 B.6.6 需要 `submit_mem_alloc` 等 4 个 legacy 方法暴露到 IGpuDriver：
  - 添加 `virtual int submit_mem_alloc(size_t size, void** out_ptr) = 0;`
  - 添加 `virtual int submit_memcpy_h2d(...)` 等
  - CudaStub / GpuDriverClient / MockGpuDriver 各自 override
- [ ] B.7.2 验证：`cmake .. && make -j4` 编译通过 + 所有测试 PASS

### B.10 验证 Phase B

- [ ] B.10.1 `cmake .. && make -j4` 编译通过
- [ ] B.10.2 所有测试 PASS：
  - test_cuda_scheduler 8/8 PASS（保持）
  - test_cuda_scheduler_universal 8/8 PASS（新增，因 MockGpuDriver 可注入）
  - test_gpu_phase2 12/12 PASS（T6-T9 现在验证 guard）
  - test_gpu_architecture 11/11 PASS
- [ ] B.10.3 验证 `cuda_scheduler.cpp` 0 处 `dynamic_cast<CudaStub*>`（`grep -c "dynamic_cast<CudaStub"` 应输出 0）

---

## Phase C：TADR + 文档同步（1 天）

### C.1 TADR-103 更新

- [ ] C.1.1 阅读 `docs/test-fixture/adr/tadr-103-h3-phase2.md` §H-3.5 Follow-up 段
- [ ] C.1.2 删除整个警示段（含 ⚠️ Mock-behavior deviation T6-T9）
- [ ] C.1.3 添加 §H-3.5 Completion 段（含 commit 链 + 验证测试套件 PASS）

### C.2 新增 TADR-109

- [ ] C.2.1 创建 `docs/test-fixture/adr/tadr-109-igpu-driver-uniform-scheduling.md`
- [ ] C.2.2 跟踪 Decision 1（IGpuDriver 接口扩展 3 个新方法）
- [ ] C.2.3 跟踪 Decision 2（MockGpuDriver guard 一致性）
- [ ] C.2.4 引用本 change `openspec/changes/2026-06-25-h3-5-followup-test-fixture-cleanup/`

### C.10 验证 Phase C

- [ ] C.10.1 `git log --follow docs/test-fixture/adr/tadr-103-h3-phase2.md` 可追历史
- [ ] C.10.2 TADR-109 内容完整，含 SCOPE: TEST-FIXTURE 元数据

---

## Phase D：跨仓同步（1 天）

### D.1 本地质量检查

- [ ] D.1.1 `cmake .. && make -j4` 编译通过（test-fixture 模式）
- [ ] D.1.2 `cmake .. -DTASKRUNNER_BUILD_MODE=umd-evolution && make -j4` 编译通过（UMD 模式）
- [ ] D.1.3 所有 doctest 测试 PASS：
  - test_cuda_scheduler 8/8
  - test_cuda_scheduler_universal 8/8
  - test_gpu_phase2 12/12
  - test_gpu_architecture 11/11
  - test_umd_skeleton 3/3
- [ ] D.1.4 `bash tools/docs-audit.sh --strict` 输出 36/36 PASS（如果文档受影响）
- [ ] D.1.5 `grep -c "dynamic_cast<CudaStub" src/test_fixture/cuda_scheduler.cpp` 输出 0

### D.2 TaskRunner 端 commit + push

- [ ] D.2.1 `cd /workspace/project/UsrLinuxEmu/external/TaskRunner`
- [ ] D.2.2 `git status -s` 确认所有改动已 staged
- [ ] D.2.3 `git add -A`
- [ ] D.2.4 `git commit -m "fix(cuda-scheduler): H-3.5 follow-up - remove 6 dynamic_cast<CudaStub*>, IGpuDriver 31 methods, MockGpuDriver guards"`
- [ ] D.2.5 `git log -1` 验证 commit 内容
- [ ] D.2.6 `git push origin main`

### D.3 UsrLinuxEmu 端 submodule pointer bump

- [ ] D.3.1 `cd /workspace/project/UsrLinuxEmu`
- [ ] D.3.2 `git status -s external/TaskRunner` 确认有新指针
- [ ] D.3.3 `git add external/TaskRunner`
- [ ] D.3.4 `git commit -m "chore(submodule): bump TaskRunner to H-3.5 (CudaScheduler uniform scheduling)"`

### D.4 UsrLinuxEmu 端 docs/00_adr/README.md TADR mirror 更新

- [ ] D.4.1 找到 TaskRunner TADR mirror 段（应在文件末尾）
- [ ] D.4.2 添加 `tadr-109` 行的镜像
- [ ] D.4.3 `git add docs/00_adr/README.md`
- [ ] D.4.4 `git commit -m "docs(adr): update TaskRunner TADR mirror (tadr-109 added for H-3.5)"`

### D.5 UsrLinuxEmu 端 openspec archive

- [ ] D.5.1 `cd /workspace/project/UsrLinuxEmu`
- [ ] D.5.2 `openspec list` 验证 change 在 active 列表
- [ ] D.5.3 `openspec archive h3-5-followup-test-fixture-cleanup -y`
- [ ] D.5.4 验证：`openspec/changes/2026-06-25-h3-5-followup-test-fixture-cleanup/` 已移至 `archive/`
- [ ] D.5.5 `git status -s` 显示 archive 操作产生新文件
- [ ] D.5.6 `git add openspec/changes/archive/`
- [ ] D.5.7 `git commit -m "chore(openspec): archive h3-5-followup-test-fixture-cleanup"`

### D.6 UsrLinuxEmu 端 push + 跨仓 PR

- [ ] D.6.1 `git log --oneline -5` 验证 commit 链顺序正确（submodule pointer → docs/adr → archive）
- [ ] D.6.2 `git push origin main`
- [ ] D.6.3 创建跨仓 PR（如适用，UsrLinuxEmu 端流程）

### D.10 验证 Phase D

- [ ] D.10.1 `openspec list` 显示 "No active changes"
- [ ] D.10.2 `git log --oneline -5` 显示完整 commit 链
- [ ] D.10.3 跨仓 PR 通过所有 CI 检查

---

## 总验证清单（所有 Phase 完成后）

- [ ] V.1 `cmake .. && make -j4` 编译通过（默认 test-fixture 模式）
- [ ] V.2 `cmake .. -DTASKRUNNER_BUILD_MODE=umd-evolution && make -j4` 编译通过
- [ ] V.3 test_cuda_scheduler 8/8 PASS
- [ ] V.4 test_cuda_scheduler_universal 8/8 PASS（新增）
- [ ] V.5 test_gpu_phase2 12/12 PASS（T6-T9 验证 guard rejection）
- [ ] V.6 test_gpu_architecture 11/11 PASS
- [ ] V.7 test_umd_skeleton 3/3 PASS
- [ ] V.8 `cuda_scheduler.cpp` 中 `dynamic_cast<CudaStub*>` 数量为 0
- [ ] V.9 IGpuDriver 接口共 31 个虚方法（28 已有 + 3 新增）
- [ ] V.10 `git log --follow docs/test-fixture/adr/tadr-103-h3-phase2.md` 可追历史
- [ ] V.11 `bash tools/docs-audit.sh --strict` 输出 36/36 PASS
- [ ] V.12 `openspec list` 显示 "No active changes"
- [ ] V.13 UsrLinuxEmu 端 `git log --oneline -5` 显示完整 commit 链

## 工时估算

| Phase | 主要任务 | 预估工时 |
|-------|---------|---------:|
| Phase A | TDD 失败测试 | 1-2 天 |
| Phase B | 实施修复（接口扩展 + 6 处 dynamic_cast 重构 + MockGpuDriver guard）| 1 周 |
| Phase C | TADR + 文档同步 | 1 天 |
| Phase D | 跨仓同步 | 1 天 |
| **总计** | | **2-3 周** |