#!/bin/bash
# check-portability.sh — ADR-072 L1 ② drv/ 跨层 include 静态分析
#
# 检查 ② 驱动代码是否直接引用了 ③ sim/ 或 hal_user/hal_mock 头文件。
# 这些 include 如果拷贝到真实 Linux 内核树会编译失败。
#
# 使用: ./tools/check-portability.sh [--baseline <n>]
#   --baseline 设置允许的已知违规上限（默认 13，当前已知数量）
#   若违规数 ≤ baseline: 退出 0（Warning），若 > baseline: 退出 1（Error）
#
# Per ADR-072 Decision 1: L1 是 pre-commit hook（毫秒级，零摩擦）

BASELINE=13
if [ "$1" = "--baseline" ]; then
  BASELINE="$2"
  shift 2
fi

ROOT=$(git rev-parse --show-toplevel 2>/dev/null || pwd)
cd "$ROOT" || exit 1

# 收集所有违规
VIOLATIONS=$(mktemp)
grep -rn '#include.*"sim/\|#include.*"hal/hal_user\|#include.*"hal/hal_mock' \
  plugins/gpu_driver/drv/ --include="*.cpp" --include="*.h" 2>/dev/null > "$VIOLATIONS"

COUNT=$(wc -l < "$VIOLATIONS" 2>/dev/null || echo 0)

if [ "$COUNT" -eq 0 ]; then
  echo "✅ ADR-072 L1 PASS: 0 处 drv/ 跨层 include 违规"
  rm -f "$VIOLATIONS"
  exit 0
fi

# 分类统计
SIM_COUNT=$(grep -c '"sim/' "$VIOLATIONS" 2>/dev/null || echo 0)
HAL_COUNT=$(grep -c '"hal/hal_user\|"hal/hal_mock' "$VIOLATIONS" 2>/dev/null || echo 0)

echo "⚠️  ADR-072 L1: $COUNT 处 drv/ 跨层 include 违规 (baseline=$BASELINE)"
echo "   sim/ headers: $SIM_COUNT"
echo "   hal_user/hal_mock: $HAL_COUNT"
echo ""
echo "   违规文件:"
cat "$VIOLATIONS" | while read line; do
  echo "   $line"
done

rm -f "$VIOLATIONS"

if [ "$COUNT" -le "$BASELINE" ]; then
  echo ""
  echo "⚠️  违规数在已知基线内 ($COUNT ≤ $BASELINE)，退出 0 (warning)"
  exit 0
else
  echo ""
  echo "❌ 新增违规! ($COUNT > $BASELINE)"
  echo "   请将新违规加入 ADR-072 已知技术债清单后更新 baseline"
  exit 1
fi