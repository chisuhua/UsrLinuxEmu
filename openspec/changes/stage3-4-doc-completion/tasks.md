# stage3-4-doc-completion Tasks

> **Status**: 📋 PROPOSED  
> **Phase**: Stage 3.4 — v1.0 文档完善  
> **Estimate**: 2-3 days

---

## 1. Doxygen API 参考设置

- [ ] 1.1 在 `docs/` 下创建 `Doxyfile`，配置覆盖 `include/kernel/`、`include/linux_compat/`、`plugins/gpu_driver/shared/`
- [ ] 1.2 运行 `doxygen docs/Doxyfile`，验证 HTML 生成到 `docs/api/html/`
- [ ] 1.3 将 `docs/api/html/` 加入 `.gitignore`
- [ ] 1.4 创建 `docs/06-reference/doxygen-api-index.md` 索引页，链接到 HTML 文档

## 2. Quickstart 验证

- [ ] 2.1 记录基准环境条件（OS, CPU，网络速度）
- [ ] 2.2 从零开始计时端到端流程（git clone → build → 首个示例通过），重复 3 次
- [ ] 2.3 若中位数超 15 分钟，优化 quickstart 文档路径
- [ ] 2.4 输出验证报告到 `docs/01-quickstart/quickstart-verification.md`

## 3. v1.0 文档收尾

- [ ] 3.1 验证 `docs/README.md` 导航覆盖所有新增文档
- [ ] 3.2 运行 `docs-audit` 确认 43/43 PASS 保持
- [ ] 3.3 验证 SSOT + ROADMAP + ADR 交叉引用一致性

## 4. 提交与归档

- [ ] 4.1 提交所有文档变更
- [ ] 4.2 更新 `docs/roadmap/stage-3-v1.0.md` 中 3.4 状态为 ✅