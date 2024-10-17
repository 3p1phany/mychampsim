import os
import glob
import time
from concurrent.futures import ThreadPoolExecutor
import threading

cpu_num = 32
binary_file = "./bin/champsim"
trace_dir = "/tmp/trace/champsim-la-mcf/"
result_dir = "./result/" + time.strftime('%Y-%m-%d_%H:%M:%S/')
os.system(f"mkdir -p {result_dir}")

cpu_core = list()
busy_list = list()
for i in range(0,cpu_num):
    busy_list.append(0)
    cpu_core.append(i*2)

task_list = glob.glob(trace_dir + "*.trace.xz")
task_num = len(task_list)

def run_one(cpu,task,worker):
    log_file = result_dir + task.split("/")[-1]
    command = f"taskset -c {cpu} {binary_file} -w 0 -i 100000000 -l {task} > {log_file}"
    os.system(command)
    busy_list[worker] = 0
    sema.release()

pool = ThreadPoolExecutor(cpu_num)
sema = threading.Semaphore(cpu_num)
lock = threading.Lock()

print("ChampSim Start Running...")
print(f"Total task num: {task_num}")

finish_num = 0
while(finish_num < task_num):
    worker = -1
    sema.acquire()
    lock.acquire()
    for i in range(0,cpu_num):
        if(busy_list[i] == 0):
            worker = i
            busy_list[i] = 1
            break
    lock.release()
    print(f"Task running: [{finish_num+1}/{task_num}]")
    pool.submit(run_one, cpu_core[worker], task_list[finish_num], worker)
    finish_num+=1
