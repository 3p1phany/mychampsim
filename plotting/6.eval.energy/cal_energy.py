#!/usr/bin/python3

import os
import sys
import json
import numpy as np
sys.path.append("../utils_py/")
from rename import rename

energy_config = json.load(open("energy_config.json"))

file_list = ["no", "misb-l2", "triage-l2", "triangel-l2", "cmc"]
weights = json.load(open("../../batch_run/task_list/weight.json"))

raw_data = {}
for f_name in file_list:
    file = json.load(open("result/"+f_name+".json"))

    raw_data[f_name] = {}
    for key, value in file.items():
        sp_key = key.split('_')
        weight = weights[key]
        test_name = rename("_".join(sp_key[:-1]))

        tmp_dict = {
            "icache" :{"read": 0, "write": 0},
            "dcache" :{"read": 0, "write": 0},
            "l2cache":{"read": 0, "write": 0},
            "l3cache":{"read": 0, "write": 0},
            "mem":{"read": 0, "write": 0},
        }

        for sub_key, sub_value in value.items():
            for subsub_key, subsub_value in sub_value.items():
                tmp_dict[sub_key][subsub_key] += weight*subsub_value

        if test_name in raw_data[f_name].keys():
            for sub_key, sub_value in tmp_dict.items():
                for subsub_key, subsub_value in sub_value.items():
                    raw_data[f_name][test_name][sub_key][subsub_key] += subsub_value
        else:
            raw_data[f_name][test_name] = tmp_dict

json.dump(raw_data, open("memory_access.json", "w"), indent=True , ensure_ascii=False)

result = {}
for f_name in file_list:
    result[f_name] = {}
    for test_name, value in raw_data[f_name].items():
        power = 0
        for mem_level, sub_value in value.items():
            sub_power = 0
            for op, subsub_value in sub_value.items():
                sub_power += energy_config[mem_level][op]*subsub_value

            sub_value["power"] = sub_power
            power += sub_power
        result[f_name][test_name] = power

json.dump(result, open("energy.json", "w"), indent=True , ensure_ascii=False)
print("Energy Calculation Success!\n")