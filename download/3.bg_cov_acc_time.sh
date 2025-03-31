#!/bin/bash
cd "$(dirname "$0")"

item_list=("berti-l1" "ipcp-l1" "la864-l1")

## Generate Json File
cov_acc_path=../plotting/3.bg_cov_acc_time/

for i in ${item_list[@]}
do
    #### Coverage & Accuracy
    if [ $i != "no" ]; then
        rm ${cov_acc_path}result/${i}.json
        ../tools/collect_dcache_cov_acc_time.py ../result/${i}/ ${cov_acc_path}result/${i}.json
    fi
done

## Run Calculation Script
cd ${cov_acc_path} && ./cal_cov_acc_time.py && cd -
