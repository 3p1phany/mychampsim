#!/usr/bin/python3

import os
import sys

type = sys.argv[1]

type_list = ["no", "stride", "stride-l1", "dbp", "cdp", "ipcp", "berti", "bop", "imp", 
             "gretch", "tyche", "domino", "isb", "misb", "triage-l1", "triangel-l1", "triangel-l2", "cmc", "catp-l1", "catp-l2",
             "domino-l2", "cmc-domino", "isb-l2", "cmc-isb", "misb-l2", "cmc-misb", 
             "triage-l2", "cmc-triage", "cmc-triangel"]

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

if(type=="tyche"):
    command = f"sed -i '{config_line},{config_line}c\        \"prefetcher\": \"tyche\",' {config_fname}"
elif(type=="stride" or type=="stride-l1" or type == "triage-l2" or type == "triangel-l2" or type == "catp-l2" or type=="misb-l2" or type=="isb-l2" or type=="domino-l2" or type=="bop"):
    command = f"sed -i '{config_line},{config_line}c\        \"prefetcher\": \"stride_l1d\",' {config_fname}"
elif(type == "berti"):
    command = f"sed -i '{config_line},{config_line}c\        \"prefetcher\": \"berti\",' {config_fname}"
elif(type == "dbp"):
    command = f"sed -i '{config_line},{config_line}c\        \"prefetcher\": \"dbp\",' {config_fname}"
elif(type == "cdp"):
    command = f"sed -i '{config_line},{config_line}c\        \"prefetcher\": \"cdp\",' {config_fname}"
elif(type == "gretch"):
    command = f"sed -i '{config_line},{config_line}c\        \"prefetcher\": \"gretch\",' {config_fname}"
elif(type == "imp"):
    command = f"sed -i '{config_line},{config_line}c\        \"prefetcher\": \"imp\",' {config_fname}"
elif(type == "ipcp"):
    command = f"sed -i '{config_line},{config_line}c\        \"prefetcher\": \"ipcp_l1d\",' {config_fname}"
elif(type == "no"):
    command = f"sed -i '{config_line},{config_line}c\        \"prefetcher\": \"no\",' {config_fname}"
elif(type == "domino"):
    command = f"sed -i '{config_line},{config_line}c\        \"prefetcher\": \"domino\",' {config_fname}"
elif(type == "isb" or type == "misb"):
    command = f"sed -i '{config_line},{config_line}c\        \"prefetcher\": \"misb\",' {config_fname}"
elif(type == "triage-l1"):
    command = f"sed -i '{config_line},{config_line}c\        \"prefetcher\": \"triage_isr\",' {config_fname}"
elif(type == "triangel-l1"):
    command = f"sed -i '{config_line},{config_line}c\        \"prefetcher\": \"triangel\",' {config_fname}"
elif(type == "catp-l1"):
    command = f"sed -i '{config_line},{config_line}c\        \"prefetcher\": \"catp\",' {config_fname}"
elif(type == "cmc" or type == "cmc-isb" or type == "cmc-misb" or type == "cmc-triage" or type == "cmc-triangel" or type == "cmc-domino"):
    command = f"sed -i '{config_line},{config_line}c\        \"prefetcher\": \"cmc\",' {config_fname}"
print(command)
os.system(command)

if(type=="cmc" or type=="isb-l1" or type=="isb-l2" or type=="misb-l1" or type=="misb-l2" or type=="triage-l1" or type=="triangel-l1" or type=="cmc-isb" or type=="cmc-misb" or type=="cmc-triage" or type=="cmc-triangel" or type=="cmc-domino"):
    command = f"sed -i 's/#define nENABLE_CMC/#define ENABLE_CMC/' inc/cmc.h"
else:
    command = f"sed -i 's/#define ENABLE_CMC/#define nENABLE_CMC/' inc/cmc.h"
print(command)
os.system(command)

if(type=="isb" or type=="misb" or type=="domino" or type=="misb-l2" or type=="cmc-isb" or type=="cmc-misb" or type=="isb-l2" or type=="domino-l2" or type=="cmc-domino"):
    command = f"sed -i 's/#define nOFFCHIP_METADATA/#define OFFCHIP_METADATA/' inc/prefetch.h"
else:
    command = f"sed -i 's/#define OFFCHIP_METADATA/#define nOFFCHIP_METADATA/' inc/prefetch.h"
print(command)
os.system(command)

if(type=="isb" or type=="isb-l2" or type=="cmc-isb"):
    command = f"sed -i 's/#define nISB_ENABLE/#define ISB_ENABLE/' inc/isb.h"
else:
    command = f"sed -i 's/#define ISB_ENABLE/#define nISB_ENABLE/' inc/isb.h"
print(command)
os.system(command)

if(type=="dbp"):
    command = f"sed -i 's/#define nENABLE_DBP/#define ENABLE_DBP/' inc/dbp.h"
else:
    command = f"sed -i 's/#define ENABLE_DBP/#define nENABLE_DBP/' inc/dbp.h"
print(command)
os.system(command)

if(type=="cdp"):
    command = f"sed -i 's/#define nENABLE_CDP/#define ENABLE_CDP/' inc/prefetch.h"
else:
    command = f"sed -i 's/#define ENABLE_CDP/#define nENABLE_CDP/' inc/prefetch.h"
print(command)
os.system(command)

if(type!="tyche"):
    command = f"sed -i 's/#define ENABLE_TYCHE/#define nENABLE_TYCHE/' inc/prefetch.h"
else:
    command = f"sed -i 's/#define nENABLE_TYCHE/#define ENABLE_TYCHE/' inc/prefetch.h"
print(command)
os.system(command)

if(type=="catp-l1" or type=="catp-l2"):
    command = f"sed -i 's/#define nENABLE_CATP/#define ENABLE_CATP/' inc/catp.h"
else:
    command = f"sed -i 's/#define ENABLE_CATP/#define nENABLE_CATP/' inc/catp.h"
print(command)
os.system(command)

if(type == "triage-l1" or type == "triage-l2" or type == "cmc-triage"):
    command = f"sed -i 's/#define nENABLE_TRIAGE/#define ENABLE_TRIAGE/' inc/triage.h"
else:
    command = f"sed -i 's/#define ENABLE_TRIAGE/#define nENABLE_TRIAGE/' inc/triage.h"
print(command)
os.system(command)


config_line = 69
if(lines[config_line-1].find("virtual_prefetch") == -1):
    print(f"L1D virtual_prefetch is not in line {config_line}")
    exit(1)

if(type == "berti"):
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

if(type == "tyche" or type == "imp" or type == "gretch" or type=="stride"):
    command = f"sed -i '{config_line},{config_line}c\        \"prefetcher\": \"stride_l2c\",' {config_fname}"
elif(type == "ipcp"):
    command = f"sed -i '{config_line},{config_line}c\        \"prefetcher\": \"ipcp_l2c\",' {config_fname}"
elif(type == "no" or type == "stride-l1" or type == "berti" or type == "dbp" or type == "cdp" or type == "domino" or type == "isb" or type == "misb" or type == "cmc" or type == "triage-l1" or type == "triangel" or type == "catp"):
    command = f"sed -i '{config_line},{config_line}c\        \"prefetcher\": \"no\",' {config_fname}"
elif(type == "triage-l2" or type == "cmc-triage"):
    command = f"sed -i '{config_line},{config_line}c\        \"prefetcher\": \"triage_isr\",' {config_fname}"
elif(type == "triangel-l2" or type == "cmc-triangel"):
    command = f"sed -i '{config_line},{config_line}c\        \"prefetcher\": \"triangel\",' {config_fname}"
elif(type == "catp-l2"):
    command = f"sed -i '{config_line},{config_line}c\        \"prefetcher\": \"catp\",' {config_fname}"
elif(type == "isb-l2" or type == "cmc-isb" or type == "misb-l2" or type == "cmc-misb"):
    command = f"sed -i '{config_line},{config_line}c\        \"prefetcher\": \"misb\",' {config_fname}"
elif(type == "domino-l2" or type == "cmc-domino"):
    command = f"sed -i '{config_line},{config_line}c\        \"prefetcher\": \"domino\",' {config_fname}"
elif(type == "bop"):
    command = f"sed -i '{config_line},{config_line}c\        \"prefetcher\": \"bop\",' {config_fname}"
print(command)
os.system(command)

config_line = 87
if(lines[config_line-1].find("virtual_prefetch") == -1):
    print(f"L2C virtual_prefetch is not in line {config_line}")
    exit(1)

command = f"sed -i '{config_line},{config_line}c\        \"virtual_prefetch\": false,' {config_fname}"
print(command)
os.system(command)

if(type=="isb-l2" or type=="cmc-isb" or type=="misb-l2" or type=="cmc-misb" or type=="triage-l2" or type=="triangel-l2" or type == "catp-l2" or type=="cmc-triage"  or type=="cmc-triangel" or type=="domino-l2" or type=="cmc-domino"):
    command = f"sed -i 's/#define TEMPORAL_L1D true/#define TEMPORAL_L1D false/' inc/prefetch.h"
else:
    command = f"sed -i 's/#define TEMPORAL_L1D false/#define TEMPORAL_L1D true/' inc/prefetch.h"
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