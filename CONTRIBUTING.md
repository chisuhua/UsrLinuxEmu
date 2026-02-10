# 贡献指南

感谢您对 UsrLinuxEmu 项目的关注！我们欢迎各种形式的贡献，无论是报告 bug、提出建议、改进文档还是提交代码。

## 目录

- [行为准则](#行为准则)
- [如何贡献](#如何贡献)
  - [报告 Bug](#报告-bug)
  - [提出功能建议](#提出功能建议)
  - [提交代码](#提交代码)
  - [改进文档](#改进文档)
- [开发环境设置](#开发环境设置)
- [代码规范](#代码规范)
- [提交规范](#提交规范)
- [Pull Request 流程](#pull-request-流程)
- [测试要求](#测试要求)
- [社区](#社区)

## 行为准则

本项目遵循开源社区的基本行为准则：

- 尊重所有贡献者
- 友好和包容的态度
- 专注于对项目最有利的事情
- 以建设性的方式提供和接受反馈

## 如何贡献

### 报告 Bug

如果您发现了 bug，请通过 [GitHub Issues](https://github.com/chisuhua/UsrLinuxEmu/issues) 报告。提交 bug 报告时，请包含：

**Bug 报告模板**：

```markdown
**描述**
简要描述 bug 是什么。

**复现步骤**
1. 执行 '...'
2. 点击 '...'
3. 看到错误

**预期行为**
描述您期望发生什么。

**实际行为**
描述实际发生了什么。

**环境信息**
- OS: [例如 Ubuntu 20.04]
- 编译器: [例如 GCC 9.3]
- 项目版本: [例如 v0.1.0]

**附加信息**
添加任何其他有助于诊断问题的信息，如截图、日志等。
```

### 提出功能建议

我们欢迎新功能建议！请通过 [GitHub Issues](https://github.com/chisuhua/UsrLinuxEmu/issues) 提交，使用 `enhancement` 标签。

**功能建议模板**：

```markdown
**功能描述**
简要描述您希望添加的功能。

**问题/需求**
这个功能要解决什么问题？或满足什么需求？

**建议的解决方案**
描述您认为应该如何实现这个功能。

**替代方案**
描述您考虑过的其他解决方案。

**附加信息**
添加任何其他相关信息，如示例代码、参考链接等。
```

### 提交代码

#### 首次贡献

1. **Fork 仓库**
   - 访问 [UsrLinuxEmu GitHub](https://github.com/chisuhua/UsrLinuxEmu)
   - 点击右上角的 "Fork" 按钮

2. **克隆到本地**
   ```bash
   git clone https://github.com/YOUR_USERNAME/UsrLinuxEmu.git
   cd UsrLinuxEmu
   ```

3. **添加上游仓库**
   ```bash
   git remote add upstream https://github.com/chisuhua/UsrLinuxEmu.git
   ```

4. **创建功能分支**
   ```bash
   git checkout -b feature/your-feature-name
   ```

5. **进行开发**
   - 编写代码
   - 添加测试
   - 更新文档

6. **提交更改**
   ```bash
   git add .
   git commit -m "feat: add your feature"
   ```

7. **推送到 Fork 仓库**
   ```bash
   git push origin feature/your-feature-name
   ```

8. **创建 Pull Request**
   - 访问您的 Fork 仓库页面
   - 点击 "New Pull Request"
   - 填写 PR 描述

### 改进文档

文档改进同样重要！您可以：

- 修正拼写或语法错误
- 改进现有文档的清晰度
- 添加缺失的文档
- 添加示例和教程
- 翻译文档

文档位于 `docs/` 目录，使用 Markdown 格式。

## 开发环境设置

### 系统要求

- Linux 环境（推荐 Ubuntu 18.04+）
- CMake ≥ 3.14
- GCC ≥ 7.0 或 Clang ≥ 5.0（支持 C++17）
- Git

### 安装依赖

```bash
# Ubuntu/Debian
sudo apt update
sudo apt install build-essential cmake git

# 可选：安装 Google Test
sudo apt install libgtest-dev

# 可选：安装代码格式化工具
sudo apt install clang-format

# 可选：安装静态分析工具
sudo apt install clang-tidy
```

### 构建项目

```bash
# 克隆仓库
git clone https://github.com/chisuhua/UsrLinuxEmu.git
cd UsrLinuxEmu

# 构建
./build.sh

# 或手动构建
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### 运行测试

```bash
cd build
make test

# 或运行特定测试
./bin/test_gpu_submit
```

### 代码格式化

```bash
# 格式化所有代码
find src include -name "*.cpp" -o -name "*.h" | xargs clang-format -i

# 或使用项目提供的脚本（如果有）
./scripts/format.sh
```

## 代码规范

### 命名规范

- **类名**: PascalCase (例如 `GpgpuDevice`)
- **函数名**: snake_case (例如 `allocate_memory`)
- **变量名**: snake_case (例如 `buffer_size`)
- **常量**: UPPER_SNAKE_CASE (例如 `MAX_BUFFER_SIZE`)
- **成员变量**: snake_case + 下划线后缀 (例如 `buffer_size_`)
- **宏**: UPPER_SNAKE_CASE (例如 `GPGPU_ALLOC_MEM`)

### 代码风格

遵循 Google C++ Style Guide 的主要原则：

```cpp
// 头文件保护
#pragma once

// 或者使用传统的保护宏
#ifndef PROJECT_MODULE_FILE_H
#define PROJECT_MODULE_FILE_H
// ...
#endif

// 命名空间
namespace usr_linux_emu {

// 类定义
class Device {
public:
    Device();
    virtual ~Device() = default;
    
    // 公共方法
    virtual int open(int flags) = 0;
    
private:
    // 私有成员
    int fd_;
    std::string name_;
};

}  // namespace usr_linux_emu
```

### 注释规范

```cpp
/**
 * @brief 简要描述函数功能
 * 
 * 详细描述函数的作用、算法等。
 * 
 * @param size 分配的大小（字节）
 * @param flags 分配标志
 * @return 成功返回地址，失败返回 nullptr
 */
void* allocate_memory(size_t size, int flags);

// 单行注释使用 //
// 解释为什么这样做，而不是做了什么
```

### 错误处理

```cpp
// 使用返回值表示成功/失败
int allocate(size_t size, uint64_t* addr) {
    if (addr == nullptr) {
        return -EINVAL;
    }
    
    // 执行分配...
    if (failed) {
        return -ENOMEM;
    }
    
    *addr = allocated_addr;
    return 0;  // 成功
}
```

### 资源管理

优先使用 RAII 和智能指针：

```cpp
// 使用 unique_ptr
auto device = std::make_unique<GpgpuDevice>();

// 使用 shared_ptr
auto device = std::make_shared<GpgpuDevice>();

// 避免裸指针和手动 delete
Device* device = new Device();  // ❌ 不推荐
delete device;
```

## 提交规范

使用 [Conventional Commits](https://www.conventionalcommits.org/) 规范：

### 提交消息格式

```
<type>(<scope>): <subject>

<body>

<footer>
```

### Type 类型

- `feat`: 新功能
- `fix`: Bug 修复
- `docs`: 文档更新
- `style`: 代码格式（不影响代码运行）
- `refactor`: 重构（既不是新功能也不是 bug 修复）
- `perf`: 性能优化
- `test`: 测试相关
- `build`: 构建系统或依赖更新
- `ci`: CI 配置更新
- `chore`: 其他不修改源代码的更改

### 示例

```bash
# 新功能
git commit -m "feat(gpu): add memory pool support"

# Bug 修复
git commit -m "fix(vfs): fix device lookup race condition"

# 文档更新
git commit -m "docs(api): update device API documentation"

# 重构
git commit -m "refactor(allocator): simplify buddy allocator logic"
```

## Pull Request 流程

### 提交前检查

在提交 PR 之前，请确保：

- [ ] 代码符合项目规范
- [ ] 所有测试通过
- [ ] 添加了必要的测试
- [ ] 更新了相关文档
- [ ] 提交消息符合规范
- [ ] 代码已格式化

### PR 描述模板

```markdown
## 变更描述
简要描述这个 PR 做了什么。

## 相关 Issue
Fixes #123
Related to #456

## 变更类型
- [ ] Bug 修复
- [ ] 新功能
- [ ] 重构
- [ ] 文档更新
- [ ] 性能优化
- [ ] 测试相关

## 测试
描述如何测试这些变更。

## 检查清单
- [ ] 代码遵循项目规范
- [ ] 所有测试通过
- [ ] 添加了新的测试
- [ ] 更新了文档
- [ ] 提交消息符合规范

## 截图（如果适用）
如果是 UI 变更，请添加截图。

## 附加信息
任何其他相关信息。
```

### Review 流程

1. 提交 PR 后，维护者会进行 review
2. 根据反馈进行修改
3. 所有讨论解决后，PR 会被合并
4. 合并后，功能分支会被删除

## 测试要求

### 单元测试

每个新功能都应该有对应的单元测试：

```cpp
#include <gtest/gtest.h>
#include "buddy_allocator.h"

class BuddyAllocatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        allocator_ = std::make_unique<BuddyAllocator>(1024 * 1024);
    }
    
    std::unique_ptr<BuddyAllocator> allocator_;
};

TEST_F(BuddyAllocatorTest, AllocateSmallBlock) {
    uint64_t addr;
    ASSERT_EQ(allocator_->allocate(256, &addr), 0);
    ASSERT_NE(addr, 0);
}

TEST_F(BuddyAllocatorTest, AllocateFreeBlock) {
    uint64_t addr;
    ASSERT_EQ(allocator_->allocate(256, &addr), 0);
    ASSERT_EQ(allocator_->free(addr), 0);
}
```

### 测试覆盖率

- 目标覆盖率：≥ 80%
- 关键路径必须有测试覆盖
- Bug 修复必须包含回归测试

### 运行测试

```bash
# 运行所有测试
make test

# 运行特定测试
./bin/test_buddy_allocator

# 生成覆盖率报告（需要构建时启用覆盖率）
cmake .. -DCMAKE_BUILD_TYPE=Coverage
make coverage
```

## 开发工作流

### 典型的开发流程

```bash
# 1. 同步上游代码
git checkout main
git pull upstream main

# 2. 创建功能分支
git checkout -b feature/my-feature

# 3. 开发和测试
# ... 编写代码 ...
make test

# 4. 提交更改
git add .
git commit -m "feat: add my feature"

# 5. 推送到 fork
git push origin feature/my-feature

# 6. 创建 PR
# 访问 GitHub 页面创建 PR

# 7. 根据 review 反馈修改
# ... 修改代码 ...
git add .
git commit -m "fix: address review comments"
git push origin feature/my-feature

# 8. PR 合并后清理
git checkout main
git pull upstream main
git branch -d feature/my-feature
```

## 社区

### 获取帮助

- 📖 阅读 [文档](docs/)
- 🐛 提交 [Issue](https://github.com/chisuhua/UsrLinuxEmu/issues)
- 💬 参与 [Discussions](https://github.com/chisuhua/UsrLinuxEmu/discussions)
- 📧 发送邮件到项目维护者

### 交流渠道

- GitHub Issues: 报告 bug 和提出建议
- GitHub Discussions: 一般性讨论和问答
- GitHub Pull Requests: 代码 review 和讨论

## 致谢

感谢所有贡献者的付出！您的贡献让项目变得更好。

贡献者列表会在项目中单独维护。

---

**欢迎加入 UsrLinuxEmu 社区！**

如有任何问题，请随时通过 Issue 或 Discussion 与我们联系。

---

**最后更新**: 2026-02-10  
**维护者**: UsrLinuxEmu Team
