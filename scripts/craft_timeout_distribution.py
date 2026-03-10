#!/usr/bin/env python3
"""Extract CRAFT timeout value distributions for selected benchmarks.

Shows how CRAFT adapts to different workloads by settling into different
timeout regions: Low (aggressive close), Mid (balanced), High (keep open).

Reads ddr.json from CRAFT_PRECHARGE results, applies slice weights from
benchmarks_selected.tsv, and produces:
1. Per-benchmark timeout distribution table (low/mid/high %)
2. Fine-grained histogram (100-cycle bins) for each benchmark
"""
import json
import os
import sys
from collections import defaultdict

RESULTS_DIR = '/root/data/smartPRE/champsim-la/results/CRAFT_PRECHARGE_1c'
MANIFEST_PATH = '/root/data/smartPRE/champsim-la/benchmarks_selected.tsv'

# The 12 selected benchmarks from craft_final_evaluation.md
SELECTED_BENCHMARKS = [
    'ligra/CF/roadNet-CA',
    'ligra/CF/higgs',
    'ligra/PageRank/higgs',
    'ligra/BFSCC/soc-pokec-short',
    'spec06/sphinx3/ref',
    'ligra/CF/soc-pokec',
    'ligra/Triangle/roadNet-CA',
    'ligra/PageRank/roadNet-CA',
    'crono/Triangle-Counting/roadNet-CA',
    'spec06/wrf/ref',
    'ligra/Components-Shortcut/soc-pokec',
    'ligra/Radii/higgs',
]

# Short display names
SHORT_NAMES = {
    'ligra/CF/roadNet-CA':                  'CF/roadNet',
    'ligra/CF/higgs':                       'CF/higgs',
    'ligra/PageRank/higgs':                 'PR/higgs',
    'ligra/BFSCC/soc-pokec-short':          'BFSCC/pokec',
    'spec06/sphinx3/ref':                   'sphinx3',
    'ligra/CF/soc-pokec':                   'CF/pokec',
    'ligra/Triangle/roadNet-CA':            'Tri/roadNet',
    'ligra/PageRank/roadNet-CA':            'PR/roadNet',
    'crono/Triangle-Counting/roadNet-CA':   'TriCnt/roadNet',
    'spec06/wrf/ref':                       'wrf',
    'ligra/Components-Shortcut/soc-pokec':  'Comp/pokec',
    'ligra/Radii/higgs':                    'Radii/higgs',
}

# Timeout range categorization (cycles)
# Low:  [0, 800)   — aggressive close, good for random/streaming access
# Mid:  [800, 2000) — balanced adaptive
# High: [2000, 3200] — keep rows open, good for temporal locality
LOW_UPPER = 800
MID_UPPER = 2000

# All 100-cycle bin boundaries
BIN_RANGES = []
for start in range(0, 3200, 100):
    BIN_RANGES.append((start, start + 99))
BIN_RANGES.append((3200, None))  # [3200-] overflow bin


def load_manifest():
    """Load benchmarks_selected.tsv -> {benchmark: [(slice, weight), ...]}"""
    manifest = defaultdict(list)
    with open(MANIFEST_PATH) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            parts = line.split()
            if parts[0].lower() == 'benchmark':
                continue
            bench, sl, weight = parts[0], parts[1], float(parts[2])
            manifest[bench].append((sl, weight))
    return dict(manifest)


def parse_binned_histogram(ch_data):
    """Extract the pre-binned histogram from one channel's data.

    Returns dict: {bin_label: count} for craft_timeout_value_sum[X-Y] keys.
    """
    hist = {}
    for k, v in ch_data.items():
        if not k.startswith('craft_timeout_value_sum['):
            continue
        # Extract range from key like 'craft_timeout_value_sum[100-199]'
        bracket = k[len('craft_timeout_value_sum['):-1]
        hist[bracket] = v
    return hist


def bin_label_to_start(label):
    """Convert bin label like '100-199' or '3200-' to start value."""
    if label == '-0':
        return -1
    parts = label.split('-')
    return int(parts[0])


def categorize_bins(hist):
    """Categorize histogram bins into low/mid/high.

    Returns (low_count, mid_count, high_count, total_count,
             fine_bins: {bin_start: count} for 100-cycle resolution)
    """
    low = mid = high = total = 0
    fine_bins = defaultdict(int)

    for label, count in hist.items():
        start = bin_label_to_start(label)
        if start < 0:
            continue  # skip [-0] bin
        fine_bins[start] += count
        total += count
        if start < LOW_UPPER:
            low += count
        elif start < MID_UPPER:
            mid += count
        else:
            high += count

    return low, mid, high, total, dict(fine_bins)


def extract_benchmark_distribution(bench, slices):
    """Extract weighted timeout distribution for one benchmark."""
    weighted_fine = defaultdict(float)
    weighted_low = weighted_mid = weighted_high = 0.0
    weighted_total = 0.0
    total_weight = 0.0
    n_ok = 0

    for sl, weight in slices:
        ddr_path = os.path.join(RESULTS_DIR, bench, sl, 'ddr.json')
        if not os.path.exists(ddr_path):
            continue

        with open(ddr_path) as f:
            data = json.load(f)

        # Aggregate across all channels
        slice_hist = defaultdict(int)
        for ch_data in data.values():
            if not isinstance(ch_data, dict):
                continue
            h = parse_binned_histogram(ch_data)
            for label, count in h.items():
                slice_hist[label] += count

        low, mid, high, total, fine = categorize_bins(slice_hist)

        if total == 0:
            continue

        n_ok += 1
        total_weight += weight

        # Weight the raw counts
        weighted_low += weight * low
        weighted_mid += weight * mid
        weighted_high += weight * high
        weighted_total += weight * total

        for b, c in fine.items():
            weighted_fine[b] += weight * c

    if weighted_total == 0:
        return None

    # Normalize to percentages
    low_pct = 100.0 * weighted_low / weighted_total
    mid_pct = 100.0 * weighted_mid / weighted_total
    high_pct = 100.0 * weighted_high / weighted_total

    # Fine-grained: normalize each bin to percentage of total
    fine_pct = {}
    for b, wc in weighted_fine.items():
        fine_pct[b] = 100.0 * wc / weighted_total

    return {
        'low_pct': low_pct,
        'mid_pct': mid_pct,
        'high_pct': high_pct,
        'fine_pct': fine_pct,
        'n_slices': n_ok,
        'total_weight': total_weight,
    }


def format_bar(pct, width=20):
    """Create a simple ASCII bar for percentage."""
    filled = int(round(pct / 100.0 * width))
    return '█' * filled + '░' * (width - filled)


def main():
    manifest = load_manifest()

    results = {}
    for bench in SELECTED_BENCHMARKS:
        if bench not in manifest:
            print(f'WARNING: {bench} not in manifest', file=sys.stderr)
            continue
        dist = extract_benchmark_distribution(bench, manifest[bench])
        if dist is None:
            print(f'WARNING: no data for {bench}', file=sys.stderr)
            continue
        results[bench] = dist

    # === Summary Table ===
    print('=' * 90)
    print('CRAFT Timeout Distribution: Adaptive Behavior Across Workloads')
    print(f'  Low:  [0, {LOW_UPPER}) cycles  — Aggressive close (streaming/random)')
    print(f'  Mid:  [{LOW_UPPER}, {MID_UPPER}) cycles — Balanced adaptive')
    print(f'  High: [{MID_UPPER}, 3200] cycles — Keep open (temporal locality)')
    print('=' * 90)
    print()

    header = f'{"Benchmark":<20s} {"Low%":>6s} {"Mid%":>6s} {"High%":>6s}  {"Distribution"}'
    print(header)
    print('-' * 80)

    for bench in SELECTED_BENCHMARKS:
        if bench not in results:
            continue
        d = results[bench]
        name = SHORT_NAMES.get(bench, bench)
        bar_l = '▓' * int(round(d['low_pct'] / 5))
        bar_m = '▒' * int(round(d['mid_pct'] / 5))
        bar_h = '░' * int(round(d['high_pct'] / 5))
        print(f'{name:<20s} {d["low_pct"]:5.1f}% {d["mid_pct"]:5.1f}% {d["high_pct"]:5.1f}%  '
              f'{bar_l}{bar_m}{bar_h}')

    print('-' * 80)
    print('Legend: ▓=Low  ▒=Mid  ░=High  (each char ≈ 5%)')

    # === Detailed Fine-Grained Histograms ===
    print()
    print('=' * 90)
    print('Fine-Grained Timeout Histograms (100-cycle bins, top bins per benchmark)')
    print('=' * 90)

    for bench in SELECTED_BENCHMARKS:
        if bench not in results:
            continue
        d = results[bench]
        name = SHORT_NAMES.get(bench, bench)
        print(f'\n--- {name} (Low={d["low_pct"]:.1f}% Mid={d["mid_pct"]:.1f}% High={d["high_pct"]:.1f}%) ---')

        fine = d['fine_pct']
        # Show all bins with > 0.5% contribution, sorted by bin start
        significant = [(b, p) for b, p in sorted(fine.items()) if p >= 0.5]
        if not significant:
            significant = sorted(fine.items(), key=lambda x: -x[1])[:5]

        for b, p in significant:
            end = b + 99 if b < 3200 else '+'
            bar = '█' * int(round(p / 2))
            region = 'L' if b < LOW_UPPER else ('M' if b < MID_UPPER else 'H')
            print(f'  [{b:>4d}-{end:>4}] {p:5.1f}% {bar} ({region})')

    # === TSV output ===
    tsv_path = os.path.join(os.path.dirname(RESULTS_DIR), 'craft_timeout_distribution.tsv')
    with open(tsv_path, 'w') as f:
        f.write('benchmark\tshort_name\tlow_pct\tmid_pct\thigh_pct\n')
        for bench in SELECTED_BENCHMARKS:
            if bench not in results:
                continue
            d = results[bench]
            name = SHORT_NAMES.get(bench, bench)
            f.write(f'{bench}\t{name}\t{d["low_pct"]:.1f}\t{d["mid_pct"]:.1f}\t{d["high_pct"]:.1f}\n')
    print(f'\n\nTSV written: {tsv_path}')

    # === Full histogram TSV (for plotting) ===
    hist_tsv_path = os.path.join(os.path.dirname(RESULTS_DIR), 'craft_timeout_histogram.tsv')
    with open(hist_tsv_path, 'w') as f:
        # Header: bin_start, then each benchmark
        names = [SHORT_NAMES.get(b, b) for b in SELECTED_BENCHMARKS if b in results]
        f.write('bin_start\t' + '\t'.join(names) + '\n')
        # All bin starts
        all_bins = sorted(set(b for d in results.values() for b in d['fine_pct']))
        for b in all_bins:
            vals = []
            for bench in SELECTED_BENCHMARKS:
                if bench not in results:
                    continue
                vals.append(f'{results[bench]["fine_pct"].get(b, 0.0):.2f}')
            f.write(f'{b}\t' + '\t'.join(vals) + '\n')
    print(f'Histogram TSV written: {hist_tsv_path}')


if __name__ == '__main__':
    main()
