#!/usr/bin/python3

import os
import sys
import json

import myutil

data_dir = sys.argv[1]
output_fname = sys.argv[2]

result = {}

myutil.begin_print(data_dir, "Cache Occupancy")

for item in os.listdir(data_dir):
    name = "_".join(item.split('.')[0].split("_")[1:])
    file = open(data_dir+item)
    dic = {}
    found = False
    for line in file.readlines():
        line_sp = line.split()
        if(line.find("cache_cnt: ") != -1):
            if dic.get("data") == None:
                dic["data"] = 0
                data_sum = 0
                for i in range(1,len(line_sp)):
                    data_sum += int(line_sp[i])
                cur_sum = 0
                for i in range(1,len(line_sp)):
                    if cur_sum < data_sum * 0.75:
                        dic["data"] += 1
                        cur_sum += int(line_sp[i])
            else:
                data_sum = 0
                for i in range(1,len(line_sp)):
                    data_sum += int(line_sp[i])
                cur_sum = 0
                for i in range(1,len(line_sp)):
                    if cur_sum < data_sum * 0.75:
                        dic["data"] += 1
                        cur_sum += int(line_sp[i])
            found = True
        if(line.find("meta_cnt: ") != -1):
            if dic.get("metadata") == None:
                dic["metadata"] = 0
                for i in range(1,len(line_sp)):
                    if int(line_sp[i]) > 0:
                        dic["metadata"] += 1
            else:
                for i in range(1,len(line_sp)):
                    if int(line_sp[i]) > 0:
                        dic["metadata"] += 1
            found = True
    result[name] = dic
    file.close()
    if(found == False):
        print(f"Can not find cnts in {item}")

sorted_dict = dict(sorted(result.items(), key=lambda x:x[0].lower()))

if sorted_dict:
    json.dump(sorted_dict, open(output_fname, "w"), indent=True , ensure_ascii=False)
else:
    print(f"ERROR: Can Not find result.log in path \"{data_dir}\"")

print("Success!")
