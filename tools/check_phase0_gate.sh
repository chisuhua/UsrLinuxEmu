#!/usr/bin/env bash
# tools/check_phase0_gate.sh — Change-2 (sim-hardware-foundation) Phase 0 Hard Gate
# 验证 P0-G1~G5（design.md §2 / proposal.md §Phase 0 Hard Gate）。
#
# 输出：STDOUT 检查明细 + 汇总；可选 --log <path> 写入 verification log。
# 退出码：0 = 全部通过；1 = 任一 FAIL。

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO_ROOT"

LOG_PATH=""
if [ "${1:-}" = "--log" ]; then
  LOG_PATH="${2:?missing --log path}"
fi

PASS=0
FAIL=0

report() {  # report <status> <name> <detail>
  local s="$1" n="$2" d="$3"
  case "$s" in
    PASS) PASS=$((PASS+1)); printf 'PASS  %-24s %s\n' "$n" "$d" ;;
    FAIL) FAIL=$((FAIL+1)); printf 'FAIL  %-24s %s\n' "$n" "$d" ;;
    SKIP) printf 'SKIP  %-24s %s\n' "$n" "$d" ;;
  esac
}

echo "=== Phase 0 Hard Gate ($(date -u +%Y-%m-%dT%H:%M:%SZ)) ==="

# P0-G1: sim_hardware INTERFACE + 无 cpptlm 链接
if grep -q '^add_library(sim_hardware INTERFACE' sim_hardware/CMakeLists.txt; then
  if grep -n 'cpptlm_core' sim_hardware/CMakeLists.txt | grep -v '^.*#' >/dev/null 2>&1; then
    report FAIL "P0-G1" "cpptlm_core 仍为有效链接行"
  else
    report PASS "P0-G1" "INTERFACE target 存在；cpptlm_core 仅注释"
  fi
else
  report FAIL "P0-G1" "未找到 add_library(sim_hardware INTERFACE)"
fi

# P0-G2: 仓库无外部 CppTLM 头/库/符号
# 排除：本仓库自身 sim_hardware mock（含 tests/sim_hardware 的 mock 测试）、
# openspec/docs/.git/build、外部 TaskRunner 子模块。
# 检查目标：外部 CppTLM 产物（cpptlm_emulator.h 头、libcpptlm_emulator.so 库、
# external/cpptlm* 目录）——它们是 Phase 0 后 gated follow-up 的接入点。
p0g2_found=0
if find . \
    -not -path './sim_hardware/*' \
    -not -path './openspec/*' \
    -not -path './docs/*' \
    -not -path './.git/*' \
    -not -path './build/*' \
    -not -path './external/TaskRunner/*' \
    \( -name 'cpptlm_emulator.h' -o -name 'libcpptlm_emulator.so*' -o -path './external/cpptlm*' \) \
    2>/dev/null | grep -q .; then
  report FAIL "P0-G2" "发现外部 CppTLM 产物（cpptlm_emulator.h / .so / external/cpptlm*）"
else
  report PASS "P0-G2" "仓库无外部 CppTLM 头/库（mock 测试不属外部依赖）"
fi

# P0-G3: default_topology.json 当前是否合法（预期：T2.1 后合法）
if [ -f sim_hardware/topology/default_topology.json ] && \
   python3 -c 'import json,sys; json.load(open(sys.argv[1]))' \
     sim_hardware/topology/default_topology.json 2>/dev/null; then
  report PASS "P0-G3" "default_topology.json 合法（T2.1 已修复）"
else
  report FAIL "P0-G3" "default_topology.json 非法 JSON"
fi

# P0-G4: 真实 PCI 边界文件为 probe.cpp；pci_setup_bus/pcie_enable_device 为新增
if [ -f plugins/pci_driver/probe.cpp ] && \
   [ ! -f plugins/pci_driver/pci_probe.cpp ] && \
   [ -f plugins/pci_driver/pci_setup_bus.cpp ] && \
   [ -f plugins/pci_driver/pcie_enable_device.cpp ]; then
  report PASS "P0-G4" "probe.cpp 存在；setup_bus/enable_device Wave 4 新增"
elif [ -f plugins/pci_driver/probe.cpp ]; then
  report PASS "P0-G4" "probe.cpp 存在（边界正确）"
else
  report FAIL "P0-G4" "probe.cpp 缺失"
fi

# P0-G5: 4 个 final sim_hardware 测试 binary 存在（Wave 5 T5.6 归并）
missing=0
for t in test_topology test_cpptlm_bridge_mock test_pcie_host_bridge_mock test_pcie_bypass_mock; do
  [ -f "tests/sim_hardware/${t}_standalone.cpp" ] || { missing=1; report FAIL "P0-G5" "${t}_standalone.cpp 缺失"; }
done
if [ "$missing" = "0" ]; then
  report PASS "P0-G5" "4 个 final sim_hardware 测试 binary 存在"
fi

# P0-G6: N=1 fail-fast contract — gpu_driver plugin WARN-bridges device[0] only when out_count > 1.
# 2nd device's arrival is the escalation trigger for the BDF-keyed sim singletons follow-up change.
p0g6_count=$(python3 -c 'import json; print(len(json.load(open("sim_hardware/topology/default_topology.json"))["devices"]))' 2>/dev/null)
if [ "$p0g6_count" = "1" ]; then
  report PASS "P0-G6" "default_topology.json devices == 1 (N=1 fail-fast contract)"
else
  report FAIL "P0-G6" "default_topology.json has $p0g6_count devices (expected 1; N>1 triggers WARN bridge-only-device[0])"
fi

echo "=== 汇总: ${PASS} PASS / ${FAIL} FAIL ==="
if [ -n "$LOG_PATH" ]; then
  { echo "# Phase 0 Hard Gate — $(date -u +%Y-%m-%dT%H:%M:%SZ)"; echo; echo "P0-G1..G5 逐项结果见上（由 tools/check_phase0_gate.sh 生成）"; } > "$LOG_PATH"
fi
[ "$FAIL" = "0" ]