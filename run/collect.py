import argparse

parser = argparse.ArgumentParser(description="Collect IPC")
parser.add_argument("directory", help="The directory to search files in")
args = parser.parse_args()

target_string = "CPU 0 cumulative IPC: "

weight_path = "/tmp/trace/champsim-la-mcf/mcf.weights"
simpoint_path = "/tmp/trace/champsim-la-mcf/mcf.simpoints"
directory = args.directory

weight_file = open(weight_path, 'r') 
simpoint_file = open(simpoint_path, 'r') 
weights = weight_file.readlines()
simpoints = simpoint_file.readlines()

assert len(weights) == len(simpoints)
total_ipc = 0
for i in range(len(weights)):
    weight = weights[i].split()[0]
    simpoint = str(int(simpoints[i].split()[0] + "0")-1)
    file = directory + "mcf_" + simpoint + "0000000" + ".champsim.trace.xz"
    with open(file, 'r') as f:
            lines = f.readlines()
            for j in range(len(lines)):
                if lines[j].startswith(target_string):
                    start = lines[j].index(target_string) + len(target_string)
                    ipc = lines[j][start:].split()[0]
                    total_ipc += float(ipc) * float(weight)
                    print("Simpoint: " + simpoint  + "\tWeight: " + weight + "\tIPC: " + ipc)

print("TOTAL IPC: " + str(total_ipc))
