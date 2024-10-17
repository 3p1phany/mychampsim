#!/usr/bin/python3

import os
import sys
import json
sys.path.append("../utils_py/")
from rename import rename

pref_list = ["isb-metadata-conflict", 
             "misb-metadata-conflict", 
             "triage-l1-metadata-conflict", 
             "triangel-metadata-conflict", 
             "cmc-metadata-conflict"
             ]

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
print("Collect Finish!\n")