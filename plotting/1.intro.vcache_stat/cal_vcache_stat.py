#!/usr/bin/python3

import os
import sys
import json
import numpy as np
sys.path.append("../utils_py/")
from rename import rename

file_list = ["ipcp", "berti"]

weights = json.load(open("../../batch_run/task_list/weight.json"))

result = {}
for f_name in file_list:
    print("result/"+f_name+".json")
    file = json.load(open("./result/"+f_name+".json"))

    result[f_name] = {}
    for key, value in file.items():
        sp_key = key.split('_')
        weight = weights[key]
        test_name = "_".join(sp_key[:-1])
        tmp_dict = {"L2C_load_hit_num": 0,
                    "L2C_load_miss_num": 0,
                    "L2C_pref_hit_num": 0,
                    "L2C_pref_miss_num": 0,
                    }
        for sub_key, sub_value in value.items():
            tmp_dict[sub_key] += weight*sub_value
        if test_name in result[f_name].keys():
            for sub_key, sub_value in tmp_dict.items():
                result[f_name][test_name][sub_key] += sub_value
        else:
            result[f_name][test_name] = tmp_dict

json.dump(result, open("vcache_stat.json", "w"), indent=True , ensure_ascii=False)
print("VCache Stat Calculation Success!\n")