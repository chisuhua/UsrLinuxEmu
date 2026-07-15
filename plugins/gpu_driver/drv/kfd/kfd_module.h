/*
 * kfd_module.h — KFD 子系统生命周期桥接接口
 *
 * C-12 Phase B.1.2 (per tasks.md §B.1.2):
 *   kfd_module.c ↔ plugin.cpp 桥接契约。
 *
 * 设计原则（Metis AMB-1 resolution, 2026-07-15）：
 *   1. kfd_module.c 不直接定义 `module mod`（避免与 plugin.cpp 的 `mod` 冲突）
 *   2. kfd_module.c 暴露 kfd_module_init() / kfd_module_exit() 函数
 *   3. plugin.cpp 的 plugin_init_internal() 在 HAL 初始化后调用 kfd_module_init()
 *   4. plugin.cpp 的 plugin_fini_internal() 在 HAL 销毁前调用 kfd_module_exit()
 *   5. kfd_module.c 内部可用 module_init()/module_exit() 宏做 zero-overhead 包装
 *
 * Kernel idiom 兼容：
 *   - 当 kfd_module.c 移植到真机 Linux 内核时，只需：
 *     (a) 删除 kfd_module_init/_exit 函数包装
 *     (b) 将 module_init()/module_exit() 宏替换为内核版本（已存在于 <linux/module.h>）
 *   - 函数签名（int kfd_module_init(void) / void kfd_module_exit(void)）与
 *     Linux kernel int init(void) / void exit(void) 一致
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/*
 * kfd_module_init — KFD 子系统初始化入口
 *
 * 调用时机：plugin.cpp plugin_init_internal() 中，HAL init 之后、IOCTL 注册之前
 * 必须已就绪：struct gpu_hal_ops（14 fn-ptr，含 ADR-061/062 新增 iommu/event）
 *
 * @return 0 成功；负值 Linux errno（-ENOMEM, -EINVAL 等）
 */
int kfd_module_init(void);

/*
 * kfd_module_exit — KFD 子系统清理入口
 *
 * 调用时机：plugin.cpp plugin_fini_internal() 中，IOCTL 注销之后、HAL 销毁之前
 * 对称性：kfd_module_init() 成功后必须配对调用 kfd_module_exit()
 */
void kfd_module_exit(void);

/*
 * Kernel-style module 宏（zero-overhead 包装）
 *
 * 在 kfd_module.c 内部使用：
 *   static int __kfd_init(void) { ... return 0; }
 *   static void __kfd_exit(void) { ... }
 *   module_init(__kfd_init);
 *   module_exit(__kfd_exit);
 *
 * 在真机 Linux 内核移植时，这些宏已存在于 <linux/module.h>，
 * 只需删除本 kfd_module.h 中的宏定义。
 */
#define module_init(init_fn) \
    int kfd_module_init(void) { return init_fn(); }
#define module_exit(exit_fn) \
    void kfd_module_exit(void) { exit_fn(); }

#ifdef __cplusplus
}  // extern "C"
#endif
