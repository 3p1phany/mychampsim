#!/usr/bin/python3

import os
import sys
import json
sys.path.append("../utils_py/")
from rename import rename

pref_list = ["triage-l2", "triangel-l2", "catp-l2"]

weights = json.load(open("../../batch_run/task_list/weight.json"))

result = {}
for pref in pref_list:
    json_data = json.load(open("result/"+pref+".json"))

    result_raw = {}
    for key, value in json_data.items():
        sp_key = key.split('_')
        w_name = "_".join(sp_key[1:])
        weight = weights[w_name]
        name = rename("_".join(sp_key[1:-1]))

        occupancy = weight*float(value["assoc"])
        if name in result_raw.keys():
            result_raw[name] += occupancy
        else:
            result_raw[name] = occupancy

    result[pref] = result_raw
    
json.dump(result, open("metadata_occupancy.json", "w"), indent=True , ensure_ascii=False)
print("Collect Finish!\n")