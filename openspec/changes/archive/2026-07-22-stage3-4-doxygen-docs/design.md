## Context

UsrLinuxEmu 使用 CMake 构建系统，现有 docs-audit 机制通过 `tools/docs-audit.sh` 验证文档完整性。当前 Doxygen 未配置，API 参考文档完全缺失。用户指南 (`docs/01-quickstart/`) 存在但未经过"15 分钟首次示例"验证。

Stage 3 验收清单要求 docs-audit 43/43 PASS（已达成），但 Doxygen API 参考尚未生成。CI pipeline 有 docs-audit job，但尚未包含 Doxygen 验证。

## Goals / Non-Goals

**Goals:**
- 创建 Doxyfile 配置，覆盖 `include/`、`plugins/gpu_driver/`、`src/kernel/` 核心公共 API
- 集成 Doxygen 生成到 CMake（`make doxygen` 目标，可选）
- 生成 API 参考文档到 `docs/api/`，加入 `.gitignore`
- 完善 `docs/01-quickstart/first-example.md` 确保 15 分钟内可完成
- docs-audit 规则扩展：验证 Doxygen 生成无 warning

**Non-Goals:**
- 100% Doxygen 注释覆盖（仅覆盖导出公共 API）
- 非 Doxygen 文档的全面重写（仅 quickstart 完善）
- v1.0 release notes / migration guide（由独立 change 处理）

## Decisions

1. **Doxyfile 位置**: `docs/Doxyfile`，不与 CMake 构建目录耦合
2. **集成方式**: `CMakeLists.txt` 顶层添加可选 `find_package(Doxygen)` + `add_custom_target(doxygen)`，缺失时跳过不报错
3. **输出目录**: `docs/api/`（相对于项目根），加入 `.gitignore`
4. **输入范围**:
   - `include/kernel/` — 内核框架公共头文件
   - `include/linux_compat/` — Linux 兼容层 API
   - `plugins/gpu_driver/drv/` — 驱动公共接口
   - `plugins/gpu_driver/hal/` — HAL 接口契约
   - `plugins/gpu_driver/shared/` — 共享头文件（gpu_ioctl.h, gpu_types.h）
   - `src/kernel/` — 核心实现（仅公共函数）
5. **EXTRACT_PRIVATE = NO** — 只生成公共 API 文档
6. **quickstart 验证**: 用 `time` 命令记录从 git clone 到编译运行第一个示例的耗时

## Risks / Trade-offs

- [Doxygen warning 噪声] → WARN_AS_ERROR = NO（首次生成可接受 warning），后续迭代收敛
- [CI 构建时间增加] → Doxygen 目标在 docs-audit job 中运行，不影响主构建矩阵
- [API 范围过宽/过窄] → 初始覆盖核心目录，后续按需扩展
- [quickstart 15 分钟依赖网络速度] → 测量时排除首次 `git clone` 时间，从本地构建开始计时