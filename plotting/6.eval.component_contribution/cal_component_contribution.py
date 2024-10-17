#!/usr/bin/python3

import os
import sys
import json
import numpy as np
sys.path.append("../utils_py/")
from rename import rename

speedup_list  = ["stride-l1", "cmc-cfg0", "cmc-cfg1", "cmc-cfg2", "cmc"]
storage_list  = ["cmc-cfg0-metadata-storage", "cmc-cfg1-metadata-storage", "cmc-cfg2-metadata-storage", "cmc-metadata-storage"]
conflict_list = ["cmc-cfg0-metadata-conflict", "cmc-cfg1-metadata-conflict", "cmc-cfg2-metadata-conflict", "cmc-metadata-conflict"]
weights = json.load(open("../../batch_run/task_list/weight.json"))

print("\nStarting Calculate SpeedUp...")

#### Calculate SpeedUp
raw_data = {}
for f_name in speedup_list:
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



#### Calculate Metadata Storage
result = {}
for pref in storage_list:
    json_data = json.load(open("result/"+pref+".json"))

    result_raw = {}
    for key, value in json_data.items():
        if "health" in key:
            continue
        sp_key = key.split('_')
        w_name = "_".join(sp_key[1:])
        weight = weights[w_name]
        name = rename("_".join(sp_key[1:-1]))

        tmp_dict = {"total": 0,
                    "recursive": 0,
                    "data": 0,
                    "other": 0
                    }
        for sub_key, sub_value in value.items():
            tmp_dict[sub_key] += weight*sub_value
        if name in result_raw.keys():
            for sub_key, sub_value in tmp_dict.items():
                result_raw[name][sub_key] += sub_value
        else:
            result_raw[name] = tmp_dict

    result[pref] = {}
    for key, value in result_raw.items():
        result[pref][key] = value["total"]
        # if(value["total"] < 1000):
        #     result[pref][key] = 0
        # else:
        #     result[pref][key] = value["total"]

json.dump(result, open("metadata_storage.json", "w"), indent=True , ensure_ascii=False)


#### Calculate Metadata Conflict
result = {}
for pref in conflict_list:
    json_data = json.load(open("result/"+pref+".json"))

    result_raw = {}
    for key, value in json_data.items():
        sp_key = key.split('_')
        w_name = "_".join(sp_key[1:])
        weight = weights[w_name]
        name = rename("_".join(sp_key[1:-1]))

        tmp_dict = {"metadata_num": 0,
                    "metadata_conf_same": 0,
                    "metadata_conf_change": 0
                    }
        for sub_key, sub_value in value.items():
            tmp_dict[sub_key] += weight*sub_value
        if name in result_raw.keys():
            for sub_key, sub_value in tmp_dict.items():
                result_raw[name][sub_key] += sub_value
        else:
            result_raw[name] = tmp_dict

    # json.dump(result_raw, open(pref+"_metadata_conflict_raw.json", "w"), indent=True , ensure_ascii=False)
    
    result[pref] = {}
    for key, value in result_raw.items():
        if(value["metadata_num"] == 0):
            result[pref][key] = 0
        else:
            result[pref][key] = value["metadata_conf_change"] / value["metadata_num"]
    
json.dump(result, open("metadata_conflict.json", "w"), indent=True , ensure_ascii=False)


print("Copmponent Contribution Calculation Success!\n")