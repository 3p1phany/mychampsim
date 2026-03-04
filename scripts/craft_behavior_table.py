#!/usr/bin/env python3
"""Aggregate per-slice CRAFT DRAM stats into a benchmark-level behavior table.

Reads ddr.json from each slice in a CRAFT results directory, sums CRAFT counters
across channels and slices (weighted by benchmarks_selected.tsv weights), and
joins with IPC data from summary.tsv and a baseline comparison TSV.

Output: TSV with columns:
  benchmark, ipc_CRAFT, ipc_GS, pct_vs_GS,
  craft_conflicts, craft_escalations, craft_deescalations,
  timeout_low_pct, timeout_mid_pct, timeout_high_pct
"""
import argparse
import json
import os
import sys
from collections import defaultdict
from pathlib import Path


def load_manifest(path):
    """Load benchmarks_selected.tsv → {(benchmark, slice): weight}"""
    manifest = {}
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            parts = line.split()
            if parts[0].lower() == 'benchmark':
                continue
            bench, sl, weight = parts[0], parts[1], float(parts[2])
            manifest[(bench, sl)] = weight
    return manifest


def load_summary(path):
    """Load summary.tsv → {benchmark: weighted_ipc}"""
    summary = {}
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            parts = line.split('\t')
            if parts[0] == 'benchmark':
                continue
            summary[parts[0]] = float(parts[1])
    return summary


def load_comparison(path):
    """Load compare TSV → {benchmark: (ipc_baseline, pct_change)}"""
    comp = {}
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            parts = line.split('\t')
            if parts[0] == 'benchmark' or parts[0].startswith('__'):
                continue
            comp[parts[0]] = (float(parts[1]), float(parts[4]))
    return comp


def extract_craft_stats(ddr_json_path):
    """Extract CRAFT counters from a ddr.json file, summing across channels."""
    with open(ddr_json_path) as f:
        data = json.load(f)

    totals = defaultdict(int)
    timeout_bins = defaultdict(int)
    keys = ['craft_conflicts', 'craft_escalations', 'craft_deescalations',
            'craft_timeout_precharges', 'craft_timeout_wrong', 'craft_timeout_correct']

    for ch_key, ch_data in data.items():
        if not isinstance(ch_data, dict):
            continue
        for k in keys:
            totals[k] += ch_data.get(k, 0)

        # Extract histogram bins from the binned keys
        for k, v in ch_data.items():
            if k.startswith('craft_timeout_value_sum['):
                timeout_bins[k] += v

    # Categorize timeout bins
    low = 0   # [0-99]
    mid = 0   # [200-299]
    high = 0  # [3200-]
    total_timeout = 0
    for k, v in timeout_bins.items():
        total_timeout += v
        if '[0-99]' in k:
            low += v
        elif '[200-299]' in k:
            mid += v
        elif '[3200-]' in k:
            high += v

    if total_timeout > 0:
        totals['timeout_low_pct'] = round(100.0 * low / total_timeout, 1)
        totals['timeout_mid_pct'] = round(100.0 * mid / total_timeout, 1)
        totals['timeout_high_pct'] = round(100.0 * high / total_timeout, 1)
    else:
        totals['timeout_low_pct'] = 0.0
        totals['timeout_mid_pct'] = 0.0
        totals['timeout_high_pct'] = 0.0

    return dict(totals)


def main():
    parser = argparse.ArgumentParser(description='Generate CRAFT behavior table')
    parser.add_argument('--results', required=True, help='CRAFT results directory')
    parser.add_argument('--manifest', required=True, help='benchmarks_selected.tsv')
    parser.add_argument('--summary', required=True, help='CRAFT summary.tsv')
    parser.add_argument('--compare', required=True, help='Baseline comparison TSV (e.g., compare_GS_1c_vs_CRAFT_1c.tsv)')
    parser.add_argument('--baseline-label', default='GS', help='Baseline label for column header')
    parser.add_argument('--out', required=True, help='Output TSV path')
    args = parser.parse_args()

    manifest = load_manifest(args.manifest)
    craft_ipc = load_summary(args.summary)
    comparison = load_comparison(args.compare)

    # Group slices by benchmark
    bench_slices = defaultdict(list)
    for (bench, sl), weight in manifest.items():
        bench_slices[bench].append((sl, weight))

    # Aggregate CRAFT stats per benchmark (sum across slices)
    bench_stats = {}
    stat_keys = ['craft_conflicts', 'craft_escalations', 'craft_deescalations',
                 'craft_timeout_precharges', 'craft_timeout_wrong', 'craft_timeout_correct']

    for bench, slices in sorted(bench_slices.items()):
        agg = defaultdict(int)
        agg_float = defaultdict(float)
        n_slices = 0

        for sl, weight in slices:
            ddr_path = os.path.join(args.results, bench, sl, 'ddr.json')
            if not os.path.exists(ddr_path):
                continue

            stats = extract_craft_stats(ddr_path)
            n_slices += 1

            for k in stat_keys:
                agg[k] += stats.get(k, 0)
            for k in ['timeout_low_pct', 'timeout_mid_pct', 'timeout_high_pct']:
                agg_float[k] += stats.get(k, 0.0)

        if n_slices == 0:
            continue

        # Average the percentages across slices
        for k in ['timeout_low_pct', 'timeout_mid_pct', 'timeout_high_pct']:
            agg_float[k] /= n_slices

        bench_stats[bench] = {**dict(agg), **dict(agg_float), 'n_slices': n_slices}

    # Write output
    bl = args.baseline_label
    header = (f'benchmark\tipc_CRAFT\tipc_{bl}\tpct_vs_{bl}\t'
              f'craft_conflicts\tcraft_escalations\tcraft_deescalations\t'
              f'timeout_low_pct\ttimeout_mid_pct\ttimeout_high_pct')

    with open(args.out, 'w') as f:
        f.write(header + '\n')
        for bench in sorted(bench_stats.keys()):
            s = bench_stats[bench]
            ipc = craft_ipc.get(bench, 0.0)
            ipc_bl, pct = comparison.get(bench, (0.0, 0.0))

            f.write(f'{bench}\t{ipc:.6f}\t{ipc_bl:.6f}\t{pct:.2f}\t'
                    f'{s["craft_conflicts"]}\t{s["craft_escalations"]}\t{s["craft_deescalations"]}\t'
                    f'{s["timeout_low_pct"]:.1f}\t{s["timeout_mid_pct"]:.1f}\t{s["timeout_high_pct"]:.1f}\n')

    print(f'Written: {args.out} ({len(bench_stats)} benchmarks)')


if __name__ == '__main__':
    main()
