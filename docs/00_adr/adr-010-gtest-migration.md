# ADR-010: 测试框架选型 — Catch2（最终采用）vs GTest

**状态**: ✅ 已接受

**日期**: 2026-02（提议） / 2026-06-16（确认 Catch2 选型）

## 背景

UsrLinuxEmu 项目在 2026-02 提出了 ADR-010 提议"迁移到 GTest"。但在 2026-04 ~ 2026-06 期间，**实际项目采用了 Catch2**（vendored 单文件 `tests/catch_amalgamated.{hpp,cpp}`），并未按本 ADR 原始方案迁移到 GTest。

本文档记录**最终选型结果**：Catch2，并解释为何最终未实施原 GTest 迁移方案。

## 最终决策

**采用 Catch2 v2.x**（vendored 单文件 amalgamation）。

## 理由

1. **零依赖**：Catch2 的 amalgamation 模式（`catch_amalgamated.hpp` + `catch_amalgamated.cpp`）无需任何外部依赖。
   - **GTest** 需要 `apt install libgtest-dev`、CMake `find_package(GTest)`、处理 ABI 不兼容问题。
   - **Catch2** 只需 `#include "catch_amalgamated.hpp"` 即可。
2. **无 ABI 兼容性问题**：Catch2 是 header-only 风格（amalgamation），编译进测试可执行文件后没有动态链接问题。GTest 在不同 Linux 发行版上 ABI 不一致（`libgtest.so` vs 静态链接）。
3. **CI 友好**：测试不依赖系统包管理器，CI 环境（容器、跨平台）一致性更好。
4. **断言语义更丰富**：`REQUIRE(expr)` vs GTest `ASSERT_TRUE`/`EXPECT_TRUE` — Catch2 的 `REQUIRE` 强制求值并使用表达式的字符串化形式，错误信息更易读。
5. **SECTION 嵌套**：Catch2 的 `SECTION("name")` 支持测试用例内的嵌套子例，比 GTest 的 `TEST_F` 固件更轻量。
6. **生态**：UsrLinuxEmu 的 `tests/test_*.cpp` 现存 30+ 个测试文件全部使用 Catch2 语法（`TEST_CASE` + `REQUIRE`），重写到 GTest 的成本高、收益低。

## GTest 方案被放弃的原因

1. **依赖管理成本**：需要在每个 CI 环境、开发机、容器中显式安装 `libgtest-dev`，不同系统版本（Ubuntu 18.04 / 20.04 / 22.04）的 GTest ABI 不兼容。
2. **现有测试已 Catch2 化**：30+ 测试文件已用 Catch2，重写需 ~500 行测试代码改动。
3. **单文件部署优势**：Catch2 amalgamation 可直接复制到任何项目，零外部依赖。

## 实施状态

| 组件 | 状态 |
|------|------|
| `tests/catch_amalgamated.{hpp,cpp}` | ✅ Vendored |
| `tests/CMakeLists.txt` | ✅ 使用 Catch2（无 `find_package(GTest)`）|
| `tests/test_*.cpp` (30+ 个) | ✅ 使用 `TEST_CASE` / `REQUIRE` / `SECTION` 语法 |
| CI/CD | ✅ 无 `apt install libgtest-dev`（Catch2 自带）|

## 后果

- ✅ 零外部依赖
- ✅ 跨平台一致（Linux/macOS/WSL/CI 容器）
- ✅ 测试代码 30+ 文件无需重写
- ⚠️ Catch2 社区比 GTest 小，部分 IDE 集成稍弱（但 VSCode/CLion 支持良好）

## 相关文档

- `docs/02_architecture/post-refactor-architecture.md` §1.7（测试框架：声称 vs 实际）
- `tests/catch_amalgamated.hpp`（vendored Catch2 v2.x）
- `tests/CMakeLists.txt`（实际测试构建配置）

---

**维护者**: UsrLinuxEmu Architecture Team
**最后更新**: 2026-06-16
**对应代码 commit**: `374d463`