# 构建和测试

这里包含构建系统、测试和 CI/CD 相关的文档。

## 导航

- [构建系统](build-system.md) - CMake 配置和编译选项
- [测试指南](testing-guide.md) - 编写和运行测试
- [CI/CD](ci-cd.md) - 持续集成和部署（待添加）

## 快速导航

| 我想... | 阅读这个 |
|---------|----------|
| 编译项目 | [构建系统](build-system.md) |
| 运行测试 | [测试指南](testing-guide.md) |
| 了解 CI 流程 | [CI/CD](ci-cd.md) |
| 配置构建选项 | [构建系统](build-system.md#构建选项) |
| 启用代码覆盖率 | [测试指南](testing-guide.md#代码覆盖率) |

## 常用命令

```bash
# 基本构建
mkdir build && cd build
cmake ..
make -j$(nproc)

# 运行测试
make test

# 启用覆盖率
cmake .. -DCOVERAGE=ON
```

---

**最后更新**: 2026-03-23
