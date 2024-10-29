#!/usr/bin/python3

import os
import sys

supported_list = ["no", "stride", "stride-l1", "dbp", "cdp", "ipcp", "berti", "imp", "bop",
                  "gretch", "tyche", "domino", "isb", "misb", "triage-l1", "triangel-l1", "triangel-l2", "cmc", "catp-l1", "catp-l2",
                  "domino-l2", "cmc-domino", "isb-l2", "cmc-isb", "misb-l2", "cmc-misb", 
                  "triage-l2", "cmc-triage", "cmc-triangel"]

target_list = sys.argv[1:]

for target in target_list:
    if target not in supported_list:
        print("Unsupported target: " + target)
        break

    command = []
    command.append("cd ../.. && ./set_config.py " + target)
    command.append("cd ../.. && ./config.sh champsim_config.json")
    command.append("cd ../.. && make -j64")
    command.append("cd ../.. && mv bin/champsim bin/" + target)
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
