#!/bin/bash
cd "$(dirname "$0")"

item_list=("cmc-ldret-8" "cmc-ldret-16" "cmc-ldret-48" "cmc-ldret-64"
           "cmc-ldidt-4" "cmc-ldidt-8"  "cmc-ldidt-24" "cmc-ldidt-32"
          )
remote_list=("epyc_9654")

## Clear Old Data
for i in ${item_list[@]}
do
    rm -r ../result/${i} 
    mkdir ../result/${i}
done

## Download From Remote
remote_path=/home/xuefeng/workspace/pt-pref/batch_run/result/
for i in ${remote_list[@]}
do
    for j in ${item_list[@]}
    do
        scp ${i}:${remote_path}*-${j}/${j}_*.log ../result/${j}/
    done
done

## Generate Json File
design_parameter_path=../plotting/6.eval.design_parameter/
for i in ${item_list[@]}
do
    #### SpeedUp
    rm ${design_parameter_path}result/${i}.json
    ../tools/collect_ipc.py ../result/${i}/ ${design_parameter_path}result/${i}.json
done

#### Base Line
rm ../result/stride-l1.json
rm ../result/cmc-ldret-32.json
rm ../result/cmc-ldidt-16.json

../tools/collect_ipc.py ../result/stride-l1/ ${design_parameter_path}result/stride-l1.json
../tools/collect_ipc.py ../result/cmc/ ${design_parameter_path}result/cmc-ldret-32.json
../tools/collect_ipc.py ../result/cmc/ ${design_parameter_path}result/cmc-ldidt-16.json

## Run Calculation Script
cd ${design_parameter_path} && ./cal_design_parameter_speedup.py && cd -