#!/usr/bin/env bash
# 一键构建并运行 GS queue-aware observability 实验
# 用法: TRACE_ROOT=/root/data/Trace/LA ./scripts/run_qaware_observation.sh
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd -P)"
CHAMPSIM_DIR="$(cd "$SCRIPT_DIR/.." && pwd -P)"
PROJECT_ROOT="$(cd "$CHAMPSIM_DIR/.." && pwd -P)"
DRAMSIM3_ROOT="${DRAMSIM3_ROOT:-$PROJECT_ROOT/dramsim3}"

# 可通过环境变量覆盖
TRACE_ROOT="${TRACE_ROOT:-/root/data/Trace/LA}"
MANIFEST="${MANIFEST:-$CHAMPSIM_DIR/benchmarks_selected.tsv}"
LABEL="${LABEL:-GS_qaware_obs}"
JOBS="${JOBS:-128}"
WARMUP="${WARMUP:-20000000}"
SIM="${SIM:-80000000}"
CONFIG_JSON="${CONFIG_JSON:-champsim_config.json}"

echo "============================================"
echo " GS Queue-Aware Observability Experiment"
echo "============================================"
echo "DRAMSIM3_ROOT: $DRAMSIM3_ROOT"
echo "CHAMPSIM_DIR:  $CHAMPSIM_DIR"
echo "TRACE_ROOT:    $TRACE_ROOT"
echo "MANIFEST:      $MANIFEST"
echo "LABEL:         $LABEL"
echo "JOBS:          $JOBS"
echo "CONFIG_JSON:   $CONFIG_JSON"
echo "============================================"

# ---------- 前置检查 ----------
if [[ ! -d "$TRACE_ROOT" ]]; then
    echo "[ERR] TRACE_ROOT 不存在: $TRACE_ROOT" >&2
    echo "请设置 TRACE_ROOT 指向 trace 根目录" >&2
    exit 1
fi
if [[ ! -f "$CHAMPSIM_DIR/$MANIFEST" && ! -f "$MANIFEST" ]]; then
    echo "[ERR] 找不到清单文件: $MANIFEST" >&2
    exit 1
fi

# ---------- Step 1: 编译 DRAMSim3 ----------
echo ""
echo "[1/4] 编译 DRAMSim3 ..."
cd "$DRAMSIM3_ROOT"
make clean 2>/dev/null || true
make -j8
echo "[1/4] DRAMSim3 编译完成"

# ---------- Step 2: 编译 ChampSim ----------
echo ""
echo "[2/4] 编译 ChampSim ..."
cd "$CHAMPSIM_DIR"
make clean 2>/dev/null || true
python3 config.sh "$CONFIG_JSON"
make -j8
echo "[2/4] ChampSim 编译完成"

# ---------- Step 3: 验证 GS 配置 ----------
echo ""
echo "[3/4] 验证 DRAM 配置使用 GS 策略 ..."
DRAM_INI=$(python3 -c "
import json
with open('$CONFIG_JSON') as f:
    cfg = json.load(f)
print(cfg.get('physical_memory', {}).get('dramsim3_ini', 'NOT_FOUND'))
")
echo "  DRAM config: $DRAM_INI"

if [[ -f "$DRAM_INI" ]]; then
    ROW_BUF=$(grep -i "row_buf_policy" "$DRAM_INI" | head -1 || echo "NOT_FOUND")
    echo "  $ROW_BUF"
    if ! echo "$ROW_BUF" | grep -qi "GS"; then
        echo "[WARN] DRAM 配置未使用 GS 策略，qaware 计数器将全为 0" >&2
        echo "请修改 $DRAM_INI 中 row_buf_policy = GS" >&2
        exit 1
    fi
else
    echo "[WARN] 未找到 DRAM 配置文件: $DRAM_INI" >&2
fi

# ---------- Step 4: 运行 benchmark ----------
echo ""
echo "[4/4] 启动 benchmark 运行 ..."

export LD_LIBRARY_PATH="$DRAMSIM3_ROOT:${LD_LIBRARY_PATH:-}"
export BINARY="$CHAMPSIM_DIR/bin/champsim"
export MANIFEST
export RESULTS_ROOT="$CHAMPSIM_DIR/results"
export JOBS
export WARMUP
export SIM
export EXTRA_ARGS="-loongarch"

# 使用非交互模式：通过管道传入 label 和确认
echo -e "${LABEL}\ny" | bash "$CHAMPSIM_DIR/scripts/run_benchmarks.sh"

echo ""
echo "============================================"
echo " 运行完成！"
echo " 结果目录: $CHAMPSIM_DIR/results/$LABEL"
echo ""
echo " 下一步: 运行分析脚本提取 qaware 计数器"
echo "   python3 scripts/summarize_qaware.py --results results/$LABEL --manifest $MANIFEST"
echo "============================================"
