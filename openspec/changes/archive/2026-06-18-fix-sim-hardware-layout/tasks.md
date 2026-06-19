# Tasks: fix-sim-hardware-layout

> **依赖**: proposal ✅
> **预估工时**: 20-30 分钟（含 build + 测试）
> **约束**: 单 commit + 2 个 standalone 测试必须 PASS

## 1. 准备 + 风险评估

- [ ] 1.1 列出移动文件 + 验证当前位置：
  ```bash
  ls -la plugins/gpu_driver/sim/doorbell_emu.cpp \
         plugins/gpu_driver/sim/hardware_puller_emu.cpp \
         plugins/gpu_driver/sim/hardware/doorbell_emu.h \
         plugins/gpu_driver/sim/hardware/hardware_puller_emu.h
  ```
- [ ] 1.2 grep 所有引用 `doorbell_emu.cpp` 和 `hardware_puller_emu.cpp` 的位置（CMakeLists + .h 文件）：
  ```bash
  grep -rn "doorbell_emu\|hardware_puller_emu" plugins/gpu_driver/sim/ \
    --include="*.cpp" --include="*.h" --include="CMakeLists.txt"
  ```
- [ ] 1.3 grep 所有 `#include "hardware/doorbell_emu.h"` 等引用（确认无路径依赖问题）
- [ ] 1.4 跑基线测试（确认移动前所有测试 PASS）：
  ```bash
  make -j4 -C build
  cd /workspace/project/UsrLinuxEmu && ./build/bin/test_doorbell_emu_standalone
  cd /workspace/project/UsrLinuxEmu && ./build/bin/test_hardware_puller_emu_standalone
  ```

## 2. 文件移动

- [ ] 2.1 `git mv plugins/gpu_driver/sim/doorbell_emu.cpp plugins/gpu_driver/sim/hardware/doorbell_emu.cpp`
- [ ] 2.2 `git mv plugins/gpu_driver/sim/hardware_puller_emu.cpp plugins/gpu_driver/sim/hardware/hardware_puller_emu.cpp`
- [ ] 2.3 验证：`ls plugins/gpu_driver/sim/hardware/` 应含 4 个文件（2 .h + 2 .cpp）

## 3. 修复 include 路径（如果需要）

- [ ] 3.1 检查 `sim/hardware/doorbell_emu.cpp` 内容（应仅含 `#include "doorbell_emu.h"`，无需修改）
- [ ] 3.2 检查 `sim/hardware/hardware_puller_emu.cpp` 内容（应仅含 `#include "hardware_puller_emu.h"`，无需修改）
- [ ] 3.3 如果 .cpp 含相对路径 include（如 `#include "../xxx.h"`），需更新

## 4. 更新 sim/CMakeLists.txt

- [ ] 4.1 读 `sim/CMakeLists.txt`，定位当前 `doorbell_emu.cpp` 和 `hardware_puller_emu.cpp` 编译条目（v0.1.6 审计发现行 7-8）
- [ ] 4.2 更新为 `hardware/doorbell_emu.cpp` 和 `hardware/hardware_puller_emu.cpp`
- [ ] 4.3 验证：grep 确认无遗漏的旧路径

## 5. 更新 SSOT §1.2（layout 已对齐，无需注释）

- [ ] 5.1 读 `post-refactor-architecture.md` line 119-128（§1.2 硬件仿真层）
- [ ] 5.2 确认 SSOT 描述 `sim/hardware/   : HardwarePullerEmu (FSM), DoorbellEmu` 现在与实际一致
- [ ] 5.3 可选：在 SSOT §1.2 加 1 行 v0.1.6 audit note（"v0.1.8 fix-sim-hardware-layout 关闭 A1 #2"）

## 6. 验证（关键！）

- [ ] 6.1 `make -j4 -C build` 必须 100% pass（编译无错误无警告）
- [ ] 6.2 `./build/bin/test_doorbell_emu_standalone` PASS
- [ ] 6.3 `./build/bin/test_hardware_puller_emu_standalone` PASS
- [ ] 6.4 `bash tools/docs-audit.sh --strict` 仍 36/36 PASS
- [ ] 6.5 `git diff --stat plugins/gpu_driver/sim/` 应显示 4 个文件变动（2 移动 + CMakeLists + .git 索引）

## 7. 提交 + 归档

- [ ] 7.1 `git add plugins/gpu_driver/sim/ docs/02_architecture/post-refactor-architecture.md`
- [ ] 7.2 单 commit：
  ```
  refactor(sim): move hardware/ .cpp files into hardware/ subdir (A1 #2)

  Closes A1 #2 from v0.1.6 audit. sim/hardware/ previously only
  contained .h files; corresponding .cpp implementations were in
  sim/ root, contradicting SSOT §1.2 line 124 description.

  Files moved:
  - sim/doorbell_emu.cpp → sim/hardware/doorbell_emu.cpp
  - sim/hardware_puller_emu.cpp → sim/hardware/hardware_puller_emu.cpp

  sim/CMakeLists.txt updated to reference new paths. No code logic
  changes (pure file layout alignment with SSOT).

  Validation: test_doorbell_emu_standalone + test_hardware_puller_emu_standalone PASS.
  ```
- [ ] 7.3 `openspec archive fix-sim-hardware-layout --yes`

## 8. 回滚预案

- 选项 A 失败（编译/测试 break）→ `git revert <commit>` + `rm -rf openspec/changes/archive/2026-06-17-fix-sim-hardware-layout/`
- 选项 A 失败且无法 revert → 走选项 B（仅更新 SSOT 加注释，不动代码）
