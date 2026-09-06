# add-moduleloader-idempotent-init Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修复 `src/kernel/module_loader.cpp` `load_plugin` 的 Oracle Q3 finding —— 同一 plugin 被 `load_plugins` 多次加载时**不应**重跑 `mod->init()`，**不应**覆写现有 `PluginInfo`（reset `ref_count = 0` 导致后续 unload_plugins dlclose 一次后 glibc refcount 卡在 1，库永不卸载）。

**Architecture:** 在 `load_plugin` 函数 dlsym 之后、resolve_dependencies 之前插入 `loaded_plugins_.count(mod->name)` 检查。若命中：递增现有 `PluginInfo->ref_count`、`dlclose` 当前冗余 handle、返回 0（**不**调用 `mod->init()`、**不**覆写 PluginInfo）。`ref_count` 语义修正：首次成功 load 设 `0`，每次重复 load 增到 `1`。`dependency ref_count` **不**在重复 load 分支递增（asymmetry，spec Requirement 已记录）。

**Tech Stack:** C++17 · `dlopen/dlsym/dlclose` · `std::unordered_map<string, std::unique_ptr<PluginInfo>>` · 现有 Catch2 测试栈

**Priority:** P1  **Wave:** 1  **Risk:** HIGH（29 个 test 文件调用 `load_plugins`；framework bug 修复 blast radius 大）

**Generated:** 2026-09-06 (post a2701cd, Oracle deep-dive revisions applied)

**Refs:** Oracle audit `ses_f8b8c32e5ffeGgFiPdo4hE0jiu` (Q3) · Oracle deep-dive `ses_f8f0f9467ffe3w1M4uCHZTcFmS` · commit `31dd5e1` (plugin-side guard, defense in depth, MUST keep) · openspec/changes/add-moduleloader-idempotent-init/

---

## File Structure

### Code

| File | Responsibility |
|------|----------------|
| `src/kernel/module_loader.cpp` | `load_plugin` 函数 (lines ~177-214) — 插入 already-loaded 检查 |

### Tests (existing — no new file)

| File | Responsibility |
|------|----------------|
| `tests/test_gpu_plugin_init_idempotent_standalone.cpp` | 现有 F4 回归测试 — load_plugins 调两次，断言 g_plugin_initialized + dev==nullptr 守卫 |
| `tests/test_module_load_and_vfs_standalone.cpp` | 现有 ModuleLoader/VFS 测试 — 验证单次 load 路径未退化 |
| All 28 other test files calling `load_plugins("plugins")` | blast radius — 必须继续 PASS |

---

## Tasks (3)

### Task 1: Preflight + 读懂 load_plugin 当前行为

- [ ] **Step 1: Verify starting state**
```bash
cd /workspace/project/UsrLinuxEmu && git log --oneline -1 src/kernel/module_loader.cpp
# Expected: 最新提交 ≠ HEAD (即 module_loader.cpp 最近未被本计划触碰)

cd build && ctest -R test_gpu_plugin_init_idempotent_standalone --output-on-failure
# Expected: PASS (现有 plugin-side guard 在 module_loader fix 之前已 work)
```

- [ ] **Step 2: Read source**
- 读 `src/kernel/module_loader.cpp` 全文（~250 行），重点关注：
  - `load_plugin` 函数定义（约 177-214 行）
  - `loaded_plugins_` 容器声明（成员变量）
  - `PluginInfo` 结构定义（应含 `int ref_count` 字段）
  - `decrease_ref` 函数（约 168 行）
  - `unload_plugins` / `unload_plugin` 的 erase 与 dlclose 顺序
- 记录：
  - `ref_count` 在首次成功 load 后的值（应等于 `0`，见 line ~208）
  - `--ref_count <= 0` 触发 exit+dlclose+erase 的位置（decrease_ref :168）
  - 现有 `resolve_dependencies` 在 `load_plugin` 中的调用位置（应在 already-loaded 检查之后）

- [ ] **Step 3: Confirm no existing already-loaded check**
```bash
cd /workspace/project/UsrLinuxEmu && grep -n 'loaded_plugins_.count\|loaded_plugins_.find' src/kernel/module_loader.cpp
# Expected: 空（确认当前无重复 load 防御）
```

### Task 2: 插入 already-loaded 守卫

- [ ] **Step 1: Verify starting state**
```bash
cd /workspace/project/UsrLinuxEmu && grep -c 'loaded_plugins_\[' src/kernel/module_loader.cpp
# Expected: 当前命中数 = N（包含 line ~211 的 `loaded_plugins_[mod->name] = info` 覆写）
```

- [ ] **Step 2: Implement**
- 修改 `src/kernel/module_loader.cpp` `load_plugin` 函数：
  - 定位 dlsym 成功之后、`resolve_dependencies(...)` 之前
  - 在 dlsym 与 mod->name 可用之间插入守卫块：

```cpp
  // Oracle Q3 fix: 同一 plugin 重复 load 时跳过 init/resolve/refcount 覆写
  auto already_it = loaded_plugins_.find(mod->name);
  if (already_it != loaded_plugins_.end()) {
    already_it->second->ref_count++;
    dlclose(handle);
    return 0;
  }
```

- **不要**修改 line ~211 的 `loaded_plugins_[mod->name] = info` —— 新分支在到达该行前 early-return，旧路径保持不变
- **不要**在 already-loaded 分支调用 `resolve_dependencies`（dependency ref_count asymmetry 由 spec Requirement 记录）
- 不要触碰 `g_plugin_initialized`（plugin-side guard 是 defense in depth，per `31dd5e1`）

- [ ] **Step 3: Run tests to verify**
```bash
cd /workspace/project/UsrLinuxEmu/build && cmake --build . -j4 --target kernel test_module_load_and_vfs_standalone test_gpu_plugin_init_idempotent_standalone
# Expected: kernel 库 build clean; 两个 target build OK
ctest -R 'test_module_load_and_vfs|test_gpu_plugin_init_idempotent' --output-on-failure
# Expected: 两个测试 PASS
```

- [ ] **Step 4: 全量 ctest blast radius 验证**
```bash
cd /workspace/project/UsrLinuxEmu/build && ctest --output-on-failure 2>&1 | tail -20
# Expected: 157/157 PASS（29 个 test_files 全部通过；框架修复对单次 load 是透明的）
```

- [ ] **Step 5: Sanity check (Oracle deep-dive finding 的核心)**
```bash
cd /workspace/project/UsrLinuxEmu
# 验证 glibc refcount 行为：两次 load + 一次 unload 后，库是否真正卸载
cat > /tmp/check_unmap.cpp << 'EOF'
#include <dlfcn.h>
#include <iostream>
#include <fstream>
#include <string>
int main() {
  // 第一次 load
  void* h1 = dlopen("./build/bin/../plugins/plugin_gpu_driver.so", RTLD_NOW);
  std::ifstream m1("/proc/self/maps"); int c1 = 0; std::string l;
  while (std::getline(m1,l)) if (l.find("plugin_gpu_driver") != std::string::npos) c1++;
  // 第二次 load (同一个 .so)
  void* h2 = dlopen("./build/bin/../plugins/plugin_gpu_driver.so", RTLD_NOW);
  std::ifstream m2("/proc/self/maps"); int c2 = 0;
  while (std::getline(m2,l)) if (l.find("plugin_gpu_driver") != std::string::npos) c2++;
  // 第一次 dlclose
  dlclose(h2);
  std::ifstream m3("/proc/self/maps"); int c3 = 0;
  while (std::getline(m3,l)) if (l.find("plugin_gpu_driver") != std::string::npos) c3++;
  dlclose(h1);
  std::ifstream m4("/proc/self/maps"); int c4 = 0;
  while (std::getline(m4,l)) if (l.find("plugin_gpu_driver") != std::string::npos) c4++;
  std::cout << "h1: " << c1 << " lines, h1+h2: " << c2 << ", after dlclose(h2): " << c3
            << ", after dlclose(h1): " << c4 << "\n";
  // 期望 (新行为): c1==c2 (ModuleLoader 不再 dlopen 第二次，ref_count 走 single-path), c3==c4==c2
  // 旧行为: c2>c1 (第二次 dlopen 拿到新 mapping), c3==c2 (dlclose 一次不够)
  return 0;
}
EOF
g++ -std=c++17 -ldl /tmp/check_unmap.cpp -o /tmp/check_unmap && /tmp/check_unmap
# Expected: c1 == c2 == c3 == c4（第二次 dlopen 在 ModuleLoader 层被 skip，没有第二个 mapping）
# 若 c2 > c1 或 c3 == c2 而 c4 == 0：说明 dlclose 顺序错误，框架修复未生效
```

- [ ] **Step 6: Commit**
```bash
cd /workspace/project/UsrLinuxEmu && git add src/kernel/module_loader.cpp
git commit -m "fix(moduleloader): skip init() when plugin already loaded (preserves refcount)

Oracle Q3 fix (audit ses_f8b8c32e5ffeGgFiPdo4hE0jiu). load_plugin
inserts loaded_plugins_.count(mod->name) check after dlsym and
before resolve_dependencies. On already-loaded branch:
  - increment existing PluginInfo->ref_count
  - dlclose the redundant handle
  - return 0 (no mod->init() call, no PluginInfo overwrite)

ref_count semantics:
  - First successful load: ref_count = 0 (existing code)
  - Subsequent load (new branch): ref_count++ → 1
  - decrease_ref 1→0 triggers exit+dlclose+erase (existing code)

Dependency ref_count asymmetry: the loaded plugin's deps do NOT
get an extra reference on duplicate load (spec Requirement
'dependency ref_count is NOT incremented on duplicate load
(asymmetry)'). Intentional scope limit; documented in openspec
specs/moduleloader-init/spec.md.

The gpu_driver plugin's own g_plugin_initialized guard (commit
31dd5e1) stays as defense in depth. Removing it would re-introduce
the F4 leak if a future refactor changes the plugin's init contract.

Verified: ctest 157/157 PASS (29 test_files blast radius confirmed).
Sanity /proc/self/maps check confirms no double-mapping.

Refs:
  Oracle audit:    ses_f8b8c32e5ffeGgFiPdo4hE0jiu
  Oracle deep-dive: ses_f8f0f9467ffe3w1M4uCHZTcFmS"
```

### Task 3: Update openspec + archive

- [ ] **Step 1: Implement**
```bash
cd /workspace/project/UsrLinuxEmu
# tasks.md 把 §2.1 / 2.2 / 2.3 标记 [x]
# 用 read+edit 把 §2.1 的代码块标 [x]，§2.2 / 2.3 同
# §3.4 sanity check 标 [x]
# §4.1 / 4.2 标 [x]
```

- [ ] **Step 2: Validate + archive**
```bash
cd /workspace/project/UsrLinuxEmu
openspec validate add-moduleloader-idempotent-init --strict
# Expected: Change 'add-moduleloader-idempotent-init' is valid

# 单独 archive (因为代码已 commit 在 main 分支):
openspec archive add-moduleloader-idempotent-init --yes
# Expected: 归档到 openspec/changes/archive/，specs/ 已 sync
```

- [ ] **Step 3: Commit openspec 归档**
```bash
cd /workspace/project/UsrLinuxEmu && git add openspec/ && git status --short | head
git commit -m "chore(openspec): archive add-moduleloader-idempotent-init (Q3 framework fix)"
```

---

## Out of scope (handled elsewhere)

- gpu_driver plugin 的 `g_plugin_initialized` guard 保留（defense in depth）
- ModuleLoader 单元测试基础设施（项目尚无；本计划通过 blast-radius test 间接覆盖）
- 修改其他 plugin 的 init 函数（Change-2 sim singleton 单独覆盖；其他 plugin 应按需自加 guard）