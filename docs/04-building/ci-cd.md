# CI/CD 配置

本文档介绍 UsrLinuxEmu 项目的持续集成和持续部署（CI/CD）配置。

**最后更新**: 2026-03-24  
**作者**: UsrLinuxEmu Team

---

## 概述

UsrLinuxEmu 使用 **GitHub Actions** 作为 CI/CD 平台，自动化执行以下任务：

- ✅ 多平台编译（Linux GCC/Clang）
- ✅ 单元测试
- ✅ 代码风格检查
- ✅ 静态分析（clang-tidy）
- ✅ 文档构建
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

#### 构建矩阵

```yaml
matrix:
  os: [ubuntu-latest]
  build_type: [Release]
  c_compiler: [gcc, clang]
```

当前配置：
- **OS**: Ubuntu Latest
- **构建类型**: Release
- **编译器**: GCC 和 Clang

#### 工作流程步骤

```yaml
steps:
1. Checkout code              # 检出代码
2. Configure CMake            # 配置 CMake
3. Build                      # 编译项目
4. Test                       # 运行测试
```

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

创建 `.github/workflows/docs-check.yml`：

```yaml
name: Documentation Check

on:
  push:
    branches: [ "main" ]
    paths:
      - 'docs/**'
  pull_request:
    branches: [ "main" ]
    paths:
      - 'docs/**'

jobs:
  docs:
    runs-on: ubuntu-latest
    
    steps:
    - uses: actions/checkout@v4
    
    - name: Check Markdown links
      uses: gaurav-nelson/github-action-markdown-link-check@v1
      with:
        use-quiet-mode: 'yes'
        config-file: '.github/link-check-config.json'
    
    - name: Check Markdown formatting
      run: |
        npm install -g markdownlint-cli
        markdownlint docs/
```

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
        sudo apt-get install -y g++ libgtest-dev
    
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

- [构建系统](build-system.md) - CMake 配置
- [测试指南](testing-guide.md) - 单元测试编写
- [代码风格](../03-development/coding-style.md) - 编码规范

---

**最后更新**: 2026-03-24
