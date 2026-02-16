#!/usr/bin/env python3
"""
Analyze GS (Greedy Scheduling) performance counter data from DRAMSim3 results.

Parses ddr.txt files from simulation results to compute GS timeout accuracy,
Row Exclusion (RE) effectiveness, row buffer hit rates, latency, bandwidth,
energy, and timeout distribution metrics per benchmark.

Usage:
    python3 scripts/analyze_gs_perf.py --results results/GS_prof_perf_1c
"""

import argparse
import os
import re
import sys
from collections import defaultdict


# ---------------------------------------------------------------------------
# Counters to extract from each channel block in ddr.txt
# ---------------------------------------------------------------------------
SCALAR_COUNTERS = [
    "gs_timeout_switches",
    "gs_timeout_precharges",
    "gs_timeout_correct",
    "gs_timeout_wrong",
    "gs_re_hits",
    "gs_re_hit_useful",
    "gs_re_hit_useless",
    "gs_re_insertions",
    "gs_re_evictions",
    "num_read_cmds",
    "num_write_cmds",
    "num_read_row_hits",
    "num_write_row_hits",
    "num_act_cmds",
    "num_pre_cmds",
    "num_ondemand_pres",
    "num_pre_for_refresh",
    "num_pre_for_demand",
    "average_read_latency",
    "average_bandwidth",
    "total_energy",
    "average_power",
]

TIMEOUT_DIST_KEYS = [f"gs_timeout_dist.{i}" for i in range(7)]

ALL_KEYS = SCALAR_COUNTERS + TIMEOUT_DIST_KEYS


def parse_ddr_txt(filepath):
    """Parse a ddr.txt file and return a dict mapping channel_id -> {counter: value}.

    Returns None if the file cannot be read.
    """
    try:
        with open(filepath, "r") as f:
            lines = f.readlines()
    except (OSError, IOError):
        return None

    channels = {}
    current_channel = None

    for line in lines:
        # Detect channel header
        m = re.match(r"^## Statistics of Channel (\d+)", line)
        if m:
            current_channel = int(m.group(1))
            channels[current_channel] = {}
            continue

        if current_channel is None:
            continue

        # Parse counter lines: "counter_name  =  value  # comment"
        m = re.match(r"^(\S+)\s+=\s+(\S+)", line)
        if m:
            name = m.group(1)
            raw_val = m.group(2)
            if name in ALL_KEYS or name in set(ALL_KEYS):
                try:
                    val = float(raw_val)
                except ValueError:
                    val = 0.0
                channels[current_channel][name] = val

    return channels if channels else None


def aggregate_channels(channels):
    """Aggregate counter values across all channels.

    For most counters we sum them.  For average_read_latency we compute a
    weighted average (weighted by num_read_cmds per channel).  For
    average_bandwidth, average_power we sum.  For total_energy we sum.
    """
    agg = {}

    # Sum counters
    sum_keys = set(ALL_KEYS) - {"average_read_latency", "average_bandwidth",
                                 "average_power", "total_energy"}
    for key in sum_keys:
        agg[key] = sum(ch.get(key, 0.0) for ch in channels.values())

    # Weighted-average read latency (weighted by num_read_cmds)
    total_reads = sum(ch.get("num_read_cmds", 0) for ch in channels.values())
    if total_reads > 0:
        agg["average_read_latency"] = sum(
            ch.get("average_read_latency", 0) * ch.get("num_read_cmds", 0)
            for ch in channels.values()
        ) / total_reads
    else:
        agg["average_read_latency"] = 0.0

    # Sum bandwidth, energy, power across channels
    agg["average_bandwidth"] = sum(ch.get("average_bandwidth", 0) for ch in channels.values())
    agg["total_energy"] = sum(ch.get("total_energy", 0) for ch in channels.values())
    agg["average_power"] = sum(ch.get("average_power", 0) for ch in channels.values())

    return agg


def safe_div(num, den):
    """Safe division returning 0.0 when denominator is zero."""
    return num / den if den != 0 else 0.0


def compute_metrics(agg):
    """Compute derived metrics from aggregated counters."""
    m = {}

    # GS Timeout Accuracy
    m["timeout_accuracy"] = safe_div(
        agg.get("gs_timeout_correct", 0),
        agg.get("gs_timeout_precharges", 0),
    )
    # GS Timeout Wrong Rate
    m["timeout_wrong_rate"] = safe_div(
        agg.get("gs_timeout_wrong", 0),
        agg.get("gs_timeout_precharges", 0),
    )
    # GS RE Effectiveness
    re_useful = agg.get("gs_re_hit_useful", 0)
    re_useless = agg.get("gs_re_hit_useless", 0)
    m["re_effectiveness"] = safe_div(re_useful, re_useful + re_useless)

    # Row Buffer Hit Rate
    rh = agg.get("num_read_row_hits", 0) + agg.get("num_write_row_hits", 0)
    total_cmds = agg.get("num_read_cmds", 0) + agg.get("num_write_cmds", 0)
    m["row_hit_rate"] = safe_div(rh, total_cmds)

    # Average read latency (already aggregated weighted)
    m["avg_read_latency"] = agg.get("average_read_latency", 0)

    # Bandwidth / energy / power
    m["avg_bandwidth"] = agg.get("average_bandwidth", 0)
    m["total_energy"] = agg.get("total_energy", 0)
    m["avg_power"] = agg.get("average_power", 0)

    # Timeout distribution (fractions)
    dist_vals = [agg.get(f"gs_timeout_dist.{i}", 0) for i in range(7)]
    dist_total = sum(dist_vals)
    m["timeout_dist"] = [safe_div(v, dist_total) for v in dist_vals]
    m["timeout_dist_raw"] = dist_vals

    # Raw counters for reference
    m["gs_timeout_precharges"] = agg.get("gs_timeout_precharges", 0)
    m["gs_timeout_correct"] = agg.get("gs_timeout_correct", 0)
    m["gs_timeout_wrong"] = agg.get("gs_timeout_wrong", 0)
    m["gs_timeout_switches"] = agg.get("gs_timeout_switches", 0)
    m["gs_re_hits"] = agg.get("gs_re_hits", 0)
    m["gs_re_hit_useful"] = agg.get("gs_re_hit_useful", 0)
    m["gs_re_hit_useless"] = agg.get("gs_re_hit_useless", 0)
    m["gs_re_insertions"] = agg.get("gs_re_insertions", 0)
    m["gs_re_evictions"] = agg.get("gs_re_evictions", 0)
    m["num_read_cmds"] = agg.get("num_read_cmds", 0)
    m["num_write_cmds"] = agg.get("num_write_cmds", 0)
    m["num_read_row_hits"] = agg.get("num_read_row_hits", 0)
    m["num_write_row_hits"] = agg.get("num_write_row_hits", 0)
    m["num_act_cmds"] = agg.get("num_act_cmds", 0)
    m["num_pre_cmds"] = agg.get("num_pre_cmds", 0)
    m["num_ondemand_pres"] = agg.get("num_ondemand_pres", 0)

    return m


def weighted_avg_metrics(slices_data):
    """Compute weighted-average metrics across slices for one benchmark.

    slices_data: list of (weight, metrics_dict) tuples.
    Returns a single metrics dict with weighted averages.
    """
    total_weight = sum(w for w, _ in slices_data)
    if total_weight == 0:
        return None

    result = {}
    # Keys that should be weighted-averaged as rates/ratios
    rate_keys = [
        "timeout_accuracy", "timeout_wrong_rate", "re_effectiveness",
        "row_hit_rate", "avg_read_latency", "avg_bandwidth", "avg_power",
    ]
    # Keys that should be weighted-summed (extensive quantities)
    sum_keys = [
        "total_energy",
        "gs_timeout_precharges", "gs_timeout_correct", "gs_timeout_wrong",
        "gs_timeout_switches",
        "gs_re_hits", "gs_re_hit_useful", "gs_re_hit_useless",
        "gs_re_insertions", "gs_re_evictions",
        "num_read_cmds", "num_write_cmds",
        "num_read_row_hits", "num_write_row_hits",
        "num_act_cmds", "num_pre_cmds", "num_ondemand_pres",
    ]

    for key in rate_keys:
        result[key] = sum(w * m[key] for w, m in slices_data) / total_weight

    for key in sum_keys:
        result[key] = sum(w * m[key] for w, m in slices_data) / total_weight

    # Timeout distribution: weighted average of fractions
    result["timeout_dist"] = [0.0] * 7
    result["timeout_dist_raw"] = [0.0] * 7
    for i in range(7):
        result["timeout_dist"][i] = sum(
            w * m["timeout_dist"][i] for w, m in slices_data
        ) / total_weight
        result["timeout_dist_raw"][i] = sum(
            w * m["timeout_dist_raw"][i] for w, m in slices_data
        ) / total_weight

    return result


def load_manifest(results_dir):
    """Load MANIFEST.tsv and return list of (benchmark, slice_id, weight)."""
    manifest_path = os.path.join(results_dir, "MANIFEST.tsv")
    entries = []
    with open(manifest_path, "r") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split("\t")
            if len(parts) < 3:
                continue
            bench = parts[0]
            slice_id = parts[1]
            weight = float(parts[2])
            entries.append((bench, slice_id, weight))
    return entries


def main():
    parser = argparse.ArgumentParser(
        description="Analyze GS performance counters from DRAMSim3 results"
    )
    parser.add_argument(
        "--results", required=True,
        help="Path to results directory (e.g., results/GS_prof_perf_1c)"
    )
    args = parser.parse_args()

    results_dir = args.results
    if not os.path.isabs(results_dir):
        results_dir = os.path.join(os.getcwd(), results_dir)

    if not os.path.isdir(results_dir):
        print(f"ERROR: results directory not found: {results_dir}", file=sys.stderr)
        sys.exit(1)

    # -----------------------------------------------------------------------
    # 1. Load manifest
    # -----------------------------------------------------------------------
    manifest = load_manifest(results_dir)
    print(f"Loaded {len(manifest)} slices from MANIFEST.tsv", file=sys.stderr)

    # -----------------------------------------------------------------------
    # 2. Parse ddr.txt for each slice, group by benchmark
    # -----------------------------------------------------------------------
    bench_slices = defaultdict(list)  # bench -> [(weight, metrics)]
    missing = 0
    parsed = 0

    for bench, slice_id, weight in manifest:
        ddr_path = os.path.join(results_dir, bench, slice_id, "ddr.txt")
        channels = parse_ddr_txt(ddr_path)
        if channels is None:
            missing += 1
            continue
        agg = aggregate_channels(channels)
        metrics = compute_metrics(agg)
        bench_slices[bench].append((weight, metrics))
        parsed += 1

    print(f"Parsed {parsed} slices, {missing} missing/failed", file=sys.stderr)

    # -----------------------------------------------------------------------
    # 3. Aggregate per-benchmark (weighted)
    # -----------------------------------------------------------------------
    bench_metrics = {}
    for bench in sorted(bench_slices.keys()):
        wm = weighted_avg_metrics(bench_slices[bench])
        if wm is not None:
            bench_metrics[bench] = wm

    print(f"Computed metrics for {len(bench_metrics)} benchmarks", file=sys.stderr)
    print("", file=sys.stderr)

    # -----------------------------------------------------------------------
    # 4. Output comprehensive TSV table
    # -----------------------------------------------------------------------
    tsv_columns = [
        "benchmark",
        "timeout_accuracy",
        "timeout_wrong_rate",
        "re_effectiveness",
        "row_hit_rate",
        "avg_read_latency",
        "avg_bandwidth",
        "total_energy",
        "avg_power",
        "gs_timeout_precharges",
        "gs_timeout_correct",
        "gs_timeout_wrong",
        "gs_timeout_switches",
        "gs_re_hits",
        "gs_re_hit_useful",
        "gs_re_hit_useless",
        "gs_re_insertions",
        "gs_re_evictions",
        "num_read_cmds",
        "num_write_cmds",
        "num_read_row_hits",
        "num_write_row_hits",
        "num_act_cmds",
        "num_pre_cmds",
        "num_ondemand_pres",
        "timeout_dist.0",
        "timeout_dist.1",
        "timeout_dist.2",
        "timeout_dist.3",
        "timeout_dist.4",
        "timeout_dist.5",
        "timeout_dist.6",
    ]

    print("=" * 120)
    print("GS PERFORMANCE ANALYSIS - PER-BENCHMARK TSV TABLE")
    print("=" * 120)
    print("\t".join(tsv_columns))

    for bench in sorted(bench_metrics.keys()):
        m = bench_metrics[bench]
        row = [bench]
        for col in tsv_columns[1:]:
            if col.startswith("timeout_dist."):
                idx = int(col.split(".")[-1])
                row.append(f"{m['timeout_dist'][idx]:.6f}")
            elif col in ("timeout_accuracy", "timeout_wrong_rate",
                         "re_effectiveness", "row_hit_rate"):
                row.append(f"{m[col]:.6f}")
            elif col == "avg_read_latency":
                row.append(f"{m[col]:.3f}")
            elif col == "avg_bandwidth":
                row.append(f"{m[col]:.6f}")
            elif col in ("total_energy",):
                row.append(f"{m[col]:.6e}")
            elif col == "avg_power":
                row.append(f"{m[col]:.3f}")
            else:
                row.append(f"{m[col]:.1f}")
        print("\t".join(row))

    # -----------------------------------------------------------------------
    # 5. Summary sections
    # -----------------------------------------------------------------------
    print()
    print()

    # --- Helper: compute overall average across benchmarks (equal weight) ---
    def avg_across_benchmarks(benchmarks, key):
        vals = [bench_metrics[b][key] for b in benchmarks if b in bench_metrics]
        return sum(vals) / len(vals) if vals else 0.0

    def avg_dist_across_benchmarks(benchmarks):
        n = len(benchmarks)
        if n == 0:
            return [0.0] * 7
        dist = [0.0] * 7
        for b in benchmarks:
            if b in bench_metrics:
                for i in range(7):
                    dist[i] += bench_metrics[b]["timeout_dist"][i]
        return [d / n for d in dist]

    all_benchmarks = sorted(bench_metrics.keys())

    # --- Group by suite ---
    suites = defaultdict(list)
    for b in all_benchmarks:
        suite = b.split("/")[0]
        suites[suite].append(b)

    # --- Overall averages ---
    def print_summary_row(label, benchmarks, col_width=18):
        """Print a formatted summary row."""
        ta = avg_across_benchmarks(benchmarks, "timeout_accuracy")
        twr = avg_across_benchmarks(benchmarks, "timeout_wrong_rate")
        ree = avg_across_benchmarks(benchmarks, "re_effectiveness")
        rhr = avg_across_benchmarks(benchmarks, "row_hit_rate")
        arl = avg_across_benchmarks(benchmarks, "avg_read_latency")
        abw = avg_across_benchmarks(benchmarks, "avg_bandwidth")
        apw = avg_across_benchmarks(benchmarks, "avg_power")
        print(f"  {label:<35s} "
              f"{ta:>8.4f}  "
              f"{twr:>8.4f}  "
              f"{ree:>8.4f}  "
              f"{rhr:>8.4f}  "
              f"{arl:>10.2f}  "
              f"{abw:>10.4f}  "
              f"{apw:>10.2f}  "
              f"(n={len(benchmarks)})")

    print("=" * 120)
    print("SUMMARY")
    print("=" * 120)
    print()

    header = (f"  {'Category':<35s} "
              f"{'TmoutAcc':>8s}  "
              f"{'TmoutWrn':>8s}  "
              f"{'RE_Eff':>8s}  "
              f"{'RowHitR':>8s}  "
              f"{'AvgRdLat':>10s}  "
              f"{'AvgBW':>10s}  "
              f"{'AvgPower':>10s}  "
              f"{'Count':>6s}")
    print(header)
    print("  " + "-" * (len(header) - 2))

    print_summary_row("OVERALL", all_benchmarks)
    print()

    # --- Per-suite ---
    print("  Per-suite breakdown:")
    print("  " + "-" * (len(header) - 2))
    for suite in sorted(suites.keys()):
        print_summary_row(f"  {suite}", suites[suite])
    print()

    # --- Top 5 worst timeout accuracy ---
    print("  Top 5 benchmarks by WORST timeout accuracy:")
    print("  " + "-" * 90)
    sorted_by_ta = sorted(all_benchmarks, key=lambda b: bench_metrics[b]["timeout_accuracy"])
    for i, b in enumerate(sorted_by_ta[:5]):
        m = bench_metrics[b]
        print(f"    {i+1}. {b:<45s}  accuracy={m['timeout_accuracy']:.4f}  "
              f"wrong_rate={m['timeout_wrong_rate']:.4f}  "
              f"precharges={m['gs_timeout_precharges']:.0f}")
    print()

    # --- Top 5 best timeout accuracy ---
    print("  Top 5 benchmarks by BEST timeout accuracy:")
    print("  " + "-" * 90)
    sorted_by_ta_desc = sorted(all_benchmarks,
                                key=lambda b: bench_metrics[b]["timeout_accuracy"],
                                reverse=True)
    for i, b in enumerate(sorted_by_ta_desc[:5]):
        m = bench_metrics[b]
        print(f"    {i+1}. {b:<45s}  accuracy={m['timeout_accuracy']:.4f}  "
              f"wrong_rate={m['timeout_wrong_rate']:.4f}  "
              f"precharges={m['gs_timeout_precharges']:.0f}")
    print()

    # --- Top 5 highest RE effectiveness ---
    print("  Top 5 benchmarks by HIGHEST RE effectiveness:")
    print("  " + "-" * 90)
    sorted_by_re = sorted(all_benchmarks,
                           key=lambda b: bench_metrics[b]["re_effectiveness"],
                           reverse=True)
    for i, b in enumerate(sorted_by_re[:5]):
        m = bench_metrics[b]
        print(f"    {i+1}. {b:<45s}  re_eff={m['re_effectiveness']:.4f}  "
              f"useful={m['gs_re_hit_useful']:.0f}  "
              f"useless={m['gs_re_hit_useless']:.0f}  "
              f"hits={m['gs_re_hits']:.0f}")
    print()

    # --- Timeout distribution histogram ---
    print("  GS Timeout Distribution (overall average across benchmarks):")
    print("  " + "-" * 70)
    overall_dist = avg_dist_across_benchmarks(all_benchmarks)
    max_frac = max(overall_dist) if max(overall_dist) > 0 else 1.0
    bar_width = 40

    for i, frac in enumerate(overall_dist):
        bar_len = int(frac / max_frac * bar_width) if max_frac > 0 else 0
        bar = "#" * bar_len
        print(f"    timeout_level {i}:  {frac:>8.4f}  ({frac*100:5.1f}%)  |{bar}")
    print()

    # --- Per-suite timeout distribution ---
    print("  GS Timeout Distribution per suite:")
    print("  " + "-" * 90)
    suite_header = f"    {'Suite':<15s}"
    for i in range(7):
        suite_header += f"  {'L'+str(i):>7s}"
    print(suite_header)
    print("    " + "-" * 75)
    for suite in sorted(suites.keys()):
        sd = avg_dist_across_benchmarks(suites[suite])
        row = f"    {suite:<15s}"
        for i in range(7):
            row += f"  {sd[i]:>7.4f}"
        print(row)
    overall_row = f"    {'OVERALL':<15s}"
    for i in range(7):
        overall_row += f"  {overall_dist[i]:>7.4f}"
    print("    " + "-" * 75)
    print(overall_row)
    print()

    print("=" * 120)
    print("Analysis complete.")
    print("=" * 120)


if __name__ == "__main__":
    main()
