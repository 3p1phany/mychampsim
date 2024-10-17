#!/usr/bin/python3

import sys
import json as js
import numpy as np
import matplotlib.pyplot as plt

sys.path.append("../utils_py/")
import myutil
from myplot import MyPlot
from test_priority import bad_tests
from test_priority import interest_tests


with open('speedup.json') as f :
    js_data = js.load(f)


input = [
    ["stride-l1", "SP"],
    ["ipcp", "IPCP"],
    ["berti", "Berti"],
    ["triage-l2", "Triage"],
    ["triangel-l2", "Triangel"],
    ["catp-l2", "AdaTP"],
]
color = [
    '#f3d27d',
    '#c66e60',
    '#8eb3c8',
    '#688fc6',
    '#dfa677',
] 
save_path = '../pdf/4.result.speedup.pdf'
label_pos = (0.5, 1.19)
label_fontsize = 12


base = input[0][0]
input.remove(input[0])

ycat = [datum[0] for datum in input]
label = [datum[1] for datum in input]

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

for bad in bad_tests :
    if bad in js_data[base].keys():
        del js_data[base][bad]
    for method in ycat:
        if bad in js_data[method].keys():
            del js_data[method][bad]

geomean = []
base_geomeam = myutil.cal_gmean(js_data[base])
for method in ycat :
    geomean.append(myutil.cal_gmean(js_data[method])/base_geomeam)
    print('prefetcher-{0}, GeoMean IPC: {1}'.format(method, geomean[-1]))
test_data.append({'x':myutil.preprocess_name('Geomean'),'y':geomean})

xs     = [datum['x'] for datum in test_data]
ydata = np.array([datum['y'] for datum in test_data])
for data in ydata:
    assert(len(label) == len(data))

for i in range(len(ydata)):
    print('xs: {0}, ydata: {1}'.format(xs[i], ydata[i]))

def pre_hook_func() :
    plt.gca().set_yticks([ x/100.0 for x in range(60,160,20)])
    plt.ylim(bottom=0.6,top=1.6)
    return
    #plt.gca().set_yticks([ x/100.0 for x in range(0,160,25)])

def post_hook_func() :
    plt.xlim(-0.5, len(xs)-0.5)
    plt.gca().axhline(y=1.0,ls='--',zorder=0.51,color='#222666')
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
        'yfactor': 0.3,
        'yoffset': 0.4,
        'line_kwargs': {
            'lw': 0.7,
        },
        'text_kwargs': lambda lvl: {
            'rotation': 0,
            'fontsize': 12,
        },
    },


    'yaxes': [
        {
            'y': ydata,
            'type': 'grouped_bar',
            'marker': '*',
            'color': color,
            'label': label,
            'axlabel': 'Speedup',
            'axlabel_kwargs': {
                'fontsize': 14,
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
                'bbox_to_anchor' : label_pos,
                'fontsize' : 11,
                'columnspacing': 1.45,
                'handletextpad' : 0.85,
            },
        },
    ],

    'figsize' : [14,3.5],

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