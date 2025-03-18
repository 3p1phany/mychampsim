#!/usr/bin/python3

import os
import sys
import json
import numpy as np
sys.path.append("../utils_py/")

file_list=["ipcp","berti"]
weights = json.load(open("../../batch_run/task_list/weight.json"))

print("\nStarting Calculate Ideal SpeedUp...")

raw_data = {}
for f_name in file_list:
    print("result/"+f_name+".json")
    file = json.load(open("result/"+f_name+".json"))
    tmp_dict = {}
    for key, value in file.items():
        sp_key = key.split('_')
        weight = weights[key]
        name = "_".join(sp_key[:-1])

        if name in tmp_dict.keys():
            tmp_dict[name]["weight"].append(weight)
            tmp_dict[name]["speedup"].append(float(value["IPC2"])/float(value["IPC1"]))
        else:
            tmptmp_dict = {}
            tmptmp_dict["weight"] = [weight]
            tmptmp_dict["speedup"] = [float(value["IPC2"])/float(value["IPC1"])]
            tmp_dict[name] = tmptmp_dict
    raw_data[f_name] = tmp_dict

result = {}
for key, value in raw_data.items():
    result[key] = {}
    for sub_key, sub_value in value.items():
        weight_array = np.array(sub_value["weight"])
        speedup_array = np.array(sub_value["speedup"])
        speedup = 1 / (np.sum(weight_array * (1 / speedup_array)))
        result[key][sub_key] = speedup

json.dump(result, open("ideal_speedup.json", "w"), indent=True , ensure_ascii=False)
print("Ideal SpeedUp Calculation Success!\n")