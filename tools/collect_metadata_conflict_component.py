#!/usr/bin/python3

import os
import sys
import json

import myutil

data_dir = sys.argv[1]
output_fname = sys.argv[2]
output_fname = output_fname.replace("-md-conflict", "-conflict-component")

result = {}

myutil.begin_print(data_dir, "Metadata Conflict Component")

for item in os.listdir(data_dir):
    name = item.split('.')[0]
    file = open(data_dir+item)
    dic = {}
    found = False
    total = 0
    for line in file.readlines():
        line_sp = line.split()
        if line.find("metadata_conf_change:") != -1:
            dic["metadata_conf_change"] = int(line_sp[-1])
        elif line.find("metadata_recursive_conf:") != -1:
            dic["metadata_recursive_conf"] = int(line_sp[-1])
            total += int(line_sp[-1])
            found = True
        elif line.find("metadata_data_conf:") != -1:
            dic["metadata_data_conf"] = int(line_sp[-1])
            total += int(line_sp[-1])
        elif line.find("metadata_other_conf:") != -1:
            dic["metadata_other_conf"] = int(line_sp[-1])
            total += int(line_sp[-1])
    if(total != dic["metadata_conf_change"]):
        print(f"ERROR: {item} metadata_conf_change: {dic['metadata_conf_change']} != metadata_recursive_conf + metadata_data_conf + metadata_other_conf: {total}")
    result[name] = dic
    file.close()
    if(found == False):
        print(f"Can not find metadata information in {item}")

sorted_dict = dict(sorted(result.items(), key=lambda x:x[0].lower()))

if sorted_dict:
    json.dump(sorted_dict, open(output_fname, "w"), indent=True , ensure_ascii=False)
else:
    print(f"ERROR: Can Not find information")

print("Success!")