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

label = ['Triage', 'Triangel', 'AdaTP']
ycat  = ['triage-l2', 'triangel-l2', 'catp-l2']
 
color = [
    '#8eb3c8',
    '#688fc6',
    '#dfa677',
] 

save_path = '../pdf/4.result.metadata.pdf'

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

triage_l2 = json_data['triage-l2']
triangel_l2 = json_data['triangel-l2']
catp_l2 = json_data['catp-l2']

tests_list = [ key for key in triage_l2.keys()]
for bad in bad_tests :
    if bad in tests_list:
        tests_list.remove(bad)
for interest in interest_tests :
    if interest in tests_list:
        tests_list.remove(interest)
tests_list = interest_tests + tests_list

# xs = [ myutil.preprocess_name(key) for key in tests_list]
# xs.append(myutil.delimiter+'AVG')

xs = []
ydata = []
for test in tests_list:
    # ydata.append([triage[key]*100,triage_hitpref[key]*100,triage_opt[key]*100,cmc[key]*100])
    xs.append(myutil.preprocess_name(test)) 
    ydata.append([triage_l2[test],triangel_l2[test],catp_l2[test]])
    print('test:{0} value:{1}'.format(test,ydata[-1]))

xs.append('AVG.')
ydata.append(np.mean(ydata, axis=0))

print('avg: {0}'.format(ydata[-1]))

ydata = np.array(ydata)

def pre_hook_func() :
    plt.gca().set_yticks([ x/100.0 for x in range(0,800,100)])
    plt.ylim(0,8)
    return

def post_hook_func() :
    plt.xlim(-0.5, len(xs)-0.5)
    return

def my_set_xtickslabel_size(ax,cfg):
    ax.set_yticklabels(ax.get_yticklabels(), fontsize=18)

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
            'rotation': 45 if lvl == 0 else 0,
            'fontsize': 14,
        },
    },

    'yaxes': [
        {
            'y': ydata,
            'type': 'grouped_bar',
            'marker': '*',
            'color': color,
            'label': label,
            'axlabel': 'Metadata Storage (Ways)',
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
                'ncol' : 6,
                'loc' : 'upper center',
                'bbox_to_anchor' : (0.5, 1.18),
                'fontsize' : 18,
            },
            'post_yax_hook': my_set_xtickslabel_size,
        },
    ],

    'figsize' : [12,5],

    'pre_main_hook': pre_hook_func,
    'post_main_hook': post_hook_func,

    # Misc
    'tight': True,
    
    'save_path': save_path,
}

if __name__ == '__main__':
    plot = MyPlot(fig_cfg)
    plot.plot()

    myutil.pdfcrop(save_path)