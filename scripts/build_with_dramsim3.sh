#!/usr/bin/env bash
# Build DRAMSim3 (clean) then build ChampSim using config.sh
set -euo pipefail

DRAMSIM3_ROOT="${DRAMSIM3_ROOT:-}"
CONFIG_JSON="${CONFIG_JSON:-champsim_config.json}"

if [[ -z "$DRAMSIM3_ROOT" ]]; then
  echo "请设置 DRAMSIM3_ROOT（DRAMSim3 仓库路径）" >&2
  exit 1
fi
if [[ ! -d "$DRAMSIM3_ROOT" ]]; then
  echo "DRAMSIM3_ROOT 不存在或不是目录: $DRAMSIM3_ROOT" >&2
  exit 1
fi
if [[ ! -f "$CONFIG_JSON" ]]; then
  echo "找不到配置文件: $CONFIG_JSON" >&2
  exit 1
fi

echo "[1/3] 编译 DRAMSim3: $DRAMSIM3_ROOT"
pushd "$DRAMSIM3_ROOT" >/dev/null
make clean
make -j
popd >/dev/null

echo "[2/3] 生成 Makefile/头文件"
python3 config.sh "$CONFIG_JSON"

echo "[3/3] 编译 ChampSim"
make -j
