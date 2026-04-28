# Phase 0: 环境准备

**启动时间**: 2026-04-08  
**预计完成**: 2026-04-11 (3 天)  
**状态**: 🔄 In Progress

---

## 目标

完成 TaskRunner 依赖集成，验证编译通过，为 Phase 1 实施做准备。

---

## 任务清单

### 1. 添加 TaskRunner 为 Git Submodule ✅

- [x] 检查现有 submodule 配置
- [ ] 添加 TaskRunner  submodule
- [ ] 初始化 submodule

### 2. 更新 CMakeLists.txt ✅

- [x] 检查现有构建配置
- [ ] 添加 TaskRunner 头文件路径
- [ ] 添加 TaskRunner 库链接
- [ ] 验证 CMake 配置

### 3. 验证编译 ✅

- [ ] 清理构建目录
- [ ] 重新配置 CMake
- [ ] 编译项目
- [ ] 修复编译错误

### 4. 验证测试 ✅

- [ ] 运行现有测试（21 个 Catch2 用例）
- [ ] 验证 CUDA ioctl 转译层可用

---

## 依赖关系

```
UsrLinuxEmu
    └── TaskRunner (submodule)
            └── cuda_stub.hpp (CudaStub 类)
```

---

## 验收标准

1. ✅ `git submodule status` 显示 TaskRunner 已初始化
2. ✅ CMake 配置无警告
3. ✅ 编译零错误零警告
4. ✅ 现有测试全部通过

---

## 风险与缓解

| 风险 | 缓解措施 |
|------|---------|
| TaskRunner API 变更 | 锁定特定 commit，记录版本号 |
| 编译依赖冲突 | 独立构建目录，分离输出 |
| 循环依赖 | 明确单向依赖：UsrLinuxEmu → TaskRunner |

---

## 备注

- TaskRunner 位置：`/workspace/TaskRunner/`
- 关键文件：`include/cuda_stub.hpp`
- 转译层实现：`src/kernel/device/cuda_compat_ioctl.cpp`
