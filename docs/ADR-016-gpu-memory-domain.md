# ADR-016: GPU Memory Domain 模型

**状态**: 已接受 (Accepted)

**日期**: 2026-04-28

**提案人**: Sisyphus (基于 ADR-015 分析)

**评审者**: UsrLinuxEmu Architecture Team, TaskRunner Team

**关联 ADR**: ADR-004 (Buddy Allocator), ADR-015 (GPU IOCTL Unification)

---

## 背景

ADR-015 分析发现，System C 的 `GPU_IOCTL_ALLOC_BO` 需要支持 memory domain 参数，这与 AMD ROCm/NVIDIA 驱动的内存模型一致。

**问题**: 现有 ADR-004 (Buddy Allocator) 只定义了 GPU 内存的子分配算法，没有定义 domain 选择层。

**决策需要**: `ALLOC_BO` 的 domain 参数应该如何工作？VRAM/GTT/CPU 三种 domain 的语义是什么？

---

## 内存 Domain 定义

### Domain 概念

Memory Domain 是 GPU 内存分配的目标位置/类型选择：

| Domain | 描述 | 典型用途 | 带宽/延迟 |
|--------|------|---------|----------|
| **VRAM** | GPU 本地视频内存 | GPU 计算内核、纹理 | 最高带宽，最低延迟 |
| **GTT** | GPU-mappable system memory (通过 GART 页表) | CPU→GPU 传输、中等性能需求 | 中等带宽，中等延迟 |
| **CPU** | 系统内存，不经 GPU 直接访问 | 主机端 pinned memory、zero-copy | 需要 GPU 映射后访问 |

### 与 AMD ROCm 对齐

AMD GPU 驱动使用以下 domain（来自 `amdgpu_drm.h`）：
- `AMDGPU_GEM_DOMAIN_VRAM` (0x4): 本地 GPU 内存
- `AMDGPU_GEM_DOMAIN_GTT` (0x2): GPU 可映射的系统内存
- `AMDGPU_GEM_DOMAIN_CPU` (0x1): 系统内存（CPU 直接访问）

**注意**：GPU 系统使用 **bitmask** 而非序数，以便支持多 domain 组合（如 VRAM | GTT）。

### 与 NVIDIA UVM 对齐

NVIDIA UVM 有类似的分类：
- **vidmem**: GPU 本地显存
- **sysmem**: 系统内存
- **一体化**: 通过 UVM API 统一管理

---

## 决策

### 决策内容

1. **采用三层 Domain 模型**: VRAM / GTT / CPU
2. **`GPU_IOCTL_ALLOC_BO` 增加 domain 参数**:
   ```c
   struct gpu_alloc_bo_args {
       __u64 size;
       __u32 domain;        // GPU_MEM_DOMAIN_VRAM / GTT / CPU
       __u32 flags;         // COHERENT / UNCACHED / etc.
       __u32 handle;        // OUT: buffer handle
       __u64 gpu_va;       // OUT: GPU virtual address
   };
   ```
3. **Domain 选择层在 Buddy Allocator 之上**: 分配时先选择 domain，再在 domain 内部使用 Buddy Allocator（VRAM）或 slab/其他算法（GTT/CPU）

### 层级关系

```
ALLOC_BO (用户请求)
    │
    ├── domain = VRAM → Buddy Allocator (VRAM 内部子分配)
    ├── domain = GTT  → GTT Allocator (系统内存，GPU 可映射)
    └── domain = CPU  → CPU Allocator (系统内存，仅 CPU 访问)
```

---

## 实现建议

### 内存分配流程

```cpp
enum gpu_mem_domain {
    GPU_MEM_DOMAIN_VRAM = 0x1,   // Bitmask: 本地 GPU 内存
    GPU_MEM_DOMAIN_GTT  = 0x2,   // Bitmask: GPU 可映射系统内存
    GPU_MEM_DOMAIN_CPU  = 0x4,   // Bitmask: 仅 CPU 访问系统内存
};

struct gpu_alloc_bo_args {
    uint64_t size;
    uint32_t domain;       // GPU_MEM_DOMAIN_* (bitmask)
    uint32_t flags;        // allocation flags
    uint32_t handle;      // OUT
    uint64_t gpu_va;      // OUT
};

int gpu_driver_alloc_bo(gpu_alloc_bo_args* args) {
    switch (args->domain) {
        case GPU_MEM_DOMAIN_VRAM:
            return vram_buddy_allocator.alloc(args->size, &args->handle, &args->gpu_va);
        case GPU_MEM_DOMAIN_GTT:
            return gtt_allocator.alloc(args->size, &args->handle, &args->gpu_va);
        case GPU_MEM_DOMAIN_CPU:
            return cpu_allocator.alloc(args->size, &args->handle, &args->gpu_va);
        default:
            return -EINVAL;
    }
}
```

### 与 ADR-004 的关系

- **ADR-004 (Buddy Allocator)**: 定义了 VRAM 内部子分配算法
- **ADR-016**: 定义 domain 选择层，Buddy Allocator 作为 VRAM domain 的子分配器

### Flags 参数

| Flag | 描述 | ROCm 对应 |
|------|------|----------|
| `GPU_ALLOC_COHERENT` | 缓存一致内存 | `hipHostMallocCoherent` |
| `GPU_ALLOC_NON_CACHED` | 非缓存内存 | `hipHostMallocNonCoherent` |
| `GPU_ALLOC_PINNED` | pinned memory (zero-copy) | `hipHostMallocMapped` |

---

## 后果

### 正面后果

- ✅ 与 AMD ROCm / NVIDIA UVM 内存模型对齐
- ✅ 支持不同性能的内存分配选择
- ✅ 为 zero-copy 和 host-memory 预留扩展空间
- ✅ 分离 domain 选择和内部分配算法，职责清晰

### 负面后果

- ⚠️ 增加内存分配的复杂度（需要选择 domain）
- ⚠️ GTT/CPU domain 的分配器需要单独实现
- ⚠️ 需要处理 domain 间数据迁移（未来功能）

---

## 备选方案

### 方案 A: 仅 VRAM（简化）

**不采用**，原因：
- 失去 GTT 的灵活性
- 无法支持 zero-copy host memory
- 与真实驱动差距过大

### 方案 B: 透明 domain

让驱动自动选择最优 domain，对用户隐藏。

**不采用**，原因：
- 用户无法控制内存位置
- 不符合 CUDA/HIP 的 explicit memory placement 语义
- 调试困难

---

## 结论

**推荐决策**: 采用三层 Domain 模型 (VRAM/GTT/CPU)，`ALLOC_BO` 支持 domain 参数。

---

**维护者**: UsrLinuxEmu Architecture Team + TaskRunner Team

**最后更新**: 2026-04-28 (Phase 0 修复：domain 字段已添加至 gpu_alloc_bo_args)

**评审截止**: 2026-05-11