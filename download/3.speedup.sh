#!/bin/bash
cd "$(dirname "$0")"

item_list=("la864" "bop" "spp" "AidOP")

## Generate Json File
speedup_path=../plotting/3.speedup/
cov_acc_path=../plotting/3.cov_acc_time/

for i in ${item_list[@]}
do
    #### SpeedUp
    rm ${speedup_path}result/${i}.json
    ../tools/collect_ipc.py ../result/${i}/ ${speedup_path}result/${i}.json

    #### Coverage & Accuracy
    if [ $i != "no" ]; then
        rm ${cov_acc_path}result/${i}.json
        ../tools/collect_vcache_cov_acc_time.py ../result/${i}/ ${cov_acc_path}result/${i}.json
    fi
done

## Run Calculation Script
cd ${speedup_path} && ./cal_speedup.py && cd -
cd ${cov_acc_path} && ./cal_cov_acc_time.py && cd -