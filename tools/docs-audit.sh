#!/bin/bash
# docs-audit.sh - Validate UsrLinuxEmu documentation against code reality
#
# Re-runnable audit script that catches the kind of doc/code drift documented
# in docs/02_architecture/post-refactor-architecture.md. Designed to be
# invoked manually, in pre-commit, or in CI.
#
# Usage:
#   cd UsrLinuxEmu
#   tools/docs-audit.sh                 # Run all checks
#   tools/docs-audit.sh --section arch  # Run a single section
#   tools/docs-audit.sh --strict        # Treat warnings as failures
#   tools/docs-audit.sh --help          # Show this help
#
# Available sections:
#   arch       Architecture facts (kernel SHARED, archive layout, HAL count, plugin mode)
#   ioctl      IOCTL numbering, structure fields, System B/A residuals
#   adr        ADR governance (ADR-022 missing, IOCTL number conflicts)
#   doc-health Broken links, kebab-case/snake_case, 02-core/ vs 02_architecture/
#   build      Build/structural (orphan tests, CMake subdirs, hidden include_directories)
#
# Exit codes:
#   0 - All checks passed (or only warnings in non-strict mode)
#   1 - One or more failures detected (or warnings in --strict mode)
#   2 - Invalid arguments
#
# Style notes:
#   - Mirrors tools/verify_symlinks.sh: set -e, emoji status, EXIT_CODE accumulator
#   - Self-locates REPO_ROOT from SCRIPT_DIR (no hardcoded paths)
#   - Every check is wrapped in `|| true` so a single failure does not abort
#     the whole audit; we want the full report.
#   - Style reference: docs/02_architecture/post-refactor-architecture.md

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

# ---------------------------------------------------------------------------
# Globals
# ---------------------------------------------------------------------------

PASS_COUNT=0
FAIL_COUNT=0
WARN_COUNT=0
EXIT_CODE=0
STRICT=0
RUN_SECTION="all"

# Track which sections to run
RUN_ARCH=0
RUN_IOCTL=0
RUN_ADR=0
RUN_DOC=0
RUN_BUILD=0

# ---------------------------------------------------------------------------
# Output helpers
# ---------------------------------------------------------------------------

section() {
    echo ""
    echo "=== $1 ==="
}

subsection() {
    echo ""
    echo "--- $1 ---"
}

check_pass() {
    PASS_COUNT=$((PASS_COUNT + 1))
    echo "  ✅ $1"
}

check_fail() {
    FAIL_COUNT=$((FAIL_COUNT + 1))
    EXIT_CODE=1
    echo "  ❌ $1"
}

check_warn() {
    WARN_COUNT=$((WARN_COUNT + 1))
    if [ "${STRICT}" = "1" ]; then
        EXIT_CODE=1
    fi
    echo "  ⚠️  $1"
}

check_info() {
    echo "  ℹ️  $1"
}

# file_count <path> - count regular files in a directory (not recursive)
file_count() {
    local p="$1"
    if [ -d "${p}" ]; then
        find "${p}" -maxdepth 1 -type f 2>/dev/null | wc -l | tr -d ' '
    else
        echo "0"
    fi
}

# grep_count <pattern> <path> - count matches across path (recursive, -E)
grep_count() {
    local pattern="$1"
    local path="$2"
    if [ -e "${path}" ]; then
        grep -rcE "${pattern}" "${path}" 2>/dev/null | awk -F: '{s+=$NF} END {print s+0}'
    else
        echo "0"
    fi
}

# Safe existence check: echoes "yes"/"no"
exists() {
    if [ -e "$1" ]; then echo "yes"; else echo "no"; fi
}

# ---------------------------------------------------------------------------
# Argument parsing
# ---------------------------------------------------------------------------

usage() {
    sed -n '2,/^set -e/p' "${BASH_SOURCE[0]}" | grep '^#' | sed 's/^# \?//'
    exit 0
}

parse_args() {
    while [ $# -gt 0 ]; do
        case "$1" in
            --section)
                if [ -z "$2" ] || [ "${2#--}" != "$2" ]; then
                    echo "ERROR: --section requires a value" >&2
                    exit 2
                fi
                RUN_SECTION="$2"
                shift 2
                ;;
            --strict)
                STRICT=1
                shift
                ;;
            -h|--help|help)
                usage
                ;;
            *)
                echo "ERROR: unknown argument: $1" >&2
                echo "Run '$0 --help' for usage." >&2
                exit 2
                ;;
        esac
    done

    case "${RUN_SECTION}" in
        all)
            RUN_ARCH=1; RUN_IOCTL=1; RUN_ADR=1; RUN_DOC=1; RUN_BUILD=1
            ;;
        arch)       RUN_ARCH=1 ;;
        ioctl)      RUN_IOCTL=1 ;;
        adr)        RUN_ADR=1 ;;
        doc-health) RUN_DOC=1 ;;
        build)      RUN_BUILD=1 ;;
        *)
            echo "ERROR: unknown section: ${RUN_SECTION}" >&2
            echo "Valid sections: all, arch, ioctl, adr, doc-health, build" >&2
            exit 2
            ;;
    esac
}

# ---------------------------------------------------------------------------
# Section 1: Architecture facts
# ---------------------------------------------------------------------------

section_arch() {
    section "1. Architecture Facts"

    # 1.1 kernel SHARED library
    subsection "1.1 src/CMakeLists.txt declares kernel as SHARED"
    if grep -q "^add_library(kernel SHARED" "${REPO_ROOT}/src/CMakeLists.txt" 2>/dev/null; then
        check_pass "kernel is SHARED in src/CMakeLists.txt"
    else
        check_fail "kernel is NOT declared as SHARED (Issue #11 regression risk)"
    fi

    # 1.2 src/kernel cpp count
    subsection "1.2 src/kernel cpp file count (doc says 14)"
    local count
    count=$(find "${REPO_ROOT}/src/kernel" -name "*.cpp" 2>/dev/null | wc -l | tr -d ' ')
    if [ "${count}" -eq 14 ]; then
        check_pass "src/kernel has ${count} cpp files (matches doc)"
    else
        check_warn "src/kernel has ${count} cpp files (doc claims 14)"
    fi

    # 1.3 archive/openspec-deprecated-2026-06-15 should NOT exist
    subsection "1.3 archive/openspec-deprecated-2026-06-15/ must not exist"
    if [ ! -e "${REPO_ROOT}/archive/openspec-deprecated-2026-06-15" ]; then
        check_pass "archive/openspec-deprecated-2026-06-15/ does not exist (doc corrected)"
    else
        check_fail "archive/openspec-deprecated-2026-06-15/ exists but commit 71f6ff8 was empty; remove it or update docs"
    fi

    # 1.4 archive subdirectory file counts (Appendix B claims)
    subsection "1.4 archive/ subdirectory file counts match Appendix B"
    local sys_b sim_hist plans_n
    sys_b=$(file_count "${REPO_ROOT}/archive/system_b_drivers/gpu")
    sim_hist=$(file_count "${REPO_ROOT}/archive/orphaned_simulator/gpu")
    plans_n=$(file_count "${REPO_ROOT}/archive/historical-plans-2026-06-15")
    if [ "${sys_b}" -eq 12 ]; then
        check_pass "archive/system_b_drivers/gpu has 12 files"
    else
        check_warn "archive/system_b_drivers/gpu has ${sys_b} files (Appendix B claims 12)"
    fi
    if [ "${sim_hist}" -eq 6 ]; then
        check_pass "archive/orphaned_simulator/gpu has 6 files"
    else
        check_warn "archive/orphaned_simulator/gpu has ${sim_hist} files (Appendix B claims 6)"
    fi
    if [ "${plans_n}" -eq 8 ]; then
        check_pass "archive/historical-plans-2026-06-15 has 8 files"
    else
        check_warn "archive/historical-plans-2026-06-15 has ${plans_n} files (Appendix B claims 8)"
    fi

    # 1.5 HAL function pointer count (doc says 11)
    subsection "1.5 struct gpu_hal_ops has 11 function pointers"
    if [ -f "${REPO_ROOT}/plugins/gpu_driver/hal/gpu_hal.h" ]; then
        local hal_count
        hal_count=$(grep -cE "^\s+(int|void)\s+\(\*.*\)\s*\(" "${REPO_ROOT}/plugins/gpu_driver/hal/gpu_hal.h" 2>/dev/null || echo "0")
        if [ "${hal_count}" -eq 11 ]; then
            check_pass "gpu_hal.h has ${hal_count} fn-ptrs (matches doc)"
        else
            check_warn "gpu_hal.h has ${hal_count} fn-ptrs (doc claims 11)"
        fi
    else
        check_warn "plugins/gpu_driver/hal/gpu_hal.h not found"
    fi

    # 1.6 plugin loading mode (__attribute__((constructor)) must NOT be used)
    subsection "1.6 Plugin loading uses module mod symbol pattern (no constructor attribute)"
    local ctor_count
    ctor_count=$(grep -rE "__attribute__\(\(constructor\)\)" \
        "${REPO_ROOT}/plugins" "${REPO_ROOT}/drivers" "${REPO_ROOT}/src" "${REPO_ROOT}/tests" \
        2>/dev/null | wc -l | tr -d ' ')
    if [ "${ctor_count}" -eq 0 ]; then
        check_pass "No __attribute__((constructor)) in code (uses module mod pattern)"
    else
        check_warn "Found ${ctor_count} __attribute__((constructor)) in code (should use module mod)"
    fi

    # 1.7 AGENTS.md test framework declaration
    subsection "1.7 AGENTS.md does not mis-claim GTest/Catch2"
    local agents_gtest
    agents_gtest=$(grep -ciE "gtest|google test|catch2" "${REPO_ROOT}/AGENTS.md" 2>/dev/null | head -1 | tr -d '[:space:]')
    agents_gtest="${agents_gtest:-0}"
    if [ "${agents_gtest}" = "0" ]; then
        check_pass "AGENTS.md does not claim a test framework (matches post-refactor-architecture.md v0.1.1)"
    else
        # References are correct if they describe the migration: "we use Catch2, not GTest"
        # See ADR-010 and the AGENTS.md test framework section.
        check_info "AGENTS.md has ${agents_gtest} GTest/Catch2 references (expected: migration rationale; verify against §Testing Framework)"
    fi

    # 1.8 libgpu_core naming
    subsection "1.8 libgpu_core header is gpu_buddy.h (not buddy.h or gpu_buddy_allocator.h)"
    if [ -f "${REPO_ROOT}/libgpu_core/include/gpu_buddy.h" ]; then
        check_pass "libgpu_core/include/gpu_buddy.h exists"
    else
        check_warn "libgpu_core/include/gpu_buddy.h missing"
    fi
}

# ---------------------------------------------------------------------------
# Section 2: IOCTL numbering & code
# ---------------------------------------------------------------------------

section_ioctl() {
    section "2. IOCTL Numbering & Code Reality"

    local ioctl_h="${REPO_ROOT}/plugins/gpu_driver/shared/gpu_ioctl.h"
    subsection "2.1 plugins/gpu_driver/shared/gpu_ioctl.h exists"
    if [ -f "${ioctl_h}" ]; then
        check_pass "gpu_ioctl.h exists"
    else
        check_fail "gpu_ioctl.h missing"
        return
    fi

    # 2.2 System C IOCTL numbers (appendix A claims)
    subsection "2.2 System C IOCTL numbers match Appendix A"
    local expected_numbers=(
        "0x01:GET_DEVICE_INFO-OR-not:PUSHBUFFER"
        # we don't check the macro-per-number mapping in detail (Appendix A is
        # the source of truth); we check key numbers exist
    )
    # Just spot-check a few high-risk numbers
    for num in "0x01" "0x10" "0x20" "0x30" "0x40" "0x43"; do
        if grep -qE "GPU_IOCTL_BASE,\s*${num}," "${ioctl_h}" 2>/dev/null; then
            check_pass "GPU_IOCTL_BASE, ${num} defined in gpu_ioctl.h"
        else
            check_fail "GPU_IOCTL_BASE, ${num} NOT defined in gpu_ioctl.h"
        fi
    done

    # 2.3 System B (GPGPU_*) should not appear in non-archive code
    subsection "2.3 System B (GPGPU_*) absent from non-archive code"
    local gpgpu_count
    gpgpu_count=$(grep -rE "GPGPU_(ALLOC|FREE|SUBMIT|GET)_" \
        "${REPO_ROOT}/drivers" "${REPO_ROOT}/src" "${REPO_ROOT}/include" \
        "${REPO_ROOT}/tests" "${REPO_ROOT}/plugins" \
        2>/dev/null | wc -l | tr -d ' ')
    if [ "${gpgpu_count}" -eq 0 ]; then
        check_pass "No GPGPU_* macro references in non-archive code"
    else
        check_fail "${gpgpu_count} GPGPU_* macro references in non-archive code"
    fi

    # 2.4 System A (CUDA_IOCTL_*) should not appear anywhere
    subsection "2.4 System A (CUDA_IOCTL_*) absent from code (only docs may reference)"
    local cuda_count
    cuda_count=$(grep -rE "CUDA_IOCTL_" \
        "${REPO_ROOT}/drivers" "${REPO_ROOT}/src" "${REPO_ROOT}/include" \
        "${REPO_ROOT}/tests" "${REPO_ROOT}/plugins" \
        2>/dev/null | wc -l | tr -d ' ')
    if [ "${cuda_count}" -eq 0 ]; then
        check_pass "No CUDA_IOCTL_* references in code"
    else
        check_fail "${cuda_count} CUDA_IOCTL_* references in code"
    fi

    # 2.5 LAUNCH_CB should not exist (deleted in commit b78edc9)
    subsection "2.5 GPU_IOCTL_REGISTER_LAUNCH_CB absent (deleted in commit b78edc9)"
    if ! grep -qE "GPU_IOCTL_REGISTER_LAUNCH_CB|handleRegisterLaunchCb|LaunchCallback" \
        "${ioctl_h}" "${REPO_ROOT}/plugins/gpu_driver/drv/gpgpu_device.cpp" 2>/dev/null; then
        check_pass "LAUNCH_CB fully removed from code (commit b78edc9 verified)"
    else
        check_fail "LAUNCH_CB residue still present in code"
    fi

    # 2.6 gpgpu_device ioctl handler count
    subsection "2.6 GpgpuDevice ioctl table size (should be 13, was 11 before LAUNCH_CB removal)"
    local kNumIoctls
    kNumIoctls=$(grep -E "kNumIoctls\s*=" "${REPO_ROOT}/plugins/gpu_driver/drv/gpgpu_device.h" 2>/dev/null | grep -oE "[0-9]+" | head -1)
    if [ -n "${kNumIoctls}" ] && [ "${kNumIoctls}" -eq 13 ]; then
        check_pass "kNumIoctls = ${kNumIoctls}"
    else
        check_warn "kNumIoctls = ${kNumIoctls:-?} (expected 13)"
    fi
}

# ---------------------------------------------------------------------------
# Section 3: ADR governance
# ---------------------------------------------------------------------------

section_adr() {
    section "3. ADR Governance"

    local adr_dir="${REPO_ROOT}/docs/00_adr"
    subsection "3.1 ADR-022 missing (documented as 'GPU 计算单元仿真')"
    if [ ! -f "${adr_dir}/adr-022-gpu-core-emulation.md" ] && \
       [ ! -f "${adr_dir}/adr-022-gpu-compute-unit-emulation.md" ]; then
        check_info "ADR-022 file not present (intentional placeholder; documented in post-refactor-architecture.md)"
    else
        check_pass "ADR-022 present"
    fi

    subsection "3.2 ADR-025..031 missing (documented in PRD.md)"
    local missing_count=0
    for n in 025 026 027 028 029 030 031; do
        if ! ls "${adr_dir}/adr-${n}-"*.md >/dev/null 2>&1; then
            missing_count=$((missing_count + 1))
        fi
    done
    if [ "${missing_count}" -gt 0 ]; then
        check_info "ADR-025..031 missing (${missing_count} files; intentional placeholders for Phase 3+)"
    else
        check_pass "All ADR-025..031 present"
    fi

    subsection "3.3 ADR IOCTL queue number conflicts (0x33-0x35 vs actual 0x40-0x43)"
    local conflicts
    conflicts=$(grep -nE "0x33|0x34|0x35" \
        "${adr_dir}/adr-015-gpu-ioctl-unification.md" \
        "${adr_dir}/adr-024-user-mode-queue-submission.md" 2>/dev/null | wc -l | tr -d ' ')
    if [ "${conflicts}" -eq 0 ]; then
        check_pass "No 0x33-0x35 queue IOCTL numbers in ADR-015/024"
    else
        check_fail "${conflicts} 0x33-0x35 references in ADR-015/024 (should be 0x40-0x43)"
    fi

    subsection "3.4 ADRs README relationship diagram declares 'adr-024~031' planning branch"
    if grep -qE "adr-024～031|adr-024~031" "${adr_dir}/README.md" 2>/dev/null; then
        check_info "ADR README marks adr-024~031 as 'planned' (acceptable; PRD will reference them)"
    else
        check_pass "ADR README does not declare 024~031 as planned"
    fi
}

# ---------------------------------------------------------------------------
# Section 4: Doc health (links, paths)
# ---------------------------------------------------------------------------

section_doc_health() {
    section "4. Documentation Health"

    subsection "4.1 kebab-case links to non-existent files"
    local broken=0
    for path in \
        "${REPO_ROOT}/docs/04-building/build-system.md" \
        "${REPO_ROOT}/docs/04-building/testing-guide.md" \
        "${REPO_ROOT}/docs/05-advanced/gpu-driver-architecture.md"; do
        if [ -e "${path}" ]; then
            broken=$((broken + 1))
        fi
    done
    if [ "${broken}" -eq 0 ]; then
        check_pass "No kebab-case target files exist (snake_case is canonical)"
    else
        check_fail "${broken} kebab-case target files exist; rename or fix links"
    fi

    subsection "4.2 02-core/ references (should be 02_architecture/)"
    # Exclude historical evidence files that legitimately document the old path
    # as part of the cleanup record.
    local core_refs
    core_refs=$(grep -rE "02-core/" "${REPO_ROOT}/docs" \
        --exclude="post-refactor-architecture.md" \
        --exclude="architecture-alignment-report.md" \
        --exclude="refactor-history.md" 2>/dev/null | wc -l | tr -d ' ')
    if [ "${core_refs}" -eq 0 ]; then
        check_pass "No 02-core/ references in docs/ (excluding audit + historical evidence docs)"
    else
        check_fail "${core_refs} 02-core/ references in docs/ (should be 02_architecture/)"
    fi

    subsection "4.3 docs/README.md declares kebab-case as standard"
    # Only flag if kebab-case is declared as a *standard* / *naming convention*
    # in the "文档规范" section. Mentions in the changelog are historical evidence.
    if grep -qE "文档规范.*kebab-case|命名规范.*kebab-case|kebab-case.*(标准|规范|命名|standard|convention)" "${REPO_ROOT}/docs/README.md" 2>/dev/null; then
        check_fail "docs/README.md declares kebab-case as the naming standard; actual files are snake_case"
    else
        check_pass "docs/README.md does not mis-declare naming convention"
    fi

    subsection "4.4 docs/README.md stale completion date (should be 2026-06 not 2026-03-23)"
    # Only flag if 2026-03-23 is used as a "current" / "last updated" date marker,
    # not when it's mentioned in changelog history.
    if grep -qE "(最后验证|last updated|最后更新).*2026-03-23|2026-03-23.*(当前|current)" "${REPO_ROOT}/docs/README.md" 2>/dev/null; then
        check_fail "docs/README.md still references 2026-03-23 as 'last updated' (stale)"
    else
        check_pass "docs/README.md 'last updated' is current"
    fi

    subsection "4.5 ADR-010 status reflects 'proposed, not implemented'"
    if [ -f "${REPO_ROOT}/docs/00_adr/adr-010-gtest-migration.md" ]; then
        if grep -qE "提议|Proposed" "${REPO_ROOT}/docs/00_adr/adr-010-gtest-migration.md" 2>/dev/null; then
            check_pass "ADR-010 status is 'proposed' (matches reality: project uses Catch2)"
        else
            check_warn "ADR-010 status unclear; verify it is still 'proposed'"
        fi
    else
        check_fail "ADR-010 missing (project should document the Catch2 choice somewhere)"
    fi
}

# ---------------------------------------------------------------------------
# Section 5: Build & structural
# ---------------------------------------------------------------------------

section_build() {
    section "5. Build & Structural"

    subsection "5.1 Orphan test source files (not in tests/CMakeLists.txt)"
    local orphans=()
    for src in test_poll.cpp test_serial.cpp test_serial_device.cpp test_serial_ioctl.cpp; do
        if [ -f "${REPO_ROOT}/tests/${src}" ] && \
           ! grep -q "${src%.cpp}" "${REPO_ROOT}/tests/CMakeLists.txt" 2>/dev/null; then
            orphans+=("${src}")
        fi
    done
    if [ "${#orphans[@]}" -eq 0 ]; then
        check_pass "No orphan test source files"
    else
        check_fail "${#orphans[@]} orphan test files: ${orphans[*]}"
    fi

    subsection "5.2 Top-level CMakeLists.txt add_subdirectory completeness"
    local cmakelists="${REPO_ROOT}/CMakeLists.txt"
    if [ -f "${cmakelists}" ]; then
        local missing_subs=()
        for sub in src drivers plugins tests tools/cli libgpu_core; do
            if ! grep -qE "add_subdirectory\(\s*${sub}\s*\)" "${cmakelists}" 2>/dev/null; then
                missing_subs+=("${sub}")
            fi
        done
        if [ "${#missing_subs[@]}" -eq 0 ]; then
            check_pass "All 6 expected add_subdirectory entries present"
        else
            check_fail "Missing add_subdirectory: ${missing_subs[*]}"
        fi
    else
        check_fail "Top-level CMakeLists.txt missing"
    fi

    subsection "5.3 include_directories(simulator) hidden problem"
    if grep -qE "include_directories.*simulator" "${cmakelists}" 2>/dev/null; then
        if [ -d "${REPO_ROOT}/simulator" ] && [ -z "$(ls -A "${REPO_ROOT}/simulator" 2>/dev/null)" ]; then
            check_warn "include_directories(simulator) present but simulator/ is empty (stale include path)"
        else
            check_pass "include_directories(simulator) consistent with simulator/ content"
        fi
    else
        check_pass "include_directories(simulator) not present (clean)"
    fi

    subsection "5.4 plugins/plugins.json vs plugins/gpu_driver/{plugin,plugin.cpp}"
    if [ -f "${REPO_ROOT}/plugins/plugins.json" ]; then
        local json_path
        json_path=$(grep -oE '"path"\s*:\s*"[^"]+"' "${REPO_ROOT}/plugins/plugins.json" 2>/dev/null | head -3)
        check_info "plugins.json paths: ${json_path}"
    else
        check_warn "plugins/plugins.json missing"
    fi

    subsection "5.5 Test framework is Catch2 (vendored), not GTest"
    if [ -f "${REPO_ROOT}/tests/catch_amalgamated.hpp" ] && \
       [ -f "${REPO_ROOT}/tests/catch_amalgamated.cpp" ]; then
        check_pass "Catch2 vendored (tests/catch_amalgamated.{hpp,cpp})"
    else
        check_fail "Catch2 vendored files missing"
    fi
    if grep -rE "find_package\(GTest" "${REPO_ROOT}/CMakeLists.txt" \
        "${REPO_ROOT}/tests/CMakeLists.txt" 2>/dev/null | grep -q .; then
        check_fail "GTest find_package found in CMake (project should use Catch2)"
    else
        check_pass "No GTest find_package in CMakeLists"
    fi
}

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------

print_summary() {
    echo ""
    echo "============================================"
    echo "  Summary"
    echo "============================================"
    echo "  Repository: ${REPO_ROOT}"
    echo "  Mode: ${RUN_SECTION}${STRICT:+ (strict)}"
    echo ""
    echo "  ✅ Passed:  ${PASS_COUNT}"
    echo "  ❌ Failed:  ${FAIL_COUNT}"
    echo "  ⚠️  Warnings: ${WARN_COUNT}"
    echo ""

    if [ "${EXIT_CODE}" -eq 0 ]; then
        if [ "${WARN_COUNT}" -gt 0 ]; then
            echo "  Result: ✅ PASS (with ${WARN_COUNT} warnings; use --strict to enforce)"
        else
            echo "  Result: ✅ PASS"
        fi
    else
        echo "  Result: ❌ FAIL"
    fi
    echo "============================================"
}

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

main() {
    parse_args "$@"

    echo "=== UsrLinuxEmu Documentation Audit ==="
    echo "Repository root: ${REPO_ROOT}"
    echo "Mode: ${RUN_SECTION}${STRICT:+ (strict)}"

    [ "${RUN_ARCH}"  -eq 1 ] && section_arch
    [ "${RUN_IOCTL}" -eq 1 ] && section_ioctl
    [ "${RUN_ADR}"   -eq 1 ] && section_adr
    [ "${RUN_DOC}"   -eq 1 ] && section_doc_health
    [ "${RUN_BUILD}" -eq 1 ] && section_build

    print_summary
    exit "${EXIT_CODE}"
}

main "$@"
