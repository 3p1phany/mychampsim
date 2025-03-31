#!/usr/bin/python3

import sys
import json
import numpy as np
sys.path.append("../utils_py/")
from rename import rename

file_list=["la864","AidOP","AdaTP", "AA"]
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

for key, value in raw_data.items():
    gcc_point = 0
    gcc_ipc_sum = 0
    for sub_key, sub_value in value.items():
        if sub_key[0:10] == "SPEC06.gcc":
            gcc_point += 1
            gcc_ipc_sum += result[key][sub_key]
    result[key]["SPEC06.gcc"] = gcc_ipc_sum / gcc_point

for key, value in raw_data.items():
    gcc_point = 0
    gcc_ipc_sum = 0
    for sub_key, sub_value in value.items():
        if sub_key[0:10] == "SPEC17.gcc":
            gcc_point += 1
            gcc_ipc_sum += result[key][sub_key]
    result[key]["SPEC17.gcc"] = gcc_ipc_sum / gcc_point

json.dump(result, open("speedup.json", "w+"), indent=True , ensure_ascii=False)
print("SpeedUp Calculation Success!\n")