# Change: stage3-3-error-injection-tests

> **状态**: 📋 PROPOSED
> **优先级**: 🟢 P2
> **创建**: 2026-07-08
> **来源**: Issue #24 §3.3
> **依赖**: C-05 stage3-3-errno-coverage-audit
> **工作目录**: `openspec/changes/2026-07-08-stage3-3-error-injection-tests/`

## Why

Stage 3.3 要求：critical path error injection 测试覆盖 ≥ 80%。

当前测试都是 happy-path + 参数 validation 错误。缺少**真实模拟失败注入**：
- mmap 失败模拟
- GPU 内存分配失败
- 队列创建超时
- 文件描述符耗尽
- 等等

## What Changes

### 1. 新建错误注入测试框架

`tests/error_inject/`：
- `error_inject_main.cpp` — 框架入口
- 各种 `error_inject_*.cpp` — 具体场景

### 2. 关键路径覆盖

至少覆盖：
- BO alloc 失败
- VA Space 创建失败
- Queue submit 失败
- Fence wait 超时
- Device reset / hang

### 3. failure mode 注入

通过环境变量 / mock 实现可控失败：

```cpp
TEST_CASE("BO alloc fails - EINJECT_NOMEM_BUDDY=1") {
    setenv("EINJECT_NOMEM_BUDDY", "1", 1);
    auto dev = VFS::instance().open("/dev/gpgpu0", O_RDWR);
    gpu_alloc_bo_args args = {...};
    int ret = dev->fops->ioctl(dev->fd, GPU_IOCTL_ALLOC_BO, &args);
    REQUIRE(ret == -ENOMEM);
}
```

## Acceptance

- [ ] 错误注入测试框架可用
- [ ] 关键路径 ≥ 80% error injection 覆盖
- [ ] 测试集 ≥ 20 cases
- [ ] ctest 全绿
- [ ] 测试 CI 可重现（环境变量模式）

## 测试方法

```bash
cd build
ctest -R "error_inject|inj_"   # 新测试集
ctest                          # 全量
```

## Cross-Repo 影响

无。纯 UsrLinuxEmu 测试基础设施。

## Dependencies

- **C-05** stage3-3-errno-coverage-audit（先验证 errno 路径正确）
