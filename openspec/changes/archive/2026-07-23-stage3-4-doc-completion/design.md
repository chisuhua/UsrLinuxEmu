## Context

UsrLinuxEmu v1.0 即将发布，Stage 3.4 是最后一个未完成的子任务。当前状态：
- docs-audit 43/43 PASS ✅（结构完整性已验证）
- ADR-064 内存模型 + `gpu-real-memory-path.md` 已创建
- Doxygen API 参考**未生成** — 缺乏自动化的公共 API 文档
- 用户 quickstart 路径**未验证** — 不清楚新用户能否在 15 分钟内跑通

本项目为纯文档变更，无代码修改。

## Goals / Non-Goals

**Goals:**
- 生成 Doxygen HTML API 参考文档，覆盖公共 API 头文件
- 创建 API 参考索引页（`docs/06-reference/doxygen-api-index.md`）
- 端到端验证 quickstart 路径：从 `git clone` 到第一个 GPU 示例 ≤15 分钟
- 确保 v1.0 文档集结构完整

**Non-Goals:**
- 不修改任何源代码或头文件
- 不生成 man pages 或 PDF（仅 HTML）
- 不新增示例程序（使用现有 tests/ 中示例）
- 不修改构建系统

## Decisions

| 决策 | 选择 | 理由 |
|------|------|------|
| Doxygen 配置位置 | `docs/Doxyfile` | 遵循项目文档目录约定 |
| Doxygen 覆盖范围 | `include/kernel/`, `include/linux_compat/`, `plugins/gpu_driver/shared/` | 公共 API 头文件，外部 consumer 最常用 |
| 索引页格式 | Markdown（链接到 HTML） | 与现有 `docs/06-reference/` 风格一致 |
| quickstart 验证方法 | 从零开始计时完整流程 | 模拟真实新用户体验 |

## Risks / Trade-offs

- **Doxygen 输出体积**: HTML 目录可能很大 → 将 `docs/api/html/` 加入 `.gitignore`，仅提交 Doxyfile + 索引页
- **quickstart 时间受环境影响**: 网络速度、CMake 缓存等 → 记录基准环境条件，多次测量取中位数