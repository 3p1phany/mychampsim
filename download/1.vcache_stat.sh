#!/bin/bash
cd "$(dirname "$0")"

## Generate Json File
result_path=../plotting/1.vcache_stat/

rm ${result_path}result/la864.json
../tools/collect_vcache_stat.py ../result/la864-l1/ ${result_path}result/la864-l1.json

cd ${result_path} && ./cal_vcache_stat.py && cd -
