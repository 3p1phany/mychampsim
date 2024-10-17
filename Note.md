## Our

### Novelty

1. Recursive loads localization.  
2. L1 Cache Latency. Now the latency of pointer chasing that hits in L1 cache is 7 cycles! We can reduce it in the config.sh.

TODO: Should Cache hit address inserted into the metadata_onchip?

TODO: mispredict_penalty set as 1.

TODO: When a recursive laod is evicted from Identity table, other loads in the same rec_id should also be evicted?

### How to detect recursive loads?

#### Scheme 1

Firstly, we identify self-recursive loads, and then we proceed to identify the loads that connect different self-recursive loads. But, there are problems when we identify self-recursive loads. For example, in mcf, **node = node->child;** is compiled into two different loads.

Commit id: d5ccfe4d48d68250ff3f5277b7905a0676527a18

#### Scheme 2

Calculate the length of dependency chain. If the length is larger than the THRESHOLD, then all loads in the chain are recursive loads.

#### Scheme 3

If a load is not a producer of any load when it is evicted from the LoadRet, then it is not a recursive load. 
TODO: We need to handle the data-load prefetching. The parent of data-load is dynamic. For example, it may come from child or sibling.

TODO: If a load is defined as a data_load, it will no longer be trained as a recursive load unless it is evicted and is retrained. So, shall we need to increse the reursive_conf peridically? For example, increase 1 every 1000000 cycles?

TODO: How to speed up the train phase? If a load is self-recursive, then its rec_conf will be increased by 1. 

TODO: For LRU, new inserted entry should be set as 0 or MAX/2?

## Triage

1. 代码中并未加入访问Metadata的延迟；
2. 