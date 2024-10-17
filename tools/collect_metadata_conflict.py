#!/usr/bin/python3

import os
import sys
import json

import myutil

data_dir = sys.argv[1]
output_fname = sys.argv[2]
output_fname = output_fname.replace("-md-conflict", "-metadata-conflict")

result = {}

myutil.begin_print(data_dir, "Metadata Conflict")

for item in os.listdir(data_dir):
    name = item.split('.')[0]
    file = open(data_dir+item)
    dic = {}
    found = False
    for line in file.readlines():
        line_sp = line.split()
        if line.find("metadata_conf_change:") != -1:
            dic["metadata_conf_change"] = int(line_sp[-1])
        if line.find("metadata_conf_same:") != -1:
            dic["metadata_conf_same"] = int(line_sp[-1])
            found = True
    dic["metadata_num"] = dic["metadata_conf_change"] + dic["metadata_conf_same"]
    result[name] = dic
    file.close()
    if(found == False):
        print(f"Can not find metadata information in {item}")
    if(dic["metadata_num"] > 100000000000):
        print(f"ERROR: {item} metadata_num: {dic['metadata_num']}")

sorted_dict = dict(sorted(result.items(), key=lambda x:x[0].lower()))

if sorted_dict:
    json.dump(sorted_dict, open(output_fname, "w"), indent=True , ensure_ascii=False)
else:
    print(f"ERROR: Can Not find information")
