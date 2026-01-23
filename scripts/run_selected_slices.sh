#!/usr/bin/env bash
# 运行精选切片（需要 TRACE_ROOT 指向 trace 根目录，例如 /path/to/Trace/LA）
set -euo pipefail

TRACE_ROOT="${TRACE_ROOT:-}"
MANIFEST_SRC="${MANIFEST_SRC:-./scripts/selected_slices.tsv}"

if [[ -z "$TRACE_ROOT" ]]; then
  echo "请设置 TRACE_ROOT（trace 根目录，例如 /root/data/Trace/LA）" >&2
  exit 1
fi
if [[ ! -d "$TRACE_ROOT" ]]; then
  echo "TRACE_ROOT 不存在或不是目录: $TRACE_ROOT" >&2
  exit 1
fi
if [[ ! -f "$MANIFEST_SRC" ]]; then
  echo "找不到清单文件: $MANIFEST_SRC" >&2
  exit 1
fi

TRACE_ROOT="${TRACE_ROOT%/}"
MANIFEST_TMP="$(mktemp)"
trap 'rm -f "$MANIFEST_TMP"' EXIT

awk -v root="$TRACE_ROOT" '
BEGIN { FS = "[ \t]+"; OFS = "\t" }
{
  sub(/\r$/, "", $0)
  if ($0 ~ /^[[:space:]]*$/) { next }
  if ($1 ~ /^#/) { print; next }
  if (tolower($1)=="benchmark" && tolower($2)=="slice") { print; next }

  bench=$1; slice=$2; weight=$3; trace=$4
  if (bench=="" || slice=="" || weight=="" || trace=="") { next }
  if (trace !~ "^/") trace = root "/" trace
  print bench, slice, weight, trace
}
' "$MANIFEST_SRC" > "$MANIFEST_TMP"

echo "使用清单: $MANIFEST_TMP"
MANIFEST="$MANIFEST_TMP" scripts/run_benchmarks.sh
