#!/bin/bash
cd "$(dirname "$0")"

item_list=("cmc-cfg0" "cmc-cfg0-md-conflict"
           "cmc-cfg1" "cmc-cfg1-md-conflict"
           "cmc-cfg2" "cmc-cfg2-md-conflict"
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
contribution_path=../plotting/6.eval.component_contribution/
for i in ${item_list[@]}
do
    if [[ ${i} != *"-md-conflict" ]]; then
        #### SpeedUp
        rm ${contribution_path}result/${i}.json
        ../tools/collect_ipc.py ../result/${i}/ ${contribution_path}result/${i}.json
    else
        #### Metadata Storage & Conflict
        storage_name=$(echo "${i}" | sed 's/-md-conflict/-metadata-storage/')
        conflict_name=$(echo "${i}" | sed 's/-md-conflict/-metadata-conflict/')
        rm ${contribution_path}result/${storage_name}.json
        rm ${contribution_path}result/${conflict_name}.json
        ../tools/collect_metadata_storage.py ../result/${i}/ ${contribution_path}result/${storage_name}.json
        ../tools/collect_metadata_conflict.py ../result/${i}/ ${contribution_path}result/${conflict_name}.json
    fi
done

../tools/collect_ipc.py ../result/stride-l1/ ${contribution_path}result/stride-l1.json
../tools/collect_ipc.py ../result/cmc/ ${contribution_path}result/cmc.json
../tools/collect_metadata_storage.py ../result/cmc-md-conflict/ ${contribution_path}result/cmc-metadata-storage.json
../tools/collect_metadata_conflict.py ../result/cmc-md-conflict/ ${contribution_path}result/cmc-metadata-conflict.json

## Run Calculation Script
cd ${contribution_path} && ./cal_component_contribution.py && cd -