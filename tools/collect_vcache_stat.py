#!/usr/bin/python3

import os
import sys
import json

import myutil

data_dir = sys.argv[1]
output_fname = sys.argv[2]

myutil.begin_print(data_dir, "VCache Stat")

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
        if line.find("cpu0_L2C LOAD      ACCESS") != -1:
            dic["L2C_load_hit_num"] = int(line_sp[5])
            dic["L2C_load_miss_num"] = int(line_sp[7])
        if line.find("cpu0_L2C PREFETCH  ACCESS") != -1:
            dic["L2C_pref_hit_num"] = int(line_sp[5])
            dic["L2C_pref_miss_num"] = int(line_sp[7])
    result[name] = dic
    file.close()

sorted_dict = dict(sorted(result.items(), key=lambda x:x[0].lower()))

if sorted_dict:
    json.dump(sorted_dict, open(output_fname, "w"), indent=True , ensure_ascii=False)
else:
    print(f"ERROR: Can Not find Information")

print("Success!")