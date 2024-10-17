#!/usr/bin/python3

import sys
import json as js
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
from matplotlib.ticker import FuncFormatter
import matplotlib.lines as mlines

sys.path.append("../utils_py/")
import myutil
from myplot import MyPlot
from test_priority import bad_tests
from test_priority import interest_tests

with open('speedup.json') as f :
    speedup_js_data = js.load(f)

with open('metadata_storage.json') as f :
    storage_js_data = js.load(f)

with open('metadata_conflict.json') as f :
    conflict_js_data = js.load(f)

speedup_input = [
    ["stride-l1", "Stide"],
    ["cmc-cfg0", "w/o opt A & B"],
    ["cmc-cfg1", "w/o opt A"],
    ["cmc-cfg2", "w/o opt B"],
    ["cmc", "Augur"],
]

storage_input = [
    ["cmc-cfg0-metadata-storage", "w/o opt A & B"],
    ["cmc-cfg1-metadata-storage", "w/o opt A"],
    ["cmc-cfg2-metadata-storage", "w/o opt B"],
    ["cmc-metadata-storage", "Augur"],
]

conflict_input = [
    ["cmc-cfg0-metadata-conflict", "w/o opt A & B"],
    ["cmc-cfg1-metadata-conflict", "w/o opt A"],
    ["cmc-cfg2-metadata-conflict", "w/o opt B"],
    ["cmc-metadata-conflict", "Augur"],
]

label = [datum[1] for datum in conflict_input]
print("Label: ", label)

save_path = '../pdf/6.eval.component_contribution.pdf'

color = [
    '#b5ccc4',
    '#f3d27d',
    '#c66e60',
    '#688fc6',
] 
line_color = '#222666'

#### Calculate Speedup
base = speedup_input[0][0]
speedup_input.remove(speedup_input[0])
speedup_ycat = [datum[0] for datum in speedup_input]
speedup_geomean = []
for method in speedup_ycat:
    speedup_test_data = {}
    for test in speedup_js_data[base].keys():
        speedup_test_data[test] = (speedup_js_data[method][test] / speedup_js_data[base][test])
    
    speedup_geomean.append(myutil.cal_gmean(speedup_test_data))

speedup_geomean = np.array([speedup_geomean])
print("Speedup Geomean: ", speedup_geomean)


#### Calculate Metadata Storage
storage_ycat = [datum[0] for datum in storage_input]
base = storage_js_data["cmc-metadata-storage"]
storage_geomean = []
for method in storage_ycat:
    data = {}
    for key,value in storage_js_data[method].items():
        data[key] = value / base[key]
    storage_geomean.append(myutil.cal_gmean(data))

storage_geomean = np.array([storage_geomean])
print("Storage Geomean: ", storage_geomean)


#### Calculate Metadata Conflict
conflict_average = []
conflict_ycat = [datum[0] for datum in conflict_input]
for method in conflict_ycat:
    data = []
    for value in conflict_js_data[method].values():
        data.append(value * 100)
    conflict_average.append(np.mean(data))

conflict_average = np.array([conflict_average])
print("Conflict Average: ", conflict_average)


#### Start Plotting
xpad = 0.5
xlim = (-1+xpad, 1-xpad)

#### Speedup Plotting Config
def speedup_pre_hook_func() :
    return

def speedup_post_hook_func(ax,cfg) :
    ax.set_xticks([])
    return

def speedup_my_set_xtickslabel_size(ax,cfg):
    ax.set_xlim(xlim)
    ax.set_ylim(0, 2.5)
    ax.set_yticks([ x/100.0 for x in range(0,255,50)])
    ax.set_yticklabels(ax.get_yticklabels(), fontsize=16)
    ax.get_yticklabels()[0].set_verticalalignment('bottom')
    ax.get_yticklabels()[-1].set_verticalalignment('top')
    return

speedup_fig_cfg = {
    'type': 'linebar',

    # X Data
    'x': ["label"],
    'xgroup': False,

    'yaxes': [
        {
            'y': speedup_geomean,
            'type': 'grouped_bar',
            'marker': '*',
            'color': color,
            'label': label,
            'axlabel': "Speedup",
            'axlabel_kwargs': {
                'fontsize': 20,
            },
            'grid': True,
            'grid_below': True,
            'grid_kwargs': {
               'linestyle': '--',
               'axis':'y',
            },
            'grouped_bar_kwargs' :{
                'group_width': 0.8,
                'padding': 0,
            },
            'post_yax_hook': speedup_my_set_xtickslabel_size,
        },
    ],

    'pre_main_hook': speedup_pre_hook_func,
    'post_main_hook': speedup_post_hook_func,
}


#### Metadata Conflict Config
def subfig2_pre_hook_func() :
    return

def subfig2_post_hook_func(ax,cfg) :
    ax.set_xticks([])
    plt.gcf().subplots_adjust(top=0.8)
    return

def storage_my_set_xtickslabel_size(ax,cfg):
    ax.set_ylim(0, 5.0)
    new_ytick_labels = [f"{tick:.1f}" for tick in ax.get_yticks()]
    ax.set_yticklabels(new_ytick_labels, fontsize=16)
    ax.get_yticklabels()[0].set_verticalalignment('bottom')
    ax.get_yticklabels()[-1].set_verticalalignment('top')
    return

def conflict_my_set_xtickslabel_size(ax,cfg):
    ax.set_xlim(xlim)
    ax.set_ylim(0, 100)
    ax.set_yticklabels(ax.get_yticklabels(), fontsize=16)
    ax.get_yticklabels()[0].set_verticalalignment('bottom')
    ax.get_yticklabels()[-1].set_verticalalignment('top')


    handle1 = mlines.Line2D([], [], color=line_color, linestyle='--', marker='*', linewidth=2,
                             markersize=12, label='Metadata Conflict')
    plt.legend(handles=[handle1],
               frameon=False,ncol=7,loc='upper center',bbox_to_anchor=(0.5, 1.13),fontsize=20)

    return

conflict_fig_cfg = {
    'type': 'linebar',

    # X Data
    'x': ["label"],
    'xgroup': False,

    'yaxes': [
        {
            'y': storage_geomean,
            'type': 'grouped_bar',
            'marker': '*',
            'color': color,
            'label': label,
            'axlabel': "Metadata Storage",
            'axlabel_kwargs': {
                'fontsize': 20,
            },
            'grid': True,
            'grid_below': True,
            'grid_kwargs': {
               'linestyle': '--',
               'axis':'y',
            },
            'grouped_bar_kwargs' :{
                'group_width': 0.8,
                'padding': 0,
            },
            'legend': True,
            'legend_kwargs': {
                'frameon' : False,
                'ncol' : 2,
                'loc' : 'lower center',
                'bbox_to_anchor' : (-0.65, 0.97),
                'fontsize' : 20,
                'labelspacing': 0.2,
                'handletextpad' : 0.5,
                'columnspacing': 1
            },
            'post_yax_hook': storage_my_set_xtickslabel_size,
        },
        {
            'y': conflict_average,
            'type': 'grouped_line',
            'side': 'right',
            'marker': '*',
            'line_kwargs': {
                "lw": 2, 
                "color": '#222666',
                "linestyle": '--',
                "markersize": 12,

            },
            'grouped_line_kwargs': {
                "group_width": 0.8,
            },

            'color': [line_color]*len(label),
            'axlabel': "Metadata Conflict",
            'axlabel_kwargs': {
                'fontsize': 20,
            },
            'post_yax_hook': conflict_my_set_xtickslabel_size,
        },
    ],

    'pre_main_hook': subfig2_pre_hook_func,
    'post_main_hook': subfig2_post_hook_func,
}


fig_cfg = {
    "figsize": [12,7],
    "subplot": True,
    "gridspec_kwargs": {
        "nrows": 1,
        "ncols": 2,
        "hspace": 0,
        "wspace": 0.35,
    },
    "subplots_kwargs": {
        "sharex": False,
        "sharey": False,
    },
    "subplot_cfgs": [
        dict(speedup_fig_cfg, pos=(0, 0)),
        dict(conflict_fig_cfg, pos=(0, 1)),
    ],

    # Misc
    'tight': False,
    
    # Save
    'save_path': save_path,
}

if __name__ == '__main__':
    plot = MyPlot(fig_cfg)
    plot.plot()

    myutil.pdfcrop(save_path)