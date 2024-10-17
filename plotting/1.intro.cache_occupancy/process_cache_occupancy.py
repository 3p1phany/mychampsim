#!/usr/bin/python3

import sys
import json as js
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.gridspec import GridSpec

sys.path.append("../utils_py/")
import myutil
from myplot import MyPlot
import matplotlib.patches as mpatches

save_path = '../pdf/1.cache_occupancy.pdf'

with open('cache_occupancy.json') as f :
    js_data = js.load(f)

tests_list = [
"spec06_gcc_166_27700000000",
"spec06_gcc_166_31100000000",
"spec06_gcc_166_33900000000",
"spec06_gcc_166_41000000000",
"spec06_gcc_g23_76000000000",
"spec06_gcc_g23_103800000000",
"spec06_gcc_s04_53200000000",
"spec06_gcc_s04_100200000000",
]

y1_data = []
xs      = []
for i,test in enumerate(tests_list) :
    xs.append(myutil.preprocess_singel_name(test,i))
    unutilized = js_data[test]['metadata']/6 - js_data[test]['assoc'] / 6
    unutilized = unutilized if unutilized > 0 else 0
    y1_data.append([js_data[test]['data']/6, js_data[test]['assoc'],unutilized])

y1_data = np.array(y1_data)

ycat = ['Data Demand','Metadata Demand','Metadata Used by Triangel']

print(y1_data)

def pre_hook_func() :
    plt.gca().set_yticks([ x for x in range(0,56,8)])
    plt.subplots_adjust(top=0.6)

def post_hook_func() :
    plt.xlim(-0.5, len(xs)-0.5)
    plt.ylim(0,56)
    plt.subplots_adjust(bottom=0.28,top=0.83)
    plt.gca().axhline(y=16,ls='--',zorder=0.51,color='#222666')
    data_patch = mpatches.Patch(facecolor=color[0],hatch='', label=ycat[0])
    metadata_patch = mpatches.Patch(facecolor=color[1],hatch='', label=ycat[1])
    assoc_patch = mpatches.Patch(facecolor=color[1],hatch='\\\\\\\\\\', label=ycat[2])
    first_legend = plt.legend(handles=[data_patch, metadata_patch], loc='upper center', bbox_to_anchor=(0.5, 1.28), ncol=2, frameon=False, fontsize=12)
    plt.gca().add_artist(first_legend)
    plt.legend(handles=[assoc_patch], loc='upper center', bbox_to_anchor=(0.5, 1.17), frameon=False, fontsize=12)

def my_set_xtickslabel_size(ax,cfg):
    ax.set_yticklabels(ax.get_yticklabels(), fontsize=12)

# Change need to be done in myplot:
# group_width = 0.8 #grouped_bar_kwargs.get("group_width", 1.0)
# padding = 0.02 #grouped_bar_kwargs.get("padding", 0.)

color = [
    # '#8eb3c8',
    # '#b5ccc4',
    # '#c66e60',
    # '#f3d27d',
    # '#688fc6',
    '#dfa677',
    # '#495a4f',
    '#8eb3c8',
]
hatch=['', '\\\\\\\\\\', '']

import matplotlib as mpl
mpl.rcParams['hatch.linewidth'] = 0.6  # previous pdf hatch linewidth

fig_cfg = {
    'type': 'linebar',
    #'title': 'test title',

    # X Data
    'x': xs,
    # X Label
    #'xlabel': 'test x label',
    'xgroup': True,
    'xgroup_kwargs': {
        'delimiter': myutil.delimiter,
        'minlevel': 1,
        'yfactor': 0.8,
        'yoffset': 0.2,
        'line_kwargs': {
            'lw': 0.7,
        },
        'text_kwargs': lambda lvl: {
            'rotation': 20 if lvl == 0 else 0,
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
            'axlabel': 'Cache Ways',
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

    'figsize' : [6,3.5],

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
