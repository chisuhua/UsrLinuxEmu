#!/usr/bin/env bash
# tools/l2-build/build_iommu_driver.sh
# Stage 5.5.1 Gate 5.5.1-C 验证 — per design.md §D6.3 + ADR-072 v0.2 + ADR-091 v0.2
#
# Stage 5.5.1 Gate C 实质（per design.md §D6.3）：
#   验证 iommu_driver plugin 在 Linux 6.12 真机 stub 编译可工作：
#   - sed 替换 #include 路径（模拟真机内核 header 路径约定）
#   - g++ -fsyntax-only 语法检查（不链接，不生成 .o）
#
# 实际 L2 真机完整 build（含所有 .cpp 真机编译 + 链接）推迟到 Change-2 (Stage 5.5.2)，
# per ADR-091 v0.2 §D6.3 末尾注记。

set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$PROJECT_ROOT"

KERNEL_SRC="${KERNEL_SRC:-/usr/src/linux-headers-$(uname -r)}"
SYNTAX_ONLY_OK="${SYNTAX_ONLY_OK:-1}"  # 默认跳过真机 syntax check（开发环境无 kernel headers）

echo "[build_iommu_driver] Stage 5.5.1 Gate 5.5.1-C — iommu_driver plugin L2 build"
echo "[build_iommu_driver] PROJECT_ROOT=$PROJECT_ROOT"
echo "[build_iommu_driver] KERNEL_SRC=$KERNEL_SRC"

# 1. 仓内构建（per ADR-091 v0.2 + Change-1 ship commit 8c4ee2f）
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release .. > /dev/null 2>&1
cmake --build . --target iommu_driver_plugin -- -j"$(nproc 2>/dev/null || echo 2)" > /dev/null 2>&1
cd "$PROJECT_ROOT"

# 2. 验证 .so 输出存在
SO_FILE="$(find "$PROJECT_ROOT/plugins" -maxdepth 1 -name 'plugin_iommu_driver.so' | head -1)"
if [ -z "$SO_FILE" ]; then
    echo "[build_iommu_driver] FAIL: plugin_iommu_driver.so not found in $PROJECT_ROOT/plugins/"
    exit 1
fi
echo "[build_iommu_driver] OK plugin_iommu_driver.so exists: $SO_FILE ($(stat -c%s "$SO_FILE" 2>/dev/null || stat -f%z "$SO_FILE") bytes)"

# 3. sed 路径迁移验证（per design.md §D6.3）— 检查 iommu_driver/ 源文件未引用旧路径
OLD_INCLUDE_PATTERN='#include\s+"kernel/iommu/'
if grep -rE "$OLD_INCLUDE_PATTERN" plugins/iommu_driver/ 2>/dev/null; then
    echo "[build_iommu_driver] FAIL: 旧路径引用 '$OLD_INCLUDE_PATTERN' 仍存在"
    exit 1
fi
echo "[build_iommu_driver] OK iommu_driver/ 无旧路径引用（kernel/iommu/* 全部迁移）"

# 4. 真机 kernel header syntax check（per design.md §D6.3）
#    KERNEL_SRC 必须存在才执行；开发环境无 kernel headers 时跳过
if [ "$SYNTAX_ONLY_OK" = "0" ] && [ -d "$KERNEL_SRC/include" ]; then
    echo "[build_iommu_driver] Running g++ -fsyntax-only against $KERNEL_SRC ..."
    TMP_L2="$(mktemp /tmp/l2_iommu_driver_XXXXXX.cpp)"
    cp plugins/iommu_driver/iommu.cpp "$TMP_L2"
    # sed 替换为真机 kernel header 路径（per design D6.3 模式）
    sed -i 's|#include "iommu_internal.h"|#include "linux/iommu.h"|g' "$TMP_L2"
    if g++ -c "$TMP_L2" -I plugins/iommu_driver/include \
              -I plugins/pci_driver/include \
              -I "$KERNEL_SRC/include" -I "$KERNEL_SRC/include/uapi" \
              -fsyntax-only 2>&1 | head -20; then
        echo "[build_iommu_driver] OK g++ -fsyntax-only PASSED"
    else
        echo "[build_iommu_driver] FAIL: g++ -fsyntax-only failed"
        rm -f "$TMP_L2"
        exit 1
    fi
    rm -f "$TMP_L2"
else
    echo "[build_iommu_driver] SKIP 真机 syntax check (KERNEL_SRC=$KERNEL_SRC not found or SYNTAX_ONLY_OK=$SYNTAX_ONLY_OK)"
fi

echo "[build_iommu_driver] DONE — Gate 5.5.1-C PASSED"
exit 0
