## Why

Stage 3.3 (errno coverage audit) 已完成归档，但 Stage 3.4 文档完善尚未开始。roadmap.md 验收清单中"用户 quickstart ≤ 15 分钟"和 v1.0 发布均依赖文档就绪。当前 Doxygen API 参考不存在，用户指南需完善以达成 Stage 3 完成条件。

## What Changes

- Doxygen 配置（Doxyfile）集成到 CMake 构建系统
- API 参考文档自动生成（`docs/api/`）
- `docs/01-quickstart/` 用户指南完善（目标: 首次示例 ≤ 15 分钟）
- docs-audit 规则扩展以覆盖 Doxygen 输出
- plan-handoff.json 更新反映 Stage 3.4 进度

## Capabilities

### New Capabilities
- `doxygen-api-ref`: Doxygen 配置与 CMake 集成，自动生成 C/C++ API 参考文档
- `quickstart-user-guide`: 用户入门指南完善，确保首次示例可在 15 分钟内完成

### Modified Capabilities
<!-- No existing specs are modified — this change introduces new capabilities only -->

## Impact

- **构建系统**: CMakeLists.txt 新增 `doxygen` 目标（可选，不影响现有构建）
- **文档**: 新增 `docs/api/` 目录（generated, .gitignore）；`docs/01-quickstart/` 内容更新
- **CI**: docs-audit job 新增 Doxygen 生成验证
- **依赖**: Doxygen 工具（构建时可选依赖，缺失时跳过）