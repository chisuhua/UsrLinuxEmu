# stage3-4-doc-completion Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 完成 Stage 3.4 文档完善：生成 Doxygen API 引用、验证 quickstart 路径、v1.0 文档收尾。

**Architecture:** 纯文档变更，不修改任何源代码。Doxygen 配置驱动 HTML 生成；quickstart 通过端到端计时验证；v1.0 收尾确保所有文档交叉引用一致。

**Tech Stack:** Doxygen, Markdown, Bash (计时验证)

---

## File Structure

### Production (新增/修改)

| File | Responsibility |
|---|---|
| `docs/Doxyfile` | Doxygen 配置，覆盖 3 个公共 API 目录 |
| `docs/06-reference/doxygen-api-index.md` | API 参考索引页 |
| `docs/01-quickstart/quickstart-verification.md` | Quickstart 验证报告 |
| `.gitignore` | 排除 `docs/api/html/` |

---

## Task 1: Doxygen Configuration

**Files:**
- Create: `docs/Doxyfile`
- Modify: `.gitignore`

- [ ] **Step 1: Create Doxyfile**

Create `docs/Doxyfile` with minimal config:
```cmake
# Doxyfile for UsrLinuxEmu public API
PROJECT_NAME           = "UsrLinuxEmu"
PROJECT_BRIEF          = "User-space Linux Kernel Emulation"
OUTPUT_DIRECTORY       = docs/api
INPUT                  = include/kernel include/linux_compat plugins/gpu_driver/shared
RECURSIVE              = YES
GENERATE_HTML          = YES
GENERATE_LATEX         = NO
GENERATE_XML           = NO
EXTRACT_ALL            = YES
FILE_PATTERNS          = *.h *.hpp
```

- [ ] **Step 2: Verify Doxygen install + run**

```bash
which doxygen || { echo "Install: apt-get install doxygen"; exit 1; }
doxygen docs/Doxyfile
```
Expected: `docs/api/html/index.html` created, no errors.

- [ ] **Step 3: Add html output to .gitignore**

Append to `.gitignore`:
```
docs/api/html/
```

- [ ] **Step 4: Commit**

```bash
git add docs/Doxyfile .gitignore
git commit -m "docs: add Doxygen config for public API headers"
```

---

## Task 2: API Reference Index Page

**Files:**
- Create: `docs/06-reference/doxygen-api-index.md`

- [ ] **Step 1: Create index page**

Create `docs/06-reference/doxygen-api-index.md`:
```markdown
# API 参考 (Doxygen)

自动生成的 API 参考文档，覆盖以下公共 API 目录：

| 目录 | 说明 |
|------|------|
| `include/kernel/` | 内核模拟框架（VFS, Device, PCIe, WaitQueue...） |
| `include/linux_compat/` | Linux 内核 API 兼容层（类型、宏、ioctl 编码） |
| `plugins/gpu_driver/shared/` | GPU 驱动共享接口（IOCTL 定义、类型、队列） |

📖 [浏览 API 参考](../api/html/index.html)

> 运行 `doxygen docs/Doxyfile` 重新生成。
```

- [ ] **Step 2: Verify link**

```bash
test -f docs/api/html/index.html && echo "✅ link valid" || echo "⚠️ run doxygen first"
```

- [ ] **Step 3: Commit**

```bash
git add docs/06-reference/doxygen-api-index.md
git commit -m "docs: add Doxygen API reference index page"
```

---

## Task 3: Quickstart Verification

**Files:**
- Create: `docs/01-quickstart/quickstart-verification.md`

- [ ] **Step 1: Record baseline environment**

```bash
echo "## 基准环境" > docs/01-quickstart/quickstart-verification.md
echo "- OS: $(lsb_release -ds 2>/dev/null || uname -a)" >> docs/01-quickstart/quickstart-verification.md
echo "- CPU: $(nproc) cores" >> docs/01-quickstart/quickstart-verification.md
echo "- CMake: $(cmake --version | head -1)" >> docs/01-quickstart/quickstart-verification.md
echo "- GCC: $(gcc --version | head -1)" >> docs/01-quickstart/quickstart-verification.md
```

- [ ] **Step 2: End-to-end timing (3 runs)**

```bash
# Run 1
time (mkdir -p /tmp/quickstart-test/build && cd /tmp/quickstart-test/build && cmake /workspace/project/UsrLinuxEmu -DCMAKE_BUILD_TYPE=Release && make -j4 test_gpu_ioctl_standalone && ./bin/test_gpu_ioctl_standalone) 2>&1 | grep real | tee -a /tmp/qt.txt

# Run 2 (clean build)
time (rm -rf /tmp/quickstart-test && mkdir -p /tmp/quickstart-test/build && cd /tmp/quickstart-test/build && cmake /workspace/project/UsrLinuxEmu -DCMAKE_BUILD_TYPE=Release && make -j4 test_gpu_ioctl_standalone && ./bin/test_gpu_ioctl_standalone) 2>&1 | grep real | tee -a /tmp/qt.txt

# Run 3 (clean build)
time (rm -rf /tmp/quickstart-test && mkdir -p /tmp/quickstart-test/build && cd /tmp/quickstart-test/build && cmake /workspace/project/UsrLinuxEmu -DCMAKE_BUILD_TYPE=Release && make -j4 test_gpu_ioctl_standalone && ./bin/test_gpu_ioctl_standalone) 2>&1 | grep real | tee -a /tmp/qt.txt
```

- [ ] **Step 3: Analyze results and write report**

```bash
echo "" >> docs/01-quickstart/quickstart-verification.md
echo "## 结果" >> docs/01-quickstart/quickstart-verification.md
echo "| Run | Time |" >> docs/01-quickstart/quickstart-verification.md
i=1; while read line; do echo "| $i | $line |"; i=$((i+1)); done < /tmp/qt.txt >> docs/01-quickstart/quickstart-verification.md
echo "" >> docs/01-quickstart/quickstart-verification.md
echo "**结论**: 若中位数超15分钟，需优化quickstart文档。" >> docs/01-quickstart/quickstart-verification.md
```

- [ ] **Step 4: Commit**

```bash
git add docs/01-quickstart/quickstart-verification.md
git commit -m "docs: add quickstart end-to-end verification report"
```

---

## Task 4: v1.0 Documentation Wrap-Up

**Files:**
- Check: `docs/README.md`, `docs/roadmap/stage-3-v1.0.md`

- [ ] **Step 1: Verify docs-audit passes**

```bash
test -f tools/docs-audit.sh && bash tools/docs-audit.sh --strict
echo "docs-audit: $(grep -c 'PASS\|pass' /tmp/docs-audit.log 2>/dev/null || echo 'N/A')"
```
Expected: 43/43 PASS (or no regressions).

- [ ] **Step 2: Verify cross-references**

```bash
# Check docs/README.md references new files
grep -q "doxygen-api-index" docs/README.md || echo "⚠️ need to update docs/README.md navigation"

# Update docs/README.md to include API reference
if ! grep -q "doxygen-api-index" docs/README.md; then
  echo "Add navigation entry for doxygen-api-index.md in docs/README.md"
fi
```

- [ ] **Step 3: Update stage-3 roadmap status**

Update `docs/roadmap/stage-3-v1.0.md`:
- Change `3.4 文档完善` status from `🔄 进行中` to `✅ 已完成`
- Update `v1.0 发布清单` checkbox: `[ ]` → `[x]` for applicable items

- [ ] **Step 4: Commit**

```bash
git add docs/README.md docs/roadmap/stage-3-v1.0.md
git commit -m "docs: complete stage3-4 — Doxygen, quickstart, v1.0 wrap-up"
```
