#!/bin/bash
cd "$(dirname "$0")"

item_list=("isb-md-conflict" "misb-md-conflict" "triage-l1-md-conflict" "triangel-md-conflict" "cmc-md-conflict")
remote_list=("epyc_9654")

## Clear Old Data
for i in ${item_list[@]}
do
    rm -r ../result/${i}/
    mkdir ../result/${i}/
done

## Download From Remote
remote_path=/home/xuefeng/workspace/pt-pref/batch_run/result/
for i in ${remote_list[@]}
do
    for j in ${item_list[@]}
    do
        scp ${i}:${remote_path}*-${j}/${j}_* ../result/${j}/
    done
done

## Generate Json File
storage_dir_path=../plotting/6.eval.metadata_storage/
conflict_dir_path=../plotting/6.eval.metadata_conflict/
component_dir_path=../plotting/3.anal.metadata_component/
for i in ${item_list[@]}
do
    #### Metadata Storage & Conflict
    if [ $i == "triage-l1-md-conflict" ] || [ $i == "triangel-md-conflict" ] || [ $i == "cmc-md-conflict" ]; then
        ../tools/collect_metadata_storage.py ../result/${i}/ ${storage_dir_path}result/${i}.json
    fi
    ../tools/collect_metadata_conflict.py ../result/${i}/ ${conflict_dir_path}result/${i}.json

    #### Metadata Storage & Conflict Component
    if [ $i == "triage-l1-md-conflict" ] || [ $i == "triangel-md-conflict" ]; then
        ../tools/collect_metadata_conflict_component.py ../result/${i}/ ${component_dir_path}result/${i}.json
        ../tools/collect_metadata_storage_component.py ../result/${i}/ ${component_dir_path}result/${i}.json
    fi
done

## Run Calculation Script
cd ${storage_dir_path} && ./cal_metadata_storage.py && cd -
cd ${conflict_dir_path} && ./cal_metadata_conflict.py && cd -
cd ${component_dir_path} && ./cal_metadata_component.py && cd -