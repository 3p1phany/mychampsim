#!/usr/bin/env bash
# 并行运行 benchmarks.tsv；支持断点续跑（结果目录固定为 results/<label>）
set -euo pipefail

# ===== 可通过环境变量覆盖的配置 =====
#MANIFEST="${MANIFEST:-./benchmarks.tsv}"     # 四列：benchmark\tslice\tweight\ttrace_path
MANIFEST="${MANIFEST:-./benchmarks_selected.tsv}"     # 四列：benchmark\tslice\tweight\ttrace_path
BINARY="${BINARY:-${1:-./bin/champsim}}"     # ChampSim 可执行文件（允许相对路径）
RESULTS_ROOT="${RESULTS_ROOT:-./results}"    # 根结果目录
JOBS="${JOBS:-128}"
WARMUP="${WARMUP:-20000000}"
SIM="${SIM:-80000000}"
EXTRA_ARGS="${EXTRA_ARGS:--loongarch}"       # 额外参数（可为空）
DRY_RUN="${DRY_RUN:-0}"                      # 1=只打印命令，不执行
# 邮件通知（需要安装并配置 msmtp）
EMAIL_SEND="${EMAIL_SEND:-1}"                # 1=发送邮件
EMAIL_TO="${EMAIL_TO:-xuhan_2333@163.com}"   # 收件人（必填）
EMAIL_FROM="${EMAIL_FROM:-xuhan_2333@163.com}" # 发件人（可选，未填则用 msmtp 配置）
EMAIL_SUBJECT_PREFIX="${EMAIL_SUBJECT_PREFIX:-[champsim]}"  # 主题前缀
EMAIL_ACCOUNT="${EMAIL_ACCOUNT:-}"           # msmtp account（可选）
MSMTP_BIN="${MSMTP_BIN:-msmtp}"              # msmtp 命令名
# ====================================

abs_path() {  # 把路径转成绝对路径（兼容没有 readlink -f 的环境）
  local p="$1"
  if [[ "$p" = /* ]]; then printf "%s\n" "$p"; else printf "%s/%s\n" "$(cd "$(dirname "$p")" && pwd -P)" "$(basename "$p")"; fi
}
fmt_hms() { local s=$1; printf "%02d:%02d:%02d" $((s/3600)) $(((s%3600)/60)) $((s%60)); }
q() { printf '"%s"' "$1"; }
count_tsv() { awk 'BEGIN{c=0} !/^[[:space:]]*($|#)/{c++} END{print c}' "$1"; }
send_mail() {
  if [[ "$EMAIL_SEND" != "1" ]]; then return 0; fi
  if [[ -z "$EMAIL_TO" ]]; then echo "[WARN] EMAIL_SEND=1 但 EMAIL_TO 为空，跳过邮件。" >&2; return 0; fi
  if ! command -v "$MSMTP_BIN" >/dev/null 2>&1; then echo "[WARN] 未找到 $MSMTP_BIN，跳过邮件。" >&2; return 0; fi

  local subject="${EMAIL_SUBJECT_PREFIX} label=${USER_LABEL_CLEAN} rc=${XARGS_STATUS}"
  local mail_cmd=("$MSMTP_BIN" -t)
  if [[ -n "$EMAIL_ACCOUNT" ]]; then mail_cmd=("$MSMTP_BIN" -a "$EMAIL_ACCOUNT" -t); fi

  {
    [[ -n "$EMAIL_FROM" ]] && echo "From: $EMAIL_FROM"
    echo "To: $EMAIL_TO"
    echo "Subject: $subject"
    echo "Date: $(date -R)"
    echo
    echo "label: $USER_LABEL_CLEAN"
    echo "run_dir: $BASE"
    echo "start_epoch: $RUN_T0"
    echo "end_epoch: $RUN_T1"
    echo "elapsed_seconds: $TOTAL_ELAPSED"
    echo "elapsed_hms: $TOTAL_HMS"
    echo "xargs_exit_code: $XARGS_STATUS"
    echo "planned_tasks: $TASKS"
    echo "remaining_tasks: $REMAINING_COUNT"
    echo "failed_tasks: $FAILED_COUNT"
    echo "succeeded_tasks: $SUCCESS_COUNT"
    echo
    echo "pending_list: $BASE/PENDING.tsv"
    echo "remaining_list: $BASE/REMAINING.tsv"
    echo "failed_list: $BASE/FAILED.tsv"
    echo "summary_file: $BASE/TOTAL_TIME.txt"
  } | "${mail_cmd[@]}"
}

# 基本检查
[[ -f "$MANIFEST" ]] || { echo "找不到清单文件: $MANIFEST" >&2; exit 1; }
[[ -x "$BINARY" ]] || { echo "找不到可执行的 ChampSim binary: $BINARY" >&2; echo "提示: 可用 BINARY=... 覆盖" >&2; exit 1; }

# 规范化为绝对路径
BINARY_ABS="$(abs_path "$BINARY")"
MANIFEST_ABS="$(abs_path "$MANIFEST")"
RESULTS_ROOT_ABS="$(abs_path "$RESULTS_ROOT")"
PWD0="$(pwd -P)"

# 读取 label，并固定为 results/<label>
read -rp "请输入本次运行的 label（例如 micro-tune-1）: " USER_LABEL
USER_LABEL="${USER_LABEL:-nolabel}"
USER_LABEL_CLEAN="$(echo "$USER_LABEL" | tr ' /:\\' '____' )"
BASE="${RESULTS_ROOT_ABS}/${USER_LABEL_CLEAN}"
mkdir -p "$BASE"

# 断点续跑：优先使用已有的 manifest 快照，并提示剩余清单
MANIFEST_SNAPSHOT="${BASE}/MANIFEST.tsv"
RUN_MANIFEST="$MANIFEST_ABS"
RESUME_NOTE=""
if [[ -f "$MANIFEST_SNAPSHOT" ]]; then
  RUN_MANIFEST="$MANIFEST_SNAPSHOT"
  RESUME_NOTE="发现已有 manifest 快照，按上次配置续跑"
  if [[ "$MANIFEST_ABS" != "$RUN_MANIFEST" ]]; then
    echo "[INFO] label 已存在，忽略当前 MANIFEST，使用 ${MANIFEST_SNAPSHOT}" >&2
  fi
else
  cp -f "$MANIFEST_ABS" "$MANIFEST_SNAPSHOT"
fi
if [[ -f "$BASE/REMAINING.tsv" ]]; then
  remaining_count=$(count_tsv "$BASE/REMAINING.tsv")
  if [[ "$remaining_count" -gt 0 ]]; then
    if [[ -n "$RESUME_NOTE" ]]; then
      RESUME_NOTE="${RESUME_NOTE}；检测到未完成清单，继续剩余切片（${remaining_count}）"
    else
      RESUME_NOTE="检测到未完成清单，继续剩余切片（${remaining_count}）"
    fi
  fi
fi
RUN_MANIFEST_ABS="$(abs_path "$RUN_MANIFEST")"

echo "================ 运行配置 ================"
echo "Manifest:     $RUN_MANIFEST_ABS"
if [[ "$RUN_MANIFEST_ABS" != "$MANIFEST_ABS" ]]; then
  echo "Manifest(输入): $MANIFEST_ABS"
fi
if [[ -n "$RESUME_NOTE" ]]; then
  echo "断点续跑:     $RESUME_NOTE"
fi
echo "Binary:       $BINARY_ABS"
echo "并行度:        $JOBS"
echo "Warmup:       $WARMUP"
echo "Sim:          $SIM"
echo "Extra args:   ${EXTRA_ARGS:-<无>}"
echo "结果目录:       $BASE   （固定为 label，无时间戳）"
echo "DRY_RUN:      $DRY_RUN"
echo "每切片独立CWD:  是（方案A）"
echo "=========================================="

read -rp "确认开始？[y/N] " go
[[ "${go:-}" =~ ^[Yy]$ ]] || { echo "已取消。"; exit 0; }

CMDS_FILE="$(mktemp)"; trap 'rm -f "$CMDS_FILE"' EXIT
PENDING_TMP="${BASE}/PENDING.tsv.tmp"
FAILED_TMP="${BASE}/FAILED.tsv.tmp"
: > "$PENDING_TMP"
: > "$FAILED_TMP"

# 用 awk 解析清单并生成命令（仅针对“未完成或失败”的切片）
awk -v BASE="$BASE" \
    -v BINARY="$BINARY_ABS" \
    -v WARMUP="$WARMUP" \
    -v SIM="$SIM" \
    -v EXTRA_ARGS="$EXTRA_ARGS" \
    -v PWD0="$PWD0" \
    -v PENDING_OUT="$PENDING_TMP" \
    -v FAILED_OUT="$FAILED_TMP" '
BEGIN { FS = "[ \t]+"; print "# benchmark\tslice\tweight\ttrace_path" > PENDING_OUT; print "# benchmark\tslice\tweight\ttrace_path" > FAILED_OUT; }
{
  sub(/\r$/, "", $0);                                 # 去 CR
  if (NR==1) sub(/^\xef\xbb\xbf/, "", $0);            # 去 BOM
  if ($0 ~ /^[[:space:]]*$/) next;                    # 空行
  if ($1 ~ /^#/) next;                                # 注释
  if (tolower($1)=="benchmark" && tolower($2)=="slice") next; # 表头

  bench=$1; slice=$2; weight=$3; trace=$4
  if (bench=="" || slice=="" || weight=="" || trace=="") { printf("[WARN] 列不完整（跳过）：%s\n", $0) > "/dev/stderr"; next }

  if (trace !~ "^/") trace = PWD0 "/" trace            # 规范 trace 为绝对路径
  outdir = BASE "/" bench "/" slice
  done_marker = outdir "/.done"
  exitcode_file = outdir "/.exitcode"

  system("mkdir -p " q(outdir))

  if (system("test -f " q(trace)) != 0) { printf("[WARN] 未找到 trace 文件（跳过）: %s\n", trace) > "/dev/stderr"; next }

  # 已完成则跳过
  if (system("test -f " q(done_marker)) == 0) next

  # 记录到 PENDING 清单
  printf("%s\t%s\t%.12f\t%s\n", bench, slice, weight+0.0, trace) >> PENDING_OUT

  # 如果上次 exitcode 非0，记录到 FAILED 清单（本次仍会重跑）
  if (system("test -f " q(exitcode_file)) == 0) {
    cmd = "sh -c \047code=$(cat " q(exitcode_file) " 2>/dev/null || echo 0); test \"$code\" != 0\047"
    if (system(cmd) == 0) {
      printf("%s\t%s\t%.12f\t%s\n", bench, slice, weight+0.0, trace) >> FAILED_OUT
    }
  }

  # 执行命令：写 .start_epoch/.end_epoch/.exitcode，并在成功时 touch .done
  printf("echo \"[START $(date +%%H:%%M:%%S)] %s/%s\"; ", bench, slice);
  printf("(cd %s && { date +%%s > .start_epoch; %s --warmup_instructions %s --simulation_instructions %s %s %s > run.log 2>&1; rc=$?; date +%%s > .end_epoch; echo \"$rc\" > .exitcode; if [ $rc -eq 0 ]; then touch .done; rm -f .failed; else echo \"$rc\" > .failed; fi; exit $rc; }); ",
         q(outdir), q(BINARY), WARMUP, SIM, EXTRA_ARGS, q(trace));
  printf("echo \"[END $(date +%%H:%%M:%%S)] %s/%s rc=$?\"\n", bench, slice);
}
function q(s) { return "\"" s "\"" }
' "$RUN_MANIFEST_ABS" > "$CMDS_FILE"
mv -f "$PENDING_TMP" "${BASE}/PENDING.tsv"

echo "调试：CMDS_FILE=$CMDS_FILE"
TASKS=$(wc -l < "$CMDS_FILE" | tr -d ' ')
echo "本次计划执行未完成切片数：$TASKS"
if [[ "$TASKS" -gt 0 ]]; then
  echo "前3条命令示例："; sed -n '1,3p' "$CMDS_FILE"
else
  echo "没有未完成的切片需要执行。"
fi

if [[ "$DRY_RUN" == "1" ]]; then
  echo; echo "DRY_RUN=1，仅打印将要执行的命令："
  echo "------------------------------------------"
  cat "$CMDS_FILE"
  echo "------------------------------------------"
  echo "检查无误后，去掉 DRY_RUN=1 再运行。"; exit 0
fi

RUN_T0=$(date +%s)
set +e
if [[ "$TASKS" -gt 0 ]]; then
  echo; echo "开始并行执行（P=$JOBS）……"; echo
  xargs -r -P "${JOBS}" -I{} bash -c '{}' < "$CMDS_FILE"
  XARGS_STATUS=$?
else
  XARGS_STATUS=0
fi
set -e
RUN_T1=$(date +%s)
TOTAL_ELAPSED=$(( RUN_T1 - RUN_T0 ))
TOTAL_HMS="$(fmt_hms "$TOTAL_ELAPSED")"

# 运行结束后刷新一次剩余/失败清单
REMAINING_TMP="${BASE}/REMAINING.tsv.tmp"
: > "$REMAINING_TMP"
awk -v BASE="$BASE" -v PWD0="$PWD0" '
BEGIN { FS = "[ \t]+"; print "# benchmark\tslice\tweight\ttrace_path" }
{
  sub(/\r$/, "", $0);
  if (NR==1) sub(/^\xef\xbb\xbf/, "", $0);
  if ($0 ~ /^[[:space:]]*$/) next;
  if ($1 ~ /^#/) next;
  if (tolower($1)=="benchmark" && tolower($2)=="slice") next;
  bench=$1; slice=$2; trace=$4;
  if (bench=="" || slice=="" || trace=="") next;
  outdir = BASE "/" bench "/" slice;
  done_marker = outdir "/.done";
  if (system("test -f " q(done_marker)) != 0) {
    if (trace !~ "^/") trace = PWD0 "/" trace;
    printf("%s\t%s\t%s\t%s\n", bench, slice, $3, trace);
  }
}
function q(s) { return "\"" s "\"" }
' "$RUN_MANIFEST_ABS" > "$REMAINING_TMP"
mv -f "$REMAINING_TMP" "${BASE}/REMAINING.tsv"

FAILED_TMP2="${BASE}/FAILED.tsv.tmp"
: > "$FAILED_TMP2"
awk -v BASE="$BASE" -v PWD0="$PWD0" '
BEGIN { FS = "[ \t]+"; print "# benchmark\tslice\tweight\ttrace_path" }
{
  sub(/\r$/, "", $0);
  if (NR==1) sub(/^\xef\xbb\xbf/, "", $0);
  if ($0 ~ /^[[:space:]]*$/) next;
  if ($1 ~ /^#/) next;
  if (tolower($1)=="benchmark" && tolower($2)=="slice") next;
  bench=$1; slice=$2; trace=$4;
  if (bench=="" || slice=="" || trace=="") next;
  outdir = BASE "/" bench "/" slice;
  done_marker = outdir "/.done";
  exitcode_file = outdir "/.exitcode";
  if (system("test -f " q(done_marker)) == 0) next;
  if (system("test -f " q(exitcode_file)) == 0) {
    cmd = "sh -c \047code=$(cat " q(exitcode_file) " 2>/dev/null || echo 0); test \"$code\" != 0\047";
    if (system(cmd) == 0) {
      if (trace !~ "^/") trace = PWD0 "/" trace;
      printf("%s\t%s\t%s\t%s\n", bench, slice, $3, trace);
    }
  }
}
function q(s) { return "\"" s "\"" }
' "$RUN_MANIFEST_ABS" > "$FAILED_TMP2"
mv -f "$FAILED_TMP2" "${BASE}/FAILED.tsv"

REMAINING_COUNT=$(count_tsv "${BASE}/REMAINING.tsv")
FAILED_COUNT=$(count_tsv "${BASE}/FAILED.tsv")
SUCCESS_COUNT=$(( TASKS - REMAINING_COUNT ))
if [[ "$SUCCESS_COUNT" -lt 0 ]]; then SUCCESS_COUNT=0; fi

# 写本轮统计（注意：不再包含时间戳 run_id）
{
  echo "run_dir=${BASE}"
  echo "label=${USER_LABEL_CLEAN}"
  echo "start_epoch=${RUN_T0}"
  echo "end_epoch=${RUN_T1}"
  echo "elapsed_seconds=${TOTAL_ELAPSED}"
  echo "elapsed_hms=${TOTAL_HMS}"
  echo "xargs_exit_code=${XARGS_STATUS}"
  echo "planned_tasks=${TASKS}"
  echo "remaining_tasks=${REMAINING_COUNT}"
  echo "failed_tasks=${FAILED_COUNT}"
  echo "succeeded_tasks=${SUCCESS_COUNT}"
} > "${BASE}/TOTAL_TIME.txt"

echo
echo "全部完成（或已无待跑切片）。结果目录：$BASE"
echo "本轮耗时：${TOTAL_ELAPSED}s（${TOTAL_HMS}）"
echo "并行执行退出码：${XARGS_STATUS}（0 表示全部成功）"
echo "待跑清单：${BASE}/PENDING.tsv（本轮开始前） / ${BASE}/REMAINING.tsv（本轮结束后）"
echo "失败清单：${BASE}/FAILED.tsv"

send_mail
