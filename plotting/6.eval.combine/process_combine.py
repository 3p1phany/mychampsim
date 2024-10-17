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


with open('combine_speedup.json') as f :
    ipcs = js.load(f)

input = [
    ["base", "Base"],
    ["cmc-domino", "Augur-Domino"],
    ["cmc-isb", "Augur-ISB"],
    ["cmc-misb", "Augur-MISB"],
    ["cmc-triage", "Augur-Triage"],
]

color = [
    '#8eb3c8',
    '#b5ccc4',
    '#f3d27d',
    '#c66e60',
    '#688fc6',
    '#dfa677',
    '#495a4f',
] 
save_path = '6.eval.combine.pdf'

base = input[0][0]
input.remove(input[0])

ycat = [datum[0] for datum in input]
label = [datum[1] for datum in input]

assert(len(ipcs[ycat[0]].keys()) ==
       len(ipcs[ycat[1]].keys()) ==
       len(ipcs[ycat[2]].keys()) ==
       len(ipcs[ycat[3]].keys()) )


def cal_gmean(data) :
    list = [data[key] for key in data.keys()]
    a = 1
    for b in list:
        a = a * b
    l = len(list)
    return pow(a, 1/l)

tests_list = [key for key in ipcs[base].keys()]
for bad in bad_tests :
    if bad in tests_list:
        tests_list.remove(bad)
for interest in interest_tests :
    if interest in tests_list:
        tests_list.remove(interest)
tests_list = interest_tests + tests_list

test_data = []
for test in tests_list:
    x = myutil.preprocess_name(test) 
    y = []
    for method in ycat :
        y.append(ipcs[method][test] / ipcs[base][test])
    test_data.append({'x':x,'y':y})

geomean = []
base_geomeam = myutil.cal_gmean(ipcs[base])
for method in ycat :
    geomean.append(myutil.cal_gmean(ipcs[method])/base_geomeam)
    print('prefetcher-{0}, GeoMean IPC: {1}'.format(method, geomean[-1]))
test_data.append({'x':myutil.preprocess_name('-GM'),'y':geomean})

xs     = [datum['x'] for datum in test_data]
ydata = np.array([datum['y'] for datum in test_data])
for data in ydata:
    assert(len(label) == len(data))

def pre_hook_func() :
    return
    #plt.gca().set_yticks([ x/100.0 for x in range(0,160,25)])

def post_hook_func() :
    plt.gca().set_yticks([ x/100.0 for x in range(0,260,50)])
    plt.xlim(-0.5, len(xs)-0.5)
    plt.gca().axhline(y=1.0,ls='-',zorder=0.51,color='#222666')
    plt.ylim(top=3.0)
    return


def cat2color(cat):
    mapping = {
        ycat[0]: 'skyblue',
        ycat[1]: 'grey',
        ycat[2]: 'green',
        ycat[3]: 'red',
        # ycat[4]: 'black',
    }

    for (k, v) in mapping.items():
        if k in cat:
            return v

fig_cfg = {
    'type': 'linebar',
    #'title': 'test title',

    # X Data
    'x': xs,
    # X Label
    # 'xlabel': 'ratio',
    'xgroup': True,
    'xgroup_kwargs': {
        'delimiter': myutil.delimiter,
        'minlevel': 1,
        'yfactor': 0.6,
        'yoffset': 0.2,
        'line_kwargs': {
            'lw': 0.7,
        },
        'text_kwargs': lambda lvl: {
            'rotation': 45#90 if lvl == 0 else 0
        },
    },


    'yaxes': [
        {
            'y': ydata,
            'type': 'grouped_bar',
            'marker': '*',
            #'color': [cat2color(cat) for cat in ycat],
            #'color': myutil.get_color(ycat), 
            'color': color,
            'label': label,
            'axlabel': 'Speedup',
            'axlabel_kwargs': {
                'fontsize': 12,
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
                'ncol' : 10,
                'loc' : 'upper center',
                'bbox_to_anchor' : (0.5, 1.16),
                'fontsize' : 12,
            }
        },
    ],

    'figsize' : [8,3.5],

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