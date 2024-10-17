#!/bin/bash
cd "$(dirname "$0")"

item_list=("triage-l2" "triangel-l2" "catp-l2")
remote_list=("epyc2")

## Clear Old Data
for i in ${item_list[@]}
do
    rm -r ../result/${i} 
    mkdir ../result/${i}
done

## Download From Remote
remote_path=/home/xuefeng/workspace/pt-pref-wjl/batch_run/result_trigger/
for i in ${remote_list[@]}
do
    for j in ${item_list[@]}
    do
        scp ${i}:${remote_path}*-${j}/${j}_*.log ../result/${j}/
    done
done

## Generate Json File
metadata_occupancy_path=../plotting/4.result.metadata_occupancy/

for i in ${item_list[@]}
do
    rm ${metadata_occupancy_path}result/${i}.json
    ../tools/collect_metadata_occupancy.py ../result/${i}/ ${metadata_occupancy_path}result/${i}.json
done

## Run Calculation Script
cd ${metadata_occupancy_path} && ./cal_metadata_occupancy.py && cd -