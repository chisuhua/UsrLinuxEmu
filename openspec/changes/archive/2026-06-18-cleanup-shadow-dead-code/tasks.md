# Tasks: cleanup-shadow-dead-code

> **依赖**: proposal ✅
> **预估工时**: 30-45 分钟（含 build + test）
> **约束**: 单 commit + docs-audit 36/36 PASS + 全部测试 PASS

## 1. 准备 + 风险评估

- [ ] 1.1 验证 A1 #4 死代码 0 实例化：
  ```bash
  grep -rln "SimBuddyAllocator\|FenceSim" plugins/ libgpu_core/ tests/ \
    --include="*.cpp" --include="*.h"
  ```
  应返回 0 匹配
- [ ] 1.2 验证 A1 #3 doorbell_emu.cpp 唯一性（仅有 `#include`）：
  ```bash
  wc -l plugins/gpu_driver/sim/hardware/doorbell_emu.cpp
  ```
  应输出 1 行
- [ ] 1.3 读 `plugins/gpu_driver/sim/CMakeLists.txt` 全文，定位编译清单
- [ ] 1.4 跑基线测试（确认删除前全部 PASS）：
  ```bash
  make -j4 -C build && (cd build && ctest --output-on-failure)
  ```

## 2. A1 #1: SSOT §1.2 translator 嵌套修订

- [ ] 2.1 读 `docs/02_architecture/post-refactor-architecture.md` line 119-128（§1.2 硬件仿真层）
- [ ] 2.2 修订 line 122-123：
  ```
  │   • sim/scheduler/  : GlobalScheduler (FIFO + engine routing)   │
  │                       + GpfifoToLaunchParamsTranslator           │
  ```
  →
  ```
  │   • sim/scheduler/  : GlobalScheduler (FIFO + engine routing)   │
  │   • sim/scheduler/translator/                                   │
  │                       : GpfifoToLaunchParamsTranslator           │
  ```

## 3. A1 #3: 删除 doorbell_emu.cpp 空壳

- [ ] 3.1 `git rm plugins/gpu_driver/sim/hardware/doorbell_emu.cpp`
- [ ] 3.2 验证 `doorbell_emu.h` 包含完整实现（应已包含 `class DoorbellEmu` + 所有方法内联）：
  ```bash
  grep -n "class DoorbellEmu\|void ring\|void ack\|void reset" plugins/gpu_driver/sim/hardware/doorbell_emu.h
  ```

## 4. A1 #4: 删除 2 个 dead code 文件

- [ ] 4.1 `git rm plugins/gpu_driver/sim/buddy_allocator.cpp`
- [ ] 4.2 `git rm plugins/gpu_driver/sim/fence_sim.cpp`
- [ ] 4.3 验证删除后：
  ```bash
  ls plugins/gpu_driver/sim/buddy_allocator.cpp plugins/gpu_driver/sim/fence_sim.cpp
  ```
  应返回 "No such file or directory"

## 5. 更新 sim/CMakeLists.txt

- [ ] 5.1 读 `plugins/gpu_driver/sim/CMakeLists.txt` 全文
- [ ] 5.2 移除 doorbell_emu.cpp 编译条目（`hardware/doorbell_emu.cpp` 行）
- [ ] 5.3 移除 `buddy_allocator.cpp` 编译条目
- [ ] 5.4 移除 `fence_sim.cpp` 编译条目
- [ ] 5.5 验证：grep 确认无残留

## 6. 更新 SSOT §1.2 line 126

- [ ] 6.1 删除 line 126：
  ```
  │   • sim/buddy_allocator.cpp, sim/fence_sim.cpp (shadow 编译)     │
  ```
- [ ] 6.2 验证 line 119-128 整体连贯（嵌套 translator 修订 + shadow 行删除后）

## 7. 验证（关键！）

- [ ] 7.1 `make -j4 -C build` 必须 100% pass（编译无错误无警告）
- [ ] 7.2 `(cd build && ctest --output-on-failure)` 全部测试 PASS（34/34）
- [ ] 7.3 关键 sim/ 测试 PASS：
  - `./build/bin/test_doorbell_emu_standalone`
  - `./build/bin/test_hardware_puller_emu_standalone`
  - `./build/bin/test_gpu_ringbuffer_standalone`
  - `./build/bin/test_hardware_puller_emu_standalone`
- [ ] 7.4 `bash tools/docs-audit.sh --strict` 仍 36/36 PASS
- [ ] 7.5 `git diff --stat plugins/gpu_driver/sim/ docs/02_architecture/` 应显示：
  - 3 删除（D buddy_allocator.cpp, fence_sim.cpp, doorbell_emu.cpp）
  - 1 CMakeLists.txt 修改
  - 1 SSOT 修改（§1.2 2 处修订）

## 8. 提交 + 归档

- [ ] 8.1 `git add plugins/gpu_driver/sim/ docs/02_architecture/post-refactor-architecture.md`
- [ ] 8.2 单 commit：
  ```
  refactor(sim): cleanup shadow compile dead code + empty stub (A1 #1+#3+#4)

  Closes v0.1.6 audit deviations A1 #1 (🟡 P2), A1 #3 (🟡 P2),
  A1 #4 (🟡 P2). With this commit, all 25 v0.1.6 audit deviations
  are closed (100%).

  A1 #1 — SSOT §1.2 line 122-123 revised to reflect
    sim/scheduler/translator/ nested structure.

  A1 #3 — Removed sim/hardware/doorbell_emu.cpp (1-line stub).
    DoorbellEmu is header-only class with all methods inline
    in sim/hardware/doorbell_emu.h.

  A1 #4 — Removed sim/buddy_allocator.cpp + sim/fence_sim.cpp
    (dead code, 0 instantiations per grep). Removed from
    sim/CMakeLists.txt. SSOT §1.2 line 126 reference removed.

  Validation: make 100%, ctest 34/34 PASS, docs-audit 36/36 PASS.
  ```
- [ ] 8.3 `openspec archive cleanup-shadow-dead-code --yes`

## 9. 回滚预案

- 删除代码后测试 break：
  - `git revert <commit>`
  - `rm -rf openspec/changes/archive/2026-06-18-cleanup-shadow-dead-code/`
- 重新启用 dead code（按需）：restore 3 文件 + 还原 CMakeLists.txt + 还原 SSOT §1.2