#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Compare DRAM row buffer hit rates across CRAFT and baselines.

Reads ddr.json from each benchmark/policy result directory, aggregates
across channels and runs (slices), and computes:
  - Read row buffer hit rate  = num_read_row_hits  / num_read_cmds
  - Write row buffer hit rate = num_write_row_hits / num_write_cmds
  - Overall row buffer hit rate = (read_hits + write_hits) / (read_cmds + write_cmds)
  - Average read latency (cycles)

Usage:
  python3 scripts/compare_row_buffer_hit_rate.py [--results-dir results/] [--out row_buffer_hit_rate.tsv]
"""

import argparse
import json
import math
import sys
from pathlib import Path

POLICIES = [
    ("CRAFT_PRECHARGE_1c", "CRAFT"),
    ("ABP_1c",             "ABP"),
    ("DYMPL_1c",           "DYMPL"),
    ("INTAP_1c",           "INTAP"),
]

BENCHMARKS = [
    "ligra/CF/roadNet-CA",
    "ligra/CF/higgs",
    "ligra/PageRank/higgs",
    "ligra/BFSCC/soc-pokec-short",
    "spec06/sphinx3/ref",
    "ligra/CF/soc-pokec",
    "ligra/Triangle/roadNet-CA",
    "ligra/PageRank/roadNet-CA",
    "crono/Triangle-Counting/roadNet-CA",
    "spec06/wrf/ref",
    "ligra/Components-Shortcut/soc-pokec",
    "ligra/Radii/higgs",
]


def load_ddr_stats(ddr_path: Path):
    """Load a ddr.json and aggregate key metrics across all channels."""
    with ddr_path.open() as f:
        data = json.load(f)

    total = {
        "num_read_row_hits": 0,
        "num_read_cmds": 0,
        "num_write_row_hits": 0,
        "num_write_cmds": 0,
        "num_act_cmds": 0,
        "num_pre_cmds": 0,
        "num_ondemand_pres": 0,
        "average_read_latency_sum": 0.0,
        "read_cmd_weight": 0,
    }

    for ch_key, ch_data in data.items():
        if not ch_key.isdigit():
            continue
        for k in ["num_read_row_hits", "num_read_cmds", "num_write_row_hits",
                   "num_write_cmds", "num_act_cmds", "num_pre_cmds", "num_ondemand_pres"]:
            total[k] += ch_data.get(k, 0)
        rc = ch_data.get("num_read_cmds", 0)
        total["average_read_latency_sum"] += ch_data.get("average_read_latency", 0.0) * rc
        total["read_cmd_weight"] += rc

    return total


def aggregate_runs(results_dir: Path, policy_dir: str, benchmark: str):
    """Aggregate ddr.json stats across all runs (slices) for a benchmark."""
    bm_dir = results_dir / policy_dir / benchmark
    if not bm_dir.is_dir():
        return None

    agg = {
        "num_read_row_hits": 0,
        "num_read_cmds": 0,
        "num_write_row_hits": 0,
        "num_write_cmds": 0,
        "num_act_cmds": 0,
        "num_pre_cmds": 0,
        "num_ondemand_pres": 0,
        "average_read_latency_sum": 0.0,
        "read_cmd_weight": 0,
    }
    count = 0

    for run_dir in sorted(bm_dir.iterdir()):
        ddr = run_dir / "ddr.json"
        if not ddr.exists():
            continue
        stats = load_ddr_stats(ddr)
        for k in agg:
            agg[k] += stats[k]
        count += 1

    if count == 0:
        return None
    return agg


def compute_rates(agg):
    """Compute hit rates and latency from aggregated stats."""
    rd_hits = agg["num_read_row_hits"]
    rd_cmds = agg["num_read_cmds"]
    wr_hits = agg["num_write_row_hits"]
    wr_cmds = agg["num_write_cmds"]

    rd_hr = rd_hits / rd_cmds if rd_cmds > 0 else float("nan")
    wr_hr = wr_hits / wr_cmds if wr_cmds > 0 else float("nan")
    total_hits = rd_hits + wr_hits
    total_cmds = rd_cmds + wr_cmds
    overall_hr = total_hits / total_cmds if total_cmds > 0 else float("nan")
    avg_rd_lat = (agg["average_read_latency_sum"] / agg["read_cmd_weight"]
                  if agg["read_cmd_weight"] > 0 else float("nan"))

    return {
        "read_hr": rd_hr,
        "write_hr": wr_hr,
        "overall_hr": overall_hr,
        "avg_read_latency": avg_rd_lat,
        "ondemand_pre_ratio": (agg["num_ondemand_pres"] / agg["num_pre_cmds"]
                               if agg["num_pre_cmds"] > 0 else float("nan")),
    }


def geomean(values):
    vals = [v for v in values if v > 0 and math.isfinite(v)]
    if not vals:
        return float("nan")
    return math.exp(sum(math.log(v) for v in vals) / len(vals))


def main():
    ap = argparse.ArgumentParser(description="Compare DRAM row buffer hit rates")
    ap.add_argument("--results-dir", default="results/",
                    help="Path to results directory (default: results/)")
    ap.add_argument("--out", default="row_buffer_hit_rate.tsv",
                    help="Output TSV file (default: row_buffer_hit_rate.tsv)")
    args = ap.parse_args()

    results_dir = Path(args.results_dir).resolve()

    # Collect data: {benchmark: {policy_label: rates_dict}}
    all_data = {}
    for bm in BENCHMARKS:
        all_data[bm] = {}
        for policy_dir, policy_label in POLICIES:
            agg = aggregate_runs(results_dir, policy_dir, bm)
            if agg is not None:
                all_data[bm][policy_label] = compute_rates(agg)

    # ---------- Print Read Row Buffer Hit Rate Table ----------
    print("=" * 100)
    print("READ Row Buffer Hit Rate (num_read_row_hits / num_read_cmds)")
    print("=" * 100)
    hdr = f"{'Benchmark':<45} {'CRAFT':>8} {'ABP':>8} {'DYMPL':>8} {'INTAP':>8}"
    print(hdr)
    print("-" * 100)
    for bm in BENCHMARKS:
        short = bm
        vals = []
        for _, pl in POLICIES:
            r = all_data[bm].get(pl)
            vals.append(f"{r['read_hr']*100:.2f}%" if r else "N/A")
        print(f"{short:<45} {vals[0]:>8} {vals[1]:>8} {vals[2]:>8} {vals[3]:>8}")

    # ---------- Print Write Row Buffer Hit Rate Table ----------
    print()
    print("=" * 100)
    print("WRITE Row Buffer Hit Rate (num_write_row_hits / num_write_cmds)")
    print("=" * 100)
    print(hdr)
    print("-" * 100)
    for bm in BENCHMARKS:
        vals = []
        for _, pl in POLICIES:
            r = all_data[bm].get(pl)
            vals.append(f"{r['write_hr']*100:.2f}%" if r else "N/A")
        print(f"{bm:<45} {vals[0]:>8} {vals[1]:>8} {vals[2]:>8} {vals[3]:>8}")

    # ---------- Print Overall Row Buffer Hit Rate Table ----------
    print()
    print("=" * 100)
    print("OVERALL Row Buffer Hit Rate ((read_hits + write_hits) / (read_cmds + write_cmds))")
    print("=" * 100)
    print(hdr)
    print("-" * 100)
    for bm in BENCHMARKS:
        vals = []
        for _, pl in POLICIES:
            r = all_data[bm].get(pl)
            vals.append(f"{r['overall_hr']*100:.2f}%" if r else "N/A")
        print(f"{bm:<45} {vals[0]:>8} {vals[1]:>8} {vals[2]:>8} {vals[3]:>8}")

    # ---------- Print Average Read Latency Table ----------
    print()
    print("=" * 100)
    print("Average Read Latency (DRAM cycles)")
    print("=" * 100)
    print(hdr)
    print("-" * 100)
    for bm in BENCHMARKS:
        vals = []
        for _, pl in POLICIES:
            r = all_data[bm].get(pl)
            vals.append(f"{r['avg_read_latency']:.2f}" if r else "N/A")
        print(f"{bm:<45} {vals[0]:>8} {vals[1]:>8} {vals[2]:>8} {vals[3]:>8}")

    # ---------- Print On-demand Precharge Ratio Table ----------
    print()
    print("=" * 100)
    print("On-demand Precharge Ratio (num_ondemand_pres / num_pre_cmds)")
    print("=" * 100)
    print(hdr)
    print("-" * 100)
    for bm in BENCHMARKS:
        vals = []
        for _, pl in POLICIES:
            r = all_data[bm].get(pl)
            vals.append(f"{r['ondemand_pre_ratio']*100:.2f}%" if r else "N/A")
        print(f"{bm:<45} {vals[0]:>8} {vals[1]:>8} {vals[2]:>8} {vals[3]:>8}")

    # ---------- CRAFT improvement over best baseline ----------
    print()
    print("=" * 100)
    print("CRAFT Read Hit Rate Improvement over Best Baseline (percentage points)")
    print("=" * 100)
    print(f"{'Benchmark':<45} {'CRAFT':>8} {'Best BL':>8} {'Diff(pp)':>10}")
    print("-" * 100)
    diffs = []
    for bm in BENCHMARKS:
        craft_r = all_data[bm].get("CRAFT")
        if not craft_r:
            continue
        craft_hr = craft_r["read_hr"]
        best_bl = -1.0
        for bl in ["ABP", "DYMPL", "INTAP"]:
            r = all_data[bm].get(bl)
            if r and math.isfinite(r["read_hr"]):
                best_bl = max(best_bl, r["read_hr"])
        if best_bl < 0:
            continue
        diff_pp = (craft_hr - best_bl) * 100
        diffs.append(diff_pp)
        print(f"{bm:<45} {craft_hr*100:>7.2f}% {best_bl*100:>7.2f}% {diff_pp:>+9.2f}pp")
    if diffs:
        print(f"{'AVERAGE':<45} {'':>8} {'':>8} {sum(diffs)/len(diffs):>+9.2f}pp")

    # ---------- CRAFT latency improvement over best baseline ----------
    print()
    print("=" * 100)
    print("CRAFT Avg Read Latency Reduction vs Best Baseline")
    print("=" * 100)
    print(f"{'Benchmark':<45} {'CRAFT':>8} {'Best BL':>8} {'Reduction':>10}")
    print("-" * 100)
    lat_reductions = []
    for bm in BENCHMARKS:
        craft_r = all_data[bm].get("CRAFT")
        if not craft_r:
            continue
        craft_lat = craft_r["avg_read_latency"]
        best_lat = float("inf")
        for bl in ["ABP", "DYMPL", "INTAP"]:
            r = all_data[bm].get(bl)
            if r and math.isfinite(r["avg_read_latency"]):
                best_lat = min(best_lat, r["avg_read_latency"])
        if not math.isfinite(best_lat):
            continue
        pct = (best_lat - craft_lat) / best_lat * 100
        lat_reductions.append(pct)
        print(f"{bm:<45} {craft_lat:>8.2f} {best_lat:>8.2f} {pct:>+9.2f}%")
    if lat_reductions:
        print(f"{'AVERAGE':<45} {'':>8} {'':>8} {sum(lat_reductions)/len(lat_reductions):>+9.2f}%")

    # ---------- Write TSV ----------
    out_path = Path(args.out).resolve()
    with out_path.open("w") as f:
        cols = ["benchmark"]
        for _, pl in POLICIES:
            cols.extend([f"{pl}_read_hr", f"{pl}_write_hr", f"{pl}_overall_hr",
                         f"{pl}_avg_rd_lat", f"{pl}_ondemand_pre_ratio"])
        f.write("\t".join(cols) + "\n")

        for bm in BENCHMARKS:
            row = [bm]
            for _, pl in POLICIES:
                r = all_data[bm].get(pl)
                if r:
                    row.extend([
                        f"{r['read_hr']:.6f}",
                        f"{r['write_hr']:.6f}",
                        f"{r['overall_hr']:.6f}",
                        f"{r['avg_read_latency']:.4f}",
                        f"{r['ondemand_pre_ratio']:.6f}",
                    ])
                else:
                    row.extend(["NaN"] * 5)
            f.write("\t".join(row) + "\n")

    print(f"\nTSV written to: {out_path}")


if __name__ == "__main__":
    main()
