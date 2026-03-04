#!/usr/bin/env python3
"""Aggregate per-slice CRAFT DRAM stats into a benchmark-level behavior table.

Reads ddr.json from each slice in a CRAFT results directory, applies slice
weights from benchmarks_selected.tsv, and produces weighted benchmark-level
metrics joined with IPC data.

Output: TSV with columns:
  benchmark, ipc_CRAFT, ipc_<baseline>, pct_vs_<baseline>,
  craft_conflicts, craft_escalations, craft_deescalations,
  timeout_low_pct, timeout_mid_pct, timeout_high_pct
"""
import argparse
import json
import os
import sys
from collections import defaultdict


def load_manifest(path):
    """Load benchmarks_selected.tsv -> {(benchmark, slice): weight}"""
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
    """Load summary.tsv -> {benchmark: weighted_ipc}"""
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
    """Load compare TSV -> {benchmark: (ipc_baseline, pct_change)}"""
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
    """Extract CRAFT counters and raw timeout bin counts from ddr.json.

    Returns dict with scalar counter totals (summed across channels)
    and raw timeout bin counts for low/mid/high/total.
    """
    with open(ddr_json_path) as f:
        data = json.load(f)

    totals = defaultdict(int)
    counter_keys = [
        'craft_conflicts', 'craft_escalations', 'craft_deescalations',
        'craft_timeout_precharges', 'craft_timeout_wrong', 'craft_timeout_correct',
    ]

    low = 0
    mid = 0
    high = 0
    total_bins = 0

    for ch_data in data.values():
        if not isinstance(ch_data, dict):
            continue
        for k in counter_keys:
            totals[k] += ch_data.get(k, 0)

        for k, v in ch_data.items():
            if not k.startswith('craft_timeout_value_sum['):
                continue
            total_bins += v
            if '[0-99]' in k:
                low += v
            elif '[200-299]' in k:
                mid += v
            elif '[3200-]' in k:
                high += v

    totals['timeout_bin_low'] = low
    totals['timeout_bin_mid'] = mid
    totals['timeout_bin_high'] = high
    totals['timeout_bin_total'] = total_bins
    return dict(totals)


def main():
    parser = argparse.ArgumentParser(description='Generate CRAFT behavior table')
    parser.add_argument('--results', required=True, help='CRAFT results directory')
    parser.add_argument('--manifest', required=True, help='benchmarks_selected.tsv')
    parser.add_argument('--summary', required=True, help='CRAFT summary.tsv')
    parser.add_argument('--compare', required=True,
                        help='Baseline comparison TSV')
    parser.add_argument('--baseline-label', default='GS',
                        help='Baseline label for column header')
    parser.add_argument('--out', required=True, help='Output TSV path')
    args = parser.parse_args()

    manifest = load_manifest(args.manifest)
    craft_ipc = load_summary(args.summary)
    comparison = load_comparison(args.compare)

    # Group slices by benchmark
    bench_slices = defaultdict(list)
    for (bench, sl), weight in manifest.items():
        bench_slices[bench].append((sl, weight))

    counter_keys = [
        'craft_conflicts', 'craft_escalations', 'craft_deescalations',
        'craft_timeout_precharges', 'craft_timeout_wrong', 'craft_timeout_correct',
    ]
    bin_keys = ['timeout_bin_low', 'timeout_bin_mid', 'timeout_bin_high',
                'timeout_bin_total']

    bench_stats = {}
    missing_slices = []

    for bench in sorted(bench_slices.keys()):
        slices = bench_slices[bench]
        w_counters = defaultdict(float)
        w_bins = defaultdict(float)
        total_weight = 0.0
        n_ok = 0

        for sl, weight in slices:
            ddr_path = os.path.join(args.results, bench, sl, 'ddr.json')
            if not os.path.exists(ddr_path):
                missing_slices.append(f'{bench}/{sl}')
                continue

            stats = extract_craft_stats(ddr_path)
            n_ok += 1
            total_weight += weight

            for k in counter_keys:
                w_counters[k] += weight * stats.get(k, 0)
            for k in bin_keys:
                w_bins[k] += weight * stats.get(k, 0)

        if n_ok == 0:
            print(f'ERROR: {bench} has no valid slices', file=sys.stderr)
            sys.exit(1)

        # Normalize weighted counters by total weight
        norm = {}
        for k in counter_keys:
            norm[k] = round(w_counters[k] / total_weight)

        # Compute timeout percentages from weighted bin totals
        wt = w_bins['timeout_bin_total']
        if wt > 0:
            norm['timeout_low_pct'] = round(100.0 * w_bins['timeout_bin_low'] / wt, 1)
            norm['timeout_mid_pct'] = round(100.0 * w_bins['timeout_bin_mid'] / wt, 1)
            norm['timeout_high_pct'] = round(100.0 * w_bins['timeout_bin_high'] / wt, 1)
        else:
            norm['timeout_low_pct'] = 0.0
            norm['timeout_mid_pct'] = 0.0
            norm['timeout_high_pct'] = 0.0

        bench_stats[bench] = norm

    # Self-check diagnostics
    if missing_slices:
        print(f'WARNING: {len(missing_slices)} manifest slices missing from results:',
              file=sys.stderr)
        for s in missing_slices[:10]:
            print(f'  {s}', file=sys.stderr)
        if len(missing_slices) > 10:
            print(f'  ... and {len(missing_slices) - 10} more', file=sys.stderr)

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
                    f'{s["craft_conflicts"]}\t{s["craft_escalations"]}'
                    f'\t{s["craft_deescalations"]}\t'
                    f'{s["timeout_low_pct"]:.1f}\t{s["timeout_mid_pct"]:.1f}'
                    f'\t{s["timeout_high_pct"]:.1f}\n')

    print(f'Written: {args.out} ({len(bench_stats)} benchmarks, '
          f'{len(missing_slices)} missing slices)')


if __name__ == '__main__':
    main()
