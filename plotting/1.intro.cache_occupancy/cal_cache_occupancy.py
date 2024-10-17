#!/usr/bin/python3

import os
import sys
import json
import numpy as np
sys.path.append("../utils_py/")

print("\nStarting Calculate Cache Occupancy...")

raw_data = {}
file1 = json.load(open("result/catp-l2.json"))
file2 = json.load(open("result/triangel-l2.json"))
result = {}
for key, value in file1.items():
    triangel_assoc = float(file2[key]["assoc"])

    result[key] = {}
    result[key]["data"] = (float(value["data"]))
    result[key]["metadata"] = (float(value["metadata"]))
    result[key]["assoc"] = (triangel_assoc)

json.dump(result, open("cache_occupancy.json", "w"), indent=True , ensure_ascii=False)
print("Cache Occupancy Calculation Success!\n")