#!/bin/bash
# 构建脚本 - UsrLinuxEmu
#
# 使用方式:
#   ./build.sh              # 构建所有 (主程序 + 插件)
#   ./build.sh clean        # 清理构建产物
#   ./build.sh test         # 构建并运行测试
#   ./build.sh <target>     # 构建特定目标 (如 gpu_driver_plugin)
#
# 注意: 插件通过 CMake 构建系统自动管理，无需单独构建

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"

usage() {
    echo "Usage: $0 [clean|test|<cmake-target>]"
    echo ""
    echo "Examples:"
    echo "  $0              # 构建所有"
    echo "  $0 clean        # 清理"
    echo "  $0 test         # 构建并运行测试"
    echo "  $0 gpu_driver_plugin  # 只构建 GPU 插件"
    exit 1
}

# 清理
clean() {
    echo "=== Cleaning build directory ==="
    rm -rf "$BUILD_DIR"
    echo "Done."
}

# 配置和构建
build() {
    local target="$1"

    # 创建 build 目录 (如果不存在)
    if [ ! -d "$BUILD_DIR" ]; then
        echo "=== Creating build directory ==="
        mkdir -p "$BUILD_DIR"
    fi

    cd "$BUILD_DIR"

    # 配置 (如果需要)
    if [ ! -f Makefile ]; then
        echo "=== Configuring CMake ==="
        cmake -DCMAKE_BUILD_TYPE=Debug ..
    fi

    # 构建
    if [ -n "$target" ]; then
        echo "=== Building $target ==="
        make -j"$(nproc)" "$target"
    else
        echo "=== Building all ==="
        make -j"$(nproc)"
    fi
}

# 测试
run_tests() {
    echo "=== Running tests ==="
    ctest -j"$(nproc)" --output-on-failure
}

# 主逻辑
case "${1:-}" in
    clean)
        clean
        ;;
    test)
        build
        run_tests
        ;;
    -h|--help|help)
        usage
        ;;
    *)
        build "$1"
        ;;
esac
