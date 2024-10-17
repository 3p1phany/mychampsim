#!/usr/bin/python3

import os
import sys
import json
import numpy as np
sys.path.append("../utils_py/")
from rename import rename

file_list=["stride-l1","berti","ipcp","triage-l2","triangel-l2","catp-l2"]
weights = json.load(open("../../batch_run/task_list/weight.json"))

print("\nStarting Calculate SpeedUp...")

raw_data = {}
for f_name in file_list:
    print("result/"+f_name+".json")
    file = json.load(open("result/"+f_name+".json"))
    tmp_dict = {}
    for key, value in file.items():
        sp_key = key.split('_')
        weight = weights[key]
        name = rename("_".join(sp_key[:-1]))

        if name in tmp_dict.keys():
            tmp_dict[name]["weight"].append(weight)
            tmp_dict[name]["ipc"].append(float(value["IPC"]))
        else:
            tmptmp_dict = {}
            tmptmp_dict["weight"] = [weight]
            tmptmp_dict["ipc"] = [float(value["IPC"])]
            tmp_dict[name] = tmptmp_dict
    raw_data[f_name] = tmp_dict

result = {}
for key, value in raw_data.items():
    result[key] = {}
    for sub_key, sub_value in value.items():
        weight_array = np.array(sub_value["weight"])
        ipc_array = np.array(sub_value["ipc"])
        ipc = 1 / (np.sum(weight_array * (1 / ipc_array)))
        result[key][sub_key] = ipc

json.dump(result, open("speedup.json", "w"), indent=True , ensure_ascii=False)
print("SpeedUp Calculation Success!\n")