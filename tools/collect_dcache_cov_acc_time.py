#!/usr/bin/python3

import os
import sys
import json

import myutil

data_dir = sys.argv[1]
output_fname = sys.argv[2]

myutil.begin_print(data_dir, "COV & ACC")

if data_dir[-1] == "/":
    no_dir = "/".join(data_dir[:-1].split("/")[:-1])+'/no/'
else:
    no_dir = "/".join(data_dir.split("/")[:-1])+'/no/'

result = {}
for item in os.listdir(data_dir):
    name = "_".join(item.split('.')[0].split("_")[1:])
    file = open(data_dir+item)
    dic = {}
    for line in file.readlines():
        line_sp = line.split()
        if line.find("cpu0_L1D PREFETCH  REQUESTED:") != -1:
            dic["L1D_pref_total_num"] = int(line_sp[8])
            dic["L1D_pref_useless_num"] = int(line_sp[12])
            dic["L1D_pref_late_num"] = int(line_sp[14])
        if line.find("cpu0_L1D LOAD      ACCESS:") != -1:
            dic["L1D_pref_miss"] = int(line_sp[-1])
    result[name] = dic
    file.close()
    
for item in os.listdir(no_dir):
    name = "_".join(item.split('.')[0].split("_")[1:])
    file = open(no_dir+item)
    for line in file.readlines():
        line_sp = line.split()
        if line.find("cpu0_L1D LOAD      ACCESS:") != -1:
            result[name]["L1D_origin_miss"] = int(line_sp[-1])
    file.close()

sorted_dict = dict(sorted(result.items(), key=lambda x:x[0].lower()))

if sorted_dict:
    json.dump(sorted_dict, open(output_fname, "w+"), indent=True , ensure_ascii=False)
else:
    print(f"ERROR: Can Not find Information")

print("Success!")