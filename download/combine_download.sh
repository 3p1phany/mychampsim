#!/bin/bash
cd "$(dirname "$0")"

item_list=("domino-l2" "cmc-domino" "isb-l2" "cmc-isb" "misb-l2" "cmc-misb" "triage-l2" "cmc-triage")
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
combine_path=../plotting/6.eval.combine/result/
for i in ${item_list[@]}
do
    #### Combine
    rm ${combine_path}${i}.json
    ../tools/collect_ipc.py ../result/${i}/ ${combine_path}${i}.json
done

## Run Calculation Script
cd ../plotting/6.eval.combine/ && ./cal_combine_speedup.py && cd -