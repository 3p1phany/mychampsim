#!/usr/bin/python3

import os
import sys

type = sys.argv[1]

type_list = ["no", "stride", "ipcp", "berti", "la864", "bop", "spp", "triangel-l2", "triage-l2", "AidOP", "AdaTP", "AA"]

if(type not in type_list):
    print("Please enter correct type")
    exit(0)

config_fname = "champsim_config.json"
file = open(config_fname, 'r')
lines = file.readlines()

command = ""
## L1 Prefetcher
config_line = 71
if(lines[config_line-1].find("prefetcher") == -1):
    print(f"L1D prefetcher is not in line {config_line}")
    exit(1)

if(type == "no"):
    command = f"sed -i '{config_line},{config_line}c\        \"prefetcher\": \"no\",' {config_fname}"
elif(type == "stride"):
    command = f"sed -i '{config_line},{config_line}c\        \"prefetcher\": \"stride\",' {config_fname}"
elif(type == "berti"):
    command = f"sed -i '{config_line},{config_line}c\        \"prefetcher\": \"berti\",' {config_fname}"
elif(type == "ipcp"):
    command = f"sed -i '{config_line},{config_line}c\        \"prefetcher\": \"ipcp_l1d\",' {config_fname}"
elif(type=="la864" or type=="AA" or type == "AidOP" or type == "AdaTP" or type == "triage-l2" or type == "triangel-l2" or type=="bop" or type=="spp"):
    command = f"sed -i '{config_line},{config_line}c\        \"prefetcher\": \"la864\",' {config_fname}"
print(command)
os.system(command)

if(type == "berti"):
    command = f"sed -i 's/#define nENABLE_BERTI/#define ENABLE_BERTI/' inc/berti.h"
else:
    command = f"sed -i 's/#define ENABLE_BERTI/#define nENABLE_BERTI/' inc/berti.h"
print(command)
os.system(command)

if(type == "triage-l2"):
    command = f"sed -i 's/#define nENABLE_TRIAGE/#define ENABLE_TRIAGE/' inc/triage.h"
else:
    command = f"sed -i 's/#define ENABLE_TRIAGE/#define nENABLE_TRIAGE/' inc/triage.h"
print(command)
os.system(command)


if(type == "AA" or type == "AidOP"):
    command = f"sed -i 's/#define nENABLE_AidOP/#define ENABLE_AidOP/' inc/aidop.h"
else:
    command = f"sed -i 's/#define ENABLE_AidOP/#define nENABLE_AidOP/' inc/aidop.h"
print(command)
os.system(command)

if(type == "AA" or type == "AdaTP"):
    command = f"sed -i 's/#define nENABLE_AdaTP/#define ENABLE_AdaTP/' inc/adatp.h"
else:
    command = f"sed -i 's/#define ENABLE_AdaTP/#define nENABLE_AdaTP/' inc/adatp.h"
print(command)
os.system(command)

config_line = 69
if(lines[config_line-1].find("virtual_prefetch") == -1):
    print(f"L1D virtual_prefetch is not in line {config_line}")
    exit(1)

if(type == "stride" or type == "ipcp"):
    command = f"sed -i '{config_line},{config_line}c\        \"virtual_prefetch\": false,' {config_fname}"
else:
    command = f"sed -i '{config_line},{config_line}c\        \"virtual_prefetch\": true,' {config_fname}"
print(command)
os.system(command)

## L2 Prefetcher
config_line = 89
if(lines[config_line-1].find("prefetcher") == -1):
    print(f"L2C prefetcher is not in line {config_line}")
    exit(1)

if(type == "ipcp"):
    command = f"sed -i '{config_line},{config_line}c\        \"prefetcher\": \"ipcp_l2c\",' {config_fname}"
elif(type == "no" or type == "stride" or type == "berti" or type == "la864"):
    command = f"sed -i '{config_line},{config_line}c\        \"prefetcher\": \"no\",' {config_fname}"
elif(type == "AA" or type == "AidOP" or type == "AdaTP"):
    command = f"sed -i '{config_line},{config_line}c\        \"prefetcher\": \"aa\",' {config_fname}"
elif(type == "triage-l2"):
    command = f"sed -i '{config_line},{config_line}c\        \"prefetcher\": \"triage_isr\",' {config_fname}"
elif(type == "triangel-l2"):
    command = f"sed -i '{config_line},{config_line}c\        \"prefetcher\": \"triangel\",' {config_fname}"
elif(type == "bop"):
    command = f"sed -i '{config_line},{config_line}c\        \"prefetcher\": \"bop\",' {config_fname}"
elif(type == "spp"):
    command = f"sed -i '{config_line},{config_line}c\        \"prefetcher\": \"spp_dev\",' {config_fname}"
print(command)
os.system(command)

config_line = 87
if(lines[config_line-1].find("virtual_prefetch") == -1):
    print(f"L2C virtual_prefetch is not in line {config_line}")
    exit(1)

command = f"sed -i '{config_line},{config_line}c\        \"virtual_prefetch\": false,' {config_fname}"
print(command)
os.system(command)

## L3 Prefetcher
config_line = 165
if(lines[config_line-1].find("prefetcher") == -1):
    print(f"LLC prefetcher is not in line {config_line}")
    exit(1)
command = f"sed -i '{config_line},{config_line}c\        \"prefetcher\": \"no\",' {config_fname}"
print(command)
os.system(command)

file.close()