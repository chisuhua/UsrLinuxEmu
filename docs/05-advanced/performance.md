# 性能优化指南

本文档介绍 UsrLinuxEmu 项目的性能分析方法和优化技巧。

**最后更新**: 2026-03-24  
**作者**: UsrLinuxEmu Team

---

## 目录

1. [性能分析方法](#性能分析方法)
2. [常见性能瓶颈](#常见性能瓶颈)
3. [优化技巧](#优化技巧)
4. [GPU 性能优化](#gpu-性能优化)
5. [内存优化](#内存优化)
6. [并发优化](#并发优化)

---

## 性能分析方法

### perf 性能分析

`perf` 是 Linux 内核自带的性能分析工具。

```bash
# 安装 perf
sudo apt-get install linux-tools-common linux-tools-generic

# 采样分析（生成 perf.data）
perf record -g ./bin/cli_tool

# 查看性能报告
perf report

# 查看热点函数
perf report --stdio | head -50

# 生成火焰图
perf script | stackcollapse-perf.pl | flamegraph.pl > flame.svg
```

### gprof 性能分析

```bash
# 编译时启用 prof
cmake -DCMAKE_CXX_FLAGS="-pg -O2" ..
make

# 运行程序生成 gmon.out
./bin/cli_tool

# 分析性能
gprof ./bin/cli_tool gmon.out > profile.txt

# 查看报告
cat profile.txt
```

### Valgrind Callgrind

```bash
# 使用 Callgrind 分析
valgrind --tool=callgrind \
         --callgrind-out-file=callgrind.out \
         ./bin/cli_tool

# 查看报告
callgrind_annotate callgrind.out

# 或使用图形界面
kcachegrind callgrind.out
```

### 自定义性能计时

```cpp
#include <chrono>
#include <iostream>

class Timer {
public:
    void start() {
        start_ = std::chrono::high_resolution_clock::now();
    }
    
    void stop(const std::string& label) {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            end - start_
        ).count();
        std::cout << label << ": " << duration << " μs" << std::endl;
    }

private:
    std::chrono::high_resolution_clock::time_point start_;
};

// 使用示例
Timer timer;
timer.start();

// ... 待测代码 ...

timer.stop("Operation");
```

---

## 常见性能瓶颈

### 1. 频繁内存分配

**问题**: 在热路径中频繁 `new/delete` 或 `malloc/free`

```cpp
// 低效代码
for (int i = 0; i < 1000000; i++) {
    auto* data = new Data();
    process(data);
    delete data;
}

// 优化：使用对象池
class DataPool {
public:
    Data* acquire() {
        if (pool_.empty()) {
            return new Data();
        }
        Data* obj = pool_.back();
        pool_.pop_back();
        return obj;
    }
    
    void release(Data* obj) {
        pool_.push_back(obj);
    }

private:
    std::vector<Data*> pool_;
};
```

### 2. 不必要的字符串拷贝

**问题**: 频繁创建临时字符串

```cpp
// 低效代码
std::string create_message() {
    std::string msg = "Status: ";
    msg += get_status();
    msg += ", Code: ";
    msg += std::to_string(get_code());
    return msg;
}

// 优化：使用字符串视图和 reserve
std::string create_message() {
    std::string msg;
    msg.reserve(64);  // 预分配
    msg = "Status: ";
    msg += get_status();
    msg += ", Code: ";
    msg += std::to_string(get_code());
    return msg;
}
```

### 3. 锁竞争

**问题**: 多个线程竞争同一个锁

```cpp
// 低效代码 - 全局锁
std::mutex global_mutex;

void process_data() {
    std::lock_guard<std::mutex> lock(global_mutex);
    // 只有一小部分代码需要保护
    shared_data++;
    // 大量不需要保护的工作
    do_heavy_computation();
}

// 优化：缩小锁范围
void process_data() {
    {
        std::lock_guard<std::mutex> lock(global_mutex);
        shared_data++;
    }
    // 锁外执行耗时操作
    do_heavy_computation();
}
```

### 4. 缓存不友好

**问题**: 内存访问模式导致缓存未命中

```cpp
// 低效代码 - 随机访问
struct Node {
    int value;
    Node* next;
};

void traverse(Node* head) {
    Node* curr = head;
    while (curr) {
        process(curr->value);  // 缓存未命中
        curr = curr->next;
    }
}

// 优化：使用连续内存
std::vector<int> data;

void traverse() {
    for (int val : data) {  // 顺序访问，缓存友好
        process(val);
    }
}
```

---

## 优化技巧

### 1. 减少系统调用

```cpp
// 低效：多次 write 调用
for (const auto& chunk : chunks) {
    write(fd, chunk.data(), chunk.size());
}

// 优化：批量写入
std::vector<char> buffer;
for (const auto& chunk : chunks) {
    buffer.insert(buffer.end(), chunk.begin(), chunk.end());
}
write(fd, buffer.data(), buffer.size());
```

### 2. 使用移动语义

```cpp
// 低效：拷贝
std::vector<int> get_data() {
    std::vector<int> data(1000000);
    // ... 填充数据
    return data;  // 可能触发拷贝
}

// 优化：移动
std::vector<int> get_data() {
    std::vector<int> data(1000000);
    // ... 填充数据
    return data;  // C++11 自动移动
}
```

### 3. 避免虚函数开销

```cpp
// 如果不需要多态，使用 CRTP
template<typename Derived>
class DeviceBase {
public:
    void process() {
        static_cast<Derived*>(this)->process_impl();
    }
};

class MyDevice : public DeviceBase<MyDevice> {
public:
    void process_impl() {
        // 实现
    }
};
```

### 4. 使用内联

```cpp
// 小函数使用 inline 或 __forceinline__
inline int clamp(int val, int min, int max) {
    return std::max(min, std::min(val, max));
}
```

---

## GPU 性能优化

### 1. 批量命令提交

```cpp
// 低效：单个提交
for (const auto& task : tasks) {
    gpu_device->submit_task(task);  // 每次都有开销
}

// 优化：批量提交
std::vector<GpuTask> batch;
for (const auto& task : tasks) {
    batch.push_back(task);
}
gpu_device->submit_batch(batch);  // 一次提交所有
```

### 2. 异步执行

```cpp
// 同步等待（低效）
gpu_device->submit_task(task);
gpu_device->wait_for_completion();  // 阻塞

// 异步执行（高效）
gpu_device->submit_task(task);
// 继续做其他工作
// ...
gpu_device->wait_for_completion();  // 需要时再等待
```

### 3. 内存复用

```cpp
// 低效：每次分配
for (int i = 0; i < 1000; i++) {
    GpuMemoryHandle handle = gpu_device->allocate_memory(size);
    process(handle);
    gpu_device->free_memory(handle);
}

// 优化：内存池
class GpuMemoryPool {
public:
    GpuMemoryHandle allocate(size_t size) {
        // 从池中分配或创建新内存
    }
    
    void deallocate(GpuMemoryHandle handle) {
        // 回收到池中
    }
    
private:
    std::vector<GpuMemoryHandle> free_list_;
};
```

---

## 内存优化

### 1. 使用内存池

```cpp
template<typename T>
class MemoryPool {
public:
    T* allocate() {
        if (free_list_.empty()) {
            allocate_block();
        }
        T* obj = free_list_.back();
        free_list_.pop_back();
        return obj;
    }
    
    void deallocate(T* obj) {
        free_list_.push_back(obj);
    }

private:
    std::vector<T*> free_list_;
    
    void allocate_block() {
        constexpr size_t BLOCK_SIZE = 1024;
        char* block = new char[BLOCK_SIZE * sizeof(T)];
        blocks_.push_back(block);
        
        for (size_t i = 0; i < BLOCK_SIZE; i++) {
            free_list_.push_back(new (block + i * sizeof(T)) T());
        }
    }
    
    std::vector<char*> blocks_;
};
```

### 2. 避免内存碎片

```cpp
// 使用连续容器代替链表
std::vector<int> vec;  // 连续内存
std::list<int> lst;    // 分散内存，缓存不友好

// 预分配容量
vec.reserve(1000000);  // 避免多次重新分配
```

### 3. 智能指针开销

```cpp
// shared_ptr 有原子操作开销
std::shared_ptr<Data> ptr = std::make_shared<Data>();

// 如果不需要共享所有权，使用 unique_ptr
std::unique_ptr<Data> ptr = std::make_unique<Data>();

// 如果确定生命周期，使用裸指针 + 明确的所有者
Data* ptr = new Data();
// ... 使用
delete ptr;
```

---

## 并发优化

### 1. 无锁编程

```cpp
// 使用原子操作代替锁
class Counter {
public:
    void increment() {
        count_.fetch_add(1, std::memory_order_relaxed);
    }
    
    int get() const {
        return count_.load(std::memory_order_relaxed);
    }

private:
    std::atomic<int> count_{0};
};
```

### 2. 读写锁

```cpp
// 读多写少的场景
class DataStore {
public:
    int read() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return data_;
    }
    
    void write(int value) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        data_ = value;
    }

private:
    mutable std::shared_mutex mutex_;
    int data_;
};
```

### 3. 线程本地存储

```cpp
// 避免线程间共享
thread_local std::string buffer;

void process() {
    // 每个线程有自己的 buffer，无需锁
    buffer = get_data();
    process_data(buffer);
}
```

---

## 性能基准测试

### 创建 Benchmark

```cpp
// benchmarks/benchmark_memory.cpp
#include <benchmark/benchmark.h>
#include "kernel/device/gpgpu_device.h"

static void BM_MemoryAllocate(benchmark::State& state) {
    GpgpuDevice device;
    
    for (auto _ : state) {
        GpuMemoryHandle handle;
        device.allocate_memory(1024, &handle);
        device.free_memory(handle);
    }
}

BENCHMARK(BM_MemoryAllocate);

BENCHMARK_MAIN();
```

### 运行 Benchmark

```bash
# 编译 benchmark
cmake -DBUILD_BENCHMARKS=ON ..
make -j$(nproc)

# 运行所有基准测试
./bin/benchmarks

# 运行特定基准
./bin/benchmarks --benchmark_filter=Memory

# 输出 CSV
./bin/benchmarks --benchmark_out=results.csv --benchmark_out_format=csv
```

### 对比结果

```bash
# 使用 compare.py 对比
python3 tools/compare.py old_results.json new_results.json

# 输出示例：
# Benchmark                  Before     After      Change
# ----------------------------------------------------------------
# BM_MemoryAllocate          100 ns     80 ns      -20%
# BM_CommandSubmit           500 ns     450 ns     -10%
```

---

## 性能优化检查清单

### 编译优化

- [ ] 使用 Release 模式 (`-DCMAKE_BUILD_TYPE=Release`)
- [ ] 启用优化标志 (`-O2` 或 `-O3`)
- [ ] 启用 LTO (`-flto`)
- [ ] 针对 CPU 优化 (`-march=native`)

### 代码优化

- [ ] 减少内存分配
- [ ] 避免不必要的拷贝
- [ ] 使用移动语义
- [ ] 缩小锁范围
- [ ] 使用缓存友好的数据结构

### 架构优化

- [ ] 批量操作
- [ ] 异步执行
- [ ] 内存池
- [ ] 无锁数据结构

### 验证优化

- [ ] 运行基准测试
- [ ] 使用 perf 分析
- [ ] 检查回归测试
- [ ] 对比优化前后性能

---

## 相关文档

- [调试指南](../03-development/debugging.md) - 调试工具
- [GPU 驱动架构](gpu-driver-architecture.md) - GPU 驱动设计
- [CI/CD 配置](../04-building/ci-cd.md) - 持续集成

---

**最后更新**: 2026-03-24
