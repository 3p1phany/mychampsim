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


with open('stride.json') as f :
    base_data = js.load(f)['stride-l1']
with open('ldret_speedup.json') as f :
    ldret_data = js.load(f)
with open('ldidt_speedup.json') as f :
    ldidt_data = js.load(f)

color = [
    '#688fc6',
    '#c66e60',
    '#b5ccc4',
    '#f3d27d',
] 
marker = [
    'X',
    'P',
    'h',
    '*',
]

save_path = '../pdf/6.eval.design-parameter.pdf'


candi_ldret_cat = [8, 16, 32, 48, 64]
candi_ldidt_cat = [4,  8, 16, 24, 32]
assert(len(candi_ldret_cat)==len(candi_ldidt_cat))

base_geomean = myutil.cal_gmean(base_data)
yname_base = ['cmc-ldret-', 'cmc-ldidt-']
ydata = []
for idx in range(len(candi_ldret_cat)) :

    y1 = myutil.cal_gmean(ldret_data[yname_base[0]+str(candi_ldret_cat[idx])]) / base_geomean
    y2 = myutil.cal_gmean(ldidt_data[yname_base[1]+str(candi_ldidt_cat[idx])]) / base_geomean

    ydata.append([y1,y2])

    print(f'geomean@{idx} :{y1}, {y2}')

xs = [ '0.25x', '0.5x', '1.0x', '1.5x', '2.0x']
ydata = np.array(ydata)
ycat = ['LHT Cap', 'LIT Cap']

def pre_hook_func() :
    # plt.ylim(0,100)
    return
    #plt.gca().set_yticks([ x/100.0 for x in range(0,160,25)])

def post_hook_func() :
    # plt.gca().set_yticks([ x/100.0 for x in range(80,130,10)])
    #plt.gca().yaxis.set_major_formatter(mticker.PercentFormatter(xmax=1,decimals=0))

    plt.xlim(-0.5, len(xs)-0.5)
    # plt.gca().axhline(y=1.0,ls='-',zorder=1,color='red')
    plt.ylim(2.0, 2.3)
    ax = plt.gca()
    ax.set_xticklabels(ax.get_xticklabels(), fontsize=20)
    ax.set_yticklabels(ax.get_yticklabels(), fontsize=16)
    return

def my_set_xtickslabel_size(ax,cfg):
    pass


fig_cfg = {
    'type': 'linebar',

    # X Data
    'x': xs,

    'yaxes': [
        {
            'y': ydata,
            'type': 'line',
            'marker': marker,
            'color': color,
            'label': ycat,
            'axlabel': 'Speedup',
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
                'bbox_to_anchor' : (0.5, 1.12),
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