#!/usr/bin/python3

import sys
import json as js
import numpy as np
import matplotlib.pyplot as plt

sys.path.append("../utils_py/")
import myutil
from myplot import MyPlot
import matplotlib.patches as mpatches
from test_priority import memory_tests
from test_priority import no_memory_tests

save_path = '../pdf/1.vcache_stat.pdf'

with open('vcache_stat.json') as f :
    js_data = js.load(f)

input = [
    ["la864-l1", "IPCP-E"],
]
work_list = [datum[0] for datum in input]
label = [datum[1] for datum in input]

tests_list = [key for key in js_data[work_list[0]].keys()]
for bad in no_memory_tests :
    if bad in tests_list:
        tests_list.remove(bad)
for interest in memory_tests :
    if interest in tests_list:
        tests_list.remove(interest)
tests_list = memory_tests + tests_list

y1_data = []
xs      = []
for i,test in enumerate(tests_list) :
    xs.append(myutil.preprocess_name(test))
    l2_load_hit = js_data[work_list[0]][test]['L2C_load_hit_num']
    l2_load_miss = js_data[work_list[0]][test]['L2C_load_miss_num']
    l2_pref_hit = js_data[work_list[0]][test]['L2C_pref_hit_num']
    l2_pref_miss = js_data[work_list[0]][test]['L2C_pref_miss_num']
    total =  l2_load_hit + l2_load_miss + l2_pref_hit + l2_pref_miss
    y1_data.append([(l2_load_hit+l2_pref_hit)/total, (l2_load_miss+l2_pref_miss)/total])

xs.append('.AVG')
y1_data.append(np.mean(y1_data, axis=0))
y1_data = np.array(y1_data)

ycat = ['L2 Hit','L2 Miss']

print(y1_data)

def pre_hook_func() :
    plt.gca().set_yticks([ x for x in np.arange(0,1.01,0.25)])
    plt.subplots_adjust(top=0.6)

def post_hook_func() :
    plt.xlim(-0.5, len(xs)-0.5)
    plt.ylim(0,1)
    plt.subplots_adjust(bottom=0.38,top=0.83)
    plt.gca().axhline(y=16,ls='--',zorder=0.51,color='#222666')
    y1_patch = mpatches.Patch(facecolor=color[0],hatch='', label=ycat[0])
    y2_patch = mpatches.Patch(facecolor=color[1],hatch='', label=ycat[1])
    plt.legend(handles=[y1_patch,y2_patch], loc='upper center', bbox_to_anchor=(0.5, 1.27), ncol=2, frameon=False, fontsize=12)

def my_set_xtickslabel_size(ax,cfg):
    ax.set_yticklabels(ax.get_yticklabels(), fontsize=12)

color = [
    '#8eb3c8',
    '#dfa677',
]
hatch=['', '', '']

fig_cfg = {
    'type': 'linebar',

    # X Data
    'x': xs,
    # X Label
    'xgroup': True,
    'xgroup_kwargs': {
        'delimiter': myutil.delimiter,
        'minlevel': 1,
        'yfactor': 1.9,
        'yoffset': 0.1,
        'line_kwargs': {
            'lw': 0.7,
        },
        'text_kwargs': lambda lvl: {
            'rotation': 60 if lvl == 0 else 0,
            'fontsize': 11,
        },
    },

    'yaxes': [
        {
            'y': y1_data,
            'type': 'bar',
            'marker': '*',
            'color': [color[0], color[1], color[1]],
            'hatch': hatch,
            'label': ycat,
            'axlabel': 'Hit / Miss Percentage',
            'axlabel_kwargs': {
                'fontsize': 12,
            },
            'grouped_bar_kwargs': {
                'group_width': 0.8,
                'padding': 0.01,
            },
             'grid': True,
             'grid_below': True,
             'grid_kwargs': {
                'linestyle': '--',
                'axis':'y',
             },
            'legend': False,
            'legend_kwargs': {
                'frameon' : False,
                'ncol' : 3,
                'loc' : 'upper center',
                'bbox_to_anchor' : (0.5, 1.17),
                'fontsize' : 12,
            },
            'post_yax_hook': my_set_xtickslabel_size,
        },
    ],

    'figsize' : [15,3.5],

    'pre_main_hook': pre_hook_func,
    'post_main_hook': post_hook_func,

    # Misc
    'tight': False,

    # Save
    'save_path': save_path
}


if __name__ == '__main__':
    plot = MyPlot(fig_cfg)
    plot.plot()
    myutil.pdfcrop(save_path)
