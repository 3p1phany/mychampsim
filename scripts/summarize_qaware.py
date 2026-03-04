#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
从 GS 运行结果中提取 queue-aware observability 计数器并生成分析报告。

用法:
  python3 scripts/summarize_qaware.py --results results/GS_qaware_obs --manifest benchmarks_selected.tsv

输出:
  1. 逐 benchmark 的 qaware 计数器表格 (TSV)
  2. 全局汇总统计
  3. 与已有 timeout_wrong/timeout_correct 的交叉验证
"""

import argparse
import json
import math
import sys
from pathlib import Path
from collections import defaultdict


QAWARE_KEYS = [
    "gs_timeout_precharges",
    "gs_timeout_qaware_hit",
    "gs_timeout_qaware_conflict",
    "gs_timeout_qaware_both",
    "gs_timeout_qaware_empty",
    "gs_timeout_wrong",
    "gs_timeout_correct",
]


def read_manifest(tsv_path: Path):
    """读取 manifest, 返回 [(bench, slice, weight), ...]"""
    rows = []
    with tsv_path.open("r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            line = line.rstrip("\n\r")
            if not line.strip() or line.lstrip().startswith("#"):
                continue
            parts = line.split("\t")
            if parts[0].lower() == "benchmark":
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


def collect_qaware_from_json(json_path: Path) -> dict:
    """从单个 ddr.json 中聚合所有 channel 的 qaware 计数器。"""
    try:
        with json_path.open("r") as f:
            data = json.load(f)
    except Exception:
        return None

    totals = {k: 0 for k in QAWARE_KEYS}

    # JSON 结构: {"0": {counters...}, "1": {counters...}, ...}
    for ch_key, ch_data in data.items():
        if not isinstance(ch_data, dict):
            continue
        for k in QAWARE_KEYS:
            totals[k] += ch_data.get(k, 0)

    return totals


def safe_pct(num, den):
    if den == 0:
        return 0.0
    return num / den * 100.0


def main():
    ap = argparse.ArgumentParser(description="GS Queue-Aware Observability 分析")
    ap.add_argument("--results", required=True, help="结果目录 (如 results/GS_qaware_obs)")
    ap.add_argument("--manifest", default="./benchmarks_selected.tsv", help="benchmark 清单")
    ap.add_argument("--out", default=None, help="输出 TSV (默认 <results>/qaware_summary.tsv)")
    args = ap.parse_args()

    results = Path(args.results).resolve()
    manifest = Path(args.manifest).resolve()

    if not results.exists():
        print(f"[ERR] 结果目录不存在: {results}", file=sys.stderr)
        sys.exit(2)
    if not manifest.exists():
        print(f"[ERR] 找不到 manifest: {manifest}", file=sys.stderr)
        sys.exit(2)

    out_path = Path(args.out).resolve() if args.out else (results / "qaware_summary.tsv")
    entries = read_manifest(manifest)
    if not entries:
        print(f"[ERR] manifest 无有效条目: {manifest}", file=sys.stderr)
        sys.exit(2)

    # 按 benchmark 聚合: per_bench[bench] = [(slice, weight, json_path), ...]
    per_bench = defaultdict(list)
    for bench, slc, w in entries:
        json_path = results / bench / slc / "ddr.json"
        per_bench[bench].append((slc, w, json_path))

    # 收集数据
    bench_data = {}  # bench -> aggregated counters
    global_totals = {k: 0 for k in QAWARE_KEYS}
    missing = 0

    for bench in sorted(per_bench.keys()):
        items = per_bench[bench]
        bench_counters = {k: 0 for k in QAWARE_KEYS}

        for slc, w, jp in items:
            if not jp.exists():
                missing += 1
                continue
            counters = collect_qaware_from_json(jp)
            if counters is None:
                missing += 1
                continue
            for k in QAWARE_KEYS:
                bench_counters[k] += counters[k]

        bench_data[bench] = bench_counters
        for k in QAWARE_KEYS:
            global_totals[k] += bench_counters[k]

    # 输出 TSV
    headers = [
        "benchmark",
        "timeout_pre",
        "qaware_hit",
        "qaware_conflict",
        "qaware_both",
        "qaware_empty",
        "timeout_wrong",
        "timeout_correct",
        "hit_pct(%)",
        "conflict_pct(%)",
        "empty_pct(%)",
        "avoidable_of_wrong(%)",
    ]

    rows = []
    for bench in sorted(bench_data.keys()):
        c = bench_data[bench]
        tp = c["gs_timeout_precharges"]
        hit = c["gs_timeout_qaware_hit"]
        conflict = c["gs_timeout_qaware_conflict"]
        both = c["gs_timeout_qaware_both"]
        empty = c["gs_timeout_qaware_empty"]
        wrong = c["gs_timeout_wrong"]
        correct = c["gs_timeout_correct"]

        rows.append([
            bench,
            str(tp),
            str(hit),
            str(conflict),
            str(both),
            str(empty),
            str(wrong),
            str(correct),
            f"{safe_pct(hit, tp):.2f}",
            f"{safe_pct(conflict, tp):.2f}",
            f"{safe_pct(empty, tp):.2f}",
            f"{safe_pct(hit, wrong):.2f}" if wrong > 0 else "N/A",
        ])

    with out_path.open("w", encoding="utf-8") as f:
        f.write("\t".join(headers) + "\n")
        for r in rows:
            f.write("\t".join(r) + "\n")

        # 全局汇总行
        tp = global_totals["gs_timeout_precharges"]
        hit = global_totals["gs_timeout_qaware_hit"]
        conflict = global_totals["gs_timeout_qaware_conflict"]
        both = global_totals["gs_timeout_qaware_both"]
        empty = global_totals["gs_timeout_qaware_empty"]
        wrong = global_totals["gs_timeout_wrong"]
        correct = global_totals["gs_timeout_correct"]

        f.write("\t".join([
            "__TOTAL__",
            str(tp),
            str(hit),
            str(conflict),
            str(both),
            str(empty),
            str(wrong),
            str(correct),
            f"{safe_pct(hit, tp):.2f}",
            f"{safe_pct(conflict, tp):.2f}",
            f"{safe_pct(empty, tp):.2f}",
            f"{safe_pct(hit, wrong):.2f}" if wrong > 0 else "N/A",
        ]) + "\n")

    # 打印摘要到 stdout
    print(f"\n{'='*70}")
    print(f"  GS Queue-Aware Observability 分析结果")
    print(f"{'='*70}")
    print(f"  结果目录: {results}")
    print(f"  Benchmarks: {len(bench_data)}")
    if missing:
        print(f"  缺失切片: {missing}")
    print()

    tp = global_totals["gs_timeout_precharges"]
    hit = global_totals["gs_timeout_qaware_hit"]
    conflict = global_totals["gs_timeout_qaware_conflict"]
    both = global_totals["gs_timeout_qaware_both"]
    empty = global_totals["gs_timeout_qaware_empty"]
    wrong = global_totals["gs_timeout_wrong"]
    correct = global_totals["gs_timeout_correct"]

    print(f"  全局统计 (所有 benchmark 聚合):")
    print(f"    timeout_precharges:    {tp:>12,}")
    print(f"    qaware_hit:            {hit:>12,}  ({safe_pct(hit, tp):5.2f}%)")
    print(f"    qaware_conflict:       {conflict:>12,}  ({safe_pct(conflict, tp):5.2f}%)")
    print(f"    qaware_both:           {both:>12,}  ({safe_pct(both, tp):5.2f}%)")
    print(f"    qaware_empty:          {empty:>12,}  ({safe_pct(empty, tp):5.2f}%)")
    print()
    print(f"  交叉验证:")
    print(f"    timeout_wrong:         {wrong:>12,}")
    print(f"    timeout_correct:       {correct:>12,}")
    print(f"    qaware_hit/wrong:      {safe_pct(hit, wrong):5.2f}%  (队列可避免的错误 precharge 比例)")
    print()
    print(f"  不变量检查:")
    qaware_sum = hit + conflict + both + empty
    invariant_ok = (qaware_sum == tp)
    print(f"    hit+conflict+both+empty = {qaware_sum:,}  {'== ' if invariant_ok else '!= '}{tp:,} timeout_precharges  {'[OK]' if invariant_ok else '[MISMATCH!]'}")
    subset_ok = (hit <= wrong) if wrong > 0 else True
    print(f"    qaware_hit({hit:,}) <= timeout_wrong({wrong:,})  {'[OK]' if subset_ok else '[VIOLATION!]'}")
    print()

    # 决策建议
    print(f"  决策参考:")
    if tp > 0:
        if safe_pct(hit, tp) > 5:
            print(f"    [V] qaware_hit > 5%: Layer 1 有明确收益，值得实现")
        else:
            print(f"    [X] qaware_hit <= 5%: Layer 1 收益有限")

        if safe_pct(conflict, tp) > 10:
            print(f"    [V] qaware_conflict > 10%: timeout 等待有显著延迟，应考虑提前 precharge")
        else:
            print(f"    [X] qaware_conflict <= 10%: timeout 等待延迟尚可接受")

        if safe_pct(empty, tp) > 90:
            print(f"    [!] qaware_empty > 90%: 队列信息极少，应聚焦改进统计预测")
        else:
            print(f"    [V] qaware_empty <= 90%: 队列有一定信息量")
    else:
        print(f"    [!] 没有 timeout precharge 事件 (timeout_precharges=0)")

    print()
    print(f"  详细 TSV: {out_path}")
    print(f"{'='*70}")


if __name__ == "__main__":
    main()
