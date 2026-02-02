set -x
#./config.sh champsim_config_8C.json
cd ../dramsim3 && make clean && make -j
cd -
./config.sh champsim_config.json
make -j
time bash scripts/run_benchmarks.sh
