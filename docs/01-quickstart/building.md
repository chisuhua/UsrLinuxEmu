# 构建指南

本指南介绍如何编译 UsrLinuxEmu 项目。

## 快速构建

最简单的方式是使用项目提供的构建脚本：

```bash
# 在项目根目录执行
./build.sh
```

这会创建一个 `build/` 目录并编译项目。

## 手动构建

### 基本构建步骤

```bash
# 1. 进入项目根目录
cd /path/to/UsrLinuxEmu

# 2. 创建构建目录
mkdir -p build && cd build

# 3. 配置 CMake
cmake ..

# 4. 编译项目
make -j$(nproc)
```

`-j$(nproc)` 参数会使用所有可用的 CPU 核心进行并行编译，加快构建速度。

### 构建选项

CMake 支持多种配置选项：

```bash
# Debug 构建（包含调试符号，无优化）
cmake .. -DCMAKE_BUILD_TYPE=Debug

# Release 构建（优化，无调试符号）
cmake .. -DCMAKE_BUILD_TYPE=Release

# 启用测试
cmake .. -DBUILD_TESTS=ON

# 启用代码覆盖率
cmake .. -DCOVERAGE=ON -DBUILD_TESTS=ON

# 自定义安装路径
cmake .. -DCMAKE_INSTALL_PREFIX=/opt/UsrLinuxEmu
```

### 常用构建配置组合

```bash
# 开发配置（Debug + 测试）
cmake .. -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON

# 生产配置（Release，无测试）
cmake .. -DCMAKE_BUILD_TYPE=Release

# 完整配置（Debug + 测试 + 覆盖率）
cmake .. -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON -DCOVERAGE=ON
```

## 编译输出

编译完成后，输出文件位于 `build/bin/` 目录：

```
build/bin/
├── cli_tool              # CLI 工具
├── test_gpu_submit       # 测试程序
├── test_gpu_memory       # 测试程序
└── ...                   # 其他测试和工具
```

## 运行程序

```bash
# 运行 CLI 工具
./build/bin/cli_tool

# 或使用提供的脚本
./run_cli.sh

# 运行测试
cd build
make test

# 或运行特定测试
./bin/test_gpu_submit
```

## 清理构建

```bash
# 清理编译产物（保留 CMake 配置）
make clean

# 完全清理（删除 build 目录）
cd .. && rm -rf build

# 重新构建
./build.sh
```

## 构建插件

如果项目包含插件：

```bash
# 进入插件目录
cd plugins

# 编译插件
make

# 或使用 CMake
cd .. && mkdir build-plugins && cd build-plugins
cmake ../plugins
make -j$(nproc)
```

## 故障排除

### 问题：CMake 找不到编译器

**错误信息**:
```
CMake Error: CMAKE_CXX_COMPILER not set
```

**解决方案**:
```bash
# 安装编译器
sudo apt install build-essential  # Ubuntu/Debian

# 或指定编译器
export CXX=g++
export CC=gcc
cmake ..
```

### 问题：C++17 不支持

**错误信息**:
```
error: 'std::filesystem' is not supported
```

**解决方案**:
```bash
# 检查编译器版本
g++ --version  # 需要 GCC 7+ 或 Clang 5+

# 升级编译器
sudo apt install gcc-9 g++-9
```

### 问题：找不到 GTest

**错误信息**:
```
Could not find GTest
```

**解决方案**:
```bash
# Ubuntu/Debian
sudo apt install libgtest-dev
cd /usr/src/gtest
sudo cmake CMakeLists.txt
sudo make
sudo cp *.a /usr/lib

# 或者禁用测试构建
cmake .. -DBUILD_TESTS=OFF
```

## 下一步

构建完成后，继续阅读：

- [第一个示例](first-example.md) - 运行你的第一个 GPU 示例
- [测试指南](../04-building/testing-guide.md) - 了解如何运行测试

---

**最后更新**: 2026-03-23
