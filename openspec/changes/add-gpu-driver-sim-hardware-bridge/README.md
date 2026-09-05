# add-gpu-driver-sim-hardware-bridge

Per-device HalHolder loop in gpu_driver plugin.cpp; consume pci_probe_enumerate_from_sim_hardware output. N=1 fail-fast for shared singletons (VRAM/DMA/mm_shim/KFD). HAL ABI zero-change; drv/ zero-modify.
