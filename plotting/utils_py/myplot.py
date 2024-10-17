import functools

import matplotlib.pyplot as plt
import numpy as np
from matplotlib.lines import Line2D
from matplotlib.transforms import Affine2D
from scipy.interpolate import make_interp_spline

from myutil import try_idx, y2btm, y2normalized, find_range_bound


class MyPlot:
    # @formatter:off
    # @staticmethod
    def expect(*keys):
        def decorator(func):
            @functools.wraps(func)
            def wrapper(self, *args, **kwargs):
                cfg = args[1] if len(args) >= 2 else self.cfg
                for key in keys:
                    if key not in cfg:
                        return
                return func(self, *args, **kwargs)
            return wrapper
        return decorator

    # @staticmethod
    def expectTrue(*keys):
        def decorator(func):
            @functools.wraps(func)
            def wrapper(self, *args, **kwargs):
                cfg = args[1] if len(args) >= 2 else self.cfg
                for key in keys:
                    if key not in cfg:
                        return
                    if not cfg[key]:
                        return
                return func(self, *args, **kwargs)
            return wrapper
        return decorator

    # @staticmethod
    def hook(name):
        def decorator(func):
            @functools.wraps(func)
            def wrapper(self, *args, **kwargs):
                ax = args[0] if len(args) >= 2 else plt.gca()
                cfg = args[1] if len(args) >= 2 else self.cfg
                def invoke_cfg_hook(hook_name):
                    from inspect import signature
                    def dummy(ax, cfg):
                        pass
                    hook_func = cfg.get(hook_name, dummy)
                    hook_nargs = len(signature(hook_func).parameters)
                    assert hook_nargs in [0, 2]
                    if hook_nargs == 2:
                        hook_func(ax, cfg)
                    else:
                        hook_func()

                invoke_cfg_hook(f"pre_{name}_hook")
                ret = func(self, *args, **kwargs)
                invoke_cfg_hook(f"post_{name}_hook")
                return ret
            return wrapper
        return decorator
    # @formatter:on

    def __init__(self, cfg=None):
        if cfg is None:
            cfg = {}
        self.cfg = cfg
        self.fig = None

    def setup_figure(self):
        self.fig = plt.figure(figsize=self.cfg.get('figsize', None))
        if 'title' in self.cfg:
            plt.title(self.cfg['title'])

    def draw_label(self, ax, cfg):
        if 'xlabel' in cfg:
            xlabel_kwargs = cfg.get("xlabel_kwargs", {})
            ax.set_xlabel(cfg['xlabel'], **xlabel_kwargs)
        if 'ylabel' in cfg:
            ylabel_kwargs = cfg.get("ylabel_kwargs", {})
            ax.set_ylabel(cfg['ylabel'], **ylabel_kwargs)

    def plot(self):
        self.setup_figure()

        if 'subplot' in self.cfg and self.cfg['subplot']:
            cfgs = self.cfg['subplot_cfgs']
        else:
            cfgs = [self.cfg]

        if 'gridspec_kwargs' in self.cfg:
            gs = self.fig.add_gridspec(**self.cfg['gridspec_kwargs'])
            subplots_kwargs = self.cfg['subplots_kwargs'] if 'subplots_kwargs' in self.cfg else {}
            axes = gs.subplots(**subplots_kwargs)
            nrows = self.cfg['gridspec_kwargs']['nrows']
            ncols = self.cfg['gridspec_kwargs']['ncols']
            axes = np.array(axes).reshape(nrows, ncols)

        for cfg in cfgs:
            if 'gridspec_kwargs' in self.cfg:
                assert 'pos' in cfg
                ax = axes[cfg['pos']]
            else:
                gs = [cfg['grid_spec']] if 'grid_spec' in cfg else []
                ax = self.fig.add_subplot(*gs)
            self.draw_plot(ax, cfg)

        self.setup_tight()
        self.save()
        self.show()

    def draw_plot(self, ax, cfg):
        self.draw_main(ax, cfg)

        self.draw_anno(ax, cfg)
        self.draw_grid(ax, cfg)

        self.draw_xgroup(ax, cfg)

        self.draw_label(ax, cfg)

    @hook("main")
    def draw_main(self, ax, cfg):
        mapping = {
            'scatter': self.draw_scatter,
            'linebar': self.draw_linebar,
            'pie': self.draw_pie,
        }
        mapping.get(cfg['type'], lambda self: None)(ax, cfg)

    @expectTrue("grid")
    @hook("grid")
    def draw_grid(self, ax, cfg):
        kwargs = cfg.get('grid_kwargs', {})
        if 'grid_linestyle' in cfg:
            kwargs['linestyle'] = cfg['grid_linestyle']
        if 'grid_below' in cfg and cfg['grid_below']:
            ax.set_axisbelow(True)
        ax.grid(cfg['grid'], **kwargs)

    @expectTrue("legend")
    def draw_legend(self, ax, cfg):
        kwargs = cfg.get('legend_kwargs', {})
        ax.legend(**kwargs)

    @expectTrue("tight")
    @hook("tight")
    def setup_tight(self):
        plt.tight_layout()

    @expect('x', 'y')
    def draw_scatter(self, ax, cfg):
        # `marker` cannot be a list of str
        # plot one point a time, to work around
        for (idx, (x, y)) in enumerate(zip(cfg['x'], cfg['y'])):
            kwargs = {}
            if 'marker' in cfg:
                kwargs['marker'] = cfg['marker'][idx]
            if 'color' in cfg:
                kwargs['c'] = cfg['color'][idx]
            if 'size' in cfg:
                kwargs['s'] = cfg['size'][idx]

            ax.scatter(x, y, **kwargs)

    @expect('x', 'yaxes')
    def draw_linebar(self, ax, cfg):
        ax1 = ax

        x = cfg['x']

        bar_cols = 0
        for (yaxidx, yax) in enumerate(cfg['yaxes']):
            if yax['type'] in ('bar', 'normalized_bar'):
                bar_cols += 1

        bar_cols_idx = 0
        for (yaxidx, yax) in enumerate(cfg['yaxes']):
            ax = ax1
            if 'side' in yax and yax['side'] == 'right':
                ax = ax1.twinx()
            self.draw_yax(ax, yax, x, bar_cols=bar_cols, bar_cols_idx=bar_cols_idx)
            if yax['type'] in ('bar', 'normalized_bar'):
                bar_cols_idx += 1

        plt.sca(ax1)

    @staticmethod
    def draw_line(ax, x, yax, **kwargs):
        y = yax['y']
        for (catidx, cat) in enumerate(y.T):
            kwargs = try_idx(yax.get("line_kwargs", {}), catidx)
            if 'marker' in yax:
                kwargs['marker'] = try_idx(yax['marker'], catidx)
            if 'color' in yax:
                kwargs['c'] = try_idx(yax['color'], catidx)
            if 'linestyle' in yax:
                kwargs['ls'] = try_idx(yax['linestyle'], catidx)
            if 'label' in yax:
                kwargs['label'] = try_idx(yax['label'], catidx)
            ax.plot(x, cat, **kwargs)

    @staticmethod
    def draw_spline_line(ax, x, yax, **kwargs):
        y = yax['y']

        xrange = list(range(len(y)))
        xsmooth = np.linspace(0, len(y) - 1, 100 * (len(y) - 1))

        for (catidx, cat) in enumerate(y.T):
            kwargs = {}
            if 'color' in yax:
                kwargs['c'] = try_idx(yax['color'], catidx)
            if 'linestyle' in yax:
                kwargs['ls'] = try_idx(yax['linestyle'], catidx)
            spline = make_interp_spline(xrange, cat)
            ysmooth = spline(xsmooth)
            ax.plot(xsmooth, ysmooth, **kwargs)

            if 'marker' in yax:
                kwargs['marker'] = yax['marker']
            kwargs['ls'] = ''
            ax.plot(x, cat, **kwargs)

    @staticmethod
    def draw_bar(ax, x, yax, **kwargs):
        bar_cols = kwargs.get('bar_cols', 1)
        bar_cols_idx = kwargs.get('bar_cols_idx', 0)

        kwargs.pop('bar_cols')
        kwargs.pop('bar_cols_idx')

        y = yax['y']
        if yax['type'] == 'normalized_bar':
            y = y2normalized(y)

        xrange = np.arange(y.shape[0])
        group_width = 0.7 #grouped_bar_kwargs.get("group_width", 1.0)
        padding = 0.05 #grouped_bar_kwargs.get("padding", 0.)
        width = (group_width + padding) / (bar_cols)
        xpos = xrange - (group_width + padding) / 2 + (bar_cols_idx + 0.5) * width

        btm = y2btm(y)
        for (catidx, (cat, bb)) in enumerate(zip(y.T, btm.T)):
            if 'color' in yax:
                kwargs['color'] = try_idx(yax['color'], catidx)
            if 'hatch' in yax:
                kwargs['hatch'] = try_idx(yax['hatch'], catidx)
                kwargs['alpha'] = 0.99
            if 'label' in yax:
                kwargs['label'] = try_idx(yax['label'], catidx)
            ax.bar(xpos, cat, width=width - padding, bottom=bb, **kwargs)
            # ax.bar(xrange - group_width / 2 + (catidx + 1) * width, cat, width=width - padding, **kwargs)


    @staticmethod
    def draw_grouped_bar(ax, x, yax, **kwargs):
        grouped_bar_kwargs = yax.get("grouped_bar_kwargs", {})

        y = yax['y']

        xrange = np.arange(y.shape[0])
        group_width = grouped_bar_kwargs.get("group_width", 1.0)
        padding = grouped_bar_kwargs.get("padding", 0.)
        width = group_width / (y.shape[1] + 1)

        ax.set_xticks(xrange, x)
        for (catidx, cat) in enumerate(y.T):
            kwargs = {}
            if 'color' in yax:
                kwargs['color'] = try_idx(yax['color'], catidx)
            if 'label' in yax:
                kwargs['label'] = try_idx(yax['label'], catidx)
            xpos = xrange - group_width / 2 + (catidx + 1) * width
            ax.bar(xpos, cat, width=width - padding, **kwargs)

    @staticmethod
    def draw_grouped_line(ax, x, yax, **kwargs):
        grouped_line_kwargs = {'color':'black', 'linestyle':"--", "lw":1}
        grouped_line_kwargs.update( yax.get("grouped_line_kwargs", {}))

        y = yax['y']
        n_cats = y.shape[1]

        xrange = np.arange(y.shape[0])
        group_width = grouped_line_kwargs.get("group_width", 1)
        padding = grouped_line_kwargs.get("padding", 0.)
        width = group_width / (n_cats + 1)

        for (grpidx, grp) in enumerate(y):
            kwargs = yax.get("line_kwargs", {})
            xpos = grpidx - group_width / 2 + (np.arange(n_cats) + 1) * width
            ax.plot(xpos, grp, **kwargs)

        ax.set_xticks(xrange, x)
        for (catidx, cat) in enumerate(y.T):
            kwargs = yax.get("line_kwargs", {})
            if 'color' in yax:
                kwargs['color'] = try_idx(yax['color'], catidx)
            if 'label' in yax:
                kwargs['label'] = try_idx(yax['label'], catidx)
            if 'marker' in yax:
                kwargs['marker'] = try_idx(yax['marker'], catidx)
            kwargs['linestyle'] = 'None'
            xpos = xrange - group_width / 2 + (catidx + 1) * width
            ax.plot(xpos, cat, **kwargs)

    @expect('x', 'y')
    def draw_pie(self, ax, cfg):
        x = cfg['x']
        y = cfg['y']
        pie_kwargs = cfg.get("pie_kwargs", {})
        ## if x_show
        if cfg['x_show']:
            ax.pie(y, labels=x, **pie_kwargs)
        else:
            ax.pie(y, **pie_kwargs)


    @hook("yax")
    def draw_yax(self, ax, yax, x, **kwargs):
        mapping = {
            'line': MyPlot.draw_line,
            'spline_line': MyPlot.draw_spline_line,
            'bar': MyPlot.draw_bar,
            'normalized_bar': MyPlot.draw_bar,
            'grouped_bar': MyPlot.draw_grouped_bar,
            'grouped_line': MyPlot.draw_grouped_line,
        }
        mapping[yax['type']](ax, x, yax, **kwargs)
        if 'axlabel' in yax:
            axlabel_kwargs = yax.get('axlabel_kwargs', {})
            ax.set_ylabel(yax['axlabel'], **axlabel_kwargs)
        if 'grid' in yax:
            self.draw_grid(ax, yax)
        if 'legend' in yax:
            self.draw_legend(ax, yax)

    @expect("anno", 'x', 'y')
    @hook("anno")
    def draw_anno(self, ax, cfg):
        offset_x = cfg.get('anno_offset_x', 0)
        offset_y = cfg.get("anno_offset_y", 0)
        for (anno_idx, (anno, x, y)) in enumerate(zip(cfg['anno'], cfg['x'], cfg['y'])):
            kwargs = try_idx(cfg.get("anno_kwargs", {}), anno_idx)
            ax.annotate(anno, (x + offset_x, y + offset_y), **kwargs)

    @expect("save_path")
    def save(self):
        plt.savefig(self.cfg['save_path'])

    def show(self):
        plt.show()

    @expectTrue("xgroup")
    def draw_xgroup(self, ax, cfg):
        kwargs = cfg.get("xgroup_kwargs", {})
        delimiter = kwargs.get('delimiter', '.')
        x_split = [x.split(delimiter) for x in cfg['x']]

        xnum = len(cfg['x'])
        width = 1 / xnum

        (xmin, xmax) = ax.get_xlim()
        xrange = xmax - xmin

        (ymin, ymax) = ax.get_ylim()
        yrange = ymax - ymin

        xscale = xnum / xrange
        yfactor = kwargs.get('yfactor', 0.5)  # y scaling factor defaults to 0.5
        yoffset = kwargs.get('yoffset', 0.2)
        yscale = yrange / plt.gcf().get_size_inches()[1] * yfactor
        xxlate = - (xmin + 0.5) / xrange
        yxlate = ymin
        xaffine = Affine2D().scale(xscale, yscale).translate(xxlate, yxlate)
        trans = xaffine + ax.get_yaxis_transform()

        x_split_T = [*zip(*x_split)]

        ax.tick_params(labelbottom=False)  # disable built-in label
        minlvl = kwargs.get('minlevel', 0)
        for (lvl, x_split_lvl) in enumerate(reversed(x_split_T)):
            line_kwargs = try_idx(kwargs.get('line_kwargs', {}), lvl)
            text_kwargs = try_idx(kwargs.get('text_kwargs', {}), lvl)
            ranges, bounds = find_range_bound(x_split_lvl)
            if lvl >= minlvl:
                for b in bounds:
                    pos_x = ((b * width), (b * width))
                    pos_y = (0, (-lvl - yoffset))
                    ax.add_line(Line2D(pos_x, pos_y,
                                       transform=trans, clip_on=False, color="k", **line_kwargs))
            for ((l, r), x) in ranges:
                pos_x = (l + r) / 2 * width
                pos_y = (-lvl - yoffset)
                ax.text(pos_x, pos_y, x,
                        horizontalalignment='center', verticalalignment='top',
                        clip_on=False, transform=trans, **text_kwargs)


if __name__ == '__main__':
    plot = MyPlot({'size': (12, 6)})
    plot.setup_figure()
