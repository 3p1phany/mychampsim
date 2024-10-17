#!/usr/bin/python3

import os
import sys
import json
import numpy as np
sys.path.append("../utils_py/")
from rename import rename

degree = ["1", "2", "4", "8", "16"]
pref_list = ["misb", "triage-l1", "triangel", "cmc"]

file_list = []
for pref in pref_list:
    for deg in degree:
        file_list.append(pref+"-degree-"+deg)

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
for pref in pref_list:
    result[pref] = {}
    for deg in degree:
        result[pref][deg] = 0

for key, value in raw_data.items():
    pref = key.split("-degree-")[0]
    degree = key.split("-degree-")[1]

    result[pref][degree] = {}
    for sub_key, sub_value in value.items():
        weight_array = np.array(sub_value["weight"])
        ipc_array = np.array(sub_value["ipc"])
        ipc = 1 / (np.sum(weight_array * (1 / ipc_array)))
        result[pref][degree][sub_key] = ipc

json.dump(result, open("speedup.json", "w"), indent=True , ensure_ascii=False)
print("SpeedUp Calculation Success!\n")