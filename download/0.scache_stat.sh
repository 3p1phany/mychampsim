#!/bin/bash
cd "$(dirname "$0")"

## Generate Json File
result_path=../plotting/0.scache_stat/

rm ${result_path}result/no.json
../tools/collect_scache_stat.py ../result/no/ ${result_path}result/no.json

cd ${result_path} && ./cal_scache_stat.py && cd -
