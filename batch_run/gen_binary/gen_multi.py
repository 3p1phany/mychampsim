#!/usr/bin/python3

import os
import sys

mode = sys.argv[1]
mode_list = ["homo", "hetero"]

if mode not in mode_list:
    print("Unsupported mode: " + mode)
    exit(1)

target_list = sys.argv[2:]
supported_list = ["stride-l1", "domino", "misb", "triage-l1", "triangel", "cmc"]

for target in target_list:
    if target not in supported_list:
        print("Unsupported target: " + target)
        break

    command = []
    command.append("cd ../.. && ./set_config_4c.py " + target)
    command.append("cd ../.. && ./config.sh champsim_config_4c.json")
    command.append("cd ../.. && make -j64")
    command.append(f"cd ../.. && mv bin/champsim bin/{target}-{mode}-4c")
    for cmd in command:
        print(cmd)
        os.system(cmd)
    os.system("cd ../.. && git restore champsim_config_4c.json")
    os.system("cd ../.. && git restore prefetcher/tyche/tyche.cc")
    os.system("cd ../.. && git restore prefetcher/triage_isr/triage_wrapper.h")
    os.system("cd ../.. && git restore inc/prefetch.h")
    os.system("cd ../.. && git restore inc/triage.h")
    os.system("cd ../.. && git restore inc/cmc.h")
    os.system("cd ../.. && git restore inc/isb.h")
    os.system("cd ../.. && git restore inc/dbp.h")