#!/usr/bin/python3

import sys
import json
import numpy as np
sys.path.append("../utils_py/")
from rename import rename

file_list = ["ipcp-l1", "berti-l1", "la864-l1"]

weights = json.load(open("../../batch_run/task_list/weight.json"))

result = {}
for f_name in file_list:
    print("result/"+f_name+".json")
    file = json.load(open("./result/"+f_name+".json"))

    result[f_name] = {}
    for key, value in file.items():
        sp_key = key.split('_')
        weight = weights[key]
        test_name = rename("_".join(sp_key[:-1]))

        tmp_dict = {"L1D_pref_miss": 0,
                    "L1D_pref_total_num": 0,
                    "L1D_pref_useless_num": 0,
                    "L1D_pref_late_num": 0,
                    "L1D_origin_miss": 0,
                    }
        for sub_key, sub_value in value.items():
            tmp_dict[sub_key] += weight*sub_value
        if test_name in result[f_name].keys():
            for sub_key, sub_value in tmp_dict.items():
                result[f_name][test_name][sub_key] += sub_value
        else:
            result[f_name][test_name] = tmp_dict

for key, value in result.items():
    gcc_point = 0
    gcc_sum = {}
    gcc_sum['L1D_pref_miss'] = 0
    gcc_sum['L1D_pref_total_num'] = 0
    gcc_sum['L1D_pref_useless_num'] = 0
    gcc_sum['L1D_pref_late_num'] = 0
    gcc_sum['L1D_origin_miss'] = 0
    for sub_key, sub_value in value.items():
        if sub_key[0:10] == "SPEC06.gcc":
            gcc_point += 1
            gcc_sum['L1D_pref_miss'] += sub_value['L1D_pref_miss']
            gcc_sum['L1D_pref_total_num'] += sub_value['L1D_pref_total_num']
            gcc_sum['L1D_pref_useless_num'] += sub_value['L1D_pref_useless_num']
            gcc_sum['L1D_pref_late_num'] += sub_value['L1D_pref_late_num']
            gcc_sum['L1D_origin_miss'] += sub_value['L1D_origin_miss']
    result[key]["SPEC06.gcc"] = {}
    result[key]["SPEC06.gcc"]['L1D_pref_miss'] = gcc_sum["L1D_pref_miss"] / gcc_point
    result[key]["SPEC06.gcc"]['L1D_pref_total_num'] = gcc_sum["L1D_pref_total_num"] / gcc_point
    result[key]["SPEC06.gcc"]['L1D_pref_useless_num'] = gcc_sum["L1D_pref_useless_num"] / gcc_point
    result[key]["SPEC06.gcc"]['L1D_pref_late_num'] = gcc_sum["L1D_pref_late_num"] / gcc_point
    result[key]["SPEC06.gcc"]['L1D_origin_miss'] = gcc_sum["L1D_origin_miss"] / gcc_point

for key, value in result.items():
    gcc_point = 0
    gcc_sum = {}
    gcc_sum['L1D_pref_miss'] = 0
    gcc_sum['L1D_pref_total_num'] = 0
    gcc_sum['L1D_pref_useless_num'] = 0
    gcc_sum['L1D_pref_late_num'] = 0
    gcc_sum['L1D_origin_miss'] = 0
    for sub_key, sub_value in value.items():
        if sub_key[0:10] == "SPEC17.gcc":
            gcc_point += 1
            gcc_sum['L1D_pref_miss'] += sub_value['L1D_pref_miss']
            gcc_sum['L1D_pref_total_num'] += sub_value['L1D_pref_total_num']
            gcc_sum['L1D_pref_useless_num'] += sub_value['L1D_pref_useless_num']
            gcc_sum['L1D_pref_late_num'] += sub_value['L1D_pref_late_num']
            gcc_sum['L1D_origin_miss'] += sub_value['L1D_origin_miss']
    result[key]["SPEC17.gcc"] = {}
    result[key]["SPEC17.gcc"]['L1D_pref_miss'] = gcc_sum["L1D_pref_miss"] / gcc_point
    result[key]["SPEC17.gcc"]['L1D_pref_total_num'] = gcc_sum["L1D_pref_total_num"] / gcc_point
    result[key]["SPEC17.gcc"]['L1D_pref_useless_num'] = gcc_sum["L1D_pref_useless_num"] / gcc_point
    result[key]["SPEC17.gcc"]['L1D_pref_late_num'] = gcc_sum["L1D_pref_late_num"] / gcc_point
    result[key]["SPEC17.gcc"]['L1D_origin_miss'] = gcc_sum["L1D_origin_miss"] / gcc_point

json.dump(result, open("cov_acc_time.json", "w"), indent=True , ensure_ascii=False)
print("Cov, Acc & Timeliness Calculation Success!\n")