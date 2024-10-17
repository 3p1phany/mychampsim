#!/bin/bash
cd "$(dirname "$0")"

item_list=("no" "stride-l1" "berti" "ipcp" "triage-l2" "triangel-l2" "catp-l2")
remote_list=("epyc2")

## Clear Old Data
for i in ${item_list[@]}
do
    rm -r ../result/${i} 
    mkdir ../result/${i}
done

## Download From Remote
remote_path=/home/xuefeng/workspace/pt-pref-wjl/batch_run/result_speed/
for i in ${remote_list[@]}
do
    for j in ${item_list[@]}
    do
        scp ${i}:${remote_path}*-${j}/${j}_*.log ../result/${j}/
    done
done

## Generate Json File
speedup_path=../plotting/4.result.speedup/
cov_acc_path=../plotting/4.result.cov_acc_time/

for i in ${item_list[@]}
do
    #### SpeedUp
    rm ${speedup_path}result/${i}.json
    ../tools/collect_ipc.py ../result/${i}/ ${speedup_path}result/${i}.json

    #### Coverage & Accuracy
    if [ $i != "no" ]; then
        rm ${cov_acc_path}result/${i}.json
        ../tools/collect_cov_acc_time.py ../result/${i}/ ${cov_acc_path}result/${i}.json
    fi
done

## Run Calculation Script
cd ${speedup_path} && ./cal_speedup.py && cd -
cd ${cov_acc_path} && ./cal_cov_acc_time.py && cd -