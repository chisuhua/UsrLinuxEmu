# Change: stage3-1-ci-matrix-multi-platform

> **状态**: 📋 PROPOSED
> **优先级**: 🟢 P2
> **创建**: 2026-07-08
> **来源**: Issue #24 §3.1（Stage 3.1 v1.0 stability）
> **依赖**: 无
> **工作目录**: `openspec/changes/2026-07-08-stage3-1-ci-matrix-multi-platform/`

## Why

CI 当前只测 `ubuntu-latest`（3 个 compiler config）。Stage 3 v1.0 stability 要求 CI 全平台覆盖：

- Linux x86_64 (Ubuntu 20.04+, Debian 11+)
- Linux aarch64 (ARM servers，可选)
- macOS x86_64 + Apple Silicon（仅 docs/Debug 验证）

**Gap**：
- 没有 LTS 锁定（LTS = Long-Term Support，ubuntu-22.04 当前是 LTS）
- 没法 macOS 用户验证
- 跨 distro 测试能力缺失

## What Changes

### 1. 加 ubuntu-22.04 LTS

`.github/workflows/cmake-multi-platform.yml` matrix：
```yaml
matrix:
  os: [ubuntu-latest, ubuntu-22.04]
  build_type: [Release]
  c_compiler: [gcc, clang]
  cpp_compiler: [g++, clang++]
```

### 2. (可选) macOS-latest

需要验证：
- `linux_compat/` ABI 兼容 macOS BSD syscalls
- `dlopen` / `dlsym` 行为跨平台

### 3. (可选) Linux aarch64

需要 cross-compile 或 QEMU emulation runner。

## Acceptance

- [ ] CI matrix 含 ubuntu-22.04 LTS
- [ ] ubuntu-22.04 build green
- [ ] 现有 3 configs (ubuntu-latest) 0 regression
- [ ] (如可行) macOS build green
- [ ] workflow 时间 < 1 hour (matrix fan-out 可控)

## 验证方法

```bash
# 触发 PR 后观察 CI runs
gh pr checks <N>
gh run list --workflow=CI --limit 10
```

## Cross-Repo 影响

无。纯 CI 配置变更。

## Dependencies

无（独立）
