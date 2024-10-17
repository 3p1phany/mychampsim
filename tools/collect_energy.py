#!/usr/bin/python3

import os
import sys
import json

import myutil

data_dir = sys.argv[1]
output_fname = sys.argv[2]

result = {}

myutil.begin_print(data_dir, "Access Number")

support_list = ["no", "stride-l1", "domino", "isb", "misb", "triage-l1", "triangel", "cmc",
                "misb-l2", "triage-l2", "triangel-l2"]
pref = data_dir.split("/")[-2]

if pref not in support_list:
    print("Error data_dir!")
    exit(1)

for item in os.listdir(data_dir):
    name = "_".join(item.split('.')[0].split("_")[1:])
    file = open(data_dir+item)
    dic = {
        "icache" :{"read": 0, "write": 0},
        "dcache" :{"read": 0, "write": 0},
        "l2cache":{"read": 0, "write": 0},
        "l3cache":{"read": 0, "write": 0},
        "mem":{"read": 0, "write": 0},
    }
    found = False
    for line in file.readlines():
        line_sp = line.split()

        if(line.find("cpu0_L1I LOAD      ACCESS:") != -1):
           dic["icache"]["read"] += int(line_sp[3])
        elif(line.find("cpu0_L1I RFO       ACCESS:") != -1):
           dic["icache"]["write"] += int(line_sp[3])
        elif(line.find("cpu0_L1I PREFETCH  ACCESS:") != -1):
           dic["icache"]["read"] += int(line_sp[3])
        if(line.find("cpu0_L1D LOAD      ACCESS:") != -1):
            dic["dcache"]["read"] += int(line_sp[3])
            found = True
        elif(line.find("cpu0_L1D RFO       ACCESS:") != -1):
            dic["dcache"]["write"] += int(line_sp[3])
        elif(line.find("cpu0_L1D PREFETCH  ACCESS:") != -1):
            dic["dcache"]["read"] += int(line_sp[3])
        elif(line.find("cpu0_L2C LOAD      ACCESS:") != -1):
            dic["l2cache"]["read"] += int(line_sp[3])
        elif(line.find("cpu0_L2C RFO       ACCESS:") != -1):
            dic["l2cache"]["write"] += int(line_sp[3])
        elif(line.find("cpu0_L2C PREFETCH  ACCESS:") != -1):
            dic["l2cache"]["read"] += int(line_sp[3])
        elif(line.find("LLC LOAD      ACCESS:") != -1):
            dic["l3cache"]["read"] += int(line_sp[3])
        elif(line.find("LLC RFO       ACCESS:") != -1):
            dic["l3cache"]["write"] += int(line_sp[3])
        elif(line.find("LLC PREFETCH  ACCESS:") != -1):
            dic["l3cache"]["read"] += int(line_sp[3])
        elif(line.find("ROW_BUFFER_MISS:") != -1):
            dic["mem"]["read"] += int(line_sp[-1])
        elif(line.find("WQ ROW_BUFFER_HIT:") != -1):
            dic["mem"]["write"] += int(line_sp[-1])
        elif(line.find("metadata_read_num:") != -1):
            if pref == "cmc":
                dic["l2cache"]["read"] += int(line_sp[-1])
            elif pref == "triage-l1" or pref == "triangel":
                dic["l3cache"]["read"] += int(line_sp[-1])
            elif (pref == "domino" or pref == "isb" or pref == "misb") and int(line_sp[-1]) != 0:
                print("Error Read!")
                exit(1)
        elif(line.find("metadata_write_num:") != -1):
            if pref == "cmc":
                dic["l2cache"]["write"] += int(line_sp[-1])
            elif pref == "triage-l1" or pref == "triangel":
                dic["l3cache"]["write"] += int(line_sp[-1])
            elif (pref == "domino" or pref == "isb" or pref == "misb") and int(line_sp[-1]) != 0:
                print("Error Write!")
                exit(1)

    result[name] = dic
    file.close()
    if(found == False):
        print(f"Can not find Info in {item}")

sorted_dict = dict(sorted(result.items(), key=lambda x:x[0].lower()))

if sorted_dict:
    json.dump(sorted_dict, open(output_fname, "w"), indent=True , ensure_ascii=False)
else:
    print(f"ERROR: Can Not find result.log in path \"{data_dir}\"")