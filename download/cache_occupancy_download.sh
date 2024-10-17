#!/bin/bash
cd "$(dirname "$0")"

item_list=("catp-l2" "triangel-l2")
remote_list=("epyc2")

## Clear Old Data
for i in ${item_list[@]}
do
    rm -r ../result/${i} 
    mkdir ../result/${i}
done

## Download From Remote
remote_path=/home/xuefeng/workspace/pt-pref-wjl/batch_run/result_demand/
for i in ${remote_list[@]}
do
    for j in ${item_list[@]}
    do
        scp ${i}:${remote_path}*-${j}/${j}_*.log ../result/${j}/
    done
done

## Generate Json File
result_path=../plotting/1.intro.cache_occupancy/
#### SpeedUp
rm ${result_path}result/catp-l2.json
../tools/collect_cache_occupancy.py ../result/catp-l2/ ${result_path}result/catp-l2.json
rm ${result_path}result/triangel-l2.json
../tools/collect_metadata_occupancy.py ../result/triangel-l2/ ${result_path}result/triangel-l2.json

cd ${result_path} && ./cal_cache_occupancy.py && cd -
