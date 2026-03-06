#!/usr/bin/env bash
# 一键运行 Intel Adaptive (INTAP) 实验
# 用法: bash scripts/run_intap.sh [label]
#   label 默认为 INTAP_1c
#
# 使用独立的 champsim_config_INTAP.json + DDR5_64GB_4ch_4800_INTAP.ini
# 不修改任何原始配置文件
set -euo pipefail

CHAMPSIM_DIR="$(cd "$(dirname "$0")/.." && pwd -P)"
PROJ_ROOT="$(cd "${CHAMPSIM_DIR}/.." && pwd -P)"
DRAMSIM3_DIR="${PROJ_ROOT}/dramsim3"

LABEL="${1:-INTAP_1c}"
JOBS="${JOBS:-128}"

echo "======================================"
echo " Intel Adaptive 一键实验"
echo " Label: ${LABEL}"
echo "======================================"

# ----- Step 1: 用 INTAP json 编译 ChampSim -----
echo ""
echo "[Step 1] 编译 ChampSim (champsim_config_INTAP.json) ..."
cd "${CHAMPSIM_DIR}"
python3 config.sh champsim_config_INTAP.json > /dev/null 2>&1
make -j8 2>&1 | tail -3

# ----- Step 2: 运行仿真 -----
echo ""
echo "[Step 2] 启动仿真 (label=${LABEL}, JOBS=${JOBS}) ..."
export LD_LIBRARY_PATH="${DRAMSIM3_DIR}:${LD_LIBRARY_PATH:-}"
echo -e "${LABEL}\ny" | DRY_RUN=0 JOBS="${JOBS}" scripts/run_benchmarks.sh

echo ""
echo "======================================"
echo " 实验完成: ${LABEL}"
echo " 结果目录: ${CHAMPSIM_DIR}/results/${LABEL}"
echo "======================================"
