/**
 * cuda_compat_ioctl.cpp - CUDA ioctl 转译层
 * 
 * 将 TaskRunner 的 CUDA ioctl 命令转译为 UsrLinuxEmu 内部调用
 * 
 * DDS v1.2 架构决策:
 * - Phase 1: CUDA 专用接口（本文件）
 * - Phase 2: 统一 GPU 接口（gpu_ioctl.h + 转译层）
 */

#include "usr_linux_emu/cuda_compat_ioctl.h"
#include "cuda_stub.hpp"  // TaskRunner CudaStub
#include <iostream>
#include <cstring>
#include <atomic>
#include <cerrno>

// 全局 CUDA Stub 实例（单例）
static taskrunner::CudaStub* g_cuda_stub = nullptr;
static std::atomic<bool> g_cuda_stub_initialized{false};

// Fence 计数器（Stub 模式）
static std::atomic<uint64_t> g_next_fence_id{1};

/**
 * 初始化 CUDA Stub（首次调用时）
 */
static int ensure_cuda_stub_initialized() {
    if (!g_cuda_stub_initialized.load()) {
        g_cuda_stub = new taskrunner::CudaStub();
        g_cuda_stub->set_stub_mode(true);  // Phase 1: Stub 模式
        auto ret = g_cuda_stub->initialize();
        if (ret != taskrunner::CudaResult::SUCCESS) {
            std::cerr << "[CUDACompat] Failed to initialize CudaStub\n";
            delete g_cuda_stub;
            g_cuda_stub = nullptr;
            return -1;
        }
        g_cuda_stub_initialized.store(true);
        std::cout << "[CUDACompat] CudaStub initialized (Stub mode)\n";
    }
    return 0;
}

/**
 * 获取下一个 Fence ID（Stub 模式）
 */
static uint64_t next_fence_id() {
    return g_next_fence_id.fetch_add(1, std::memory_order_relaxed);
}

/**
 * 转换 CudaResult 为 errno
 */
static int cuda_result_to_errno(taskrunner::CudaResult result) {
    switch (result) {
        case taskrunner::CudaResult::SUCCESS:
            return 0;
        case taskrunner::CudaResult::ERROR_OUT_OF_MEMORY:
            return ENOMEM;
        case taskrunner::CudaResult::ERROR_INVALID_VALUE:
            return EINVAL;
        case taskrunner::CudaResult::ERROR_NOT_INITIALIZED:
            return ENODEV;
        default:
            return EIO;
    }
}

/**
 * CUDA ioctl 转译入口
 * 
 * @param request ioctl 命令号
 * @param argp 参数指针
 * @return 0 成功，-errno 失败
 */
int cuda_compat_ioctl(unsigned long request, void* argp) {
    // 确保 CUDA Stub 已初始化
    int ret = ensure_cuda_stub_initialized();
    if (ret != 0) {
        return ret;
    }

    switch (request) {
        // ========================================================================
        // 内存管理
        // ========================================================================
        
        case CUDA_IOCTL_MEM_ALLOC: {
            auto req = static_cast<struct cuda_mem_alloc_request*>(argp);
            if (!req) return -EINVAL;

            // 调用 CudaStub
            uint64_t device_ptr = 0;
            auto result = g_cuda_stub->mem_alloc(req->size, &device_ptr);
            if (result != taskrunner::CudaResult::SUCCESS) {
                std::cerr << "[CUDACompat] mem_alloc failed (size=" << req->size << ")\n";
                return -cuda_result_to_errno(result);
            }

            req->device_ptr = device_ptr;
            req->fence_id = next_fence_id();

            std::cout << "[CUDACompat] MEM_ALLOC: size=" << req->size 
                      << " → ptr=0x" << std::hex << device_ptr << std::dec
                      << " fence=" << req->fence_id << "\n";
            break;
        }

        case CUDA_IOCTL_MEM_FREE: {
            auto req = static_cast<struct cuda_mem_free_request*>(argp);
            if (!req) return -EINVAL;

            auto result = g_cuda_stub->mem_free(req->device_ptr);
            if (result != taskrunner::CudaResult::SUCCESS) {
                std::cerr << "[CUDACompat] mem_free failed (ptr=0x" 
                          << std::hex << req->device_ptr << std::dec << ")\n";
                return -cuda_result_to_errno(result);
            }

            req->fence_id = next_fence_id();

            std::cout << "[CUDACompat] MEM_FREE: ptr=0x" << std::hex 
                      << req->device_ptr << std::dec << " fence=" << req->fence_id << "\n";
            break;
        }

        case CUDA_IOCTL_MEMCPY_H2D: {
            auto req = static_cast<struct cuda_memcpy_h2d_request*>(argp);
            if (!req) return -EINVAL;

            // Stub 模式：直接返回（不实际拷贝）
            // Phase 2: 实现真实拷贝
            auto result = g_cuda_stub->memcpy_h2d(req->device_ptr, req->host_ptr, req->size);
            if (result != taskrunner::CudaResult::SUCCESS) {
                std::cerr << "[CUDACompat] memcpy_h2d failed\n";
                return -cuda_result_to_errno(result);
            }

            req->fence_id = next_fence_id();

            std::cout << "[CUDACompat] MEMCPY_H2D: ptr=0x" << std::hex 
                      << req->device_ptr << std::dec << " offset=" << req->offset
                      << " size=" << req->size << " fence=" << req->fence_id << "\n";
            break;
        }

        case CUDA_IOCTL_MEMCPY_D2H: {
            auto req = static_cast<struct cuda_memcpy_d2h_request*>(argp);
            if (!req) return -EINVAL;

            // Stub 模式：填充 0（用于测试）
            auto result = g_cuda_stub->memcpy_d2h(req->host_ptr, req->device_ptr, req->size);
            if (result != taskrunner::CudaResult::SUCCESS) {
                std::cerr << "[CUDACompat] memcpy_d2h failed\n";
                return -cuda_result_to_errno(result);
            }

            req->fence_id = next_fence_id();

            std::cout << "[CUDACompat] MEMCPY_D2H: ptr=0x" << std::hex 
                      << req->device_ptr << std::dec << " offset=" << req->offset
                      << " size=" << req->size << " fence=" << req->fence_id << "\n";
            break;
        }

        // ========================================================================
        // Kernel 启动
        // ========================================================================

        case CUDA_IOCTL_LAUNCH_KERNEL: {
            auto req = static_cast<struct cuda_launch_kernel_request*>(argp);
            if (!req) return -EINVAL;

            // 构建 LaunchParams
            taskrunner::LaunchParams params;
            params.kernel_name = req->kernel_name;
            params.params = req->params;
            params.grid_dim_x = req->grid_dim_x;
            params.grid_dim_y = req->grid_dim_y;
            params.grid_dim_z = req->grid_dim_z;
            params.block_dim_x = req->block_dim_x;
            params.block_dim_y = req->block_dim_y;
            params.block_dim_z = req->block_dim_z;

            uint64_t task_id = 0;
            auto result = g_cuda_stub->launch_kernel(params, &task_id);
            if (result != taskrunner::CudaResult::SUCCESS) {
                std::cerr << "[CUDACompat] launch_kernel failed\n";
                return -cuda_result_to_errno(result);
            }

            req->task_id = task_id;
            req->fence_id = next_fence_id();

            std::cout << "[CUDACompat] LAUNCH_KERNEL: '" << req->kernel_name << "'\n";
            std::cout << "  grid=" << req->grid_dim_x << "x" << req->grid_dim_y << "x" << req->grid_dim_z << "\n";
            std::cout << "  block=" << req->block_dim_x << "x" << req->block_dim_y << "x" << req->block_dim_z << "\n";
            std::cout << "  task_id=" << req->task_id << " fence=" << req->fence_id << "\n";
            break;
        }

        // ========================================================================
        // 同步原语
        // ========================================================================

        case CUDA_IOCTL_WAIT_FENCE: {
            auto req = static_cast<struct cuda_wait_fence_request*>(argp);
            if (!req) return -EINVAL;

            // Phase 1: Fence 与 Event 映射（简化：直接用 fence_id 作为 event_id）
            auto result = g_cuda_stub->wait_event(req->fence_id, req->timeout_ms);
            if (result != taskrunner::CudaResult::SUCCESS) {
                std::cerr << "[CUDACompat] wait_event failed (fence=" << req->fence_id << ")\n";
                return -cuda_result_to_errno(result);
            }

            std::cout << "[CUDACompat] WAIT_FENCE: fence=" << req->fence_id;
            if (req->timeout_ms > 0) {
                std::cout << " timeout=" << req->timeout_ms << "ms";
            }
            std::cout << " → signaled\n";
            break;
        }

        case CUDA_IOCTL_QUERY_FENCE: {
            auto req = static_cast<struct cuda_query_fence_request*>(argp);
            if (!req) return -EINVAL;

            int signaled = 0;
            auto result = g_cuda_stub->query_event(req->fence_id, &signaled);
            if (result != taskrunner::CudaResult::SUCCESS) {
                std::cerr << "[CUDACompat] query_event failed (fence=" << req->fence_id << ")\n";
                return -cuda_result_to_errno(result);
            }

            req->signaled = signaled;

            std::cout << "[CUDACompat] QUERY_FENCE: fence=" << req->fence_id << " → " 
                      << (signaled ? "signaled" : "unsignaled") << "\n";
            break;
        }

        // ========================================================================
        // Phase 2 预留（Graph/Batch）
        // ========================================================================

        case CUDA_IOCTL_GRAPH_CREATE:
        case CUDA_IOCTL_GRAPH_LAUNCH: {
            std::cerr << "[CUDACompat] Graph commands not implemented (Phase 2)\n";
            return -ENOTSUP;
        }

        // ========================================================================
        // 未知命令
        // ========================================================================

        default:
            std::cerr << "[CUDACompat] Unknown ioctl command: 0x" << std::hex << request << std::dec << "\n";
            return -ENOTSUP;
    }

    return 0;
}

/**
 * 清理 CUDA Stub（进程退出时调用）
 */
void cuda_compat_cleanup() {
    if (g_cuda_stub_initialized.load()) {
        g_cuda_stub->shutdown();
        delete g_cuda_stub;
        g_cuda_stub = nullptr;
        g_cuda_stub_initialized.store(false);
        std::cout << "[CUDACompat] CudaStub shutdown\n";
    }
}
