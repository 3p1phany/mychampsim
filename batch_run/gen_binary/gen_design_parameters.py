#!/usr/bin/python3

import os
import sys

supported_list = ["cmc"]

target_list = sys.argv[1:]

#### Get Target Line Number
target_file_name = "../../inc/cmc.h"
target_file = open(target_file_name)
line_cnt = 0
for line in target_file.readlines():
    line_cnt += 1
    if(line.find("#define LoadReturn_SIZE ") != -1):
       ldret_line_num = line_cnt
    if(line.find("#define LoadIdentity_SIZE ") != -1):
       ldidt_line_num = line_cnt
    if(line.find("#define CMC_AGQ_SIZE ") != -1):
       agq_line_num = line_cnt
target_file.close()

#define LoadReturn_SIZE
#### Generate binary
for raw_target in target_list:
    target = raw_target.split("-")[0]
    if target not in supported_list:
        print("Unsupported target: " + target)
        exit(0)

    size = raw_target.split("-")[-1]

    command = []

    if "ldret" in raw_target:
        command.append(f"sed -i '{ldret_line_num},{ldret_line_num}c\#define LoadReturn_SIZE {size}' {target_file_name}")
    elif "ldidt" in raw_target:
        command.append(f"sed -i '{ldidt_line_num},{ldidt_line_num}c\#define LoadIdentity_SIZE {size}' {target_file_name}")
    elif "agq" in raw_target:
        command.append(f"sed -i '{agq_line_num},{agq_line_num}c\#define CMC_AGQ_SIZE {size}' {target_file_name}")
    else:
        print("Error : " + target)
        exit(0)

    command.append("cd ../.. && ./set_config.py " + target)
    command.append("cd ../.. && ./config.sh champsim_config.json")
    command.append("cd ../.. && make -j64")
    command.append("cd ../.. && mv bin/champsim bin/" + raw_target)
    for cmd in command:
        print(cmd)
        os.system(cmd)
    os.system("cd ../.. && git restore champsim_config.json")
    os.system("cd ../.. && git restore prefetcher/tyche/tyche.cc")
    os.system("cd ../.. && git restore prefetcher/triage_isr/triage_wrapper.h")
    os.system("cd ../.. && git restore inc/prefetch.h")
    os.system("cd ../.. && git restore inc/triage.h")
    os.system("cd ../.. && git restore inc/cmc.h")
    os.system("cd ../.. && git restore inc/isb.h")
    os.system("cd ../.. && git restore inc/dbp.h")