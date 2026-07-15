#!/bin/bash
#
# check_kfd_includes.sh — C-12 Phase B gate: enforce zero amdgpu header dependency
#
# Purpose: Prevent any KFD module from directly or transitively including
# amdgpu headers, which caused Stage 1.4 PoC commit 5341c3f 8-iteration failure.
#
# Per ADR-059 §R-6 + A.2 report §3.4 + §5 prevention strategy #1/#2.
#
# Usage:
#   tools/ci/check_kfd_includes.sh [--strict]
#     --strict  Exit with non-zero on violations (CI mode)
#     (no arg)  Warn but exit 0 (local dev mode)
#
# Exit codes:
#   0  Clean: zero amdgpu header violations
#   1  Violations found (with --strict flag)
#
# Created: 2026-07-15 (C-12 Phase A.2 hard gate — A.2.4 CI checkpoint)
# Associated: openspec/changes/2026-08-15-stage1-4-kfd-multi-file-integration/tasks.md §A.2.4
#              docs/05-advanced/kfd-abi-comparison-report.md §3.4

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
KFD_DIR="${REPO_ROOT}/plugins/gpu_driver/drv/kfd"
STRICT=false

if [ "${1:-}" = "--strict" ]; then
    STRICT=true
fi

# ── Header existence check ──────────────────────────────────────

if [ ! -d "$KFD_DIR" ]; then
    echo "[check_kfd_includes] WARNING: $KFD_DIR does not exist — nothing to check"
    exit 0
fi

# ── Forbidden include patterns ───────────────────────────────────

# Pattern group 1: Any amdgpu header (direct or transitive entry point)
# Pattern group 2: Upstream amdkfd headers (except our local ones)
# Pattern group 3: PCIe/hardware register headers that proxy amdgpu types
FORBIDDEN_PATTERNS=(
    '#include.*amdgpu_'
    '#include.*linux/drivers/gpu/drm/amd/amdkfd/kfd_'
    '#include.*cik_regs'
    '#include.*dce_regs'
    '#include.*soc15'
    '#include.*gfx_v[0-9]'
    '#include.*psp'
    '#include.*ifwi'
    '#include.*amdgpu_psp'
)

VIOLATIONS=0
VIOLATION_FILES=""

while IFS= read -r -d '' file; do
    for pattern in "${FORBIDDEN_PATTERNS[@]}"; do
        matches=$(grep -n "$pattern" "$file" 2>/dev/null || true)
        if [ -n "$matches" ]; then
            echo "[check_kfd_includes] VIOLATION: $file"
            echo "$matches" | sed 's/^/  /'
            VIOLATIONS=$((VIOLATIONS + 1))
            VIOLATION_FILES="$VIOLATION_FILES $file"
        fi
    done
done < <(find "$KFD_DIR" \( -name '*.c' -o -name '*.h' -o -name '*.cpp' -o -name '*.hpp' \) -print0 2>/dev/null)

# ── Allowed include whitelist (confirm compliance) ───────────────

# These are the ONLY external include paths allowed for KFD modules.
# Per §3.1 report: 0 direct amdgpu headers, only linux_compat + shared + local.
ALLOWED_PREFIXES=(
    'linux_compat/'
    'shared/'
)

# Verify: no file includes anything outside the allowed set.
# Standard C/C++ headers (#include <...>) and local kfd headers are exempt.
EXTRA_CHECK=0
while IFS= read -r -d '' file; do
    while IFS= read -r line; do
        # Skip comments and empty lines
        [[ "$line" =~ ^[[:space:]]*// ]] && continue
        [[ "$line" =~ ^[[:space:]]*/\* ]] && continue
        [[ "$line" =~ ^[[:space:]]*\* ]] && continue

        # Determine include style: system <> or local ""
        if echo "$line" | grep -q '#include[[:space:]]*<'; then
            # System headers (<...>) are always allowed (stdint.h, cstdint, etc.)
            continue
        fi

        # Extract local include path
        include_path=$(echo "$line" | sed -n 's/.*#include[[:space:]]*"\([^"]*\)".*/\1/p')
        [ -z "$include_path" ] && continue

        # Local kfd headers are always allowed (kfd_priv.h, kfd_svm.h, etc.)
        if [[ "$include_path" == kfd_* ]]; then
            continue
        fi

        # Check if this include is in the allowed set (linux_compat/, shared/)
        allowed=false
        for prefix in "${ALLOWED_PREFIXES[@]}"; do
            if [[ "$include_path" == $prefix* ]]; then
                allowed=true
                break
            fi
        done

        if [ "$allowed" = false ]; then
            echo "[check_kfd_includes] SUSPICIOUS: $file includes '$include_path' — not in allowed set"
            EXTRA_CHECK=$((EXTRA_CHECK + 1))
        fi
    done < <(grep -n '#include' "$file" 2>/dev/null || true)
done < <(find "$KFD_DIR" \( -name '*.c' -o -name '*.h' -o -name '*.cpp' -o -name '*.hpp' \) -print0 2>/dev/null)

# ── Summary ─────────────────────────────────────────────────────

echo ""
echo "============================================"
echo "  check_kfd_includes.sh — Report"
echo "============================================"
echo "  KFD directory: $KFD_DIR"
echo "  Forbidden pattern violations: $VIOLATIONS"
echo "  Suspicious (non-whitelist) includes: $EXTRA_CHECK"
echo "============================================"

TOTAL_ISSUES=$((VIOLATIONS + EXTRA_CHECK))

if [ "$TOTAL_ISSUES" -eq 0 ]; then
    echo "  ✅ PASS: Zero amdgpu header dependencies in KFD modules"
    exit 0
fi

if [ "$STRICT" = true ]; then
    echo "  ❌ FAIL: $TOTAL_ISSUES issue(s) found (strict mode)"
    echo ""
    echo "  Fix: Remove forbidden amdgpu includes. Use local stub types"
    echo "       or HAL ops (per ADR-023 + ADR-059 §D3) instead."
    echo "  See: docs/05-advanced/kfd-abi-comparison-report.md §3.4"
    exit 1
else
    echo "  ⚠️  WARNING: $TOTAL_ISSUES issue(s) found (non-strict mode, exiting 0)"
    exit 0
fi
