#!/usr/bin/python3

import sys
import json as js
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.gridspec import GridSpec
import matplotlib.ticker as mticker

sys.path.append("../utils_py/")
from myplot import MyPlot
import myutil
from test_priority import bad_tests
from test_priority import interest_tests

with open('metadata_storage.json') as f :
    js_data = js.load(f)

input = [
    ["triage-l1", "Triage"],
    ["triangel", "Triangel"],
    ["cmc", "Augur"],
]

save_path = '../pdf/6.eval.metadata_storage.pdf'

color = [
    '#f3d27d',
    '#c66e60',
    '#688fc6',
]

ycat = [datum[0] for datum in input]
label = [datum[1] for datum in input]

tests_list = [key for key in js_data[ycat[0]].keys()]
for bad in bad_tests :
    if bad in tests_list:
        tests_list.remove(bad)
for interest in interest_tests :
    if interest in tests_list:
        tests_list.remove(interest)
tests_list = interest_tests + tests_list


#### Calculate Data
xs = []
ydata = []
for test in tests_list:
    xs.append(myutil.preprocess_name(test)) 
    ydata_item = []
    for method in ycat:
        ydata_item.append(js_data[method][test])
    ydata.append(ydata_item)

# xs.append('AVG')
# ydata.append(np.mean(ydata, axis=0))
ydata = np.array(ydata)
for i in range(len(ydata)):
    print(xs[i], list(ydata[i]))

def pre_hook_func() :
    ax = plt.gca()
    ax.set_yscale('log', base=2)
    ax.set_ylim((2<<12), (2<<21)+1) 
    # ax.set_ylim(0, (2<<21)+1) 
    return

def post_hook_func() :
    plt.xlim(-0.5, len(xs)-0.5)
    return

def my_set_xtickslabel_size(ax,cfg):
    ax.set_yticklabels(ax.get_yticklabels(), fontsize=18)
    print(ax.get_yticklabels())

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
        'yfactor': 1.0,
        'yoffset': 2**(-10),
        'line_kwargs': {
            'lw': 0.7,
        },
        'text_kwargs': lambda lvl: {
            'rotation': 20,
            'fontsize': 18,
        },
    },

    'yaxes': [
        {
            'y': ydata,
            'type': 'grouped_bar',
            'marker': '*',
            'color': color,
            'label': label,
            'axlabel': 'Trigger Address Number',
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
                'ncol' : 10,
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

if __name__ == '__main__':
    plot = MyPlot(fig_cfg)
    plot.plot()

    myutil.pdfcrop(save_path)