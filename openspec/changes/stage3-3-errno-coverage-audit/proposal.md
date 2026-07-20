# Change: stage3-3-errno-coverage-audit

> **状态**: 🔄 IN-PROGRESS（Reactivated 2026-07-21，承接 archived 提案）
> **优先级**: 🟢 P2
> **创建**: 2026-07-08（原提案）；2026-07-21（re-create from archive）
> **来源**: Issue #24 §3.3（Stage 3.3 error handling completeness）
> **工作目录**: `openspec/changes/stage3-3-errno-coverage-audit/`

## Why

Stage 3.3 要求所有路径返回 Linux 风格错误码。已验证主 `gpgpu_device.cpp` handler 全部使用正确 errno（`-EINVAL`/`-EFAULT`/`-ENOMEM`），但 **`sim/` 层仍有 12 处 `return -1`**：

| 文件 | `return -1` 数量 |
|------|-----------------|
| `sim/graph.cpp` | 5 |
| `sim/stream_capture.cpp` | 4 |
| `sim/gpu_queue_emu.cpp` | 2 |
| `sim/fence_id.cpp` | 1 |

这些 handler 对应 20 个 IOCTL 命令（0x50-0x68），错误路径未正确返回 Linux errno。

## What Changes

### 1. 审计 sim/ 层 12 处 `return -1`

每个替换为标准 Linux errno：
- 参数无效 → `-EINVAL`
- 资源耗尽 / 空指针 → `-ENOMEM` / `-EFAULT`
- 不支持操作 → `-ENOSYS`
- 内部错误 → `-EIO`

### 2. 加测试 case

每个 error path 一个 Catch2 test case。

## 涉及 Handler

| IOCTL # | Handler 类型 | 位置 |
|---------|-------------|------|
| 0x50-0x59 | 10 stream+graph | sim/{stream_capture,graph}.cpp |
| 0x60-0x67 | 8 mempool | sim/mem_pool.cpp |
| 0x68 | 1 mempool_export | sim/mem_pool.cpp |

## Acceptance

- [ ] 12 处 `return -1` 全部替换为标准 Linux errno
- [ ] 新增测试覆盖关键 error path（≥ 5 个 Catch2 TEST_CASE）
- [ ] ctest 104/104 PASS（无 regression）
- [ ] `grep -r "return -1" plugins/gpu_driver/sim/` 返回 0 匹配
- [ ] docs-audit 无新 warning

## 测试方法

```bash
cd build && ctest --output-on-failure
grep -r "return -1" plugins/gpu_driver/sim/   # 应无输出
```

## Cross-Repo 影响

无（仅 UsrLinuxEmu sim/ 层内部修复）。

## Dependencies

无。C-02（stage3-ioctl-dispatch-completeness）已合入。
