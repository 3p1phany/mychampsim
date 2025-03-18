#!/usr/bin/python3

import os
import sys

supported_list = ["no", "stride", "ipcp", "berti", "la864", "bop", "triangel-l2", "triage-l2", "AidOP", "AdaTP", "AA"]

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
    os.system("cd ../.. && git restore prefetcher/triage_isr/triage_wrapper.h")
    os.system("cd ../.. && git restore inc/prefetch.h")
    os.system("cd ../.. && git restore inc/triage.h")
    os.system("cd ../.. && git restore inc/aidop.h")
    os.system("cd ../.. && git restore inc/adatp.h")
    os.system("cd ../.. && git restore inc/berti.h")
