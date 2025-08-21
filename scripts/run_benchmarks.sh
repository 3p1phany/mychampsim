#!/usr/bin/env bash
# 并行运行 benchmarks.tsv；仅统计整体总耗时
# 结果目录：results/<YYYYmmdd-HHMMSS>-<label>/<benchmark>/<slice>/
set -euo pipefail

# ===== 可通过环境变量覆盖的配置 =====
MANIFEST="${MANIFEST:-./benchmarks.tsv}"     # 四列：benchmark\tslice\tweight\ttrace_path
BINARY="${BINARY:-${1:-./bin/champsim}}"     # ChampSim 可执行文件
RESULTS_ROOT="${RESULTS_ROOT:-./results}"    # 根结果目录
JOBS=64
WARMUP="${WARMUP:-20000000}"
SIM="${SIM:-100000000}"
EXTRA_ARGS="${EXTRA_ARGS:--loongarch}"                 # 额外参数（可为空）
DRY_RUN="${DRY_RUN:-0}"                      # 1=只打印命令，不执行
# ====================================

fmt_hms() { # 秒 -> HH:MM:SS
  local s=$1
  printf "%02d:%02d:%02d" $((s/3600)) $(((s%3600)/60)) $((s%60))
}

# 基本检查
[[ -f "$MANIFEST" ]] || { echo "找不到清单文件: $MANIFEST" >&2; exit 1; }
[[ -x "$BINARY" ]] || { echo "找不到可执行的 ChampSim binary: $BINARY" >&2; echo "提示: 可用 BINARY=... 覆盖" >&2; exit 1; }

# 交互式输入 label
read -rp "请输入本次运行的 label（例如 micro-tune-1）: " USER_LABEL
USER_LABEL="${USER_LABEL:-nolabel}"
USER_LABEL_CLEAN="$(echo "$USER_LABEL" | tr ' /:\\' '____' )"
RUN_ID="$(date +%Y%m%d-%H%M%S)-${USER_LABEL_CLEAN}"
BASE="${RESULTS_ROOT}/${RUN_ID}"
mkdir -p "$BASE"

echo "================ 运行配置 ================"
echo "Manifest:     $MANIFEST"
echo "Binary:       $BINARY"
echo "并行度:        $JOBS"
echo "Warmup:       $WARMUP"
echo "Sim:          $SIM"
echo "Extra args:   ${EXTRA_ARGS:-<无>}"
echo "结果目录:       $BASE"
echo "DRY_RUN:      $DRY_RUN"
echo "=========================================="

read -rp "确认开始？[y/N] " go
[[ "${go:-}" =~ ^[Yy]$ ]] || { echo "已取消。"; exit 0; }
RUN_T0=$(date +%s)

# 逐行读取 TSV（跳过空行与注释；容忍首行表头）
# 列: benchmark \t slice \t weight \t trace_path
# 逐行读取 TSV（跳过空行与注释；容忍首行表头；兼容 BOM/CRLF）
# ===== 可通过环境变量覆盖的配置 =====
# ====================================
# 生成命令清单
CMDS_FILE="$(mktemp)"
trap 'rm -f "$CMDS_FILE"' EXIT

# 用 awk 解析（兼容 TAB/多空格/BOM/CRLF/注释/表头）
# 任何失败都不会让整个脚本退出（不受 set -e 影响）
awk -v BASE="$BASE" \
    -v BINARY="$BINARY" \
    -v WARMUP="$WARMUP" \
    -v SIM="$SIM" \
    -v EXTRA_ARGS="${EXTRA_ARGS:--loongarch}" '
BEGIN {
  FS = "[ \t]+";   # TAB 或若干空格
}
{
  # 去 CRLF
  sub(/\r$/, "", $0)
  # 去 BOM
  if (NR==1) sub(/^\xef\xbb\xbf/, "", $0)
  # 跳过空行/注释
  if ($0 ~ /^[[:space:]]*$/) next
  if ($1 ~ /^#/) next
  # 跳过表头
  if (tolower($1)=="benchmark" && tolower($2)=="slice") next

  bench=$1; slice=$2; weight=$3; trace=$4
  if (bench=="" || slice=="" || weight=="" || trace=="") {
    # 列不完整
    printf("[WARN] 列不完整（跳过）：%s\n", $0) > "/dev/stderr"
    next
  }

  outdir = BASE "/" bench "/" slice
  # 创建输出目录（失败不终止脚本）
  cmd = "mkdir -p " q(outdir)
  system(cmd)

  # 检查 trace 存在
  testcmd = "test -f " q(trace)
  if (system(testcmd) != 0) {
    printf("[WARN] 未找到 trace 文件（跳过）: %s\n", trace) > "/dev/stderr"
    next
  }

  # 拼装一条命令写进命令清单
  # 注意 %% 以便 echo 里的 date 展开在运行时再计算
  printf("echo \"[START $(date +%%H:%%M:%%S)] %s/%s 开始\"; ", bench, slice)
  printf("%s --warmup_instructions %s --simulation_instructions %s %s %s > %s 2>&1; ",
         q(BINARY), WARMUP, SIM, EXTRA_ARGS, q(trace), q(outdir "/run.log"))
  printf("echo \"[END $(date +%%H:%%M:%%S)] %s/%s 结束\"\n", bench, slice)
}
function q(s) { return "\"" s "\"" }
' "$MANIFEST" > "$CMDS_FILE"


# 调试：看看是否写出任务
echo "调试：CMDS_FILE=$CMDS_FILE"
if [[ ! -s "$CMDS_FILE" ]]; then
  echo "[ERROR] 没有生成任何任务。检查 TSV 的前几行如下：" >&2
  head -n 5 "$MANIFEST" >&2
  exit 2
fi
echo "任务条数：$(wc -l < "$CMDS_FILE")"
echo "前3条命令示例："
sed -n '1,3p' "$CMDS_FILE"


if [[ "$DRY_RUN" == "1" ]]; then
  echo
  echo "DRY_RUN=1，仅打印将要执行的命令："
  echo "------------------------------------------"
  cat "$CMDS_FILE" 2>/dev/null || true
  echo "------------------------------------------"
  echo "检查无误后，去掉 DRY_RUN=1 再运行。"
  exit 0
fi

echo
echo "开始并行执行（P=$JOBS）……"
echo

# 避免 set -e 在有任务失败时中断统计总耗时
set +e
xargs -r -P "${JOBS}" -I{} bash -c '{}' < "$CMDS_FILE"


XARGS_STATUS=$?
set -e

RUN_T1=$(date +%s)
TOTAL_ELAPSED=$(( RUN_T1 - RUN_T0 ))
TOTAL_HMS="$(fmt_hms "$TOTAL_ELAPSED")"

# 写到文件并在终端打印
{
  echo "run_id=${RUN_ID}"
  echo "start_epoch=${RUN_T0}"
  echo "end_epoch=${RUN_T1}"
  echo "elapsed_seconds=${TOTAL_ELAPSED}"
  echo "elapsed_hms=${TOTAL_HMS}"
  echo "xargs_exit_code=${XARGS_STATUS}"
} > "${BASE}/TOTAL_TIME.txt"

echo
echo "全部完成！结果目录：$BASE"
echo "总耗时：${TOTAL_ELAPSED}s（${TOTAL_HMS}）"
echo "并行执行退出码：${XARGS_STATUS}（0 表示全部成功）"

