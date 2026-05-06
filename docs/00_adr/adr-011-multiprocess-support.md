# ADR-011: 多进程支持方案

**状态**: 提议

**日期**: 2026-03

## 背景

当前架构下所有设备驱动运行在单一进程内，这在以下场景存在局限：

1. **故障隔离**: 一个设备的驱动崩溃会影响所有设备
2. **资源隔离**: 无法限制单个设备的内存使用
3. **权限控制**: 无法为不同设备设置不同权限
4. **跨进程通信**: 未来可能需要支持多进程共享设备

## 决策

采用**分层渐进式**方案，支持进程级隔离但不强制使用：

### Phase 1: 共享内存 + 进程内隔离（当前推荐）

保持单进程架构，但通过 Namespaces/Cgroups 实现资源隔离：

```cpp
// 设备级进程隔离（使用 Linux namespaces）
class IsolatedDevice {
    pid_t namespace_pid_;  // 独立的 PID namespace
    std::shared_ptr<SharedMemoryRegion> shm_region_;
};

// 共享内存区域用于进程间通信
struct SharedCommandBuffer {
    std::atomic<uint32_t> write_offset;
    std::atomic<uint32_t> read_offset;
    uint8_t data[MAX_BUFFER_SIZE];
};
```

### Phase 2: 真正的多进程支持（可选）

对于需要真正隔离的场景，采用以下设计：

```
┌─────────────────────────────────────────────────────┐
│                   主进程 (Init)                      │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐ │
│  │  VFS Proxy  │  │ Plugin Mgr  │  │   Config    │ │
│  └──────┬──────┘  └──────┬──────┘  └─────────────┘ │
│         │                │                         │
│         │     IPC (Unix Domain Socket)              │
│         │                │                         │
└─────────┼────────────────┼─────────────────────────┘
          │                │
    ┌─────▼─────┐    ┌─────▼─────┐
    │ GPU 进程  │    │ Serial 进程│
    │  (独立)   │    │  (独立)   │
    └───────────┘    └───────────┘
```

## 方案比较

| 方案 | 隔离级别 | 复杂度 | 性能开销 | 适用场景 |
|------|---------|--------|---------|---------|
| **当前架构** | 无 | 低 | 无 | 开发测试、单用户 |
| **Namespace 隔离** | 进程级 | 中 | 低 (~5%) | 多租户、资源限制 |
| **独立进程** | 完全隔离 | 高 | 中 (~15%) | 高安全要求 |

## 实现要点

### 进程隔离内存管理

```cpp
// 共享内存池
class SharedMemoryPool {
    int shm_fd_;                    // System V / POSIX 共享内存
    size_t total_size_;
    std::atomic<size_t> used_size_;

    // 分配器（Buddy 或 Slab）
    BuddyAllocator* allocator_;
};

// 进程间同步
class IPCSemaphore {
    sem_t* sem_;                    // POSIX 信号量
    key_t key_;
};
```

### VFS 代理模式

```cpp
// 主进程的 VFS 代理
class VFSProxy : public VFS {
    // 转发设备操作到对应进程
    int ioctl(const std::string& device_path,
              unsigned long cmd,
              void* arg) override {
        // 1. 查找设备所在进程
        pid_t target_pid = device_table_[device_path].pid;

        // 2. 通过 IPC 发送请求
        IPCMessage req = {...};
        send_to_process(target_pid, req);

        // 3. 等待响应
        return wait_for_response(target_pid);
    }
};
```

## 后果

- ✅ **Phase 1**: 极低的实现复杂度，保持向后兼容
- ✅ **Phase 2**: 可选的完全隔离能力
- ✅ 共享内存提供高效 IPC
- ⚠️ Namespace 隔离需要 Linux 环境
- ⚠️ 独立进程模式增加调试复杂度

## 待决策项

1. 是否需要支持实时迁移（进程暂停/恢复）？
2. 共享内存的分配策略（预分配 vs 动态）？
3. IPC 通信协议（自定义 vs 标准化如 ZeroMQ）？

---

**维护者**: UsrLinuxEmu Architecture Team
**最后更新**: 2026-03