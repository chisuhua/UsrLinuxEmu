#!/bin/bash
# verify_symlinks.sh - Validate that required symlinks are correctly configured
#
# This script checks that plugins/gpu_driver/shared is a symlink pointing to
# the TaskRunner shared headers directory. Run this as a CI pre-check step.
#
# Usage:
#   cd UsrLinuxEmu
#   tools/verify_symlinks.sh
#
# Exit codes:
#   0 - All symlinks are valid
#   1 - One or more symlinks are missing or misconfigured

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
SHARED_LINK="${REPO_ROOT}/plugins/gpu_driver/shared"

echo "=== UsrLinuxEmu Symlink Verification ==="
echo "Repository root: ${REPO_ROOT}"
echo ""

EXIT_CODE=0

# Check that plugins/gpu_driver/shared is a symlink
if [ ! -L "${SHARED_LINK}" ]; then
    echo "❌ ERROR: plugins/gpu_driver/shared is NOT a symlink!"
    echo ""
    echo "   Expected a symlink pointing to TaskRunner/shared."
    echo "   To create it, run:"
    echo ""
    echo "     cd ${REPO_ROOT}/plugins/gpu_driver"
    echo "     ln -sf ../../../../TaskRunner/shared ./shared"
    echo ""
    EXIT_CODE=1
else
    TARGET="$(readlink "${SHARED_LINK}")"
    echo "✅ plugins/gpu_driver/shared -> ${TARGET}"

    # Check that the symlink target exists and contains expected headers
    RESOLVED="$(cd "$(dirname "${SHARED_LINK}")" && cd "$(readlink "${SHARED_LINK}")" 2>/dev/null && pwd || true)"
    if [ -z "${RESOLVED}" ]; then
        echo "   ⚠️  WARNING: Symlink target does not exist (TaskRunner not cloned?)"
        echo "   Expected headers: gpu_types.h, gpu_regs.h, gpu_ioctl.h, gpu_events.h"
    else
        echo "   Resolved to: ${RESOLVED}"
        MISSING_HEADERS=""
        for header in gpu_types.h gpu_regs.h gpu_ioctl.h gpu_events.h; do
            if [ ! -f "${RESOLVED}/${header}" ]; then
                MISSING_HEADERS="${MISSING_HEADERS} ${header}"
            fi
        done
        if [ -n "${MISSING_HEADERS}" ]; then
            echo "   ⚠️  WARNING: Missing headers in shared/:${MISSING_HEADERS}"
        else
            echo "   ✅ All required headers present"
        fi
    fi
fi

echo ""
if [ "${EXIT_CODE}" -eq 0 ]; then
    echo "✅ Symlink verification passed"
else
    echo "❌ Symlink verification FAILED"
fi

exit "${EXIT_CODE}"
