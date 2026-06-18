# Tasks: cleanup-orphan-struct-gpu-create-queue-args

> **依赖**: proposal ✅ / specs N/A（纯清理 + 文档同步，无新需求）
> **预估总工时**: ~20 min（读 7 文件 + 7 编辑 + 3 验证 + 提交）
> **约束**: docs-audit 36/36 PASS + 编译 100% + ctest 34/34 PASS（全部已验证）

## 1. 上下文读取（7 文件）

- [x] 1.1 读 `plugins/gpu_driver/shared/gpu_queue.h`（L1-83）确认 orphan struct 位置
- [x] 1.2 读 `docs/06-reference/api-reference.md:460-485` 与 `L685-705` 确认用法
- [x] 1.3 读 `docs/05-advanced/gpu_driver_architecture.md:585-600` 确认用法
- [x] 1.4 读 `docs/pending/umq-implementation-plan.md:140-165` 与 `L260-280` 确认用法
- [x] 1.5 读 `docs/00_adr/adr-015-gpu-ioctl-unification.md:395-415` 历史 ADR 用法
- [x] 1.6 读 `docs/00_adr/adr-024-user-mode-queue-submission.md:163-185` 历史 ADR 用法
- [x] 1.7 读 `plugins/gpu_driver/shared/gpu_ioctl.h:200-213` 确认 `gpu_queue_args` 当前定义

## 2. 决策（混合策略）

- [x] 2.1 决定：删除 orphan struct（无人引用 = 真死代码）
- [x] 2.2 决定：5 个活的文档文件 → 改用 `gpu_queue_args`（当前实际 IOCTL struct）
- [x] 2.3 决定：2 个历史 ADR → 保留 struct 定义 + 加注脚（ADR 是历史快照，不应改原文）

## 3. 实施（7 文件编辑）

- [x] 3.1 `plugins/gpu_driver/shared/gpu_queue.h:54-62` 删除 `gpu_create_queue_args` 定义，替换为 6 行 Doxygen 设计决策注释（指向 `gpu_ioctl.h`）
- [x] 3.2 `docs/06-reference/api-reference.md:470-478` 替换 struct 定义为指向 `gpu_queue_args` 的注释
- [x] 3.3 `docs/06-reference/api-reference.md:692-697` 替换示例代码为 `gpu_queue_args`（加 `va_space_handle` 字段示例 + 注释）
- [x] 3.4 `docs/05-advanced/gpu_driver_architecture.md:592-600` 替换 struct 定义为指向 `gpu_ioctl.h:205-212` 的注释
- [x] 3.5 `docs/pending/umq-implementation-plan.md:145-152` 删除 `gpu_create_queue_args` 定义（保留 `gpu_queue_map_ring_args`），加注释说明 `gpu_queue_args`
- [x] 3.6 `docs/pending/umq-implementation-plan.md:265` `create_queue` 入参从 `gpu_create_queue_args*` 改为 `gpu_queue_args*`
- [x] 3.7 `docs/00_adr/adr-015-gpu-ioctl-unification.md:401-408` 保留 struct 定义 + 加 ADR 历史注脚（2026-06-18）
- [x] 3.8 `docs/00_adr/adr-024-user-mode-queue-submission.md:169-176` 保留 struct 定义 + 加 ADR 历史注脚（2026-06-18）

## 4. 验证

- [x] 4.1 `grep -rn "gpu_create_queue_args"` 全仓库搜索：仅剩 8 处预期保留（2 ADR struct + 2 ADR 注脚 + 4 审计/SSOT/归档历史）
- [x] 4.2 `bash tools/docs-audit.sh` 跑全量审计：**36/36 PASS, 0 FAIL, 0 WARNING** ✅
- [x] 4.3 `make -j4` 编译：**100% Built target**，无错误 ✅
- [x] 4.4 `ctest --output-on-failure` 跑测试：**34/34 PASS, 0 FAILED** ✅

## 5. SSOT 同步

- [x] 5.1 创建 OpenSpec change archive 目录 + 写 proposal.md + 写 tasks.md
- [x] 5.2 更新 SSOT 变更记录（追加 v0.1.7.1 entry）
- [x] 5.3 更新 SSOT footer（最后更新日期 + 对应 commit）

## 6. 提交

- [x] 6.1 `git add` 9 项（gpu_queue.h + 5 文档 + 2 ADR + OpenSpec archive）
- [x] 6.2 `git commit`（pre-commit hook 自动跑 docs-audit，已通过 36/36）
