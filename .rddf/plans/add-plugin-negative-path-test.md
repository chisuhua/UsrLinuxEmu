# add-plugin-negative-path-test Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 写 1 个回归测试，覆盖 plugin init 失败时 `-ENOENT` 传播路径。**关键**（Oracle deep-dive REJECT — tautology fix）：必须在 chdir 到空 tmp dir 之前**捕获绝对 plugins 路径**，否则 `load_plugins("plugins")` 在 `scan_candidates` 阶段就因 `fs::exists("plugins")` 失败而退出，`-ENOENT` 路径从未跑。

**Architecture:** 新增 Catch2 standalone binary。Test 用 `getcwd` + `realpath` 在 chdir 前捕获 `<saved_cwd>/plugins` 的绝对路径；`chdir` 到一个空 `tmp` 子目录；调用 `ModuleLoader::load_plugins(<absolute_plugins_path>)`。预期 `dev = VFS::instance().open("/dev/gpgpu0", O_RDWR)` 返回 `nullptr`（gpgpu0 设备未注册，因为 plugin init 失败）；日志含 "topology" 或 "ENOENT" 关键字。

**Tech Stack:** Catch2 · `filesystem` · `dlopen` · 现有 ModuleLoader 接口

**Priority:** P3  **Wave:** 2  **Risk:** LOW（独立 test file；不动生产代码）

**Generated:** 2026-09-06 (post a2701cd, Oracle deep-dive revisions applied)

**Refs:** Oracle deep-dive `ses_f8f0f9467ffe3w1M4uCHZTcFmS` (REJECT — tautology fix) · openspec/changes/add-plugin-negative-path-test/

---

## File Structure

### Tests (new)

| File | Responsibility |
|------|----------------|
| `tests/test_gpu_plugin_init_negative_path_standalone.cpp` | 新 Catch2 standalone — 验证 `-ENOENT` 通过 plugin init 失败传播到 `VFS::open()` |

### Tests/CMakeLists (no source change)

| File | Responsibility |
|------|----------------|
| `tests/CMakeLists.txt` | register new test binary |

---

## Tasks (2)

### Task 1: Preflight + 理解 plugin 拓扑失败路径

- [ ] **Step 1: Verify starting state**
```bash
cd /workspace/project/UsrLinuxEmu/build && ctest --output-on-failure 2>&1 | tail -3
# Expected: 157/157 PASS（基线）
ls tests/test_gpu_plugin_init_negative_path_standalone.cpp 2>&1
# Expected: No such file（不存在）
```

- [ ] **Step 2: Locate the -ENOENT propagation point**
- 读 `src/kernel/module_loader.cpp`：
  - 找到 `scan_candidates` 函数（应为 plugin 扫描入口）
  - 找到 `topo_sort` 或类似拓扑路径读取代码
  - 记录：如果 plugin 内部依赖的 topology file（如 `pcie.json`）缺失，errno 是什么？路径是什么？

- [ ] **Step 3: Locate gpu_driver 内部 topology 依赖**
```bash
cd /workspace/project/UsrLinuxEmu && grep -rn 'fs::exists\|std::ifstream\|pcie.*\.json\|topology' plugins/gpu_driver/plugin.cpp plugins/gpu_driver/drv/ 2>&1 | head -20
# Expected: 列出 gpu_driver 加载时尝试读取的相对路径文件 — 这些是 chdir 后会 ENOENT 的文件
```

### Task 2: 实现 regression test

- [ ] **Step 1: Verify starting state**
```bash
cd /workspace/project/UsrLinuxEmu && ls build/bin/test_gpu_plugin_init_idempotent_standalone
# Expected: file exists (参考该 binary 的 test infrastructure)
```

- [ ] **Step 2: 写 test file**
- 创建 `tests/test_gpu_plugin_init_negative_path_standalone.cpp`：

```cpp
#include <catch_amalgamated.hpp>
#include "kernel/vfs.h"
#include "kernel/module_loader.h"
#include <filesystem>
#include <cstdlib>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

TEST_CASE("load_plugins fails when internal topology file missing (-ENOENT propagation)", "[plugin][negative-path]") {
  // === CRITICAL: capture ABSOLUTE plugins path BEFORE chdir ===
  // Otherwise load_plugins("plugins") fails at scan_candidates BEFORE any
  // plugin's init() runs — the -ENOENT propagation path is never exercised
  // (Oracle deep-dive REJECT — tautology fix).
  std::error_code ec;
  fs::path saved_cwd = fs::current_path(ec);
  REQUIRE(ec.value() == 0);
  fs::path plugins_abs = fs::absolute(saved_cwd / "plugins", ec);
  REQUIRE(ec.value() == 0);
  REQUIRE(fs::exists(plugins_abs));  // sanity: pre-chdir path MUST exist

  // Capture stderr to detect the -ENOENT log
  std::stringstream captured_stderr;
  auto* old_cerr = std::cerr.rdbuf();
  std::cerr.rdbuf(captured_stderr.rdbuf());

  // Create empty tmp dir + chdir into it
  fs::path tmp = saved_cwd / "tmp_empty_for_neg_test";
  fs::remove_all(tmp, ec);  // idempotent cleanup
  fs::create_directory(tmp, ec);
  REQUIRE(ec.value() == 0);
  fs::current(tmp, ec);
  REQUIRE(ec.value() == 0);

  // Sanity: after chdir, relative "plugins" path no longer exists
  REQUIRE_FALSE(fs::exists("plugins"));

  // === Exercise the propagation: pass ABSOLUTE path so scan_candidates finds
  //     the .so, dlopens it, plugin's init() tries to read a topology file via
  //     a now-relative path → ENOENT → -ENOENT must propagate ===
  int rc = usr_linux_emu::ModuleLoader::load_plugins(plugins_abs.string());

  // Restore stderr
  std::cerr.rdbuf(old_cerr);
  // Restore cwd BEFORE assertions that touch any other code
  fs::current(saved_cwd, ec);
  REQUIRE(ec.value() == 0);

  // Cleanup tmp
  fs::remove_all(tmp, ec);

  // === Assertions ===
  SECTION("rc is non-zero indicating failure propagated") {
    REQUIRE(rc != 0);
  }

  SECTION("captured stderr contains ENOENT or topology marker") {
    std::string log = captured_stderr.str();
    INFO("captured stderr: " << log);
    // Either keyword is acceptable — the spec allows either as the log marker
    REQUIRE((log.find("ENOENT") != std::string::npos ||
             log.find("topology") != std::string::npos ||
             log.find("No such file") != std::string::npos));
  }

  SECTION("VFS::open returns nullptr for /dev/gpgpu0") {
    auto dev = usr_linux_emu::VFS::instance().open("/dev/gpgpu0", O_RDWR);
    REQUIRE(dev == nullptr);
  }

  // === Process isolation note ===
  // This test mutates cwd; if any other test in the same process relies on
  // CWD-relative plugin loading, run as a standalone binary (not via ctest
  // --build-and-test) to ensure process boundary.
}
```

- **若** `ModuleLoader::load_plugins` 实际签名是 `(const std::string&)` 或 `(const char*)`，调整调用
- **若** `VFS::instance().open` 返回 `std::shared_ptr<>` 而非 raw ptr，调整 REQUIRE
- **若** errno 返回是负的（如 `-ENOENT`），`rc != 0` 同样成立

- [ ] **Step 3: 注册 test**
- 修改 `tests/CMakeLists.txt`：
  - 在 `CATCH2_TESTS` 列表添加 `test_gpu_plugin_init_negative_path_standalone`
  - **若** `add_standalone_test` macro 接受额外 link 依赖（如 kernel），检查现有同类 test 调用

- [ ] **Step 4: Run test (target binary verification)**
```bash
cd /workspace/project/UsrLinuxEmu/build && cmake --build . -j4 --target test_gpu_plugin_init_negative_path_standalone 2>&1 | tail -10
# Expected: build clean
./bin/test_gpu_plugin_init_negative_path_standalone
# Expected: 全部 3 SECTION 全 PASS（前提：absolute-path 捕获正确 + plugin 内部相对路径确实失败）
```

- [ ] **Step 5: Confirm NOT a tautology（Oracle 强调）**
```bash
# 临时回滚 -ENOENT 传播，断言 test FAIL（否则测试可能是无意义 PASS）
cd /workspace/project/UsrLinuxEmu
# 找到 plugin.cpp 中读取 topology 的代码，临时改成 swallow rc
# 然后 rebuild + run test → MUST FAIL
# 还原修改后再 rebuild + run → MUST PASS
# 如果两轮都 PASS：说明测试是 tautology，需要重新设计
```

- [ ] **Step 6: 全量 blast radius**
```bash
cd /workspace/project/UsrLinuxEmu/build && ctest --output-on-failure 2>&1 | tail -3
# Expected: 158/158 PASS（+1 新 test）
# 注意：新 test 在 ctest 中作为独立 binary 运行（process boundary），不会影响其他 test
```

- [ ] **Step 7: Commit**
```bash
cd /workspace/project/UsrLinuxEmu && git add tests/test_gpu_plugin_init_negative_path_standalone.cpp tests/CMakeLists.txt
git commit -m "test(plugin): regression test for -ENOENT propagation on init failure

Verifies that when a plugin's init() fails because an internal
topology file is missing (relative path), the -ENOENT propagates
up to load_plugins' return code and VFS::open() returns nullptr.

CRITICAL: absolute plugins path MUST be captured BEFORE chdir to
the empty tmp dir. Otherwise load_plugins fails at scan_candidates
before any plugin's init() runs — the propagation path is never
exercised (tautology).

The test temporarily mutates cwd; runs as standalone binary to
guarantee process isolation from other ctest cases.

Refs:
  Oracle deep-dive: ses_f8f0f9467ffe3w1M4uCHZTcFmS (REJECT — tautology fix)"
```

- [ ] **Step 8: Archive**
```bash
cd /workspace/project/UsrLinuxEmu
# tasks.md §2.1-2.6 / §3 / §4 标 [x]
openspec validate add-plugin-negative-path-test --strict
# Expected: Change 'add-plugin-negative-path-test' is valid
openspec archive add-plugin-negative-path-test --yes
git add openspec/ && git commit -m "chore(openspec): archive add-plugin-negative-path-test"
```

---

## Out of scope (handled elsewhere)

- 修复 gpu_driver plugin 中相对路径拓扑文件加载（即便该路径在 chdir 后失败，是预期行为；测试只验证错误传播）
- 其他 plugin 的 topology 依赖
- Process-isolation framework（如 gtest death test 模式）—— 本计划通过 standalone binary + cwd restore 实现