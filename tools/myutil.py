def begin_print(data_dir, type):
    if data_dir[-1] == "/":
        print_info = "Starting Collect "+ type + " of " + data_dir.split('/')[-2]+"..."
    else:
        print_info = "Starting Collect "+ type + " of " + data_dir.split('/')[-1]+"..."

    max_length = 40
    if(len(print_info) < max_length):
        print_info += " "*(max_length-len(print_info))
    print(print_info, end="\n")