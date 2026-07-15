/*
 * kfd_module.c — KFD 子系统初始化桩实现
 *
 * C-12 Phase B.1.1 stub (per tasks.md §B.1.1):
 *   当前仅提供 kfd_module_init/exit 函数签名，使 plugin.cpp 可链接。
 *   完整模块切分（kfd_pasid.c / kfd_process.c / kfd_topology 等）将在
 *   B.1.3-B.1.9 阶段实施。
 *
 * 与 kfd_module.h 桥接契约配套使用：
 *   - plugin_init_internal() → kfd_module_init() → 当前 return 0
 *   - plugin_fini_internal() → kfd_module_exit() → 当前 no-op
 *
 * B.1.1 实施时将：
 *   1. 替换为 module_init(__kfd_module_init) / module_exit(__kfd_module_exit)
 *   2. __kfd_module_init 内部调用：
 *      - kfd_topology_init()           (B.1.8 stub 扩展后可用)
 *      - kfd_pasid_init()              (B.1.3 实施后可用)
 *      - kfd_process_init()            (B.1.5 实施后可用)
 *   3. __kfd_module_exit 反向清理
 */
#include "kfd_module.h"

int kfd_module_init(void) {
  /* B.1.1 stub: 实际实现见 kfd_pasid.c / kfd_process.c (B.1.3 / B.1.5) */
  return 0;
}

void kfd_module_exit(void) {
  /* B.1.1 stub: 实际实现见 kfd_pasid.c / kfd_process.c (B.1.3 / B.1.5) */
}
