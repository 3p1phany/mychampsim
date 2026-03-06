#!/usr/bin/env bash
# 一键运行 GS_ALIGNED 实验：构建 → 仿真 → 汇总 → 对比 GS baseline
# 用法:
#   ./scripts/run_gs_aligned.sh              # 使用默认配置
#   JOBS=64 ./scripts/run_gs_aligned.sh      # 自定义并行度
#   SKIP_BUILD=1 ./scripts/run_gs_aligned.sh # 跳过构建步骤
#   BASELINE=GS_1c ./scripts/run_gs_aligned.sh  # 自定义 baseline label
set -euo pipefail

# ===== 可配置参数 =====
LABEL="${LABEL:-GS_ALIGNED_1c}"
BASELINE="${BASELINE:-GS_1c}"
JOBS="${JOBS:-128}"
WARMUP="${WARMUP:-20000000}"
SIM="${SIM:-80000000}"
SKIP_BUILD="${SKIP_BUILD:-0}"
MANIFEST="${MANIFEST:-./benchmarks_selected.tsv}"

PROJ_ROOT="$(cd "$(dirname "$0")/.." && pwd -P)"
DRAMSIM3_ROOT="$(cd "$PROJ_ROOT/../dramsim3" && pwd -P)"
DRAMSIM3_INI="$PROJ_ROOT/dramsim3_configs/DDR5_64GB_4ch_4800_GS_ALIGNED.ini"
BINARY="$PROJ_ROOT/bin/champsim"
RESULTS_ROOT="$PROJ_ROOT/results"

echo "======================================================"
echo "  GS_ALIGNED 一键运行脚本"
echo "======================================================"
echo "  Label:      $LABEL"
echo "  Baseline:   $BASELINE"
echo "  并行度:     $JOBS"
echo "  Warmup:     $WARMUP"
echo "  Sim:        $SIM"
echo "  Manifest:   $MANIFEST"
echo "  DRAM config: $DRAMSIM3_INI"
echo "======================================================"

# ===== Step 1: 构建 =====
if [[ "$SKIP_BUILD" != "1" ]]; then
    echo
    echo "[1/4] 构建 DRAMSim3 ..."
    cd "$DRAMSIM3_ROOT"
    mkdir -p build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Release > /dev/null 2>&1
    make -j8 2>&1 | tail -3
    echo "  → libdramsim3.so 构建完成"

    echo
    echo "[1/4] 构建 ChampSim ..."
    cd "$PROJ_ROOT"
    make -j8 2>&1 | tail -3
    echo "  → champsim 构建完成"
else
    echo
    echo "[1/4] 跳过构建 (SKIP_BUILD=1)"
fi

# ===== Step 2: 验证 =====
echo
echo "[2/4] 验证环境 ..."

if [[ ! -x "$BINARY" ]]; then
    echo "  [ERROR] 找不到 champsim: $BINARY" >&2
    exit 1
fi
if [[ ! -f "$DRAMSIM3_INI" ]]; then
    echo "  [ERROR] 找不到 DRAM 配置: $DRAMSIM3_INI" >&2
    exit 1
fi
if [[ ! -f "$MANIFEST" ]]; then
    # 尝试相对于 PROJ_ROOT
    if [[ -f "$PROJ_ROOT/$MANIFEST" ]]; then
        MANIFEST="$PROJ_ROOT/$MANIFEST"
    else
        echo "  [ERROR] 找不到 manifest: $MANIFEST" >&2
        exit 1
    fi
fi

export LD_LIBRARY_PATH="${DRAMSIM3_ROOT}:${LD_LIBRARY_PATH:-}"
echo "  LD_LIBRARY_PATH=$LD_LIBRARY_PATH"
echo "  Binary: $BINARY"
echo "  DRAM config: $DRAMSIM3_INI"
echo "  Manifest: $MANIFEST ($(grep -vc '^\s*#\|^\s*$\|^benchmark' "$MANIFEST" || true) slices)"

# ===== Step 3: 运行仿真 =====
echo
echo "[3/4] 运行仿真 (label=$LABEL, P=$JOBS) ..."

BASE="$RESULTS_ROOT/$LABEL"
mkdir -p "$BASE"

# 保存 manifest 快照（支持断点续跑）
MANIFEST_ABS="$(cd "$(dirname "$MANIFEST")" && pwd -P)/$(basename "$MANIFEST")"
if [[ ! -f "$BASE/MANIFEST.tsv" ]]; then
    cp -f "$MANIFEST_ABS" "$BASE/MANIFEST.tsv"
fi
RUN_MANIFEST="$BASE/MANIFEST.tsv"

# 统计待跑切片
CMDS_FILE="$(mktemp)"
trap 'rm -f "$CMDS_FILE"' EXIT

awk -v BASE="$BASE" \
    -v BINARY="$BINARY" \
    -v WARMUP="$WARMUP" \
    -v SIM="$SIM" \
    -v DRAM_CFG="$DRAMSIM3_INI" '
BEGIN { FS = "[ \t]+"; }
{
  sub(/\r$/, "", $0);
  if (NR==1) sub(/^\xef\xbb\xbf/, "", $0);
  if ($0 ~ /^[[:space:]]*$/) next;
  if ($1 ~ /^#/) next;
  if (tolower($1)=="benchmark" && tolower($2)=="slice") next;

  bench=$1; slice=$2; weight=$3; trace=$4;
  if (bench=="" || slice=="" || weight=="" || trace=="") next;

  outdir = BASE "/" bench "/" slice;
  done_marker = outdir "/.done";

  if (system("test -f " q(done_marker)) == 0) next;
  system("mkdir -p " q(outdir));
  if (system("test -f " q(trace)) != 0) {
    printf("[WARN] trace not found: %s\n", trace) > "/dev/stderr";
    next;
  }

  printf("(cd %s && { date +%%s > .start_epoch; %s --warmup_instructions %s --simulation_instructions %s -loongarch --dramsim3_config %s %s > run.log 2>&1; rc=$?; date +%%s > .end_epoch; echo \"$rc\" > .exitcode; if [ $rc -eq 0 ]; then touch .done; fi; exit $rc; })\n",
         q(outdir), q(BINARY), WARMUP, SIM, q(DRAM_CFG), q(trace));
}
function q(s) { return "\"" s "\"" }
' "$RUN_MANIFEST" > "$CMDS_FILE"

TASKS=$(wc -l < "$CMDS_FILE" | tr -d ' ')
echo "  待运行切片数: $TASKS"

if [[ "$TASKS" -eq 0 ]]; then
    echo "  所有切片已完成，跳过仿真。"
else
    RUN_T0=$(date +%s)
    xargs -r -P "${JOBS}" -I{} bash -c '{}' < "$CMDS_FILE" || true
    RUN_T1=$(date +%s)
    ELAPSED=$(( RUN_T1 - RUN_T0 ))
    printf "  仿真完成，耗时 %02d:%02d:%02d\n" $((ELAPSED/3600)) $(((ELAPSED%3600)/60)) $((ELAPSED%60))

    # 统计成功/失败
    DONE_COUNT=$(find "$BASE" -name ".done" | wc -l)
    TOTAL_SLICES=$(grep -vc '^\s*#\|^\s*$\|^benchmark' "$RUN_MANIFEST" || true)
    echo "  完成: $DONE_COUNT / $TOTAL_SLICES slices"
fi

# ===== Step 4: 汇总与对比 =====
echo
echo "[4/4] 汇总 IPC 并对比 ..."

cd "$PROJ_ROOT"

# 汇总 GS_ALIGNED
python3 scripts/summarize_ipc.py \
    --results "$BASE" \
    --manifest "$RUN_MANIFEST" \
    --out "$BASE/summary.tsv" 2>&1 | tail -5
echo "  → $BASE/summary.tsv"

# 对比 baseline（如果存在）
BASELINE_DIR="$RESULTS_ROOT/$BASELINE"
if [[ -f "$BASELINE_DIR/summary.tsv" ]]; then
    COMPARE_OUT="$RESULTS_ROOT/compare_${BASELINE}_vs_${LABEL}.tsv"
    python3 scripts/compare_ipc.py \
        --a "$BASELINE_DIR/summary.tsv" \
        --b "$BASE/summary.tsv" \
        --a-label "$BASELINE" \
        --b-label "$LABEL" \
        --out "$COMPARE_OUT" 2>&1
    echo
    echo "  → 对比结果: $COMPARE_OUT"
    echo
    echo "======================================================"
    echo "  对比摘要 ($BASELINE → $LABEL)"
    echo "======================================================"
    # 打印最后几行（包含 GEOMEAN）
    column -t -s $'\t' "$COMPARE_OUT" | tail -20
else
    echo "  [WARN] 未找到 baseline 结果: $BASELINE_DIR/summary.tsv"
    echo "  请先运行 baseline (GS) 仿真，或设置 BASELINE= 指向已有结果。"
fi

echo
echo "完成！结果目录: $BASE"
