# Design: Stage 4 Portability L2 — Linux 6.12 LTS Build Harness

## L1 + L2 + L3 framework (per ADR-072)

### L1 — 静态分析 (已通过，2026-06-19 验收)

- HAL 边界 grep：`grep -rn '#include.*"sim/' plugins/gpu_driver/drv/` 必须空
- 模拟符号泄漏：grep 检测 drv/ 不含 `<gpu_drm_driver.h>` 内部数据结构
- Stage 4.1-4.6 实施已维护此 invariant

### L2 — Build harness (本 change)

**Core idea**: 把 `plugins/gpu_driver/drv/` 剥离 UsrLinuxEmu 全树，单独在 Linux 6.12 LTS 内核源码树中以可加载模块形式编译，验证 zero-modify 编译。

**Architecture**:

```
[ UsrLinuxEmu repo root ]
    │
    ├── plugins/gpu_driver/drv/         ← 源码 (no changes here)
    │    ├── gpgpu_device.h
    │    ├── gpgpu_device.cpp
    │    ├── drv_ioctl.cpp
    │    └── ...
    │
    └── tools/ci/l2-portability/
         ├── build-drv-against-linux-6.12.sh   ← NEW (本 change)
         ├── kernel-patches/                   ← Optional: minimal
         │                                       compatibility 模拟 fix
         │                                       shims (per-match)
         └── README.md
```

**Build flow**:

1. CI workflow 启动 ubuntu-latest runner
2. Fetch kernel `torvalds/linux` at tag `v6.12` (LTS)
3. Untar to cache dir
4. Build tool: `make -C /lib/modules/$(uname -r)/build M=...`
   - 不实际加载 module（仅 build：`modules` target 而非 `modules_install`）
5. Capture warnings/errors:
   - `errors=0 warnings=0`: ✅ baseline OK
   - `errors>0`: ❌ gate fail → PR blocked
   - `warnings>0 but errors=0`: ⚠ advisory（记录但不阻断）

### L3 — docs-audit (已存在)

- `tools/docs-audit.sh --strict` 跑 pre-commit hook + CI

## Build script design

`tools/ci/l2-portability/build-drv-against-linux-6.12.sh` skeleton:

```bash
#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
KERNEL_VERSION="${KERNEL_VERSION:-6.12}"
KERNEL_TARBALL="${KERNEL_TARBALL:-${HOME}/.cache/linux-${KERNEL_VERSION}.tar.xz}"
BUILD_DIR="${BUILD_DIR:-${HOME}/.cache/l2-build}"

# 1. Fetch kernel source (cached)
if [[ ! -f "${KERNEL_TARBALL}" ]]; then
  echo "[L2] Fetching Linux ${KERNEL_VERSION}..."
  mkdir -p "$(dirname "${KERNEL_TARBALL}")"
  curl -fsSL "https://cdn.kernel.org/pub/linux/kernel/v$(echo ${KERNEL_VERSION} | head -c 1).x/linux-${KERNEL_VERSION}.tar.xz" \
    -o "${KERNEL_TARBALL}"
fi

# 2. Untar + configure
mkdir -p "${BUILD_DIR}"
tar xf "${KERNEL_TARBALL}" -C "${BUILD_DIR}"
KERNEL_SRC="${BUILD_DIR}/linux-${KERNEL_VERSION}"

# 3. Symlink drv/ to /drivers/gpu/emu-drv/ in kernel tree
mkdir -p "${KERNEL_SRC}/drivers/gpu"
ln -sfn "${REPO_ROOT}/plugins/gpu_driver/drv" "${KERNEL_SRC}/drivers/gpu/emu-drv"

# 4. Build (only; no load)
pushd "${KERNEL_SRC}"
make modules M=drivers/gpu/emu-drv \
  ARCH=x86_64 \
  CROSS_COMPILE="" \
  -j"$(nproc)" 2>&1 | tee "${BUILD_DIR}/build.log"
popd

# 5. Result
ERRORS=$(grep -c "error:" "${BUILD_DIR}/build.log" || true)
WARNINGS=$(grep -c "warning:" "${BUILD_DIR}/build.log" || true)
echo "[L2] Result: errors=${ERRORS} warnings=${WARNINGS}"
[[ "${ERRORS}" -eq 0 ]] || { echo "[L2] ❌ build failed"; exit 1; }
echo "[L2] ✅ build OK"
```

## Known compatibility assumptions

- Target Linux kernel 6.12 LTS (released 2024-11)
- x86_64 architecture（usermode-emulation 仅此 arch 支持）
- `linux_compat/` header paths substituted via symlink (not -I patches)
- Module compilation only（不实际 load，避免 rtld 模拟兼容问题）

## CI workflow integration

`.github/workflows/l2-portability.yml` skeleton:

```yaml
name: l2-portability
on:
  pull_request:
    paths:
      - 'plugins/gpu_driver/drv/**'
      - 'include/kernel/**'
      - 'tools/ci/l2-portability/**'
jobs:
  build:
    runs-on: ubuntu-22.04
    strategy:
      matrix:
        kernel: ['6.6', '6.12']  # baseline 2 LTS kernels
    steps:
      - uses: actions/checkout@v4
      - name: L2 build
        run: ./tools/ci/l2-portability/build-drv-against-linux-${KERNEL}.sh
        env:
          KERNEL_VERSION: ${{ matrix.kernel }}
      - name: Upload build log on failure
        if: failure()
        uses: actions/upload-artifact@v4
        with:
          name: l2-build-log-${{ matrix.kernel }}
          path: .cache/l2-build/build.log
```

## Failure handling

- **Build failure** → PR blocked; build log artifact uploaded; comment auto-generated listing first 3 errors
- **Warning only** → not blocked; recorded in weekly L2 report (not in this change scope)
