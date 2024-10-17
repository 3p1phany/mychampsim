#!/bin/bash
cd "$(dirname "$0")"

degree_list=(2 4 12 16)
pref_list=("misb" "triage-l1" "triangel" "cmc")
remote_list=("epyc_9654")

## Clear Old Data
for pref in ${pref_list[@]}
do
    for degree in ${degree_list[@]}
    do
        rm -r ../result/${pref}-degree-${degree}
        mkdir ../result/${pref}-degree-${degree}
    done
done

## Download From Remote
remote_path=/home/xuefeng/workspace/pt-pref/batch_run/result/
for remote in ${remote_list[@]}
do
    for pref in ${pref_list[@]}
    do
        for degree in ${degree_list[@]}
        do
            name=${pref}-degree-${degree}
            scp -r ${remote}:${remote_path}*-${name}/${name}_*.log ../result/${name}/
        done
    done
done


## Generate Json File
dir_path=../plotting/6.eval.sensitivity/
for pref in ${pref_list[@]}
do
    ## Remote
    for degree in ${degree_list[@]}
    do
        name=${pref}-degree-${degree}
        rm ${dir_path}result/${name}.json
        ../tools/collect_ipc.py ../result/${name}/ ${dir_path}result/${name}.json
    done

    #### Base Line
    rm ../result/${pref}-degree-8
    ln -s  ../result/${pref} ../result/${pref}-degree-8
    rm ${dir_path}result/${pref}-degree-8.json

    ../tools/collect_ipc.py ../result/${pref}-degree-8/ ${dir_path}result/${pref}-degree-8.json
done

../tools/collect_ipc.py ../result/stride-l1/ ${dir_path}result/stride-l1.json

## Run Calculation Script
cd ${dir_path} && ./cal_degree_speedup.py && cd -
