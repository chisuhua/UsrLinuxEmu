# 安装指南

本指南将帮助你在 Linux 系统上安装 UsrLinuxEmu 及其依赖。

## 系统要求

### 最低要求

- **操作系统**: Linux (推荐 Ubuntu 18.04+)
- **编译器**: GCC 7+ 或 Clang 5+ (支持 C++17)
- **构建工具**: CMake ≥ 3.14
- **内存**: 至少 2GB 可用内存
- **磁盘空间**: 至少 500MB

### 推荐配置

- **操作系统**: Ubuntu 20.04 LTS 或更新版本
- **编译器**: GCC 9+ 或 Clang 10+
- **内存**: 4GB+
- **磁盘空间**: 1GB+ (包含测试和调试符号)

## 安装依赖

### Ubuntu/Debian 系统

```bash
# 更新包列表
sudo apt update

# 安装构建依赖
sudo apt install -y \
    build-essential \
    cmake \
    git \
    libgtest-dev \
    pkg-config

# (可选) 安装开发工具
sudo apt install -y \
    gdb \
    valgrind \
    clang-tidy \
    clang-format
```

### CentOS/RHEL 系统

```bash
# 安装开发工具组
sudo yum groupinstall "Development Tools"

# 安装 CMake 和 Git
sudo yum install -y cmake git

# (可选) 安装额外工具
sudo yum install -y gdb valgrind
```

### Fedora 系统

```bash
sudo dnf install -y \
    gcc \
    gcc-c++ \
    cmake \
    git \
    gtest-devel \
    gdb \
    valgrind
```

### Arch Linux

```bash
sudo pacman -S \
    base-devel \
    cmake \
    git \
    gtest \
    gdb \
    valgrind
```

## 验证安装

安装完成后，验证工具是否正确安装：

```bash
# 检查编译器版本
g++ --version    # 应显示 GCC 7+ 或更高
clang++ --version # 如果有 Clang

# 检查 CMake 版本
cmake --version   # 应显示 3.14+

# 检查 Git
git --version
```

## 下一步

依赖安装完成后，继续阅读：

- [构建指南](building.md) - 编译项目
- [第一个示例](first-example.md) - 运行示例程序

---

**故障排除**

| 问题 | 解决方案 |
|------|----------|
| CMake 版本过低 | 从 https://cmake.org/download/ 下载最新版本 |
| 编译器不支持 C++17 | 升级 GCC 到 7+ 或 Clang 到 5+ |
| 找不到 libgtest | 安装 libgtest-dev 包并编译 |

**最后更新**: 2026-03-23
