#include <string>
#include <cstring>
#include <fstream>
#include <iostream>

#include "regfile.h"


void REGFILE::set_init_fname(const std::string fname){
    // Set init_fname
    init_fname = fname;

    // Set check_fname
    int pos = fname.find(".regfile.txt");
    if (pos == -1){
        std::cout << "Regfile Name Error!" << std::endl;
        exit(1);
    }
    check_fname = fname.substr(0, pos) + ".regfile500.txt";
}

void REGFILE::init(){
    uint64_t icount = 0;
    std::ifstream f(init_fname);
    if(f.fail()){
        printf("Could not open %s\n", init_fname.c_str());
        exit(1);
    }

    for (std::string line; std::getline(f, line);) {
        data[icount] = stoull(line.substr(line.length()-16, 16), NULL, 16);
        icount++;
    }
    f.close();

    if(data[0] != 0){
        printf("Register File Init Error!\n");
        exit(1);
    }
    printf("Register File Load %ld registers\n", icount);
}

uint64_t REGFILE::read(uint8_t addr){
    return data[addr];
}

void REGFILE::write(uint8_t addr, uint64_t wdata){
    if(addr > 71){
        printf("Error Destination Register Num: %d", addr);
        exit(1);
    }
    data[addr] = wdata;
    return ;
}

void REGFILE::check(){
    std::ifstream f(check_fname);
    if(f.fail()){
        printf("Could not open %s\n", check_fname.c_str());
        exit(1);
    }

    uint64_t icount = 0;
    bool has_diff = false;
    for (std::string line; std::getline(f, line);) {
        if(data[icount] != stoull(line.substr(line.length()-16, 16), NULL, 16)){
            has_diff = true;
            std::cout << "Different Register Value at Regfile[" << icount
                      << "] : " << std::hex << data[icount] << std::dec
                      << std::endl;
        }
        icount++;
    }

    if(has_diff){
        printf("Register File Check Error!\n");
        exit(1);
    }

    printf("Register File Check Pass!\n");
    return ;
}