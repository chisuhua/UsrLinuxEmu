#!/usr/bin/env bash
# tools/l2-build/build_iommu_driver.sh
# Stage 5.5.1 v0.2: L2 build script for iommu_driver plugin
# per ADR-072 v0.2 + ADR-091 v0.2

set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$PROJECT_ROOT"

echo "[build_iommu_driver] Stage 5.5.1 - iommu_driver plugin L2 build"
echo "[build_iommu_driver] PROJECT_ROOT=$PROJECT_ROOT"

mkdir -p build
cd build

cmake -DCMAKE_BUILD_TYPE=Release \
      -DUSR_LINUX_EMU_L2_BUILD=ON \
      -DBUILD_IOMMU_DRIVER=ON \
      ..

cmake --build . --target iommu_driver_plugin -- -j"$(nproc 2>/dev/null || echo 2)"

echo "[build_iommu_driver] DONE - artifacts:"
ls -la "$PROJECT_ROOT/plugins/plugin_iommu_driver.so" 2>/dev/null