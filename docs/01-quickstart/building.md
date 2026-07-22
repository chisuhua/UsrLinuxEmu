# 构建指南

本指南介绍如何编译 UsrLinuxEmu 项目。

## 快速构建

最简单的方式是使用项目提供的构建脚本：

```bash
# 在项目根目录执行
./build.sh              # 构建所有目标
./build.sh test         # 构建 + 运行所有测试
./build.sh clean        # 清理构建产物
```

## 手动构建

### 基本构建步骤

```bash
# 1. 进入项目根目录
cd /workspace/project/UsrLinuxEmu

# 2. 创建构建目录并配置 CMake
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..

# 3. 编译项目
make -j$(nproc)
```

`-j$(nproc)` 会使用所有可用 CPU 核心并行编译。

### 构建选项

```bash
# Debug 构建（包含调试符号，默认推荐）
cmake .. -DCMAKE_BUILD_TYPE=Debug

# Release 构建（优化，无调试符号）
cmake .. -DCMAKE_BUILD_TYPE=Release

# 启用 AddressSanitizer（内存错误检测）
cmake .. -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON

# 启用 UndefinedBehaviorSanitizer
cmake .. -DCMAKE_BUILD_TYPE=Debug -DENABLE_UBSAN=ON

# 启用 ThreadSanitizer（需 Clang）
cmake .. -DCMAKE_BUILD_TYPE=Debug -DENABLE_TSAN=ON
```

Sanitizer 构建也可以通过环境变量简化：

```bash
SANITIZER=asan  ./build.sh test    # ASan 构建 + 运行测试
SANITIZER=ubsan ./build.sh test    # UBSan 构建 + 运行测试
SANITIZER=tsan  ./build.sh test    # TSan 构建 + 运行测试
```

## 编译输出

编译完成后，输出文件位于：

| 路径 | 说明 |
|------|------|
| `build/bin/cli` | CLI 交互工具 |
| `build/bin/test_gpu_ioctl_standalone` | GPU IOCTL 测试 |
| `build/bin/test_va_space_standalone` | VA Space 测试 |
| `build/bin/test_*_standalone` | 其他 Catch2 测试（30+） |
| `build/lib/libkernel.so` | 内核框架 SHARED 库 |
| `build/plugins/gpu_driver/gpu_driver_plugin.so` | GPU 驱动插件 |
| `docs/api/index.html` | Doxygen API 文档（`make doxygen`） |

## 运行程序

```bash
# 运行 CLI 工具
./build/bin/cli

# 运行所有测试（从项目根目录，插件使用相对路径）
cd /workspace/project/UsrLinuxEmu
cd build && ctest --output-on-failure && cd ..

# 运行特定测试
./build/bin/test_gpu_ioctl_standalone
./build/bin/test_va_space_standalone
```

## 清理构建

```bash
# 使用构建脚本（推荐）
./build.sh clean

# 或手动删除
rm -rf build
```

## 生成 API 文档

```bash
# CMake 方式（需要 Doxygen 已安装）
cmake .. && make doxygen  # 输出到 docs/api/

# 直接调用（不需要 CMake 配置）
cd /workspace/project/UsrLinuxEmu
doxygen docs/Doxyfile
```

## 故障排除

### 问题：CMake 找不到编译器

**错误信息**: `CMake Error: CMAKE_CXX_COMPILER not set`

**解决方案**:
```bash
sudo apt install build-essential  # Ubuntu/Debian
```

### 问题：C++17 不支持

**错误信息**: `error: 'std::filesystem' is not supported`

**解决方案**: GCC 7+ / Clang 5+ 必须。升级：
```bash
sudo apt install g++-11
```

### 问题：测试链接失败

**错误信息**: `undefined reference to Catch::TestRegistrar`

**解决方案**: Catch2 已 vendored 在 `tests/catch_amalgamated.{hpp,cpp}`。如果新加测试文件，确保在 `tests/CMakeLists.txt` 中正确注册。

### 问题：`make doxygen` 目标不存在

CMake 会在配置阶段检测 Doxygen。如果已安装但目标不存在，重新 `cmake ..` 即可。如果未安装：

```bash
sudo apt install doxygen graphviz
```

## 下一步

- [第一个示例](first-example.md) — 运行 GPU 端到端示例
- [测试指南](../04-building/testing_guide.md) — 通过 Catch2 运行和编写测试

---

**最后更新**: 2026-07-22
