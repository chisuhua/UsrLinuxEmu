# CI/CD 配置

本文档介绍 UsrLinuxEmu 项目的持续集成和持续部署（CI/CD）配置。

**最后更新**: 2026-06-16 (commit `374d463`)  
**作者**: UsrLinuxEmu Team

---

## 概述

UsrLinuxEmu 使用 **GitHub Actions** 作为 CI/CD 平台，自动化执行以下任务：

- ✅ 多平台编译（Linux GCC/Clang）
- ✅ 单元测试（Catch2）
- ✅ 代码风格检查
- ✅ 静态分析（clang-tidy）
- ✅ **文档审计**（`tools/docs-audit.sh --strict`，防止文档漂移回归）
- ✅ **Pre-commit hook**（`scripts/install-hooks.sh`，本地漂移拦截）
- 🔄 自动发布（规划中）

---

## 工作流程

### cmake-multi-platform.yml

主 CI 工作流程，位于 `.github/workflows/cmake-multi-platform.yml`。

#### 触发条件

```yaml
on:
  push:
    branches: [ "main" ]
  pull_request:
    branches: [ "main" ]
```

- **push**: 推送到 main 分支时触发
- **pull_request**: 创建或更新 PR 时触发

#### Job 列表

| Job | OS | 说明 | 必检 |
|-----|-----|------|------|
| `build` | ubuntu-latest × ubuntu-22.04 × {gcc, clang} | CMake configure + build + ctest | ✅ |
| `docs-audit` | ubuntu-latest | `tools/docs-audit.sh --strict` | ✅ |

#### docs-audit Job 详情

```yaml
docs-audit:
  name: Documentation Audit (--strict)
  runs-on: ubuntu-latest
  steps:
    - uses: actions/checkout@v4
    - name: Run docs-audit
      run: bash tools/docs-audit.sh --strict
    - name: Annotate failure
      if: failure()
      run: |
        echo "::error::docs-audit --strict failed..."
```

**关键设计**：
- 使用 `--strict` 模式（warnings 也算失败）— 任何文档漂移都会阻塞 PR
- **不依赖路径过滤器** — 代码提交也可能引入文档漂移
- **不依赖额外依赖** — 纯 bash + grep + find，在 ubuntu-latest 上开箱即用

**失败时的修复指引**会在 PR 中以 `::error::` annotation 显示。

---

## 文档审计 (`tools/docs-audit.sh`)

SSOT 关联文档：[docs/02_architecture/post-refactor-architecture.md](../02_architecture/post-refactor-architecture.md)

### 设计目标

防止文档与代码之间的漂移（doc/code drift）。文档可能因为以下原因"撒谎"：

- 文件被重命名（如某目录 `core/` → `architecture/`）
- 数量变化（如 `src/kernel/` 多了/少了 cpp 文件）
- API 被替换（如 `GPGPU_*` → `GPU_IOCTL_*`）
- 约定变更（如 kebab-case → snake_case）

### 用法

```bash
# 跑全量审计（warnings 不阻塞）
tools/docs-audit.sh

# CI 模式（warnings 也算失败）
tools/docs-audit.sh --strict

# 跑单个 section
tools/docs-audit.sh --section ioctl     # 仅 IOCTL 编号
tools/docs-audit.sh --section arch      # 仅架构事实
tools/docs-audit.sh --section doc-health # 仅文档健康

# 看帮助
tools/docs-audit.sh --help
```

### 5 个审计 Section

| Section | 检查内容 | 失败样例 |
|---------|---------|---------|
| **arch** | kernel SHARED / cpp 计数 / archive 目录 / HAL 数量 / 插件加载模式 | kernel 改为 STATIC（Issue #11 回归）|
| **ioctl** | System C IOCTL 编号 / System A/B 残留 / LAUNCH_CB 残留 | 误用 `GPGPU_*` 宏 |
| **adr** | ADR-022~031 编号 gap / IOCTL 编号冲突 | ADR-015 引用 0x33-0x35（应 0x40-0x43）|
| **doc-health** | 链接完整性 / 02-core 引用 / 命名一致性 / 完成度日期 | 文档写"kebab-case 是标准"但文件是 snake_case |
| **build** | 孤儿测试 / `add_subdirectory` 完整性 / include 路径 | `tests/test_foo.cpp` 未加入 CMakeLists.txt |

### 退出码

- `0` — 全部通过（或非 strict 模式下只有 warnings）
- `1` — 有失败项（或 strict 模式下有 warnings）
- `2` — 参数错误

### 退出码 vs CI 集成

CI 调用 `tools/docs-audit.sh --strict`，任何 warning 都会让 `docs-audit` job 失败 → PR 无法合并。

---

## Pre-commit Hook（本地漂移拦截）

`scripts/install-hooks.sh` 安装 Git 钩子，在 commit 时自动跑 `docs-audit`，**仅在 docs/ 相关文件被 staged 时触发**（避免拖慢纯代码提交）。

### 安装

```bash
# 一次性安装（已跟踪的钩子模板在 scripts/hooks/）
scripts/install-hooks.sh

# 卸载
scripts/install-hooks.sh --uninstall
```

### 工作原理

`scripts/hooks/pre-commit` 钩子：

1. 调用 `code-review-graph`（如果已安装）— 维护知识图谱
2. 检查 `git diff --cached` 中是否有匹配以下模式的文件：
   - `docs/**`
   - `AGENTS.md` / `CONTRIBUTING.md`
   - 任何 `CMakeLists.txt`
   - `tools/docs-audit.sh`
3. 如果有，跑 `tools/docs-audit.sh --strict`
4. 失败 → 阻止 commit 并打印修复指引

### 跳过单次 commit

```bash
SKIP_DOCS_AUDIT=1 git commit -m "hotfix"
```

> 只在确实知道 audit 是误报时使用；并在 PR 描述中说明原因。

### 为什么不在所有 commit 上都跑？

docs-audit 检查范围广（含代码结构、IOCTL 编号等）。在纯代码 commit 上跑会拖慢提交速度且通常不会失败。仅在涉及可能影响文档的路径时跑，性能与严谨度平衡。

---

## 本地重现 CI 构建

### 使用相同配置

```bash
# 1. 创建构建目录
mkdir -p build && cd build

# 2. 配置 CMake（使用 GCC）
cmake -DCMAKE_C_COMPILER=gcc \
      -DCMAKE_CXX_COMPILER=g++ \
      -DCMAKE_BUILD_TYPE=Release \
      ..

# 或使用 Clang
cmake -DCMAKE_C_COMPILER=clang \
      -DCMAKE_CXX_COMPILER=clang++ \
      -DCMAKE_BUILD_TYPE=Release \
      ..

# 3. 编译
cmake --build . --config Release -j$(nproc)

# 4. 运行测试
ctest --build-config Release --output-on-failure
```

### 启用所有检查

```bash
# 启用 clang-tidy
cmake -DCMAKE_CXX_CLANG_TIDY=clang-tidy \
      -DCMAKE_BUILD_TYPE=Debug \
      ..

# 启用 AddressSanitizer
cmake -DCMAKE_CXX_FLAGS="-fsanitize=address -g" \
      -DCMAKE_BUILD_TYPE=Debug \
      ..

# 启用 ThreadSanitizer
cmake -DCMAKE_CXX_FLAGS="-fsanitize=thread -g" \
      -DCMAKE_BUILD_TYPE=Debug \
      ..
```

---

## 测试覆盖率

### 启用覆盖率

```bash
# 安装依赖
sudo apt-get install gcovr

# 编译启用覆盖率
cmake -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_CXX_FLAGS="--coverage -g" \
      -DCMAKE_C_FLAGS="--coverage -g" \
      ..
make -j$(nproc)

# 运行测试
ctest --output-on-failure

# 生成覆盖率报告
gcovr -r .. --html --html-details -o coverage.html

# 查看报告
firefox coverage.html
```

### 覆盖率目标

| 模块 | 目标覆盖率 | 当前状态 |
|------|------------|----------|
| 核心框架 | 80% | 🔄 进行中 |
| 设备驱动 | 75% | 🔄 进行中 |
| GPU 驱动 | 70% | 🔄 进行中 |
| 工具类 | 85% | 🔄 进行中 |

---

## 代码风格检查

### clang-format

```bash
# 安装 clang-format
sudo apt-get install clang-format

# 检查代码风格
find src include drivers -name "*.cpp" -o -name "*.h" | \
    xargs clang-format --dry-run --Werror

# 自动格式化
find src include drivers -name "*.cpp" -o -name "*.h" | \
    xargs clang-format -i
```

### clang-tidy

```bash
# 安装 clang-tidy
sudo apt-get install clang-tidy

# 编译时启用 clang-tidy
cmake -DCMAKE_CXX_CLANG_TIDY=clang-tidy \
      -DCMAKE_BUILD_TYPE=Debug \
      ..

# 手动运行
clang-tidy src/kernel/device.cpp \
    -p build \
    -- -std=c++17
```

### .clang-tidy 配置

```yaml
# .clang-tidy
---
Checks: >
  -*,
  bugprone-*,
  cert-*,
  cppcoreguidelines-*,
  misc-*,
  modernize-*,
  performance-*,
  portability-*,
  readability-*,
  -modernize-use-trailing-return-type,
  -cppcoreguidelines-pro-bounds-pointer-arithmetic

WarningsAsErrors: 'bugprone-*,cert-*,cppcoreguidelines-*'

HeaderFilterRegex: '.*'
AnalyzeTemporaryDtors: false

FormatStyle: none

CheckOptions:
  - key: readability Identifier Case.RenameCase
    value: true
```

---

## 添加新的 CI 工作流

### 示例：文档检查

UsrLinuxEmu 使用 `tools/docs-audit.sh` 做文档审计（已在主 workflow `cmake-multi-platform.yml` 中）。**不需要单独的 `docs-check.yml` workflow** —— 直接在主流程里加 job 即可（保持单一可信来源）。

如果未来需要添加纯链接检查或 markdown lint，可在主 workflow 中新增 job：

```yaml
  link-check:
    name: Markdown Link Check
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Check links
        uses: gaurav-nelson/github-action-markdown-link-check@v1
        with:
          use-quiet-mode: 'yes'
          config-file: '.github/link-check-config.json'
```

> 注意：markdownlint 等 npm 工具与本项目的 C++/bash 工具栈不匹配，引入会增加 CI 维护成本。`docs-audit.sh` 已经覆盖了 90% 的链接与格式问题。

### 示例：静态分析

创建 `.github/workflows/static-analysis.yml`：

```yaml
name: Static Analysis

on:
  push:
    branches: [ "main" ]
  pull_request:
    branches: [ "main" ]

jobs:
  clang-tidy:
    runs-on: ubuntu-latest
    
    steps:
    - uses: actions/checkout@v4
    
    - name: Install dependencies
      run: |
        sudo apt-get update
        sudo apt-get install -y clang-tidy
    
    - name: Configure CMake
      run: |
        cmake -B build \
          -DCMAKE_CXX_CLANG_TIDY=clang-tidy \
          -DCMAKE_BUILD_TYPE=Debug
    
    - name: Build
      run: cmake --build build
    
    - name: Run clang-tidy
      run: |
        cmake --build build \
          --target clang-tidy
```

### 示例：内存安全检查

创建 `.github/workflows/memory-check.yml`：

```yaml
name: Memory Check (ASan)

on:
  push:
    branches: [ "main" ]
  pull_request:
    branches: [ "main" ]

jobs:
  asan:
    runs-on: ubuntu-latest
    
    steps:
    - uses: actions/checkout@v4
    
    - name: Install dependencies
      run: |
        sudo apt-get update
        sudo apt-get install -y g++
    
    - name: Configure CMake (ASan)
      run: |
        cmake -B build-asan \
          -DCMAKE_CXX_FLAGS="-fsanitize=address -g" \
          -DCMAKE_C_FLAGS="-fsanitize=address -g" \
          -DCMAKE_BUILD_TYPE=Debug
    
    - name: Build
      run: cmake --build build-asan -j$(nproc)
    
    - name: Run tests
      run: |
        cd build-asan
        ctest --output-on-failure \
          --exclude-regex "performance"
```

---

## 发布流程

### 版本号规范

遵循 [Semantic Versioning](https://semver.org/)：

```
MAJOR.MINOR.PATCH
  |     |     |
  |     |     └─ 向后兼容的 bug 修复
  |     └─────── 向后兼容的新功能
  └───────────── 不兼容的 API 变更
```

示例：
- `0.1.0` - 初始版本
- `0.1.1` - bug 修复
- `0.2.0` - 新功能
- `1.0.0` - 稳定版本

### 创建发布

```bash
# 1. 更新版本号（CMakeLists.txt）
project(UsrLinuxEmu VERSION 0.2.0)

# 2. 更新 CHANGELOG.md
# 添加新版本的变更日志

# 3. 提交并打标签
git add CMakeLists.txt CHANGELOG.md
git commit -m "chore: bump version to 0.2.0"
git tag -a v0.2.0 -m "Release v0.2.0"

# 4. 推送标签
git push origin v0.2.0
```

### 自动发布工作流

创建 `.github/workflows/release.yml`：

```yaml
name: Release

on:
  push:
    tags:
      - 'v*'

jobs:
  release:
    runs-on: ubuntu-latest
    
    steps:
    - uses: actions/checkout@v4
    
    - name: Build
      run: |
        cmake -B build -DCMAKE_BUILD_TYPE=Release
        cmake --build build -j$(nproc)
    
    - name: Create Release
      id: create_release
      uses: actions/create-release@v1
      env:
        GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}
      with:
        tag_name: ${{ github.ref }}
        release_name: Release ${{ github.ref }}
        draft: false
        prerelease: false
    
    - name: Upload Binary
      uses: actions/upload-release-asset@v1
      env:
        GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}
      with:
        upload_url: ${{ steps.create_release.outputs.upload_url }}
        asset_path: ./build/bin/cli_tool
        asset_name: cli_tool-linux-x64
        asset_content_type: application/octet-stream
```

---

## 构建优化

### 缓存依赖

```yaml
- name: Cache dependencies
  uses: actions/cache@v3
  with:
    path: |
      ~/.cache/apt
      ~/vcpkg/installed
    key: ${{ runner.os }}-deps-${{ hashFiles('vcpkg.json') }}
    restore-keys: |
      ${{ runner.os }}-deps-
```

### 并行编译

```yaml
- name: Build
  run: cmake --build build -j$(nproc)
```

### 减少测试时间

```yaml
- name: Run tests
  run: |
    ctest --output-on-failure \
          --parallel $(nproc) \
          --exclude-regex "performance"  # 跳过耗时的性能测试
```

---

## 故障排查

### CI 构建失败

**问题**: 构建在 GitHub Actions 失败，但本地成功

**解决步骤**:

```bash
# 1. 使用相同的 Ubuntu 版本
# GitHub Actions 使用 ubuntu-latest (Ubuntu 22.04)

# 2. 使用相同的编译器版本
gcc --version
# 对比 GitHub Actions 的版本

# 3. 启用详细日志
cmake -DCMAKE_VERBOSE_MAKEFILE=ON ..

# 4. 检查依赖
ldd ./build/bin/cli_tool
```

### 测试超时

**问题**: 测试在 CI 中超时（默认 6 小时）

**解决步骤**:

```bash
# 1. 本地测试执行时间
time ctest --output-on-failure

# 2. 识别慢测试
ctest --output-on-failure -T test

# 3. 设置测试超时
set_tests_properties(slow_test PROPERTIES TIMEOUT 300)

# 4. 排除性能测试
ctest --exclude-regex "performance"
```

### 内存不足

**问题**: CI 构建内存不足（GitHub Actions 限制：7GB RAM）

**解决步骤**:

```bash
# 1. 减少并行编译任务数
cmake --build build -j2

# 2. 使用 Release 模式（内存更少）
cmake -DCMAKE_BUILD_TYPE=Release ..

# 3. 排除大型测试
ctest --exclude-regex "large_memory"
```

---

## 环境变量

### CI 环境变量

```yaml
env:
  DEBIAN_FRONTEND: noninteractive
  CI: true
  BUILD_TYPE: Release
```

### 条件设置

```yaml
- name: Set environment
  run: |
    if [ "${{ matrix.c_compiler }}" == "clang" ]; then
      echo "CC=clang" >> $GITHUB_ENV
      echo "CXX=clang++" >> $GITHUB_ENV
    fi
```

---

## 最佳实践

### 1. 快速反馈

- PR 检查应在 10 分钟内完成
- 主分支构建应在 30 分钟内完成
- 排除耗时的性能测试

### 2. 可重现性

- 固定依赖版本
- 使用容器或指定 OS 版本
- 记录所有环境变量

### 3. 错误处理

- 设置 `fail-fast: false` 以获取所有矩阵结果
- 使用 `--output-on-failure` 显示测试错误
- 保存日志和测试报告

### 4. 安全性

- 不提交敏感信息（使用 secrets）
- 定期更新依赖
- 启用安全扫描（CodeQL）

---

## 相关文档

- [构建系统](build_system.md) - CMake 配置
- [测试指南](testing_guide.md) - 单元测试编写
- [代码风格](../03-development/coding-style.md) - 编码规范

---

**最后更新**: 2026-06-16 (commit `374d463`)
