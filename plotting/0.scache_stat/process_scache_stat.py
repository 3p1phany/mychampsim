#!/usr/bin/python3

import sys
import json as js
import numpy as np
import matplotlib.pyplot as plt

with open('scache_stat.json') as f :
    js_data = js.load(f)

interst_result = [
    name for name, value in js_data["no"].items()
    if value["LLC_load_miss_num"] > 100000 and name[7:11] != "gcc_" 
]
interst_result = sorted(interst_result)

bad_result = [
    name for name, value in js_data["no"].items()
    if value["LLC_load_miss_num"] <= 100000 or name[7:11] == "gcc_" 
]
bad_result = sorted(bad_result)

print("memory_tests = " + str(interst_result))
print("no_memory_tests = " + str(bad_result))
