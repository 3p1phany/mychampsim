#!/usr/bin/python3

import os
import sys
import json

import myutil

data_dir = sys.argv[1]
output_fname = sys.argv[2]

result = {}

myutil.begin_print(data_dir, "IPC")

for item in os.listdir(data_dir):
    name = "_".join(item.split('.')[0].split("_")[1:])
    file = open(data_dir+item)
    dic = {}
    found = False
    for line in file.readlines():
        line_sp = line.split()
        if(line.find("CPU 0 cumulative IPC: ") != -1):
            dic["IPC"] = line_sp[4]
            found = True
    result[name] = dic
    file.close()
    if(found == False):
        print(f"Can not find IPC in {item}")

sorted_dict = dict(sorted(result.items(), key=lambda x:x[0].lower()))

if sorted_dict:
    json.dump(sorted_dict, open(output_fname, "w"), indent=True , ensure_ascii=False)
else:
    print(f"ERROR: Can Not find result.log in path \"{data_dir}\"")

print("Success!")
