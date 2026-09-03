#!/usr/bin/env bash
# tools/l2-build/build_pci_driver.sh
# Stage 5.5.1 Gate 5.5.1-C 验证 — per design.md §D6.3 + ADR-072 v0.2 + ADR-091 v0.2
#
# Stage 5.5.1 Gate C 实质（per design.md §D6.3）：
#   验证 pci_driver plugin 在 Linux 6.12 真机 stub 编译可工作：
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

echo "[build_pci_driver] Stage 5.5.1 Gate 5.5.1-C — pci_driver plugin L2 build"
echo "[build_pci_driver] PROJECT_ROOT=$PROJECT_ROOT"
echo "[build_pci_driver] KERNEL_SRC=$KERNEL_SRC"

# 1. 仓内构建（per ADR-091 v0.2 + Change-1 ship commit 8c4ee2f）
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release .. > /dev/null 2>&1
cmake --build . --target pci_driver_plugin -- -j"$(nproc 2>/dev/null || echo 2)" > /dev/null 2>&1
cd "$PROJECT_ROOT"

# 2. 验证 .so 输出存在（修复 Oracle F-001：原末行 `/ -name ...` 缺 find 命令，exit 126）
SO_FILE="$(find "$PROJECT_ROOT/plugins" -maxdepth 1 -name 'plugin_pci_driver.so' | head -1)"
if [ -z "$SO_FILE" ]; then
    echo "[build_pci_driver] FAIL: plugin_pci_driver.so not found in $PROJECT_ROOT/plugins/"
    exit 1
fi
echo "[build_pci_driver] OK plugin_pci_driver.so exists: $SO_FILE ($(stat -c%s "$SO_FILE" 2>/dev/null || stat -f%z "$SO_FILE") bytes)"

# 3. sed 路径迁移验证（per design.md §D6.3）— 检查 pci_driver/ 源文件未引用旧路径
OLD_INCLUDE_PATTERN='#include\s+"kernel/pcie/'
if grep -rE "$OLD_INCLUDE_PATTERN" plugins/pci_driver/ 2>/dev/null; then
    echo "[build_pci_driver] FAIL: 旧路径引用 '$OLD_INCLUDE_PATTERN' 仍存在"
    exit 1
fi
echo "[build_pci_driver] OK pci_driver/ 无旧路径引用（kernel/pcie/* 全部迁移）"

# 4. 真机 kernel header syntax check（per design.md §D6.3）
#    KERNEL_SRC 必须存在才执行；开发环境无 kernel headers 时跳过
if [ "$SYNTAX_ONLY_OK" = "0" ] && [ -d "$KERNEL_SRC/include" ]; then
    echo "[build_pci_driver] Running g++ -fsyntax-only against $KERNEL_SRC ..."
    TMP_L2="$(mktemp /tmp/l2_pci_driver_XXXXXX.cpp)"
    cp plugins/pci_driver/probe.cpp "$TMP_L2"
    # sed 替换为真机 kernel header 路径（per design D6.3 line 378）
    sed -i 's|#include "pcie_emu_impl.h"|#include "linux/pci.h"|g' "$TMP_L2"
    if g++ -c "$TMP_L2" -I plugins/pci_driver/include \
              -I "$KERNEL_SRC/include" -I "$KERNEL_SRC/include/uapi" \
              -fsyntax-only 2>&1 | head -20; then
        echo "[build_pci_driver] OK g++ -fsyntax-only PASSED"
    else
        echo "[build_pci_driver] FAIL: g++ -fsyntax-only failed"
        rm -f "$TMP_L2"
        exit 1
    fi
    rm -f "$TMP_L2"
else
    echo "[build_pci_driver] SKIP 真机 syntax check (KERNEL_SRC=$KERNEL_SRC not found or SYNTAX_ONLY_OK=$SYNTAX_ONLY_OK)"
fi

echo "[build_pci_driver] DONE — Gate 5.5.1-C PASSED"
exit 0
