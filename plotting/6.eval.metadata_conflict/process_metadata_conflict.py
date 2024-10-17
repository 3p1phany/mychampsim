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

with open('metadata_conflict.json') as f :
    js_data = js.load(f)

input = [
    # ["isb-metadata-conflict", "ISB"],
    ["misb-metadata-conflict", "MISB"],
    ["triage-l1-metadata-conflict", "Triage"],
    ["triangel-metadata-conflict", "Triangel"],
    ["cmc-metadata-conflict", "Augur"]
]
color = [
    # '#dfa677',
    '#b5ccc4',
    '#f3d27d',
    '#c66e60',
    '#688fc6',
] 
marker = [
    'X',
    'P',
    'h',
    '*',
]

save_path = '../pdf/6.eval.metadata-conflict.pdf'

ycat = [datum[0] for datum in input]
label = [datum[1] for datum in input]

tests_list = [ key for key in js_data[ycat[0]].keys()]
for bad in bad_tests :
    if bad in tests_list:
        tests_list.remove(bad)
for interest in interest_tests :
    if interest in tests_list:
        tests_list.remove(interest)
tests_list = interest_tests + tests_list

xs = [ myutil.preprocess_name(key) for key in tests_list]
xs.append('AVG')

ydata = []
for key in tests_list:
    ydata.append([js_data[cat][key]*100 for cat in ycat])
    print('test:{0} value:{1}'.format(key,ydata[-1]))

ydata.append(np.mean(ydata, axis=0))

print('avg: {0}'.format(ydata[-1]))

ydata = np.array(ydata)

def pre_hook_func() :
    # plt.ylim(0,100)
    return
    #plt.gca().set_yticks([ x/100.0 for x in range(0,160,25)])

def post_hook_func() :
    # plt.gca().set_yticks([ x/100.0 for x in range(80,130,10)])
    #plt.gca().yaxis.set_major_formatter(mticker.PercentFormatter(xmax=1,decimals=0))

    plt.xlim(-0.5, len(xs)-0.5)
    # plt.gca().axhline(y=1.0,ls='-',zorder=1,color='red')
    plt.ylim(0,100)
    ax = plt.gca()
    ax.set_yticklabels(ax.get_yticklabels(), fontsize=16)
    return

def my_set_xtickslabel_size(ax,cfg):
    pass


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
        'yfactor': 1.0,
        'yoffset': 0.2,
        'line_kwargs': {
            'lw': 0.7,
        },
        'text_kwargs': lambda lvl: {
            'rotation': 20,#90 if lvl == 0 else 0,
            'fontsize': 17,
        },
    },

    'yaxes': [
        {
            'y': ydata,
            'type': 'grouped_bar',
            'marker': marker,
            'color': color,
            'label': label,
            'axlabel': 'Metadata Conflict (%)',
            'axlabel_kwargs': {
                'fontsize': 20,
            },
            'line_kwargs': {
                'markersize': 10,
                'lw': 2,
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

if __name__ == '__main__':
    plot = MyPlot(fig_cfg)
    plot.plot()

    myutil.pdfcrop(save_path)