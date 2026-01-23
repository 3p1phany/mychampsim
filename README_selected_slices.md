# 精选切片快速验证

本仓库提供一组精选 trace 切片，用于快速验证想法。切片清单在
`scripts/selected_slices.tsv`，推荐通过 `scripts/run_selected_slices.sh` 运行。

## 基线性能（1c）

以下 IPC 来自已有结果目录：
`results/GS_1c`、`results/open_page_1c`、`results/oracle_1c`。

| benchmark | ipc_open | ipc_GS | ipc_oracle |
| --- | --- | --- | --- |
| crono/PageRank/higgs | 0.457756 | 0.504997 | 0.692740 |
| crono/PageRank/soc-pokec | 0.423516 | 0.461220 | 0.595065 |
| hashjoin/hj-2-NPO_st | 0.367277 | 0.391584 | 0.631758 |

## 编译（新机器）

1) 准备 DRAMSim3（需要 `libdramsim3.so`）
2) 设置环境变量并一键编译：
   - `export DRAMSIM3_ROOT=/path/to/dramsim3`
   - `./scripts/build_with_dramsim3.sh`
3) 手动流程（等价）：
   - `export DRAMSIM3_ROOT=/path/to/dramsim3`
   - `cd $DRAMSIM3_ROOT && make clean && make -j`
   - `cd /path/to/champsim-la`
   - `python3 config.sh champsim_config.json`
   - `make -j`
4) 确保运行时能找到 `libdramsim3.so`：
   - 方式一：系统可搜索到（如安装到系统库目录）
   - 方式二：运行前设置 `LD_LIBRARY_PATH=/path/to/dramsim3`

## 快速运行（新机器）

1) 准备 trace 并设置根目录：
   - `export TRACE_ROOT=/path/to/Trace/LA`
2) 创建目录结构（一次性）：
   - `mkdir -p "$TRACE_ROOT/crono/PageRank/higgs"`
   - `mkdir -p "$TRACE_ROOT/crono/PageRank/soc-pokec"`
   - `mkdir -p "$TRACE_ROOT/hashjoin/hj-2-NPO_st"`
3) 把 trace 文件放到对应目录（文件名保持不变）：
   - `mv /path/to/crono_PageRank_higgs_0.champsim.trace.xz "$TRACE_ROOT/crono/PageRank/higgs/"`
   - `mv /path/to/crono_PageRank_soc-pokec.champsim.trace.xz "$TRACE_ROOT/crono/PageRank/soc-pokec/"`
   - `mv /path/to/hj-2-NPO_st_290000000.champsim.trace.xz "$TRACE_ROOT/hashjoin/hj-2-NPO_st/"`
4) 检查 `champsim_config.json` 里的 DRAMSim3 配置路径：
   - `physical_memory.dramsim3_config` 当前为绝对路径，需按本机更新
5) 运行：
   - `TRACE_ROOT="$TRACE_ROOT" BINARY=./bin/champsim ./scripts/run_selected_slices.sh`

说明：
- 脚本会提示输入 label 并确认。
- `DRY_RUN=1` 可仅打印命令不执行。
- 可通过 `WARMUP`、`SIM`、`JOBS`、`EXTRA_ARGS` 覆盖默认参数。
