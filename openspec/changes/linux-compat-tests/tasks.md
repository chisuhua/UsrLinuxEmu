# linux-compat-tests: 实施任务

## 1. types.h 测试
- [ ] 1.1 `test_linux_types` — u8/u16/u32/u64/s8/s16/s32/s64 定义
- [ ] 1.2 `test_linux_limits` — PAGE_SIZE/PAGE_SHIFT 定义

## 2. iommu 测试
- [ ] 2.1 `test_iommu_domain_init` — domain 创建和销毁
- [ ] 2.2 `test_iommu_map_unmap` — 基本 map/unmap

## 3. drm 测试
- [ ] 3.1 `test_drm_device_init` — 设备创建生命周期

## 4. pci 测试
- [ ] 4.1 `test_pci_device_probe` — PCI 设备探测

## 5. CMake 集成
- [ ] 5.1 添加 `test_linux_compat` target 到 tests/