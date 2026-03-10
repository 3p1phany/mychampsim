#!/usr/bin/env python3
"""Plot CRAFT_PRECHARGE IPC vs ABP+DYMPL+INTAP on top-12 benchmarks."""

import os
import math
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import matplotlib.ticker as mticker
import numpy as np

# ── collect data ──────────────────────────────────────────────────────────
results_dir = os.path.join(os.path.dirname(__file__), '..', 'results')
results_dir = os.path.abspath(results_dir)

excluded_prefixes = ['GS_', 'oracle', 'open_page', 'smart_close', 'close_page']
data = {}
for dirname in sorted(os.listdir(results_dir)):
    summary = os.path.join(results_dir, dirname, 'summary.tsv')
    if not os.path.isfile(summary):
        continue
    if any(dirname.startswith(p) or dirname == 'GS_1c' for p in excluded_prefixes):
        continue
    data[dirname] = {}
    with open(summary) as f:
        for line in f:
            line = line.strip()
            if line.startswith('benchmark') or not line:
                continue
            parts = line.split('\t')
            data[dirname][parts[0]] = float(parts[1])

bls = ['ABP_1c', 'DYMPL_1c', 'INTAP_1c']
cv = 'CRAFT_PRECHARGE_1c'
benchmarks = sorted(data[bls[0]].keys())

# select winning benchmarks, sort by improvement over best baseline, take top 12
winning = []
for b in benchmarks:
    max_bl = max(data[bl][b] for bl in bls)
    imp = (data[cv][b] - max_bl) / max_bl * 100
    if imp > 0:
        winning.append((b, imp))
winning.sort(key=lambda x: x[1], reverse=True)
selected = [b for b, _ in winning[:12]]

# ── short names ───────────────────────────────────────────────────────────
def short_name(bench):
    parts = bench.split('/')
    if len(parts) == 3:
        suite, prog, inp = parts
        if suite in ('spec06', 'spec17'):
            return f"{prog}({suite[-2:]})"
        return f"{prog}/{inp}"
    elif len(parts) == 2:
        return '/'.join(parts)
    return bench

short_labels = [short_name(b) for b in selected]

# ── normalize to CRAFT = 1.0 ─────────────────────────────────────────────
policies = ['ABP', 'DYMPL', 'INTAP', 'CRAFT']
policy_keys = bls + [cv]
colors = ['#4e79a7', '#f28e2b', '#76b7b2', '#59a14f']

norm_data = {p: [] for p in policies}
for b in selected:
    craft_ipc = data[cv][b]
    for p, key in zip(policies, policy_keys):
        norm_data[p].append(data[key][b] / craft_ipc)

# append GEOMEAN
for p, key in zip(policies, policy_keys):
    ratios = [data[key][b] / data[cv][b] for b in selected]
    geo = math.exp(sum(math.log(r) for r in ratios) / len(ratios))
    norm_data[p].append(geo)

labels = short_labels + ['GEOMEAN']
n = len(labels)

# ── plot ──────────────────────────────────────────────────────────────────
fig, ax = plt.subplots(figsize=(14, 5))

bar_w = 0.18
x = np.arange(n)
for i, p in enumerate(policies):
    offset = (i - 1.5) * bar_w
    ax.bar(x + offset, norm_data[p], bar_w,
           label=p, color=colors[i], edgecolor='white', linewidth=0.4)

# y-axis range
ymin = 0.86
ax.set_ylim(ymin, 1.02)
ax.set_yticks(np.arange(ymin, 1.021, 0.02))
ax.yaxis.set_minor_locator(mticker.MultipleLocator(0.01))

ax.set_ylabel('Normalized IPC (CRAFT = 1.0)', fontsize=12)
ax.set_xticks(x)
ax.set_xticklabels(labels, rotation=40, ha='right', fontsize=9)

# GEOMEAN separator
ax.axvline(x=n - 1.5, color='gray', linestyle='--', linewidth=0.8)

# reference line at 1.0
ax.axhline(y=1.0, color='black', linestyle='-', linewidth=0.6, zorder=0)

ax.legend(loc='lower left', ncol=4, fontsize=10, framealpha=0.9)
ax.grid(axis='y', linestyle=':', alpha=0.4)
ax.set_xlim(-0.6, n - 0.4)

plt.title('CRAFT (RS+RW+SD) vs Baselines — Top 12 Benchmarks\n'
          'Normalized IPC (higher is better, CRAFT = 1.0)',
          fontsize=13, pad=10)

plt.tight_layout()
out = os.path.join(results_dir, '..', 'craft_vs_baselines.png')
out = os.path.abspath(out)
plt.savefig(out, dpi=200, bbox_inches='tight')
print(f'Saved to {out}')
