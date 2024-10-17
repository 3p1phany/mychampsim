#!/usr/bin/python3

import os
import sys
import json

import myutil

data_dir = sys.argv[1]
output_fname = sys.argv[2]
output_fname = output_fname.replace("-md-conflict", "-metadata-storage")

result = {}

myutil.begin_print(data_dir, "Metadata Storage")

for item in os.listdir(data_dir):
    name = item.split('.')[0]
    file = open(data_dir+item)
    dic = {}
    recursive_found = False
    data_found = False
    other_found = False
    total = 0
    for line in file.readlines():
        line_sp = line.split()
        if line.find("recursive entry:") != -1:
            dic["recursive"] = int(line_sp[-1])
            total += int(line_sp[-1])
            recursive_found = True
        elif line.find("data entry:") != -1:
            dic["data"] = int(line_sp[-1])
            total += int(line_sp[-1])
            data_found = True
        elif line.find("other entry:") != -1:
            dic["other"] = int(line_sp[-1])
            total += int(line_sp[-1])
            other_found = True

    if(recursive_found == False):
        print(f"ERROR: {name} can not find recursive entry")
        exit(1)
    if(data_found == False):
        dic["data"] = 0
    if(other_found == False):
        dic["other"] = 0
    dic["total"] = dic["recursive"] + dic["data"] + dic["other"]
    if(total != dic["total"]):
        print(f"ERROR: {name} total is not equal to sum of recursive, data and other")
        exit(1)
    result[name] = dic
    file.close()

sorted_dict = dict(sorted(result.items(), key=lambda x:x[0].lower()))

if sorted_dict:
    json.dump(sorted_dict, open(output_fname, "w"), indent=True , ensure_ascii=False)
else:
    print(f"ERROR: Can Not find information")
