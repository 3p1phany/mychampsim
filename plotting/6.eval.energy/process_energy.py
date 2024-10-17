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


with open('energy.json') as f :
    js_data = js.load(f)

input = [
    ["no", "No"],
    ["misb-l2", "MISB"],
    ["triage-l2", "Triage"],
    ["triangel-l2", "Triangel"],
    ["cmc", "Augur"],
]
color = [
    '#b5ccc4',
    '#f3d27d',
    '#c66e60',
    '#688fc6',
] 
save_path = '../pdf/6.eval.energy.pdf'

base = input[0][0]
input.remove(input[0])

ycat = [datum[0] for datum in input]
label = [datum[1] for datum in input]

assert(len(js_data[ycat[0]].keys()) ==
       len(js_data[ycat[1]].keys()) ==
       len(js_data[ycat[2]].keys()) ==
       len(js_data[ycat[3]].keys()) )

tests_list = [key for key in js_data[base].keys()]
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
        y.append(js_data[method][test] / js_data[base][test])
    test_data.append({'x':x,'y':y})

geomean = []
base_geomeam = myutil.cal_gmean(js_data[base])
for method in ycat :
    geomean.append(myutil.cal_gmean(js_data[method])/base_geomeam)
    print('prefetcher-{0}, GeoMean Energy: {1}'.format(method, geomean[-1]))
test_data.append({'x':myutil.preprocess_name('Geomean'),'y':geomean})

xs     = [datum['x'] for datum in test_data]
ydata = np.array([datum['y'] for datum in test_data])
for data in ydata:
    assert(len(label) == len(data))

def pre_hook_func() :
    return
    #plt.gca().set_yticks([ x/100.0 for x in range(0,160,25)])

def post_hook_func() :
    plt.gca().set_yticks([ x/100.0 for x in range(0,410,100)])
    plt.xlim(-0.5, len(xs)-0.5)
    plt.gca().axhline(y=1.0,ls='-',zorder=0.51,color='#222666')
    plt.ylim(top=4.0)
    ax = plt.gca()
    ax.set_yticklabels(ax.get_yticklabels(), fontsize=16)
    return


def my_set_xtickslabel_size(ax,cfg):
    # height = 2.79

    # for rect in ax.containers[-2].patches:
    #     if rect.get_height() > 2.9:
    #         x = rect.get_x() + rect.get_width() / 2
    #         y = rect.get_y() + rect.get_height() + 0.1
    #         ax.text(x - 0.37, height, '%.2f' % y, ha='left', va='bottom', fontsize=9)


    # for rect in ax.containers[-1].patches:
    #     if rect.get_height() > 2.9:
    #         x = rect.get_x() + rect.get_width() / 2
    #         y = rect.get_y() + rect.get_height() + 0.1
    #         ax.text(x + 0.07, height, '%.2f' % y, ha='left', va='bottom', fontsize=9)
    return

fig_cfg = {
    'type': 'linebar',

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
            'rotation': 20,
            'fontsize': 17,
        },
    },


    'yaxes': [
        {
            'y': ydata,
            'type': 'grouped_bar',
            'marker': '*',
            'color': color,
            'label': label,
            'axlabel': 'Energy Consumption',
            'axlabel_kwargs': {
                'fontsize': 20,
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
                'bbox_to_anchor' : (0.5, 1.15),
                'fontsize' : 20,
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