#!/usr/bin/env bash
# tools/l2-build/build_pci_driver.sh
# Stage 5.5.1 v0.2: L2 build script for pci_driver plugin
# per ADR-072 v0.2 + ADR-091 v0.2

set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$PROJECT_ROOT"

echo "[build_pci_driver] Stage 5.5.1 - pci_driver plugin L2 build"
echo "[build_pci_driver] PROJECT_ROOT=$PROJECT_ROOT"

# Out-of-tree build
mkdir -p build
cd build

cmake -DCMAKE_BUILD_TYPE=Release \
      -DUSR_LINUX_EMU_L2_BUILD=ON \
      -DBUILD_PCI_DRIVER=ON \
      ..

cmake --build . --target pci_driver_plugin -- -j"$(nproc 2>/dev/null || echo 2)"

echo "[build_pci_driver] DONE - artifacts:"
/ -name "plugin_pci_driver.so" "$PROJECT_ROOT/plugins/" 2>/dev/null | head -3