#!/usr/bin/python3

import os
import sys
import json
import time
from concurrent.futures import ThreadPoolExecutor
import threading
import random

binary_list = json.load(open(sys.argv[1]))
config_file = sys.argv[2]
config = json.load(open(config_file))

mode = config["mode"]
trace_dir = config["trace_path"]
task_list = config["task_list"]
core_list = config["core_list"]
cpu_num = len(core_list)

machine_name = "multi"

if(mode == "collect_instr"):
    print("Start Collect Info!")
else:
    print("Strat Run Simulation!")

default_warmup_len = 100000000
default_sim_len = 100000000

#### For Single Core
program_list = []
trace_list = []
for i in task_list:
    cur_list = json.load(open(i))
    for item in cur_list.items():
        for program in item[1]:
            program_list.append(os.path.join(trace_dir,item[0],program))

length_dict = json.load(open("task_list/length.json"))

for program in program_list:
    count = 0
    for file in os.listdir(program):
        if(file[-18:]==".champsim.trace.xz"):
            trace_name = os.path.join(program,file)
            item_name = trace_name.split("/")[-1][0:-18]
            if(mode == "collect_instr"):
                sim_len = 0
                warmup_len = 10000000000
            elif(mode == "collect_observation"):
                sim_len = length_dict[item_name]
                warmup_len = 0
            elif(item_name in length_dict.keys()):
                sim_len = length_dict[item_name]
                warmup_len = default_warmup_len
            else:
                print(f"{item_name} can not find entry in length.json" )
                exit(1)
            trace_list.append([trace_name, warmup_len, sim_len])
            count += 1
    print(f"The Slice Number of {program}: {count}")


job_list = []
for binary in binary_list:
    result_dir = "./result/" + time.strftime('%Y%m%d-%H%M%S') + "-" + binary
    os.system(f"mkdir -p {result_dir}")
    for trace_item in trace_list:
        if "-md-conflict" in binary or "Trace/LA/spec" not in trace_item[0]:
            trace_item[1] = 0

        if(binary.split("-")[-1] == "4c"):
            core_num = 4
        elif(binary.split("-")[-1] == "8c"):
            core_num = 8
        else:
            core_num = 1
            only_spec = "-degree-" in binary
            if only_spec and "Trace/LA/spec" not in trace_item[0]:
                continue
        full_trace = (trace_item[0]+" ")*core_num
        job_list.append([binary] + trace_item + [result_dir, full_trace])

job_num = len(job_list)

print(f"Single Binary List Num: {len(binary_list)}")
print(f"Single Binary List: {',  '.join(binary_list)}")
print(f"The Total Slice Number: {len(trace_list)}")
print(f"The Total Job Number: {job_num}")

busy_list = list()
for i in range(0,cpu_num):
    busy_list.append(0)

def run_one(cpu,task,worker):
    binary = task[0]
    if binary[-9:] == "hetero-4c" or binary[-9:] == "hetero-8c":
        trace_name = task[1]
        result_dir = task[2]
        id = task[3]
        log_file = os.path.join(result_dir, "_".join([binary, machine_name, str(id)]) + ".log")
        command = f"taskset -c {cpu} ../bin/{binary} --warmup_instructions {default_warmup_len} --simulation_instructions {default_sim_len} --loongarch {trace_name} > {log_file}"
    else:
        trace_name = task[1]
        warmup_len = task[2]
        sim_len = task[3]
        result_dir = task[4]
        full_trace = task[5]
        log_file = os.path.join(result_dir, binary + "_" + trace_name.split("/")[-1].split(".")[0] + ".log")
        command = f"taskset -c {cpu} ../bin/{binary} --warmup_instructions {warmup_len} --simulation_instructions {sim_len} --loongarch {full_trace} > {log_file}"
    #print(command)
    os.system(command)
    busy_list[worker] = 0
    sema.release()

pool = ThreadPoolExecutor(cpu_num)
sema = threading.Semaphore(cpu_num)
lock = threading.Lock()

print("ChampSim Start Running!")

finish_num = 0
while(finish_num < job_num):
    worker = -1
    sema.acquire()
    lock.acquire()
    for i in range(0,cpu_num):
        if(busy_list[i] == 0):
            worker = i
            busy_list[i] = 1
            break
    lock.release()
    print(f"Task running: [{finish_num}/{job_num-1}]")
    pool.submit(run_one, core_list[worker], job_list[finish_num], worker)
    finish_num+=1
