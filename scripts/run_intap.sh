#!/usr/bin/env bash
# 一键运行 Intel Adaptive (INTAP) 实验
# 用法: bash scripts/run_intap.sh [label]
#   label 默认为 INTAP_1c
#
# 流程:
#   1. 将 champsim_config.json 中的 dramsim3_config 指向 INTAP ini
#   2. 重新 config.sh + make (ChampSim)
#   3. 运行全部 benchmark 切片
#   4. 恢复 champsim_config.json
set -euo pipefail

CHAMPSIM_DIR="$(cd "$(dirname "$0")/.." && pwd -P)"
PROJ_ROOT="$(cd "${CHAMPSIM_DIR}/.." && pwd -P)"
CONFIG_JSON="${CHAMPSIM_DIR}/champsim_config.json"
INTAP_INI="${CHAMPSIM_DIR}/dramsim3_configs/DDR5_64GB_4ch_4800_INTAP.ini"
DRAMSIM3_DIR="${PROJ_ROOT}/dramsim3"

LABEL="${1:-INTAP_1c}"
JOBS="${JOBS:-128}"

echo "======================================"
echo " Intel Adaptive 一键实验"
echo " Label: ${LABEL}"
echo "======================================"

# ----- Step 1: 备份 champsim_config.json，切换 ini -----
echo ""
echo "[Step 1] 切换 dramsim3_config → INTAP ini ..."
cp "${CONFIG_JSON}" "${CONFIG_JSON}.bak"
sed -i "s|\"dramsim3_config\":.*|\"dramsim3_config\": \"${INTAP_INI}\",|" "${CONFIG_JSON}"
grep dramsim3_config "${CONFIG_JSON}"

# 确保退出时恢复配置
restore_config() {
    if [[ -f "${CONFIG_JSON}.bak" ]]; then
        mv -f "${CONFIG_JSON}.bak" "${CONFIG_JSON}"
        echo "[CLEANUP] champsim_config.json 已恢复"
    fi
}
trap restore_config EXIT

# ----- Step 2: 编译 -----
echo ""
echo "[Step 2] 编译 ChampSim (config.sh + make) ..."
cd "${CHAMPSIM_DIR}"
python3 config.sh champsim_config.json > /dev/null 2>&1
make -j8 2>&1 | tail -3

# ----- Step 3: 运行仿真 -----
echo ""
echo "[Step 3] 启动仿真 (label=${LABEL}, JOBS=${JOBS}) ..."
export LD_LIBRARY_PATH="${DRAMSIM3_DIR}:${LD_LIBRARY_PATH:-}"
cd "${CHAMPSIM_DIR}"
echo -e "${LABEL}\ny" | DRY_RUN=0 JOBS="${JOBS}" scripts/run_benchmarks.sh

echo ""
echo "======================================"
echo " 实验完成: ${LABEL}"
echo " 结果目录: ${CHAMPSIM_DIR}/results/${LABEL}"
echo "======================================"
