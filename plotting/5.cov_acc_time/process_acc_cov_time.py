#!/usr/bin/python3

import sys
import json as js
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.ticker import FuncFormatter

sys.path.append("../utils_py/")
import myutil
from myplot import MyPlot
from test_priority import bad_tests
from test_priority import interest_tests

with open('cov_acc_time.json') as f :
    cov_acc_time = js.load(f)

input = [
    ["ipcp", "IPCP"],
    ["berti", "BERTI"],
    ["la864", "IPCP-E"],
]
color = [
    '#f3d27d',
    '#c66e60',
    '#8eb3c8',
    '#688fc6',
    '#dfa677',
] 

save_path = '../pdf/5.cover_acc_time.pdf'

work_list = [datum[0] for datum in input]
label = [datum[1] for datum in input]

tests_list = [key for key in cov_acc_time[work_list[0]].keys()]
for bad in bad_tests :
    if bad in tests_list:
        tests_list.remove(bad)
for interest in interest_tests :
    if interest in tests_list:
        tests_list.remove(interest)
tests_list = interest_tests + tests_list

def cal_cov(data) :
    if data['L1D_origin_miss'] == 0:
        return 1
    else:
        return (1 - data['L1D_pref_miss']/data['L1D_origin_miss'])

def cal_acc(data) :
    if data['L1D_pref_total_num'] == 0:
        return 0
    else:
        return (1 - data['L1D_pref_useless_num'] / data['L1D_pref_total_num'])

def cal_timeliness(data) :
    if data['L1D_pref_total_num'] == 0:
        return 0
    else:
        return (1 - data['L1D_pref_late_num'] / data['L1D_pref_total_num'])

xs      = []
cov_ydata = []
acc_ydata = []
timeliness_ydata = []

for test in tests_list :
    xs.append(myutil.preprocess_name(test))
    
    cov_ydata_item = []
    acc_ydata_item = []
    timeliness_ydata_item = []
    for work in work_list:
        cov_ydata_item.append(cal_cov(cov_acc_time[work][test]))
        acc_ydata_item.append(cal_acc(cov_acc_time[work][test]))
        timeliness_ydata_item.append(cal_timeliness(cov_acc_time[work][test]))
    cov_ydata.append(cov_ydata_item)
    acc_ydata.append(acc_ydata_item)
    timeliness_ydata.append(timeliness_ydata_item)

xs.append('.AVG')
cov_ydata.append(np.mean(cov_ydata, axis=0))
acc_ydata.append(np.mean(acc_ydata, axis=0))
timeliness_ydata.append(np.mean(timeliness_ydata, axis=0))

print("Coverage of", label)
for i in range(len(xs)):
    print(f"{xs[i]}: {cov_ydata[i]}")

print("\nAccuracy of", label)
for i in range(len(xs)):
    print(f"{xs[i]}: {acc_ydata[i]}")

print("\nTimeliness:", timeliness_ydata[-1])
for i in range(len(xs)):
    print(f"{xs[i]}: {timeliness_ydata[i]}")

cov_ydata = np.array(cov_ydata)
acc_ydata = np.array(acc_ydata)
timeliness_ydata = np.array(timeliness_ydata)

def pre_hook_func() :
    plt.gca().set_yticks([ x for x in np.arange(0,1.01,0.25)])
    plt.subplots_adjust(bottom=0.2)

    
def my_set_xtickslabel_size(ax,cfg):
    ax.set_yticklabels(ax.get_yticklabels(), fontsize=14)
    ax.get_yticklabels()[0].set_verticalalignment('bottom')
    ax.get_yticklabels()[-1].set_verticalalignment('top')

def gen_config(data, axlabel:str, first:bool, last:bool):

    def post_hook_func() :
        plt.xlim(-0.5, len(xs)-0.5)
        plt.ylim(0,1)
        def format_func(value, tick_number):
            if value == 0:
                return '0'
            else:
                return '{:.0%}'.format(value)
        plt.gca().yaxis.set_major_formatter(FuncFormatter(format_func))

        plt.xticks(rotation=60)

        ax = plt.gca()
        ax.set_xticklabels(ax.get_xticklabels(), fontsize=14)

        if not last:
            ax.set_xticks([])

    fig_cfg = {
        'type': 'linebar',
    
        # X Data
        'x': xs,

        'xgroup': last,
            'xgroup_kwargs': {
            'delimiter': myutil.delimiter,
            'minlevel': 1,
            'yfactor': 5.0,
            'yoffset': 0.05,
            'line_kwargs': {
                'lw': 0.7,
            },
            'text_kwargs': lambda lvl: {
                'rotation': 90 if lvl == 0 else 0,
                'fontsize': 13,
            },
        },

        'yaxes': [
            {
                'y': data,
                'type': 'grouped_bar',
                'marker': '*',
                'color': color,
                'label': label,
                'axlabel': axlabel,
                'axlabel_kwargs': {
                    'fontsize': 15,
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
                'legend': first,
                'legend_kwargs': {
                    'frameon' : False,
                    'ncol' : 8,
                    'loc' : 'upper center',
                    'bbox_to_anchor' : (0.5, 1.18),
                    'fontsize' : 14,
                    'columnspacing': 1.2,
                    'handletextpad' : 0.6,
                },
                'post_yax_hook': my_set_xtickslabel_size,
            },
        ],
    
        'pre_main_hook': pre_hook_func,
        'post_main_hook': post_hook_func,
    }

    return fig_cfg

use_subplot_example = True
if use_subplot_example:
    cov = dict(gen_config(cov_ydata, "Coverage", True, False), pos=(0, 0))
    acc = dict(gen_config(acc_ydata, "Accuracy", False, False), pos=(1, 0))
    timeliness = dict(gen_config(timeliness_ydata, "Timeliness", False, True), pos=(2, 0))
    fig_cfg = {
        "figsize": [12, 9.5],
        "subplot": True,
        "gridspec_kwargs": {
            "nrows": 3,
            "ncols": 1,
            "hspace": 0.07,
            "wspace": 0,
        },
        "subplots_kwargs": {
            "sharex": False,
            "sharey": True,
        },
        "subplot_cfgs": [cov, acc, timeliness],

        # Misc
        'tight': True,
    
        # Save
        'save_path': save_path,
    }

if __name__ == '__main__':
    plot = MyPlot(fig_cfg)
    plot.plot()

    myutil.pdfcrop(save_path)