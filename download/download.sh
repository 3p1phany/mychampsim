#!/bin/bash
cd "$(dirname "$0")"

item_list=("no" "la864" "AA" "bop" "spp" "AidOP" "triage-l2" "triangel-l2" "AdaTP")
remote_list=("epyc2")
remote_path=/home/xuefeng/workspace/champsim-la/batch_run/result/

## Clear Old Data
for i in ${item_list[@]}
do
    rm -r ../result/${i} 
    mkdir ../result/${i}
done

## Download From Remote
for i in ${remote_list[@]}
do
    for j in ${item_list[@]}
    do
        scp ${i}:${remote_path}*-${j}/${j}_*.log ../result/${j}/
    done
done
