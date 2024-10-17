#!/bin/bash

rm binary.json
echo "[]" > binary.json

## Homo 4-Core
# ./gen_all_binary.py homo-4c

## Hetero 4-Core
# ./gen_all_binary.py hetero-4c

## Metadata Conflict
#./gen_all_binary.py metadata_conflict

## Component Contribution
#./gen_all_binary.py component_switch

## Single-Core
./gen_all_binary.py single

## Prefetching Degree
#./gen_all_binary.py pref_degree

## Design Parameters
#./gen_all_binary.py design_parameters

## Start Run
./run.py binary.json config.json