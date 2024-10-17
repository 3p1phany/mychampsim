#!/usr/bin/python3

import os
import sys
import json
import numpy as np
sys.path.append("../utils_py/")
from rename import rename

file_list = ["domino-l2", "cmc-domino", 
             "isb-l2", "cmc-isb", 
             "misb-l2", "cmc-misb", 
             "triage-l2", "cmc-triage"
            ]

item_list = []
for i in range(len(file_list)):
    if i % 2 == 0:
        item = file_list[i].split("-")[0]
    else:
        assert file_list[i].endswith("-"+item), "Item Error!"
        item_list.append(item)


weights = json.load(open("../../batch_run/task_list/weight.json"))

print("\nStarting Calculate Combine SpeedUp...")


## Calculate IPC
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

ipc_result = {}
for key, value in raw_data.items():
    ipc_result[key] = {}
    for sub_key, sub_value in value.items():
        weight_array = np.array(sub_value["weight"])
        ipc_array = np.array(sub_value["ipc"])
        ipc = 1 / (np.sum(weight_array * (1 / ipc_array)))
        ipc_result[key][sub_key] = ipc


## Calculate Combine SpeedUp
result = {}

#### Insert Empty Base
result["base"] = {}
for key in ipc_result[file_list[0]].keys():
    result["base"][key] = 1

for item in item_list:
    l2_ipc = ipc_result[item+"-l2"]
    combine_ipc = ipc_result["cmc-"+item]
    
    speedup = {}
    for key, value in l2_ipc.items():
        speedup[key] = combine_ipc[key] / value
    result["cmc-"+item] = speedup

json.dump(result, open("combine_speedup.json", "w"), indent=True , ensure_ascii=False)
print("Combine SpeedUp Calculation Success!\n")