#!/usr/bin/env bash
# regression-test.sh - UsrLinuxEmu 跨配置回归测试
#
# Usage:
#   scripts/regression-test.sh [quick|full]
#
# Modes:
#   quick (default) - default 构建只跑核心测试（~20 项，<30s）
#   full            - default + ASan + UBSan 各跑全量 ctest（~3min）
#
# TSan 不纳入本脚本（Clang-only + test_ioctl_standalone 预存超时）。
# 需要 TSan 时手动执行:
#   CC=clang CXX=clang++ SANITIZER=tsan ./build.sh test
#
# 退出码:
#   0 - 全部配置 PASS
#   1 - 用法错误
#   2 - 构建失败
#   3 - 一个或多个测试配置失败

set -euo pipefail

# ── Argument parsing ──────────────────────────────────────────────────
MODE="${1:-quick}"

case "$MODE" in
    quick|full) ;;
    -h|--help|help)
        cat <<'EOF'
Usage: scripts/regression-test.sh [quick|full]

  quick (default) - default build, core tests only (~20 tests, <30s)
  full            - default + ASan + UBSan, full ctest each (~3min)

TSan is excluded (Clang-only + pre-existing test_ioctl_standalone timeout).
EOF
        exit 0
        ;;
    *)
        echo "ERROR: Unknown mode '$MODE'. Valid: quick, full" >&2
        exit 1
        ;;
esac

# ── Paths ─────────────────────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT"

# ── Color helpers ─────────────────────────────────────────────────────
if [ -t 1 ]; then
    GREEN='\033[0;32m'
    RED='\033[0;31m'
    YELLOW='\033[1;33m'
    BLUE='\033[0;34m'
    BOLD='\033[1m'
    NC='\033[0m'
else
    GREEN='' RED='' YELLOW='' BLUE='' BOLD='' NC=''
fi

color_print() {
    local color="$1"; shift
    printf "${color}%s${NC}\n" "$@"
}

# ── Quick mode test selection ─────────────────────────────────────────
# Core tests covering: memory pool, ringbuffer, queue, ioctl, va_space,
# kfd core, module loading, buddy allocator.
QUICK_REGEX='(mem_pool|ringbuffer|queue_puller|gpu_ioctl|gpu_memory|va_space|kfd_queue|kfd_dispatch|kfd_pasid|kfd_process|kfd_module|kfd_events|module_load_and_vfs|mempool_export|buddy)'

# ── Result tracking ───────────────────────────────────────────────────
declare -a CONFIG_NAMES=()
declare -a CONFIG_RESULTS=()
declare -a CONFIG_DETAILS=()
FAIL_COUNT=0

# ── Helper: run one build+test configuration ──────────────────────────
# Args: $1=config_name  $2=sanitizer_env  $3=build_dir  $4=ctest_args...
run_config() {
    local config_name="$1"
    local sanitizer="$2"
    local build_dir="$3"
    shift 3
    local -a ctest_args=("$@")

    echo ""
    color_print "$BLUE$BOLD" "═══════════════════════════════════════════════════════════"
    color_print "$BLUE$BOLD" "  [$config_name]  sanitizer=${sanitizer:-<none>}  build=$build_dir"
    color_print "$BLUE$BOLD" "═══════════════════════════════════════════════════════════"

    # ── Build ─────────────────────────────────────────────────────
    echo ""
    color_print "$BOLD" "[$config_name] Building..."
    if ! SANITIZER="$sanitizer" bash "$PROJECT_ROOT/build.sh" 2>&1; then
        color_print "$RED" "[$config_name] BUILD FAILED"
        CONFIG_NAMES+=("$config_name")
        CONFIG_RESULTS+=("BUILD_FAIL")
        CONFIG_DETAILS+=("sanitizer=${sanitizer:-default}")
        FAIL_COUNT=$((FAIL_COUNT + 1))
        return 0  # continue to next config instead of aborting
    fi

    # ── Stage plugin (ensures plugins/plugin_gpu_driver.so matches) ──
    echo ""
    color_print "$BOLD" "[$config_name] Staging plugin..."
    if ! bash "$PROJECT_ROOT/scripts/stage-plugin.sh" "$build_dir" 2>&1; then
        color_print "$RED" "[$config_name] PLUGIN STAGING FAILED"
        CONFIG_NAMES+=("$config_name")
        CONFIG_RESULTS+=("STAGE_FAIL")
        CONFIG_DETAILS+=("sanitizer=${sanitizer:-default}")
        FAIL_COUNT=$((FAIL_COUNT + 1))
        return 0
    fi

    # ── Run tests ─────────────────────────────────────────────────
    echo ""
    color_print "$BOLD" "[$config_name] Running tests..."
    local start_time end_time elapsed
    start_time=$(date +%s)

    local ctest_output
    local ctest_exit=0
    # shellcheck disable=SC2086
    ctest_output=$(cd "$PROJECT_ROOT" && ctest --test-dir "$build_dir" \
        --output-on-failure \
        "${ctest_args[@]}" 2>&1) || ctest_exit=$?

    end_time=$(date +%s)
    elapsed=$((end_time - start_time))

    echo "$ctest_output"

    # ── Record result ─────────────────────────────────────────────
    CONFIG_NAMES+=("$config_name")
    CONFIG_DETAILS+=("elapsed=${elapsed}s")

    if [ "$ctest_exit" -eq 0 ]; then
        # Extract pass/fail counts from ctest output
        local pass_count fail_count
        pass_count=$(echo "$ctest_output" | grep -oP '^\s*(\d+)% tests passed' | grep -oP '^\s*\d+' || echo "?")
        fail_count=$(echo "$ctest_output" | grep -oP '(\d+) tests failed' | grep -oP '\d+' || echo "0")
        CONFIG_RESULTS+=("PASS")
        color_print "$GREEN" "[$config_name] PASS  (${pass_count}% passed, ${fail_count} failed, ${elapsed}s)"
    else
        CONFIG_RESULTS+=("TEST_FAIL")
        color_print "$RED" "[$config_name] TEST FAIL  (exit=$ctest_exit, ${elapsed}s)"
        FAIL_COUNT=$((FAIL_COUNT + 1))
    fi
}

# ── Execute configurations ────────────────────────────────────────────
case "$MODE" in
    quick)
        run_config "default-quick" "" "build" -R "$QUICK_REGEX"
        ;;

    full)
        run_config "default" "" "build"
        run_config "asan" "asan" "build-asan"
        run_config "ubsan" "ubsan" "build-ubsan"
        ;;
esac

# ── Summary ───────────────────────────────────────────────────────────
echo ""
color_print "$BOLD" "═══════════════════════════════════════════════════════════"
color_print "$BOLD" "  Regression Test Summary  (mode=$MODE)"
color_print "$BOLD" "═══════════════════════════════════════════════════════════"
echo ""

for i in "${!CONFIG_NAMES[@]}"; do
    local_name="${CONFIG_NAMES[$i]}"
    local_result="${CONFIG_RESULTS[$i]}"
    local_detail="${CONFIG_DETAILS[$i]}"
    if [ "$local_result" = "PASS" ]; then
        printf "  ${GREEN}%-5s${NC}  %-20s  %s\n" "PASS" "$local_name" "$local_detail"
    else
        printf "  ${RED}%-5s${NC}  %-20s  %s\n" "FAIL" "$local_name" "$local_detail"
    fi
done

echo ""
if [ "$FAIL_COUNT" -eq 0 ]; then
    color_print "$GREEN$BOLD" "All configurations PASSED."
    exit 0
else
    color_print "$RED$BOLD" "$FAIL_COUNT configuration(s) FAILED."
    exit 3
fi
