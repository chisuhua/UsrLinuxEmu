# Quickstart Verification Report

> **Stage 3.4**: 端到端验证 — 从零到第一个 GPU 示例

## 基准环境

- OS: Linux x86_64 (kernel 5.10)
- CPU: 4 cores (Intel Xeon Platinum)
- RAM: 15Gi
- CMake: cmake version 3.28.3
- GCC: gcc 13.3.0

## 测量结果 (Release Build)

| Phase | Time |
|-------|------|
| cmake configure | ~1.2s |
| make -j4 test_gpu_ioctl_standalone | ~9.6s |
| test run (standalone) | ~1.3s |
| **Total** | **~12s** |

## 结论

Release 构建下完整 quickstart 路径约 12 秒，远低于 15 分钟目标。
即使 Debug 构建或低配机器上也应能在 2-3 分钟内完成。

### quickstart 路径
1. `git clone https://github.com/chisuhua/UsrLinuxEmu` → `cd UsrLinuxEmu`
2. `mkdir build && cd build`
3. `cmake -DCMAKE_BUILD_TYPE=Release ..` → ~1s
4. `make -j4 test_gpu_ioctl_standalone` → ~10s
5. `cd .. && ./build/bin/test_gpu_ioctl_standalone` → ~1s

> ⚠️ 测试需从项目根目录运行（插件路径为相对路径）。
