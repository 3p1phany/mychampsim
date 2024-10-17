#!/usr/bin/python3

import sys
import json as js
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.gridspec import GridSpec

sys.path.append("../utils_py/")
import myutil
from myplot import MyPlot
from test_priority import bad_tests
from test_priority import interest_tests


with open('speedup.json') as f :
    js_data = js.load(f)

input = [
    ["stride-l1", "Stride"],
    ["misb", "MISB"],
    ["triage-l1", "Triage"],
    ["triangel", "Triangel"],
    ["cmc", "Augur"],
]
color = [
    '#b5ccc4',
    '#f3d27d',
    '#c66e60',
    '#688fc6',
    '#688fc6',
    '#688fc6',
    '#688fc6',
] 
save_path = '../pdf/6.eval.sensitivity_degree.pdf'

base = input[0][0]
input.remove(input[0])

ycat = [datum[0] for datum in input]
label = [datum[1] for datum in input]

# assert(len(js_data[ycat[0]].keys()) ==
#        len(js_data[ycat[1]].keys()) ==
#        len(js_data[ycat[2]].keys()) ==
#        len(js_data[ycat[3]].keys()) ==
#        len(js_data[ycat[4]].keys())  )

tests_list = [key for key in js_data[base].keys()]
for bad in bad_tests :
    if bad in tests_list:
        tests_list.remove(bad)
for interest in interest_tests :
    if interest in tests_list:
        tests_list.remove(interest)
tests_list = interest_tests + tests_list

xs     = list(js_data[ycat[0]].keys())

base_geomean = myutil.cal_gmean(js_data[base])
cats_y = []
for cat in ycat:
    cat_y = []
    for x in sorted(js_data[cat].keys(),key=lambda x:int(x)):
        cat_y.append(myutil.cal_gmean(js_data[cat][x]))
        # print(cat, x, myutil.cal_gmean(js_data[cat][x]))
    
    for idx in range(len(cat_y)):
        cat_y[idx] = cat_y[idx] / base_geomean
        print(cat, xs[idx], cat_y[idx])
    cats_y.append(cat_y)

ydata = np.array(cats_y).T

for data in ydata:
    assert(len(label) == len(data))

def pre_hook_func() :
    return

def post_hook_func() :
    # plt.gca().set_yticks([ x/100.0 for x in range(110,130,5)])
    # plt.xlim(-0.5, len(xs)-0.5)
    return


def my_set_xtickslabel_size(ax,cfg):
    ax.set_ylim(1.1, 1.35)
    ax.set_xticklabels(ax.get_xticklabels(), fontsize=18)
    ax.set_yticklabels(ax.get_yticklabels(), fontsize=18)
    return

print("yfatay", data)
print(cats_y)

fig_cfg = {
    'type': 'linebar',

    # X Data
    'x': xs,

    'yaxes': [
        {
            'y': ydata,
            'type': 'line',
            'marker': ['^', 's', 'v', 'o', 'D'],
            'color': color,
            'line_kwargs': lambda catidx: {"lw": 2, 'markersize': 10},
            'label': label,
            'axlabel': 'Speedup',
            'axlabel_kwargs': {
                'fontsize': 18,
            },
            'grid': True,
            'grid_below': True,
            'grid_kwargs': {
               'linestyle': '--',
               'axis':'y',
            },
            'legend': True,
            'legend_kwargs': {
                'frameon' : False,
                'ncol' : 11,
                'loc' : 'upper center',
                'bbox_to_anchor' : (0.5, 1.12),
                'fontsize' : 18,
            },
            'post_yax_hook': my_set_xtickslabel_size,
        },
    ],

    'figsize' : [12,6],

    'pre_main_hook': pre_hook_func,
    'post_main_hook': post_hook_func,

    # Misc
    'tight': True,

    # Save
    'save_path': save_path
}

use_subplot_example = False
if use_subplot_example:
    subfig_cfg = fig_cfg
    gs = GridSpec(1, 2)
    cfg1 = dict(subfig_cfg, grid_spec=gs[0, 0])
    cfg2 = dict(subfig_cfg, grid_spec=gs[0, 1])
    fig_cfg = {
        'figsize': [12, 6],
        'subplot': True,
        'tight': True,
        'subplot_cfgs': [cfg1, cfg2]
    }

if __name__ == '__main__':
    plot = MyPlot(fig_cfg)
    plot.plot()

    myutil.pdfcrop(save_path)