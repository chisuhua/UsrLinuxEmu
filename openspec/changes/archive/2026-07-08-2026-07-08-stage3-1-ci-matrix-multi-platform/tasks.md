# Tasks: stage3-1-ci-matrix-multi-platform

> **状态**: 📋 PROPOSED
> **目标**: CI matrix 扩展到 LTS + 可选 macOS

## 1. 准备（15 分钟）

- [ ] 1.1 读 `.github/workflows/cmake-multi-platform.yml`
- [ ] 1.2 检查当前 matrix 结构
- [ ] 1.3 评估 ubuntu-22.04 runner 可用性
- [ ] 1.4 评估 macOS-latest ABI 兼容性（linx_compat 是否兼容）

## 2. ubuntu-22.04 LTS 添加（30 分钟）

- [ ] 2.1 matrix.os 加 `ubuntu-22.04`
- [ ] 2.2 排除不兼容 config（如 clang+clang++）
- [ ] 2.3 在 PR 中观察 6 configs build/test 全绿
- [ ] 2.4 时间检查（每个 job < 20 min）

## 3. (可选) macOS 添加（半天）

- [ ] 3.1 加 `macos-latest` 到 matrix
- [ ] 3.2 修复 ABI 不兼容（如 pthread vs BSD threads）
- [ ] 3.3 修复 `linux_compat/` macOS 编译错误
- [ ] 3.4 macOS build green

## 4. (可选) aarch64 添加（1 周）

- [ ] 4.1 加 cross-compile setup
- [ ] 4.2 加 QEMU emulation runner（如不用 cross-compile）
- [ ] 4.3 aarch64 build/test green

## 5. 验证（10 分钟）

- [ ] 5.1 所有 platform build 0 warnings
- [ ] 5.2 所有 platform tests 0 failures
- [ ] 5.3 总 CI 时间 < 1h（matrix 并行）
- [ ] 5.4 commit：`ci(matrix): add ubuntu-22.04 LTS (+optional macOS/aarch64)`
