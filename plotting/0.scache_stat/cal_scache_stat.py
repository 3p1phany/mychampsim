#!/usr/bin/python3

import sys
import json
sys.path.append("../utils_py/")
from rename import rename

file_list = ["no"]

weights = json.load(open("../../batch_run/task_list/weight.json"))

result = {}
for f_name in file_list:
    print("result/"+f_name+".json")
    file = json.load(open("./result/"+f_name+".json"))

    result[f_name] = {}
    for key, value in file.items():
        sp_key = key.split('_')
        weight = weights[key]
        test_name = rename("_".join(sp_key[:-1]))
        tmp_dict = {"LLC_load_miss_num": 0}
        for sub_key, sub_value in value.items():
            tmp_dict[sub_key] += weight*sub_value
        if test_name in result[f_name].keys():
            for sub_key, sub_value in tmp_dict.items():
                result[f_name][test_name][sub_key] += sub_value
        else:
            result[f_name][test_name] = tmp_dict

for key, value in result.items():
    gcc_point = 0
    gcc_sum = 0
    for sub_key, sub_value in value.items():
        if sub_key[0:10] == "SPEC06.gcc":
            gcc_point += 1
            gcc_sum += sub_value['LLC_load_miss_num']
    result[key]["SPEC06.gcc"] = {}
    result[key]["SPEC06.gcc"]['LLC_load_miss_num'] = gcc_sum / gcc_point

for key, value in result.items():
    gcc_point = 0
    gcc_sum = 0
    for sub_key, sub_value in value.items():
        if sub_key[0:10] == "SPEC17.gcc":
            gcc_point += 1
            gcc_sum += sub_value['LLC_load_miss_num']
    result[key]["SPEC17.gcc"] = {}
    result[key]["SPEC17.gcc"]['LLC_load_miss_num'] = gcc_sum / gcc_point


json.dump(result, open("scache_stat.json", "w+"), indent=True , ensure_ascii=False)
print("SCache Stat Calculation Success!\n")