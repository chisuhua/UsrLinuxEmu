#!/usr/bin/env bash
# stage-plugin.sh — 从指定 build 目录重新构建 GPU plugin 并校验 sanitizer 符号
#
# Usage:
#   scripts/stage-plugin.sh <build-dir>
#
# Examples:
#   scripts/stage-plugin.sh build-asan
#   scripts/stage-plugin.sh build-ubsan
#   scripts/stage-plugin.sh build-tsan
#   scripts/stage-plugin.sh build-asan-ubsan
#   scripts/stage-plugin.sh build-default
#
# Exit codes:
#   0 — staging successful, plugin symbols match expected sanitizer config
#   1 — usage error (missing or invalid build dir)
#   2 — plugin symlink / build target missing
#   3 — symbol mismatch (plugin symbols don't match expected config)
#   4 — other error

set -euo pipefail

# ── Argument validation ──────────────────────────────────────────────
if [ $# -ne 1 ]; then
    echo "Usage: $0 <build-dir>" >&2
    exit 1
fi

BUILD_DIR="$1"
PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PLUGIN_PATH="${PROJECT_ROOT}/plugins/plugin_gpu_driver.so"

if [ ! -d "${PROJECT_ROOT}/${BUILD_DIR}" ]; then
    echo "ERROR: build directory '${BUILD_DIR}' not found at ${PROJECT_ROOT}/${BUILD_DIR}" >&2
    echo "       Run cmake configure first, e.g.:" >&2
    echo "       cmake -DENABLE_ASAN=ON -DCMAKE_BUILD_TYPE=Debug -B ${BUILD_DIR} -S ." >&2
    exit 1
fi

# ── Detect expected sanitizer config from build dir name ──────────────
DETECT_ASAN=0
DETECT_UBSAN=0
DETECT_TSAN=0

case "$BUILD_DIR" in
    build-asan-ubsan)
        DETECT_ASAN=1
        DETECT_UBSAN=1
        ;;
    build-asan)
        DETECT_ASAN=1
        ;;
    build-ubsan)
        DETECT_UBSAN=1
        ;;
    build-tsan)
        DETECT_TSAN=1
        ;;
    build-default|build)
        # no sanitizer expected
        ;;
    *)
        echo "Unknown build dir pattern: ${BUILD_DIR}" >&2
        echo "Expected one of: build-asan, build-ubsan, build-tsan, build-asan-ubsan, build-default" >&2
        exit 1
        ;;
esac

# ── Rebuild the plugin ───────────────────────────────────────────────
echo "[stage] Rebuilding gpu_driver_plugin from ${BUILD_DIR}..."
cd "$PROJECT_ROOT"
cmake --build "$BUILD_DIR" --target gpu_driver_plugin -j$(nproc 2>/dev/null || echo 4) || {
    echo "ERROR: cmake --build failed for target gpu_driver_plugin" >&2
    exit 2
}

# POST_BUILD copy only runs when the target actually rebuilds; force copy here
# so that staging always writes the correct build dir's artifact to the shared path.
SRC_PLUGIN="${PROJECT_ROOT}/${BUILD_DIR}/plugins/gpu_driver/gpu_driver_plugin.so"
if [ -f "$SRC_PLUGIN" ]; then
    cp "$SRC_PLUGIN" "$PLUGIN_PATH"
    echo "[stage] Copied ${SRC_PLUGIN} -> ${PLUGIN_PATH}"
fi

if [ ! -f "$PLUGIN_PATH" ]; then
    echo "ERROR: plugin not found at ${PLUGIN_PATH} after build" >&2
    exit 2
fi

# ── Symbol verification ──────────────────────────────────────────────
echo "[stage] Verifying sanitizer symbols in ${PLUGIN_PATH}..."

# Detect actual symbols
HAS_ASAN=0
HAS_UBSAN=0
HAS_TSAN=0

if nm -D "$PLUGIN_PATH" 2>/dev/null | grep -q '__asan_init'; then
    HAS_ASAN=1
fi
if nm -D "$PLUGIN_PATH" 2>/dev/null | grep -q '__ubsan_handle'; then
    HAS_UBSAN=1
fi
if nm -D "$PLUGIN_PATH" 2>/dev/null | grep -q '__tsan_init'; then
    HAS_TSAN=1
fi

echo "  ASan  symbols: $( [ $HAS_ASAN -eq 1 ] && echo 'PRESENT' || echo 'absent' )"
echo "  UBSan symbols: $( [ $HAS_UBSAN -eq 1 ] && echo 'PRESENT' || echo 'absent' )"
echo "  TSan  symbols: $( [ $HAS_TSAN -eq 1 ] && echo 'PRESENT' || echo 'absent' )"

# ── Match check ──────────────────────────────────────────────────────
MISMATCH=0

if [ "$DETECT_ASAN" -eq 1 ] && [ "$HAS_ASAN" -ne 1 ]; then
    echo "ERROR: ASan expected but not found in plugin" >&2
    MISMATCH=1
fi
if [ "$DETECT_ASAN" -eq 0 ] && [ "$HAS_ASAN" -ne 0 ]; then
    echo "ERROR: ASan NOT expected but found in plugin" >&2
    MISMATCH=1
fi

if [ "$DETECT_UBSAN" -eq 1 ] && [ "$HAS_UBSAN" -ne 1 ]; then
    echo "ERROR: UBSan expected but not found in plugin" >&2
    MISMATCH=1
fi
if [ "$DETECT_UBSAN" -eq 0 ] && [ "$HAS_UBSAN" -ne 0 ]; then
    echo "ERROR: UBSan NOT expected but found in plugin" >&2
    MISMATCH=1
fi

if [ "$DETECT_TSAN" -eq 1 ] && [ "$HAS_TSAN" -ne 1 ]; then
    echo "ERROR: TSan expected but not found in plugin" >&2
    MISMATCH=1
fi
if [ "$DETECT_TSAN" -eq 0 ] && [ "$HAS_TSAN" -ne 0 ]; then
    echo "ERROR: TSan NOT expected but found in plugin" >&2
    MISMATCH=1
fi

if [ "$MISMATCH" -eq 1 ]; then
    echo "FAIL: Plugin symbol mismatch — rebuild or use correct build directory." >&2
    exit 3
fi

echo "[stage] OK — plugin at ${PLUGIN_PATH} matches ${BUILD_DIR} config."
exit 0
