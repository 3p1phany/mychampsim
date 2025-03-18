def rename(raw_name):
    new_name = ""
    if raw_name[0:6] == "spec17":
        sp_name = raw_name.split("_")
        if sp_name[1] == "gcc":
            if sp_name[2] == "smaller":
                new_name = "SPEC17." + sp_name[1] + "_small"
            else:
                new_name = "SPEC17." + sp_name[1] + "_" + sp_name[2]
        elif sp_name[1] == "xalancbmk":
            new_name = "SPEC17.xalanc"
        else:
            new_name = "SPEC17." + sp_name[1]
    elif raw_name[0:6] == "spec06":
        sp_name = raw_name.split("_")
        if sp_name[1] == "gcc":
            new_name = "SPEC06." + sp_name[1] + "_" + sp_name[2]
        elif sp_name[1] == "xalancbmk":
            new_name = "SPEC06.xalanc"
        else:
            new_name = "SPEC06." + sp_name[1]
            
    else:
        return raw_name
        # print("Unknown Name")
        # exit(1)
    return new_name