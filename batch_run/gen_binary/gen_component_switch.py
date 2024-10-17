#!/usr/bin/python3

import os
import sys

# supported_list = ["cmc-cfg0", "cmc-cfg1", "cmc-cfg2", "cmc-cfg3", "cmc-cfg4"]
supported_list = ["cmc-cfg0", "cmc-cfg0-md-conflict",
                  "cmc-cfg1", "cmc-cfg1-md-conflict",
                  "cmc-cfg2", "cmc-cfg2-md-conflict",
                  ]

#        RECORD_ALL    RESPECTIVE_ADDR    PC_LOCALIZATION    DIRECT_INDEX    PREF_ONLY_DIRECT
# cfg0      T                T                  T                 T                 T           All disable
# cfg1      F                T                  T                 T                 T           Enable Filter
# cfg2      F                F                  T                 T                 T           Enable Base Address
# cfg3      F                F                  F                 T                 T           Enable Global Localizatin
# cfg4      F                F                  F                 F                 T           Enable Hash Index
# cmc       F                F                  F                 F                 F           Enable precomputation

target_list = sys.argv[1:]

for target in target_list:
    if target not in supported_list:
        print("Unsupported target: " + target)
        break

    command = []

    # if(target=="cmc-cfg0"):
    #     command.append("cd ../.. && sed -i 's/#define nCMC_RECORD_ALL/#define CMC_RECORD_ALL/' ./inc/cmc.h")

    # if(target=="cmc-cfg0" or target=="cmc-cfg1"):
    #     command.append("cd ../.. && sed -i 's/#define nCMC_RESPECTIVE_ADDR/#define CMC_RESPECTIVE_ADDR/' ./inc/cmc.h")

    # if(target=="cmc-cfg0" or target=="cmc-cfg1" or target=="cmc-cfg2"):
    #     command.append("cd ../.. && sed -i 's/#define nCMC_PC_LOCALIZATION/#define CMC_PC_LOCALIZATION/' ./inc/cmc.h")

    # if(target=="cmc-cfg0" or target=="cmc-cfg1" or target=="cmc-cfg2" or target=="cmc-cfg3"):
    #     command.append("cd ../.. && sed -i 's/#define nCMC_DIRECT_INDEX/#define CMC_DIRECT_INDEX/' ./inc/cmc.h")

    # if(target=="cmc-cfg0" or target=="cmc-cfg1" or target=="cmc-cfg2" or target=="cmc-cfg3" or target=="cmc-cfg4"):
    #     command.append("cd ../.. && sed -i 's/#define nCMC_PREF_ONLY_DIRECT/#define CMC_PREF_ONLY_DIRECT/' ./inc/cmc.h")

    pref = target.replace("-md-conflict", "")
    md_conflict = "-md-conflict" in target

    if(pref=="cmc-cfg0" or pref=="cmc-cfg1"):
        command.append("cd ../.. && sed -i 's/#define nCMC_RECORD_ALL/#define CMC_RECORD_ALL/' ./inc/cmc.h")

    if(pref=="cmc-cfg0" or pref=="cmc-cfg2"):
        command.append("cd ../.. && sed -i 's/#define nCMC_RESPECTIVE_ADDR/#define CMC_RESPECTIVE_ADDR/' ./inc/cmc.h")

    if(pref=="cmc-cfg0" or pref=="cmc-cfg2"):
        command.append("cd ../.. && sed -i 's/#define nCMC_PC_LOCALIZATION/#define CMC_PC_LOCALIZATION/' ./inc/cmc.h")

    if(pref=="cmc-cfg0" or pref=="cmc-cfg2"):
        command.append("cd ../.. && sed -i 's/#define nCMC_DIRECT_INDEX/#define CMC_DIRECT_INDEX/' ./inc/cmc.h")

    if(pref=="cmc-cfg0" or pref=="cmc-cfg2"):
        command.append("cd ../.. && sed -i 's/#define nCMC_PREF_ONLY_DIRECT/#define CMC_PREF_ONLY_DIRECT/' ./inc/cmc.h")


    if(md_conflict):
        command.append("cd ../.. && sed -i 's/#define nCOLLECT_METADATA_CONFLICT/#define COLLECT_METADATA_CONFLICT/' ./inc/prefetch.h")


    command.append("cd ../.. && ./set_config.py cmc")
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