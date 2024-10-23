#!/usr/bin/python3

import os
import sys
import json

mode = sys.argv[1]

## Read old binary list
binary_file = open("binary.json", "r")
old_binary = json.load(binary_file)
binary_file.close()

# run_list = ["no", "stride", "stride-l1", "dbp", "cdp", "berti", "ipcp", "domino", 
#             "isb", "misb", "triage-l1", "cmc",
#             "domino-l2", "cmc-domino", "isb-l2", "cmc-isb", "misb-l2", "cmc-misb", 
#             "triage-l2", "cmc-triage"]

## Binary List
single_binary_list = ["no","ipcp"]
md_confict_binary_list = ["isb-md-conflict", "misb-md-conflict", "triage-l1-md-conflict", 
                          "triangel-md-conflict", "cmc-md-conflict"
                          ]
component_switch_binary_list = ["cmc-cfg0", "cmc-cfg0-md-conflict",
                                "cmc-cfg1", "cmc-cfg1-md-conflict",
                                "cmc-cfg2", "cmc-cfg2-md-conflict",
                                ]
pref_degree_binary_list = ["misb-degree-2",  "triage-l1-degree-2",  "triangel-degree-2",  "cmc-degree-2",
                           "misb-degree-4",  "triage-l1-degree-4",  "triangel-degree-4",  "cmc-degree-4",
                          #"misb-degree-8",  "triage-l1-degree-8",  "triangel-degree-8",  "cmc-degree-8", ## Default
                           "misb-degree-12", "triage-l1-degree-12", "triangel-degree-12", "cmc-degree-12",
                           "misb-degree-16", "triage-l1-degree-16", "triangel-degree-16", "cmc-degree-16",
                           "misb-degree-12", "triage-l1-degree-12", "triangel-degree-12", "cmc-degree-12",
                           "misb-degree-16", "triage-l1-degree-16", "triangel-degree-16", "cmc-degree-16",
                           ]
pref_design_parameters_list = ["cmc-ldret-8", "cmc-ldret-16", "cmc-ldret-48", "cmc-ldret-64", ## Default 32
                               "cmc-ldidt-4", "cmc-ldidt-8", "cmc-ldidt-24",  "cmc-ldidt-32", ## Default 16
                               "cmc-agq-4",
                               ]

homo_4c_binary_list = ["stride-l1-homo-4c", "misb-homo-4c", "triage-l1-homo-4c", "triangel-homo-4c", "cmc-homo-4c"]
hetero_4c_binary_list = ["stride-l1-hetero-4c", "misb-hetero-4c", "triage-l1-hetero-4c", "triangel-hetero-4c", "cmc-hetero-4c"]

homo_4c_binary_list = ["stride-l1-homo-4c", "misb-homo-4c", "triage-l1-homo-4c", "triangel-homo-4c", "cmc-homo-4c"]
hetero_4c_binary_list = ["stride-l1-hetero-4c", "misb-hetero-4c", "triage-l1-hetero-4c", "triangel-hetero-4c", "cmc-hetero-4c"]

if mode == "single":
    command = "cd ./gen_binary && ./gen_single.py " + ' '.join(map(str, single_binary_list))
    print(command)
    os.system(command)
    append_data = single_binary_list
elif mode == "metadata_conflict":
    command = "cd ./gen_binary && ./gen_metadata_conflict.py " + ' '.join(map(str, md_confict_binary_list))
    print(command)
    os.system(command)
    append_data = md_confict_binary_list
elif mode == "component_switch":
    command = "cd ./gen_binary && ./gen_component_switch.py " + ' '.join(map(str, component_switch_binary_list))
    print(command)
    os.system(command)
    append_data = component_switch_binary_list
elif mode == "pref_degree":
    command = "cd ./gen_binary && ./gen_pref_degree.py " + ' '.join(map(str, pref_degree_binary_list))
    print(command)
    os.system(command)
    append_data = pref_degree_binary_list
elif mode == "design_parameters":
    command = "cd ./gen_binary && ./gen_design_parameters.py " + ' '.join(map(str, pref_design_parameters_list))
    print(command)
    os.system(command)
    append_data = pref_design_parameters_list
elif mode == "homo-4c":
    pref = ""
    for item in homo_4c_binary_list:
        pref += item.replace("-homo-4c", " ")
    command = "cd ./gen_binary && ./gen_multi.py " + "homo " + pref
    print(command)
    os.system(command)
    append_data = homo_4c_binary_list
elif mode == "hetero-4c":
    pref = ""
    for item in hetero_4c_binary_list:
        pref += item.replace("-hetero-4c", " ")
    command = "cd ./gen_binary && ./gen_multi.py " + "hetero " + pref
    print(command)
    os.system(command)
    append_data = hetero_4c_binary_list
elif mode == "homo-4c":
    pref = ""
    for item in homo_4c_binary_list:
        pref += item.replace("-homo-4c", " ")
    command = "cd ./gen_binary && ./gen_multi.py " + "homo " + pref
    print(command)
    os.system(command)
    append_data = homo_4c_binary_list
elif mode == "hetero-4c":
    pref = ""
    for item in hetero_4c_binary_list:
        pref += item.replace("-hetero-4c", " ")
    command = "cd ./gen_binary && ./gen_multi.py " + "hetero " + pref
    print(command)
    os.system(command)
    append_data = hetero_4c_binary_list
else:
    print("Unsupported mode: " + mode)
    sys.exit(1)

# for i in append_data[:]:
#     in_run_list = False
#     for run in run_list:
#         if i.startswith(run):
#             in_run_list = True
#     if in_run_list == False:
#         append_data.remove(i)

new_data = old_binary + append_data

## Write binary list to file
binary_file = open("binary.json", "w")
json.dump(new_data, binary_file, indent=True , ensure_ascii=False)
binary_file.close()
