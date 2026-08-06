# complete-hal-mem-map-bo

## Why

ADR-064 §Decision 2 + ADR-069 §Decision 4 规定 BAR2 VRAM mmap 路径，对应一个 HAL fn-ptr：

```c
int (*mem_map_bo)(struct gpgpu_device *dev, uint64_t bo_offset,
                  size_t size, void **user_map);
```

**当前状态**：

| 实现 | 行为 | file:line |
|------|------|-----------|
| `user_mem_map_bo` | **返回 -ENOSYS**（所有参数 (void) cast 忽略）| `hal_user.cpp:245-249` |
| `mock_mem_map_bo` | 真实映射：通过 `usr_linux_emu::g_vram_store` 返回 host pointer | `hal_mock.cpp:257-266` |

mock 实现完整：

```cpp
static int mock_mem_map_bo(struct gpgpu_device* dev, uint64_t bo_offset,
                           size_t size, void** user_map) {
    (void)dev;
    (void)size;
    auto* store = &usr_linux_emu::g_vram_store;
    if (!store->initialized) return -ENODEV;
    *user_map = static_cast<uint8_t*>(store->pool_backing) + bo_offset;
    return 0;
}
```

**关键含义**：mock 已验证 VRAM store 可用，user 实施是"故意 stub"（有 sim 模块可复用但未 wire）。

## What Changes

**In Scope**:

- `plugins/gpu_driver/hal/hal_user.cpp:245-249` — `user_mem_map_bo` 从 stub 升级为真实映射（参照 mock）
- 复用 `usr_linux_emu::g_vram_store` 全局实例
- 完整 ctest PASS（基线 130/130）
- 新增 BAR2 mmap 单元测试

### 关键场景

- GIVEN drv/ 调用 `hal_mem_map_bo(dev, bo_offset, size, &user_map)`
  - WHEN **修复前** THEN 返回 -ENOSYS，drv 调用失败
  - WHEN **修复后** THEN 返回 `g_vram_store.pool_backing + bo_offset`，drv 可读/写该 VRAM 区域
- GIVEN `g_vram_store.initialized == false` WHEN drv/ 调用 THEN 返回 `-ENODEV`
- GIVEN `bo_offset + size > VRAM 总大小` WHEN drv/ 调用 THEN 返回 `-EINVAL`（需加 bounds check）

**Out of Scope**:

- `g_vram_store` 自身的完整化（如缺失部分，独立 task）
- DRM ioctl mmap 路径
- Multi-process BAR 隔离（ADR-011 推迟）

## Capabilities

- MUST 复用 `usr_linux_emu::g_vram_store`（与 mock 一致）
- MUST 添加 bounds check（`bo_offset + size <= pool_size`）
- MUST NOT 引入新分配机制（无 mmap，无 malloc）
- MUST NOT 修改 mock 实现
- MUST NOT 修改 `struct gpu_hal_ops` 签名
- SHOULD 复用 mock 实现的错误处理模式（`-ENODEV` for uninitialized）
- SHOULD 添加线程安全考虑（如 `g_vram_store` 是全局单例）

## Impact

- MUST 复用 `usr_linux_emu::g_vram_store`（与 mock 一致）
- MUST 添加 bounds check（`bo_offset + size <= pool_size`）
- MUST NOT 引入新分配机制（无 mmap，无 malloc）
- MUST NOT 修改 mock 实现
- MUST NOT 修改 `struct gpu_hal_ops` 签名
- SHOULD 复用 mock 实现的错误处理模式（`-ENODEV` for uninitialized）
- SHOULD 添加线程安全考虑（如 `g_vram_store` 是全局单例）

## Acceptance

- `user_mem_map_bo` 真实化（不再返回 -ENOSYS）
- 添加 bounds check（offset + size 越界返回 -EINVAL）
- 新增单元测试 `test_mem_map_bo_basic`：
  - 成功路径：valid offset/size 返回 host pointer
  - 错误路径：uninitialized store → -ENODEV
  - 错误路径：out-of-bounds → -EINVAL
- `make -j4` 编译通过
- `ctest --output-on-failure` 全部 PASS
- 端到端：drv/ 调用 mmap → 用户可访问 VRAM 区域
- Sanitizer run PASS
- `lsp_diagnostics` 无 error

