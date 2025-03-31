#!/bin/bash
cd "$(dirname "$0")"

item_list=("no" "ipcp" "berti" "la864" "stride")
remote_list=("epyc2")
remote_path=/home/xuefeng/workspace/champsim-la/batch_run/result-l1/

## Clear Old Data
for i in ${item_list[@]}
do
    rm -r ../result/${i}-l1 
    mkdir ../result/${i}-l1
done

## Download From Remote
for i in ${remote_list[@]}
do
    for j in ${item_list[@]}
    do
        scp ${i}:${remote_path}*-${j}/${j}_*.log ../result/${j}-l1/
    done
done
