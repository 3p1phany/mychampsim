set -x
#./config.sh champsim_config_8C.json
./config.sh champsim_config.json
make -j
time bash scripts/run_benchmarks.sh
