/*
 * plugin.cpp - GPU 驱动仿真插件入口
 */
#include "shared/gpu_ioctl.h"
#include "shared/gpu_types.h"
#include "shared/gpu_events.h"
#include "kernel/file_ops.h"
#include "kernel/vfs.h"
#include "kernel/device/device.h"
#include "kernel/module_loader.h"
#include <iostream>
  #include <cstring>
  #include <atomic>
  #include <map>
  #include <mutex>
  #include <vector>
  #include <thread>
  #include <chrono>

 constexpr u32 VENDOR_SIMULATED = 0x1000;
 constexpr u32 DEVICE_SIMULATED_V1 = 0x1001;
 constexpr u64 SIMULATED_VRAM_SIZE = 8ULL * 1024 * 1024 * 1024;
 constexpr u64 SIMULATED_BAR0_SIZE = 16ULL * 1024 * 1024;
 constexpr u32 SIMULATED_MAX_CHANNELS = 32;
 constexpr u32 SIMULATED_COMPUTE_UNITS = 64;
 constexpr u32 SIMULATED_GPFIFO_CAPACITY = 1024;
 constexpr u32 SIMULATED_CACHE_LINE_SIZE = 64;

class BuddyAllocator {
  public:
    static constexpr u64 MIN_BLOCK_SIZE = 4 * 1024ULL;
    static constexpr u64 MAX_BLOCK_SIZE = SIMULATED_VRAM_SIZE;
    static constexpr u32 MAX_ORDER = 21;

    BuddyAllocator(u64 base, u64 size) : base_(base), size_(size) {
      for (u32 i = 0; i <= MAX_ORDER; ++i) {
        free_lists_[i] = nullptr;
      }
      max_order_ = 0;
      while ((MIN_BLOCK_SIZE << max_order_) < size_) {
        ++max_order_;
      }
      if (max_order_ > MAX_ORDER) max_order_ = MAX_ORDER;

      Block* initial = allocateBlock(base_, MIN_BLOCK_SIZE << max_order_);
      if (initial) {
        insertFree(initial, max_order_);
      }

      std::cout << "[BuddyAllocator] Initialized: base=0x" << std::hex << base_
                << " size=" << std::dec << size_
                << " max_order=" << max_order_ << "\n";
    }

    u64 allocate(u64 size) {
      if (size == 0) return 0;

      u64 aligned_size = roundUpToPowerOf2(size);
      if (aligned_size < MIN_BLOCK_SIZE) {
        aligned_size = MIN_BLOCK_SIZE;
      }

      u32 order = orderForSize(aligned_size);
      if (order > MAX_ORDER) {
        std::cerr << "[BuddyAllocator] allocate: size " << aligned_size
                  << " exceeds max block size\n";
        return 0;
      }

      std::lock_guard<std::mutex> lock(mutex_);

      u32 target_order = order;
      for (u32 i = order; i <= max_order_; ++i) {
        if (free_lists_[i] != nullptr) {
          target_order = i;
          break;
        }
      }

      if (target_order > max_order_ || free_lists_[target_order] == nullptr) {
        std::cerr << "[BuddyAllocator] allocate: out of memory for size " << aligned_size << "\n";
        return 0;
      }

      while (target_order > order) {
        Block* block = removeFree(target_order);
        if (!block) break;

        u64 block_size = MIN_BLOCK_SIZE << target_order;
        u64 half_size = block_size / 2;

        Block* left = allocateBlock(block->addr, half_size);
        Block* right = allocateBlock(block->addr + half_size, half_size);

        delete block;

        insertFree(left, target_order - 1);
        insertFree(right, target_order - 1);

        --target_order;
      }

      Block* block = removeFree(target_order);
      if (!block) {
        std::cerr << "[BuddyAllocator] allocate: failed to get block at order " << target_order << "\n";
        return 0;
      }

      u64 addr = block->addr;
      delete block;

      allocated_blocks_[addr] = {addr, aligned_size, target_order};

      std::cout << "[BuddyAllocator] allocate: addr=0x" << std::hex << addr
                << " size=" << std::dec << aligned_size
                << " order=" << target_order << "\n";

      return addr;
    }

    void free(u64 addr) {
      if (addr == 0) return;

      std::lock_guard<std::mutex> lock(mutex_);

      auto it = allocated_blocks_.find(addr);
      if (it == allocated_blocks_.end()) {
        std::cerr << "[BuddyAllocator] free: invalid address 0x" << std::hex << addr << "\n";
        return;
      }

      AllocInfo info = it->second;
      allocated_blocks_.erase(it);

      Block* block = allocateBlock(addr, MIN_BLOCK_SIZE << info.order);
      insertFree(block, info.order);

      coalesce(info.order);

      std::cout << "[BuddyAllocator] free: addr=0x" << std::hex << addr
                << " order=" << std::dec << info.order << "\n";
    }

    void reset() {
      std::lock_guard<std::mutex> lock(mutex_);

      for (u32 i = 0; i <= MAX_ORDER; ++i) {
        Block* current = free_lists_[i];
        while (current) {
          Block* next = current->next;
          delete current;
          current = next;
        }
        free_lists_[i] = nullptr;
      }

      allocated_blocks_.clear();

      Block* initial = allocateBlock(base_, MIN_BLOCK_SIZE << max_order_);
      if (initial) {
        insertFree(initial, max_order_);
      }

      std::cout << "[BuddyAllocator] Reset\n";
    }

    void dump() const {
      std::lock_guard<std::mutex> lock(mutex_);

      std::cout << "[BuddyAllocator] State:\n";
      std::cout << "  Base: 0x" << std::hex << base_ << " Size: " << std::dec << size_ << "\n";
      std::cout << "  Max order: " << max_order_ << "\n";

      for (u32 i = 0; i <= MAX_ORDER; ++i) {
        Block* current = free_lists_[i];
        u32 count = 0;
        while (current) {
          ++count;
          current = current->next;
        }
        if (count > 0) {
          u64 block_size = MIN_BLOCK_SIZE << i;
          std::cout << "  Order " << i << " (" << block_size << " bytes): " << count << " blocks\n";
        }
      }

      std::cout << "  Allocated: " << allocated_blocks_.size() << " blocks\n";
    }

  private:
    struct Block {
      u64 addr;
      Block* next;
      Block* prev;
    };

    struct AllocInfo {
      u64 addr;
      u64 size;
      u32 order;
    };

    Block* allocateBlock(u64 addr, u64 size) {
      Block* block = new (std::nothrow) Block;
      if (block) {
        block->addr = addr;
        block->next = nullptr;
        block->prev = nullptr;
      }
      return block;
    }

    void insertFree(Block* block, u32 order) {
      if (!block || order > MAX_ORDER) return;

      block->next = free_lists_[order];
      block->prev = nullptr;
      if (free_lists_[order]) {
        free_lists_[order]->prev = block;
      }
      free_lists_[order] = block;
    }

    Block* removeFree(Block* block, u32 order) {
      if (!block || order > MAX_ORDER) return nullptr;

      if (block->prev) {
        block->prev->next = block->next;
      } else {
        free_lists_[order] = block->next;
      }
      if (block->next) {
        block->next->prev = block->prev;
      }
      block->next = nullptr;
      block->prev = nullptr;

      return block;
    }

    Block* removeFree(u32 order) {
      if (order > MAX_ORDER || !free_lists_[order]) return nullptr;

      Block* block = free_lists_[order];
      free_lists_[order] = block->next;
      if (block->next) {
        block->next->prev = nullptr;
      }
      block->next = nullptr;
      block->prev = nullptr;

      return block;
    }

    void coalesce(u32 order) {
      if (order >= max_order_) return;

      Block* current = free_lists_[order];
      while (current) {
        u64 block_size = MIN_BLOCK_SIZE << order;
        u64 buddy_addr = getBuddyAddr(current->addr, block_size);

        Block* buddy = findBuddy(buddy_addr, order);
        if (buddy && areAdjacent(current, buddy, block_size)) {
          removeFree(current, order);
          removeFree(buddy, order);

          u64 merged_addr = std::min(current->addr, buddy->addr);
          delete current;
          delete buddy;

          Block* merged = allocateBlock(merged_addr, block_size * 2);
          insertFree(merged, order + 1);

          coalesce(order + 1);

          current = free_lists_[order];
        } else {
          current = current->next;
        }
      }
    }

    Block* findBuddy(u64 buddy_addr, u32 order) {
      Block* current = free_lists_[order];
      while (current) {
        if (current->addr == buddy_addr) {
          return current;
        }
        current = current->next;
      }
      return nullptr;
    }

    bool areAdjacent(Block* a, Block* b, u64 block_size) const {
      if (!a || !b) return false;
      return (a->addr + block_size == b->addr) || (b->addr + block_size == a->addr);
    }

    u64 getBuddyAddr(u64 addr, u64 block_size) const {
      return base_ + ((addr - base_) ^ block_size);
    }

    static u64 roundUpToPowerOf2(u64 size) {
      --size;
      size |= size >> 1;
      size |= size >> 2;
      size |= size >> 4;
      size |= size >> 8;
      size |= size >> 16;
      size |= size >> 32;
      return size + 1;
    }

    u32 orderForSize(u64 size) const {
      u32 order = 0;
      u64 block_size = MIN_BLOCK_SIZE;
      while (block_size < size && order < MAX_ORDER) {
        block_size *= 2;
        ++order;
      }
      return order;
    }

    u64 base_;
    u64 size_;
    u32 max_order_;
    Block* free_lists_[MAX_ORDER + 1];
    std::map<u64, AllocInfo> allocated_blocks_;
    mutable std::mutex mutex_;
  };

 class HandleManager {
 public:
     u32 allocate() {
         std::lock_guard<std::mutex> lock(mutex_);
         for (u32 i = 1; i <= max_handles_; ++i) {
             if (handles_.find(i) == handles_.end()) {
                 handles_[i] = true;
                 return i;
             }
         }
         return 0;
     }

     bool free(u32 handle) {
         std::lock_guard<std::mutex> lock(mutex_);
         if (handle == 0 || handles_.find(handle) == handles_.end()) {
             return false;
         }
         handles_.erase(handle);
         return true;
     }

     bool valid(u32 handle) const {
         std::lock_guard<std::mutex> lock(mutex_);
         return handle != 0 && handles_.find(handle) != handles_.end();
     }

 private:
     static constexpr u32 max_handles_ = 65535;
     std::map<u32, bool> handles_;
     mutable std::mutex mutex_;
 };

 struct BoInfo {
     u64 gpu_va;
     u64 size;
     u32 domain;
     u32 flags;
 };

struct FenceInfo {
      std::atomic<bool> signaled{false};
  };

 class GpgpuDevice : public FileOperations {
 public:
     GpgpuDevice() : buddy_(0x100000000ULL, SIMULATED_VRAM_SIZE) {
         registered_kernels_["simple_kernel"] = 0;
         registered_kernels_["matmul_kernel"] = 1;
     }

     long ioctl(int fd, unsigned long request, void* argp) override {
         (void)fd;
         switch (request) {
             case GPU_IOCTL_GET_DEVICE_INFO:
                 return handle_get_device_info(argp);
             case GPU_IOCTL_ALLOC_BO:
                 return handle_alloc_bo(argp);
             case GPU_IOCTL_FREE_BO:
                 return handle_free_bo(argp);
             case GPU_IOCTL_MAP_BO:
                 return handle_map_bo(argp);
             case GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH:
                 return handle_pushbuffer_submit_batch(argp);
             case GPU_IOCTL_WAIT_FENCE:
                 return handle_wait_fence(argp);
             default:
                 std::cerr << "[GpgpuDevice] Unknown ioctl: 0x" << std::hex << request << std::dec << "\n";
                 return -EINVAL;
         }
     }

     int open(const char* path, int flags) override {
         (void)path;
         (void)flags;
         std::cout << "[GpgpuDevice] Opened\n";
         return 0;
     }

     int close(int fd) override {
         (void)fd;
         std::cout << "[GpgpuDevice] Closed\n";
         return 0;
     }

     std::string name = "gpgpu0";

 private:
     long handle_get_device_info(void* argp) {
         auto* info = static_cast<struct gpu_device_info*>(argp);
         if (!info) return -EFAULT;

         info->vendor_id = VENDOR_SIMULATED;
         info->device_id = DEVICE_SIMULATED_V1;
         info->vram_size = SIMULATED_VRAM_SIZE;
         info->bar0_size = SIMULATED_BAR0_SIZE;
         info->max_channels = SIMULATED_MAX_CHANNELS;
         info->compute_units = SIMULATED_COMPUTE_UNITS;
         info->gpfifo_capacity = SIMULATED_GPFIFO_CAPACITY;
         info->cache_line_size = SIMULATED_CACHE_LINE_SIZE;

         std::cout << "[GpgpuDevice] GET_DEVICE_INFO: vendor=0x" << std::hex << info->vendor_id
                   << " device=0x" << info->device_id << " vram=" << std::dec << info->vram_size << "\n";
         return 0;
     }

     long handle_alloc_bo(void* argp) {
         auto* args = static_cast<struct gpu_alloc_bo_args*>(argp);
         if (!args) return -EFAULT;

         if (args->domain == 0) {
             std::cerr << "[GpgpuDevice] ALLOC_BO: invalid domain (0)\n";
             return -EINVAL;
         }

         u64 gpu_va = buddy_.allocate(args->size);
         if (gpu_va == 0) {
             std::cerr << "[GpgpuDevice] ALLOC_BO: out of memory (size=" << args->size << ")\n";
             return -ENOMEM;
         }

         u32 handle = handles_.allocate();
         if (handle == 0) {
             buddy_.free(gpu_va);
             std::cerr << "[GpgpuDevice] ALLOC_BO: no available handles\n";
             return -ENOMEM;
         }

         bo_map_[handle] = {gpu_va, args->size, args->domain, args->flags};

         args->handle = handle;
         args->gpu_va = gpu_va;

         std::cout << "[GpgpuDevice] ALLOC_BO: handle=" << handle << " va=0x" << std::hex << gpu_va
                   << " size=" << std::dec << args->size << "\n";
         return 0;
     }

     long handle_free_bo(void* argp) {
         auto handle = *static_cast<u32*>(argp);
         if (handle == 0) return -EINVAL;

         if (!handles_.valid(handle)) {
             std::cerr << "[GpgpuDevice] FREE_BO: invalid handle " << handle << "\n";
             return -EINVAL;
         }

         auto it = bo_map_.find(handle);
         if (it != bo_map_.end()) {
             buddy_.free(it->second.gpu_va);
             bo_map_.erase(it);
         }

         handles_.free(handle);
         std::cout << "[GpgpuDevice] FREE_BO: handle=" << handle << "\n";
         return 0;
     }

     long handle_map_bo(void* argp) {
         auto* args = static_cast<struct gpu_map_bo_args*>(argp);
         if (!args) return -EFAULT;

         if (!handles_.valid(args->handle)) {
             std::cerr << "[GpgpuDevice] MAP_BO: invalid handle " << args->handle << "\n";
             return -EINVAL;
         }

         auto it = bo_map_.find(args->handle);
         if (it == bo_map_.end()) {
             return -EINVAL;
         }

         args->gpu_va = it->second.gpu_va;
         std::cout << "[GpgpuDevice] MAP_BO: handle=" << args->handle << " va=0x" << std::hex << args->gpu_va << "\n";
         return 0;
     }

     long handle_pushbuffer_submit_batch(void* argp) {
         auto* args = static_cast<struct gpu_pushbuffer_args*>(argp);
         if (!args) return -EFAULT;

         if (args->count == 0 || args->count > 16) {
             std::cerr << "[GpgpuDevice] PUSHBUFFER_SUBMIT_BATCH: invalid count " << args->count << "\n";
             return -EINVAL;
         }

         const struct gpu_gpfifo_entry* entries = args->entries;

         for (u32 i = 0; i < args->count; ++i) {
             const auto& e = entries[i];
             if (!e.valid) continue;

             switch (e.method) {
                 case GPU_OP_LAUNCH_KERNEL: {
                     u32 kernel_idx = static_cast<u32>(e.payload[0]);
                     u32 grid_dim = static_cast<u32>(e.payload[1]);
                     u32 block_dim = static_cast<u32>(e.payload[2]);

                     std::cout << "[GpgpuDevice] LAUNCH_KERNEL: idx=" << kernel_idx
                               << " grid=0x" << std::hex << grid_dim << " block=0x" << block_dim << "\n";
                     break;
                 }
                 case GPU_OP_MEMCPY: {
                     u64 src = e.payload[0];
                     u64 dst = e.payload[1];
                     u64 size = e.payload[2];

                     std::cout << "[GpgpuDevice] MEMCPY: src=0x" << std::hex << src
                               << " dst=0x" << dst << " size=" << std::dec << size << "\n";
                     break;
                 }
                 case GPU_OP_MEMSET: {
                     u64 dst = e.payload[0];
                     u32 val = static_cast<u32>(e.payload[1]);
                     u64 size = e.payload[2];

                     std::cout << "[GpgpuDevice] MEMSET: dst=0x" << std::hex << dst
                               << " val=0x" << val << " size=" << std::dec << size << "\n";
                     break;
                 }
case GPU_OP_FENCE: {
                      u64 fence_id = ++fence_counter_;
                      fences_[fence_id].signaled.store(true, std::memory_order_release);
                      std::cout << "[GpgpuDevice] FENCE: created id=" << fence_id << "\n";
                      break;
                  }
                 default:
                     std::cerr << "[GpgpuDevice] PUSHBUFFER: unknown method 0x" << std::hex << e.method << "\n";
                     break;
             }
         }

         return 0;
     }

long handle_wait_fence(void* argp) {
          auto* args = static_cast<struct gpu_wait_fence_args*>(argp);
          if (!args) return -EFAULT;

          u64 fence_id = args->fence_id;
          u32 timeout_ms = args->timeout_ms;

          auto it = fences_.find(fence_id);
          if (it == fences_.end()) {
              std::cerr << "[GpgpuDevice] WAIT_FENCE: id=" << fence_id << " not found, returning timeout\n";
              args->status = 0;
              return 0;
          }

          u64 elapsed_ms = 0;
          const u64 poll_interval_ms = 1;

          while (elapsed_ms < timeout_ms || timeout_ms == 0) {
              if (it->second.signaled.load(std::memory_order_acquire)) {
                  args->status = 1;
                  std::cout << "[GpgpuDevice] WAIT_FENCE: id=" << fence_id << " signaled=true (waited " << elapsed_ms << "ms)\n";
                  return 0;
              }
              std::this_thread::sleep_for(std::chrono::milliseconds(poll_interval_ms));
              elapsed_ms += poll_interval_ms;
          }

          args->status = 0;
          std::cout << "[GpgpuDevice] WAIT_FENCE: id=" << fence_id << " timeout after " << elapsed_ms << "ms\n";
          return 0;
      }

     BuddyAllocator buddy_;
     HandleManager handles_;
     std::map<u32, BoInfo> bo_map_;
     std::map<u64, FenceInfo> fences_;
     std::atomic<u64> fence_counter_{1};
     std::map<std::string, u32> registered_kernels_;
 };

extern "C" {

  static int plugin_init_internal() {
      std::cout << "[GpuPlugin] Initializing...\n";

      auto device = std::make_shared<GpgpuDevice>();

      VFS& vfs = VFS::instance();
      auto dev = std::make_shared<Device>(device->name, 0, device, nullptr);
      vfs.register_device(dev);

      std::cout << "[GpuPlugin] Registered /dev/gpgpu0\n";
      return 0;
  }

  static void plugin_fini_internal() {
      std::cout << "[GpuPlugin] Shutting down...\n";
  }

  module mod = {
      .name = "gpu_driver",
      .depends = nullptr,
      .init = plugin_init_internal,
      .exit = plugin_fini_internal,
  };

  }  // extern "C"
