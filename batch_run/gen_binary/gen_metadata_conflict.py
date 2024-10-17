#!/usr/bin/python3

import os
import sys

supported_list = ["isb", "misb", "triage-l1", "triangel", "cmc"]

target_list = sys.argv[1:]

for raw_target in target_list:
    if raw_target.endswith("-md-conflict"):
        target = raw_target.replace("-md-conflict", "")
    else:
        print("Input Error: " + target)
        break

    if target not in supported_list:
        print("Unsupported target: " + target)
        break

    command = []
    command.append("cd ../.. && sed -i 's/#define nCOLLECT_METADATA_CONFLICT/#define COLLECT_METADATA_CONFLICT/' ./inc/prefetch.h")
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