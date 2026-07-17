#!/bin/bash
# 构建脚本 - UsrLinuxEmu
#
# 使用方式:
#   ./build.sh              # 构建所有 (主程序 + 插件)
#   ./build.sh clean        # 清理构建产物
#   ./build.sh test         # 构建并运行测试
#   ./build.sh <target>     # 构建特定目标 (如 gpu_driver_plugin)
#
# 环境变量:
#   SANITIZER=asan|ubsan|tsan|asan-ubsan|default  选择 sanitizer 配置
#   SANITIZER=asan   ./build.sh test   →  ASan build + ctest
#   SANITIZER=ubsan  ./build.sh test   →  UBSan build + ctest
#   SANITIZER=tsan   ./build.sh test   →  TSan build + ctest (需 Clang)
#
# 注意: 插件通过 CMake 构建系统自动管理，无需单独构建

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# ── Sanitizer selection ──────────────────────────────────────────────
# 未设置 SANITIZER (或空字符串) → 行为不变: BUILD_DIR=build/
# 显式 SANITIZER=default      → 隔离构建: BUILD_DIR=build-default/
case "${SANITIZER:-}" in
  asan)
    SANITIZER_FLAGS="-DENABLE_ASAN=ON -DCMAKE_BUILD_TYPE=Debug"
    BUILD_DIR="$SCRIPT_DIR/build-asan"
    ;;
  ubsan)
    SANITIZER_FLAGS="-DENABLE_UBSAN=ON -DCMAKE_BUILD_TYPE=Debug"
    BUILD_DIR="$SCRIPT_DIR/build-ubsan"
    ;;
  tsan)
    SANITIZER_FLAGS="-DENABLE_TSAN=ON -DCMAKE_BUILD_TYPE=Debug"
    BUILD_DIR="$SCRIPT_DIR/build-tsan"
    ;;
  asan-ubsan)
    SANITIZER_FLAGS="-DENABLE_ASAN=ON -DENABLE_UBSAN=ON -DCMAKE_BUILD_TYPE=Debug"
    BUILD_DIR="$SCRIPT_DIR/build-asan-ubsan"
    ;;
  "")
    SANITIZER_FLAGS="-DENABLE_ASAN=OFF -DENABLE_UBSAN=OFF -DENABLE_TSAN=OFF -DCMAKE_BUILD_TYPE=Debug"
    BUILD_DIR="$SCRIPT_DIR/build"
    ;;
  default)
    SANITIZER_FLAGS="-DENABLE_ASAN=OFF -DENABLE_UBSAN=OFF -DENABLE_TSAN=OFF -DCMAKE_BUILD_TYPE=Debug"
    BUILD_DIR="$SCRIPT_DIR/build-default"
    ;;
  *)
    echo "ERROR: Unknown SANITIZER='${SANITIZER}'. Valid: asan, ubsan, tsan, asan-ubsan, default" >&2
    exit 1
    ;;
esac

usage() {
    echo "Usage: $0 [clean|test|<cmake-target>]"
    echo ""
    echo "Examples:"
    echo "  $0              # 构建所有 (default build)"
    echo "  $0 clean        # 清理"
    echo "  $0 test         # 构建并运行测试"
    echo "  $0 gpu_driver_plugin  # 只构建 GPU 插件"
    echo ""
    echo "With sanitizer:"
    echo "  SANITIZER=asan  $0 test   # ASan build + ctest"
    echo "  SANITIZER=ubsan $0 test   # UBSan build + ctest"
    echo "  SANITIZER=tsan  $0 test   # TSan build + ctest"
    exit 1
}

clean() {
    echo "=== Cleaning build directory ($BUILD_DIR) ==="
    rm -rf "$BUILD_DIR"
    echo "Done."
}

build() {
    local target="$1"

    if [ ! -d "$BUILD_DIR" ]; then
        echo "=== Creating build directory ($BUILD_DIR) ==="
        mkdir -p "$BUILD_DIR"
    fi

    cd "$BUILD_DIR"

    # Force reconfigure if CMakeCache sanitizer flags don't match current selection
    if [ -f CMakeCache.txt ]; then
        local cache_asan=$(grep '^ENABLE_ASAN:' CMakeCache.txt 2>/dev/null | cut -d= -f2)
        local cache_ubsan=$(grep '^ENABLE_UBSAN:' CMakeCache.txt 2>/dev/null | cut -d= -f2)
        local cache_tsan=$(grep '^ENABLE_TSAN:' CMakeCache.txt 2>/dev/null | cut -d= -f2)
        local want_asan="OFF"; local want_ubsan="OFF"; local want_tsan="OFF"
        case "${SANITIZER:-}" in
            asan) want_asan="ON" ;;
            ubsan) want_ubsan="ON" ;;
            tsan) want_tsan="ON" ;;
            asan-ubsan) want_asan="ON"; want_ubsan="ON" ;;
        esac
        if [ "$cache_asan" != "$want_asan" ] || [ "$cache_ubsan" != "$want_ubsan" ] || [ "$cache_tsan" != "$want_tsan" ]; then
            echo "=== Sanitizer config changed, reconfiguring ==="
            cmake $SANITIZER_FLAGS ..
        fi
    fi

    if [ ! -f Makefile ]; then
        echo "=== Configuring CMake ==="
        cmake $SANITIZER_FLAGS ..
    fi

    if [ -n "$target" ]; then
        echo "=== Building $target ==="
        make -j"$(nproc)" "$target"
    else
        echo "=== Building all ==="
        make -j"$(nproc)"
    fi
}

run_tests() {
    echo "=== Running tests ==="
    ctest -j"$(nproc)" --output-on-failure
}

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
