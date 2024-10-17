def rename(raw_name):
    new_name = ""
    if raw_name[0:6] == "spec06":
        sp_name = raw_name.split("_")
        new_name = sp_name[1]
        if(sp_name[1] != "gcc"):
            new_name += "06"
        if sp_name[1] == "gcc":
            new_name = new_name + "_" + sp_name[2]
        elif sp_name[2] == "lakes":
            new_name = new_name + "_lakes"
        elif sp_name[2] == "pds":
            new_name = new_name + "_pds"
    elif raw_name[0:6] == "spec00":
        sp_name = raw_name.split("_")
        new_name = sp_name[1]+"00"
    elif raw_name[0:6] == "health":
        return raw_name
    elif raw_name[0:3] == "llu":
        return raw_name
    elif raw_name[0:7] == "treeadd":
        return raw_name
    elif raw_name[0:3] == "tsp":
        return raw_name
    else:
        return raw_name
        # print("Unknown Name")
        # exit(1)
    return new_name