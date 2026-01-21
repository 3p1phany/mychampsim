set -x
rm ./test_run/ -rf
cd ../dramsim3 && make clean && make -j
cd ../champsim-la
make clean
./config.sh champsim_config.json
#./config.sh champsim_config_8C.json
make -j
mkdir -p test_run && cd test_run
time ../bin/champsim --warmup_instructions 20000000 --simulation_instructions 80000000 -loongarch /root/data/Trace/LA/spec06/mcf/ref/spec06_mcf_ref_225300000000.champsim.trace.xz  > output.log 2>&1
#time ../bin/champsim --warmup_instructions 2000 --simulation_instructions 8000 -loongarch /root/data/Trace/LA/spec06/mcf/ref/spec06_mcf_ref_225300000000.champsim.trace.xz  > output.log 2>&1
#time ../bin/champsim --warmup_instructions 20000000 --simulation_instructions 80000000 -loongarch /root/data/Trace/LA/crono/APSP/16384-32/crono_APSP_16384-32_57390000000.champsim.trace.xz  > output.log 2>&1
#time ../bin/champsim --warmup_instructions 20000 --simulation_instructions 80000 -loongarch /root/data/Trace/LA/crono/APSP/16384-32/crono_APSP_16384-32_57390000000.champsim.trace.xz > output.log  2>&1
