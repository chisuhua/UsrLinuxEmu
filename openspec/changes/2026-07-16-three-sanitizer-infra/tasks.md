# Tasks: three-sanitizer-infra

> **状态**: 📋 PROPOSED（2026-07-16）
> **目标**: 补齐 ASan + UBSan CMake infra；修复暴露的真 bug
> **依赖**: C-12 stage1-4-kfd-multi-file-integration（已归档）
> **工期**: 3-5 天

---

## Phase A: CMake Infra (1 天)

- [ ] A.1 `CMakeLists.txt` 顶层添加 `ENABLE_ASAN` option + compile/link flags
- [ ] A.2 `CMakeLists.txt` 顶层添加 `ENABLE_UBSAN` option + compile/link flags
- [ ] A.3 `tests/CMakeLists.txt` 添加 `ENABLE_ASAN` / `ENABLE_UBSAN` 处理（与 TSan 模式对称）
- [ ] A.4 验证 `ENABLE_TSAN=ON` 仍正常 build（无 regression）
- [ ] A.5 验证 `ENABLE_ASAN=ON` 独立 build 成功（`make -j4` 0 errors）
- [ ] A.6 验证 `ENABLE_UBSAN=ON` 独立 build 成功
- [ ] A.7 验证 `ENABLE_ASAN=ON ENABLE_UBSAN=ON` 共 build 成功

## Phase B: ASan Run + Bug Fixes (1-2 天)

- [ ] B.1 `cmake -DENABLE_ASAN=ON` 重 build
- [ ] B.2 跑全 ctest，记录 ASan errors
- [ ] B.3 修复 `kfd_process.c` 双 free（如果 ASan 报告）
- [ ] B.4 修复 `kfd_sim_bridge.cpp` buffer overflow（如果报告）
- [ ] B.5 修复 `sim_page_migration.cpp` heap overflow（如果报告）
- [ ] B.6 修复 `sim_event.c` use-after-free on cleanup（如果报告）
- [ ] B.7 修复 `gpu_sim` 其他模块的 ASan errors（如 mem_pool.cpp / page_fault_handler.cpp）
- [ ] B.8 验证 ASan 0 errors

## Phase C: UBSan Run + Bug Fixes (1-2 天)

- [ ] C.1 `cmake -DENABLE_UBSAN=ON` 重 build
- [ ] C.2 跑全 ctest，记录 UBSan errors
- [ ] C.3 修复对齐违规（alignment）
- [ ] C.4 修复移位越界（shift out of range）
- [ ] C.5 修复有符号溢出（signed integer overflow）
- [ ] C.6 修复 null pointer 解引用（if any）
- [ ] C.7 验证 UBSan 0 errors

## Phase D: 文档 + CI 集成 (0.5 天)

- [ ] D.1 `docs/04-building/ci-cd.md` §Sanitizer builds 章节新增 ASan/UBSan 说明
- [ ] D.2 `docs/05-advanced/kfd-portability-boundary.md` v1.4 更新
- [ ] D.3 (可选) Stage 3.1 CI matrix 新增 sanitizer entries
- [ ] D.4 docs-audit 43/43 PASS

## Phase E: 验证 + 归档 (0.5 天)

- [ ] E.1 default build 无 sanitizer，104/104 ctest PASS
- [ ] E.2 ASan build，104/104 ctest PASS，0 sanitizer errors
- [ ] E.3 UBSan build，104/104 ctest PASS，0 sanitizer errors
- [ ] E.4 TSan build，104/104 ctest PASS，0 data races（保持 baseline）
- [ ] E.5 docs-audit 43/43 PASS，0 warnings
- [ ] E.6 `openspec archive 2026-07-16-three-sanitizer-infra`

---

## 任务统计

| Phase | 数量 |
|-------|-----:|
| A | 7 |
| B | 8 |
| C | 7 |
| D | 4 |
| E | 6 |
| **总计** | **32** |