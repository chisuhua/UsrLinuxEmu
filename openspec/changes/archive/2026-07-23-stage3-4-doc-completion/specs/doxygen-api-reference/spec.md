# doxygen-api-reference

## ADDED Requirements

### Requirement: Doxygen Configuration

Doxygen 配置文件（`docs/Doxyfile`）MUST 存在且可运行，覆盖以下公共 API 目录。

#### Scenario: Doxygen generates HTML output

- **GIVEN** `docs/Doxyfile` 存在且配置正确
- **WHEN** 运行 `doxygen docs/Doxyfile`
- **THEN** HTML 文档生成到 `docs/api/html/index.html`
- **AND** 覆盖 `include/kernel/`、`include/linux_compat/`、`plugins/gpu_driver/shared/` 三个目录

### Requirement: API Reference Index

MUST 在 `docs/06-reference/` 下创建索引页，链接到 Doxygen 生成的 HTML。

#### Scenario: Index page links to Doxygen HTML

- **GIVEN** Doxygen HTML 已生成到 `docs/api/html/`
- **WHEN** 打开 `docs/06-reference/doxygen-api-index.md`
- **THEN** 页面包含指向 `../api/html/index.html` 的链接
- **AND** 页面描述覆盖的 API 范围

### Requirement: Build Artifacts Excluded

Doxygen 生成的 HTML 目录 MUST NOT 纳入版本控制。

#### Scenario: HTML directory gitignored

- **GIVEN** `.gitignore` 存在
- **WHEN** `docs/api/html/` 目录被创建
- **THEN** `git status` 不显示该目录