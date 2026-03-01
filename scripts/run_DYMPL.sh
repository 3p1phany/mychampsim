#!/bin/bash
# ============================================================
# run_DYMPL.sh — 一键运行 DYMPL 基准测试并与现有策略对比
#
# 用法:
#   ./scripts/run_DYMPL.sh              # 运行 DYMPL 并对比所有已有结果
#   ./scripts/run_DYMPL.sh --dry-run    # 只打印命令，不实际运行
#   ./scripts/run_DYMPL.sh --compare-only  # 跳过运行，只做对比
# ============================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
DRAMSIM3_ROOT="$(cd "$PROJECT_ROOT/../dramsim3" && pwd)"

cd "$PROJECT_ROOT"

# ---------- 参数解析 ----------
DRY_RUN=0
COMPARE_ONLY=0
for arg in "$@"; do
    case "$arg" in
        --dry-run)      DRY_RUN=1 ;;
        --compare-only) COMPARE_ONLY=1 ;;
    esac
done

# ---------- 配置 ----------
LABEL="DYMPL_1c"
DYMPL_INI="$PROJECT_ROOT/dramsim3_configs/DDR5_64GB_4ch_4800_DYMPL.ini"
BINARY="$PROJECT_ROOT/bin/champsim"

# 对比基线（只选已有结果的）
BASELINES=()
for candidate in open_page_1c smart_close_1c GS_1c oracle_1c; do
    if [ -f "results/${candidate}/summary.tsv" ]; then
        BASELINES+=("$candidate")
    fi
done

# ---------- 环境检查 ----------
echo "=========================================="
echo "  DYMPL Benchmark Runner"
echo "=========================================="

# 检查 libdramsim3.so
if [ ! -f "$DRAMSIM3_ROOT/libdramsim3.so" ]; then
    echo "[ERROR] libdramsim3.so not found in $DRAMSIM3_ROOT"
    echo "        请先编译 DRAMSim3: cd dramsim3 && mkdir -p build && cd build && cmake .. && make -j8"
    exit 1
fi
export LD_LIBRARY_PATH="${DRAMSIM3_ROOT}:${LD_LIBRARY_PATH:-}"

# 检查 champsim 二进制
if [ ! -x "$BINARY" ]; then
    echo "[ERROR] champsim binary not found at $BINARY"
    echo "        请先编译: cd champsim-la && python3 config.sh champsim_config.json && make -j8"
    exit 1
fi

# 检查 DYMPL ini
if [ ! -f "$DYMPL_INI" ]; then
    echo "[ERROR] DYMPL config not found: $DYMPL_INI"
    exit 1
fi

echo "Binary:       $BINARY"
echo "DRAM Config:  $DYMPL_INI"
echo "Label:        $LABEL"
echo "Baselines:    ${BASELINES[*]:-<无已有结果>}"
echo ""

# ---------- Step 1: 运行 DYMPL 基准测试 ----------
if [ "$COMPARE_ONLY" -eq 0 ]; then
    echo "========== Step 1: 运行 DYMPL 基准测试 =========="
    echo ""

    # 通过 EXTRA_ARGS 传递 --dramsim3_config 覆盖
    export EXTRA_ARGS="-loongarch --dramsim3_config $DYMPL_INI"
    export BINARY
    export DRY_RUN

    # 向 run_benchmarks.sh 传入 label 和确认（两个 read 提示）
    printf '%s\ny\n' "$LABEL" | "$SCRIPT_DIR/run_benchmarks.sh"

    echo ""
    echo "[DONE] DYMPL simulation finished."
    echo ""
fi

# ---------- Step 2: 汇总 IPC ----------
echo "========== Step 2: 汇总 DYMPL IPC 结果 =========="
RESULTS_DIR="$PROJECT_ROOT/results/$LABEL"
SUMMARY="$RESULTS_DIR/summary.tsv"

if [ -d "$RESULTS_DIR" ]; then
    python3 "$SCRIPT_DIR/summarize_ipc.py" \
        --results "$RESULTS_DIR" \
        --manifest "$PROJECT_ROOT/benchmarks_selected.tsv" \
        --out "$SUMMARY" \
        --verbose
    echo ""
    echo "[DONE] Summary written to $SUMMARY"
    echo ""
else
    echo "[ERROR] Results directory not found: $RESULTS_DIR"
    exit 1
fi

# ---------- Step 3: 与各基线对比 ----------
if [ ${#BASELINES[@]} -gt 0 ]; then
    echo "========== Step 3: 与各基线对比 =========="
    echo ""

    for baseline in "${BASELINES[@]}"; do
        COMPARE_OUT="$PROJECT_ROOT/results/compare_${baseline}_vs_${LABEL}.tsv"
        echo "--- $baseline vs $LABEL ---"

        python3 "$SCRIPT_DIR/compare_ipc.py" \
            --a "results/$baseline" \
            --b "results/$LABEL" \
            --a-label "$baseline" \
            --b-label "$LABEL" \
            --out "$COMPARE_OUT"

        echo "  -> $COMPARE_OUT"

        # 打印 GEOMEAN 和各 benchmark speedup
        echo ""
        echo "  Per-benchmark speedup ($LABEL / $baseline):"
        # 打印表头和数据（跳过 header 行）
        awk -F'\t' 'NR==1 { printf "    %-45s %s\n", $1, $NF }
                    NR>1  { printf "    %-45s %s\n", $1, $NF }' "$COMPARE_OUT"
        echo ""
    done
else
    echo "[SKIP] No baseline results found for comparison."
    echo "       Run other policies first (open_page, smart_close, GS, oracle) to enable comparison."
fi

echo "=========================================="
echo "  All done!"
echo "=========================================="
