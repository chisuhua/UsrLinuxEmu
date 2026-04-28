/**
 * cuda_compat_ioctl.h - CUDA ioctl 转译层接口
 * 
 * DDS v1.2 架构定义：
 * - 将 TaskRunner 的 CUDA ioctl 命令转译为 UsrLinuxEmu 内部调用
 * - Phase 1: CUDA 专用接口
 * - Phase 2: 统一 GPU 接口（gpu_ioctl.h + 转译层）
 */

#ifndef _USR_LINUX_EMU_CUDA_COMPAT_IOCTL_H
#define _USR_LINUX_EMU_CUDA_COMPAT_IOCTL_H

#include "cuda_ioctl.h"

// 前向声明
namespace taskrunner {
    class CudaStub;
}

#ifdef __cplusplus
extern "C" {
#endif

/**
 * CUDA ioctl 转译入口
 * 
 * @param request ioctl 命令号（CUDA_IOCTL_*）
 * @param argp 参数指针
 * @return 0 成功，-errno 失败
 */
int cuda_compat_ioctl(unsigned long request, void* argp);

/**
 * 清理 CUDA Stub（进程退出时调用）
 */
void cuda_compat_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // _USR_LINUX_EMU_CUDA_COMPAT_IOCTL_H
