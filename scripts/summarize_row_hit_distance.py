#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
按权重汇总每个 benchmark 的 Row Hit Distance Distribution：
- 从每个 slice 的 ddr.txt 解析 Row Hit Distance Distribution
- 将每个 slice 的分布按 total_row_hits 归一化为概率分布
- 按 manifest 中的权重做加权平均，得到每个 benchmark 的加权分布
"""

import argparse
import sys
from pathlib import Path
from collections import defaultdict


def read_manifest(tsv_path: Path):
    rows = []
    with tsv_path.open('r', encoding='utf-8', errors='ignore') as f:
        for line in f:
            line = line.rstrip('\n\r')
            if not line.strip() or line.lstrip().startswith('#'):
                continue
            parts = line.split('\t')
            if parts[0] == 'benchmark':
                continue
            if len(parts) < 4:
                continue
            bench, slc, w_str = parts[0], parts[1], parts[2]
            try:
                w = float(w_str)
            except ValueError:
                continue
            rows.append((bench, slc, w))
    return rows


def parse_row_hit_distance_distribution(ddr_path: Path, verbose: bool = False):
    try:
        lines = ddr_path.read_text(errors="ignore").splitlines()
    except Exception as e:
        if verbose:
            print(f"[WARN] 读取失败: {ddr_path} ({e})", file=sys.stderr)
        return None, None

    start = None
    for i, line in enumerate(lines):
        if "Row Hit Distance Distribution" in line:
            start = i + 1
            break
    if start is None:
        if verbose:
            print(f"[WARN] 未找到分布段: {ddr_path}", file=sys.stderr)
        return None, None

    dist = {}
    total = None
    for line in lines[start:]:
        s = line.strip()
        if not s:
            continue
        if s.startswith("distance[") and "]:" in s:
            left, right = s.split("]:", 1)
            label = left + "]"
            try:
                count = float(right.strip())
            except ValueError:
                continue
            dist[label] = count
            continue
        if s.startswith("total_row_hits:"):
            try:
                total = float(s.split(":", 1)[1].strip())
            except ValueError:
                total = None
            break

    if not dist or total is None or total <= 0:
        if verbose:
            print(f"[WARN] 分布无效或 total_row_hits 缺失: {ddr_path}", file=sys.stderr)
        return None, None

    return dist, total


def main():
    ap = argparse.ArgumentParser(
        description=(
            "按权重汇总每个 benchmark 的 Row Hit Distance Distribution："
            "从 ddr.txt 中解析分布并归一化为概率，再按权重做加权平均。"
        )
    )
    ap.add_argument("--results", required=True, help="结果目录，如 results/open_page_row_reuse_distance_1c")
    ap.add_argument("--manifest", default="./benchmarks.tsv", help="benchmarks.tsv 路径")
    ap.add_argument("--out", default=None, help="输出 TSV（默认 <results>/row_hit_distance.tsv）")
    ap.add_argument("--verbose", action="store_true", help="打印缺失/无法解析的切片细节")
    args = ap.parse_args()

    results = Path(args.results).resolve()
    manifest = Path(args.manifest).resolve()
    if not results.exists():
        print(f"[ERR] 结果目录不存在: {results}", file=sys.stderr); sys.exit(2)
    if not manifest.exists():
        print(f"[ERR] 找不到 manifest: {manifest}", file=sys.stderr); sys.exit(2)
    out_path = Path(args.out).resolve() if args.out else (results / "row_hit_distance.tsv")

    entries = read_manifest(manifest)
    if not entries:
        print(f"[ERR] manifest 中无有效条目: {manifest}", file=sys.stderr); sys.exit(2)

    per_bench = defaultdict(list)
    for bench, slc, w in entries:
        ddr = results / bench / slc / "ddr.txt"
        per_bench[bench].append((slc, w, ddr))

    global_bins = []
    out_lines = []
    missing = 0

    for bench in sorted(per_bench.keys()):
        items = per_bench[bench]
        total_slices = len(items)
        ok = 0
        wsum = 0.0
        acc = defaultdict(float)

        for slc, w, ddr in items:
            if not ddr.exists():
                missing += 1
                if args.verbose:
                    print(f"[WARN] 缺少 ddr.txt: {ddr}", file=sys.stderr)
                continue

            dist, total = parse_row_hit_distance_distribution(ddr, verbose=args.verbose)
            if dist is None or total is None:
                missing += 1
                continue

            if not global_bins:
                for label in dist.keys():
                    global_bins.append(label)
            else:
                for label in dist.keys():
                    if label not in global_bins:
                        global_bins.append(label)

            for label in global_bins:
                count = dist.get(label, 0.0)
                acc[label] += w * (count / total)

            wsum += w
            ok += 1

        row = {
            "benchmark": bench,
            "slices_ok": str(ok),
            "slices_total": str(total_slices),
            "weight_sum": f"{wsum:.6f}",
        }
        if wsum > 0.0 and ok > 0:
            for label in global_bins:
                row[label] = f"{(acc[label] / wsum):.6f}"
        else:
            for label in global_bins:
                row[label] = "NaN"
        out_lines.append(row)

    header = ["benchmark", "slices_ok", "slices_total", "weight_sum"] + global_bins

    with out_path.open("w", encoding="utf-8") as f:
        f.write("\t".join(header) + "\n")
        for row in out_lines:
            f.write("\t".join(row.get(h, "NaN") for h in header) + "\n")

    print(f"完成：写入 {out_path}（benchmarks={len(out_lines)}）")
    if missing:
        print(f"[INFO] 有 {missing} 个切片缺 ddr.txt 或解析失败。", file=sys.stderr)


if __name__ == "__main__":
    main()
