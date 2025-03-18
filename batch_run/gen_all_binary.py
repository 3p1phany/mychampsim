#!/usr/bin/python3

import os
import sys
import json

mode = sys.argv[1]

## Read old binary list
binary_file = open("binary.json", "r")
old_binary = json.load(binary_file)
binary_file.close()

## Binary List
single_binary_list = ["no", "stride", "ipcp", "berti", "la864", "bop", "triangel-l2", "triage-l2", "AidOP", "AdaTP", "AA"]

if mode == "single":
    command = "cd ./gen_binary && ./gen_single.py " + ' '.join(map(str, single_binary_list))
    print(command)
    os.system(command)
    append_data = single_binary_list
else:
    print("Unsupported mode: " + mode)
    sys.exit(1)

new_data = old_binary + append_data

## Write binary list to file
binary_file = open("binary.json", "w")
json.dump(new_data, binary_file, indent=True , ensure_ascii=False)
binary_file.close()
