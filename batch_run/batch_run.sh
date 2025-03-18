#!/bin/bash

rm binary.json
echo "[]" > binary.json

## Single-Core
./gen_all_binary.py single

## Start Run
./run.py binary.json config.json