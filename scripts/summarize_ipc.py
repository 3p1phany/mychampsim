#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
按权重汇总每个 benchmark 的加权 IPC（严格多线程版）：
- 仅从日志中的 "Finished CPU ... cumulative IPC: <val>" 提取每个线程 IPC
- 对同一 slice 的所有线程 IPC 做几何平均，作为该 slice 的 IPC
- 若没有任何 Finished 行，或线程 IPC 全部无效（非数/<=0），该 slice 记为缺失
- 按 manifest 中的权重对各 slice 的 IPC 做加权平均，得到每个 benchmark 的加权 IPC
"""

import argparse
import re
import sys
import math
from pathlib import Path
from collections import defaultdict

# -----------------------------------------------------------------------------
# 正则表达式：仅匹配 "Finished CPU <id> ... cumulative IPC: <数字>"
#   - 数字形态：整数/小数/科学计数法（如 1.23e-3）
#   - 使用 IGNORECASE，使大小写不敏感
# -----------------------------------------------------------------------------
NUM = r'([0-9]*\.?[0-9]+(?:[eE][+-]?[0-9]+)?)'
FINISHED_PER_THREAD_IPC = re.compile(
    rf'Finished\s+CPU\s*\d+.*?cumulative\s+IPC\s*[:=]\s*{NUM}',
    re.IGNORECASE
)

# -----------------------------------------------------------------------------
# 工具函数：几何平均（仅对正数），返回 float；若无有效值返回 None
#   - 几何平均用于多线程指标聚合，避免被极端大值/小值过度拉动
#   - 非正数（<=0）直接忽略；如果全是无效值，返回 None
# -----------------------------------------------------------------------------
def geometric_mean_positive(values):
    logs = []
    for v in values:
        try:
            fv = float(v)
        except Exception:
            continue  # 非数字，跳过
        if fv > 0.0:
            logs.append(math.log(fv))
    if not logs:
        return None
    return math.exp(sum(logs) / len(logs))

# -----------------------------------------------------------------------------
# 从单个 run.log 中解析所有线程的 Finished IPC，并做几何平均
# 严格模式：仅使用 Finished 行；若没有或无效，则返回 None
#   - verbose=True 时打印详细原因，便于定位问题
# -----------------------------------------------------------------------------
def parse_slice_ipc_geomean_from_finished(log_path: Path, verbose: bool = False):
    try:
        text = log_path.read_text(errors="ignore")
    except Exception as e:
        if verbose:
            print(f"[WARN] 读取日志失败: {log_path} ({e})", file=sys.stderr)
        return None

    # findall 可能返回 ["... 0.49019", "... 0.48723", ...] 或 [('0.49019',), ('0.48723',), ...]
    matches = FINISHED_PER_THREAD_IPC.findall(text)

    if not matches:
        if verbose:
            print(f"[WARN] 未找到 Finished 行: {log_path}", file=sys.stderr)
        return None

    # 统一提取每个匹配的捕获组中的数字字符串
    vals = []
    for m in matches:
        # m 可能是 str 或 tuple；我们只需要捕获组里的数字
        if isinstance(m, (tuple, list)):
            token = m[0] if m else None
        else:
            token = m  # 实际上 FINISHED_PER_THREAD_IPC 有捕获组，因此多数实现会给出 tuple
        try:
            fv = float(token)
            vals.append(fv)
        except Exception:
            # 捕获到的不是合法数字，忽略
            continue

    # 仅对正数做几何平均
    gm = geometric_mean_positive(vals)
    if gm is None:
        if verbose:
            # 进一步区分是“全部非正数/非数”，还是“确实抓到但都<=0”
            if any(isinstance(v, float) for v in vals) and all((isinstance(v, float) and v <= 0.0) for v in vals):
                print(f"[WARN] Finished 行存在，但 IPC 全部 <= 0（几何平均无意义）: {log_path}", file=sys.stderr)
            else:
                print(f"[WARN] Finished 行存在，但未解析到有效正数 IPC: {log_path}", file=sys.stderr)
    return gm

# -----------------------------------------------------------------------------
# 读取 manifest（TSV）
# 期望列：benchmark, slice, weight, trace（至少 4 列）
# 返回列表：[(bench, slice, weight_float), ...]
# -----------------------------------------------------------------------------
def read_manifest(tsv_path: Path):
    rows = []
    with tsv_path.open('r', encoding='utf-8', errors='ignore') as f:
        for i, line in enumerate(f, 1):
            line = line.rstrip('\n\r')
            # 跳过空行与注释行
            if not line.strip() or line.lstrip().startswith('#'):
                continue
            parts = line.split('\t')
            # 跳过表头
            if parts[0] == 'benchmark':
                continue
            if len(parts) < 4:
                # 列不足，跳过
                continue
            bench, slc, w_str, trace = parts[0], parts[1], parts[2], parts[3]
            try:
                w = float(w_str)
            except ValueError:
                # 权重无法解析，跳过
                continue
            rows.append((bench, slc, w))
    return rows

# -----------------------------------------------------------------------------
# 主流程：
#   1) 组织每个 benchmark 下的 (slice, weight, run.log)
#   2) 每个 slice 只用 Finished 行做几何平均（严格模式）
#   3) 对有效 slice 按权重做加权平均，输出 TSV
# -----------------------------------------------------------------------------
def main():
    ap = argparse.ArgumentParser(
        description=(
            "按权重汇总每个 benchmark 的加权 IPC（严格多线程版）："
            "仅使用日志中所有 'Finished CPU ... cumulative IPC: <val>' 的几何平均作为 slice IPC；"
            "若无 Finished 或无有效正数 IPC，则记为缺失。"
        )
    )
    ap.add_argument("--results", required=True, help="结果目录，如 results/20250820-203125-oracle")
    ap.add_argument("--manifest", default="./benchmarks.tsv", help="benchmarks.tsv 路径")
    ap.add_argument("--out", default=None, help="输出 TSV（默认 <results>/summary.tsv）")
    ap.add_argument("--verbose", action="store_true", help="打印缺失/无法解析的切片细节")
    args = ap.parse_args()

    results = Path(args.results).resolve()
    manifest = Path(args.manifest).resolve()
    if not results.exists():
        print(f"[ERR] 结果目录不存在: {results}", file=sys.stderr); sys.exit(2)
    if not manifest.exists():
        print(f"[ERR] 找不到 manifest: {manifest}", file=sys.stderr); sys.exit(2)
    out_path = Path(args.out).resolve() if args.out else (results / "summary.tsv")

    entries = read_manifest(manifest)
    if not entries:
        print(f"[ERR] manifest 中无有效条目: {manifest}", file=sys.stderr); sys.exit(2)

    # 将 manifest 记录聚合为：per_bench[bench] = [(slice, weight, log_path), ...]
    per_bench = defaultdict(list)
    for bench, slc, w in entries:
        log = results / bench / slc / "run.log"
        per_bench[bench].append((slc, w, log))

    # 输出 TSV 的表头
    out_lines = [("benchmark", "weighted_ipc", "slices_ok", "slices_total")]
    missing = 0  # 统计缺失或无效 slice 的数量

    # 逐 benchmark 计算加权平均 IPC
    for bench in sorted(per_bench.keys()):
        items = per_bench[bench]
        total = len(items)  # 该 benchmark 总切片数
        acc = 0.0           # 权重 * slice_ipc 的累加
        wsum = 0.0          # 权重累加
        ok = 0              # 成功解析并纳入加权的切片数

        for slc, w, log in items:
            if not log.exists():
                missing += 1
                if args.verbose:
                    print(f"[WARN] 缺少日志文件: {log}", file=sys.stderr)
                continue

            # 严格模式：只用 Finished 行做几何平均
            ipc = parse_slice_ipc_geomean_from_finished(log, verbose=args.verbose)

            # 解析失败或无效，计入 missing
            if ipc is None or not (ipc == ipc):  # NaN 检查
                missing += 1
                if args.verbose:
                    print(f"[WARN] 未能得到有效几何平均 IPC: {log}", file=sys.stderr)
                continue

            # 纳入加权平均
            acc += w * ipc
            wsum += w
            ok += 1

        # 计算该 benchmark 的加权 IPC（若无有效切片则输出 NaN）
        weighted = acc / wsum if ok > 0 and wsum > 0.0 else float('nan')

        out_lines.append((
            bench,
            f"{weighted:.6f}" if weighted == weighted else "NaN",
            str(ok),
            str(total)
        ))

    # 写出汇总 TSV
    with out_path.open("w", encoding="utf-8") as f:
        for line in out_lines:
            f.write("\t".join(line) + "\n")

    print(f"完成：写入 {out_path}（benchmarks={len(out_lines)-1}）")
    if missing:
        print(
            f"[INFO] 有 {missing} 个切片缺日志或严格解析失败（仅统计 Finished 行并做几何平均）。",
            file=sys.stderr
        )

# -----------------------------------------------------------------------------
if __name__ == "__main__":
    main()
