#!/usr/bin/python3

import os
import sys
import json

import myutil

origin_dir = sys.argv[1]
ideal_dir = sys.argv[2]
output_fname = sys.argv[3]

result = {}

myutil.begin_print(origin_dir, "IPC")
myutil.begin_print(ideal_dir, "IPC")

for item in os.listdir(origin_dir):
    name = "_".join(item.split('.')[0].split("_")[1:])
    file1 = open(origin_dir+item)
    file2 = open(ideal_dir+item)
    dic = {}
    found1 = False
    found2 = False
    for line in file1.readlines():
        line_sp = line.split()
        if(line.find("CPU 0 cumulative IPC: ") != -1):
            dic["IPC1"] = line_sp[4]
            found1 = True
    for line in file2.readlines():
        line_sp = line.split()
        if(line.find("CPU 0 cumulative IPC: ") != -1):
            dic["IPC2"] = line_sp[4]
            found2 = True
    result[name] = dic
    file1.close()
    file2.close()
    if(found1 and found2 == False):
        print(f"Can not find IPC in {item}")

sorted_dict = dict(sorted(result.items(), key=lambda x:x[0].lower()))

if sorted_dict:
    json.dump(sorted_dict, open(output_fname, "w"), indent=True , ensure_ascii=False)
else:
    print(f"ERROR: Can Not find result.log in path \"{origin_dir}\"")

print("Success!")
