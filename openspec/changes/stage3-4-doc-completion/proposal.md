## Why

UsrLinuxEmu Stage 3 (v1.0 稳定) 已接近完成：CUDA E2E ✅、sanitizer ✅、bridge ✅、errno 审计 ✅、perf ✅。但 Stage 3.4 文档完善仍有未完成项：Doxygen API 参考未生成，用户 quickstart 路径尚未验证 ≤15 分钟可达。v1.0 发布前必须完成文档交付。

## What Changes

- **新增 Doxygen 配置**：为 `include/kernel/`、`include/linux_compat/`、`plugins/gpu_driver/shared/` 等公共 API 生成结构化 API 参考文档
- **新增 API 参考索引**：`docs/06-reference/doxygen-api-index.md` — 从 Doxygen 生成的导航索引
- **完善 quickstart 路径**：验证并优化从零到第一个 GPU 示例的时间 ≤15 分钟
- **文档结构收尾**：确保 v1.0 文档集完整（SSOT + ROADMAP + ADR + API 参考 + 用户指南）
- **docs-audit 持续通过**：保持 43/43 PASS

## Capabilities

### New Capabilities
- `doxygen-api-reference`: Doxygen 配置与 API 参考文档自动生成，覆盖公共 API 头文件
- `quickstart-verification`: 端到端验证用户 quickstart 路径，确保 ≤15 分钟可达

### Modified Capabilities
<!-- 无现有 spec 修改 -->

## Impact

- **新增文件**: `Doxyfile`（或 `docs/Doxyfile`），`docs/06-reference/doxygen-api-index.md`
- **影响文件**: `docs/01-quickstart/` 下各文件可能需要优化以缩短上手时间
- **无代码变更**: 纯文档工作，对构建/测试无影响
- **无 API 变更**: 不修改任何头文件或接口