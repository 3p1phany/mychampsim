import os
import numpy as np

def try_idx(x, idx):
    if isinstance(x, list):
        return x[idx]
    elif callable(x):
        return x(idx)
    else:
        return x


def y2btm(y2d):
    btm2d = np.zeros(y2d.shape)

    for i in range(y2d.shape[1]):
        btm2d[:, i] = np.sum(y2d[:, 0:i], axis=1)

    return btm2d


def y2normalized(y2d):
    return y2d / np.sum(y2d, axis=1)[:, np.newaxis]


def find_range_bound(xs):
    ranges, bounds = [], []

    xs = [None] + list(xs) + [None]  # add sentinels
    last_idx = -1

    for (idx, (x, xp)) in enumerate(zip(xs, xs[1:])):
        if x != xp:
            bounds.append(idx)
            if last_idx != -1:
                ranges.append(((last_idx, idx), x))
            last_idx = idx

    return ranges, bounds

def nth_repl(s, sub, repl, n):
    find = s.find(sub)
    # If find is not -1 we have found at least one match for the substring
    i = find != -1
    # loop util we find the nth or we find no match
    while find != -1 and i != n:
        # find + 1 means we start searching from after the last match
        find = s.find(sub, find + 1)
        i += 1
    # If i is equal to n we found nth match so replace
    if i == n:
        return s[:find] + repl + s[find+len(sub):]
    return s

delimiter = '.'

    #     if(name.find("32pad0") != -1):
    #         return 'ListInsert-32'
    #     elif(name.find("32pad3") != -1):
    #         return 'ListInsert-56'
    else:
        return " ."+name
    return name


def cal_gmean(data) :
    list = [data[key] for key in data.keys()]
    a = 1
    for b in list:
        a = a * b
    l = len(list)
    return pow(a, 1/l)

class pltcolor:
    __rgb_blue0 = [[ 91,155,213], [222,235,247], [189,215,238], \
                   [157,195,230], [ 46,117,182], [ 31, 78,121]]
    __rgb_blue1 = [[ 68,114,196], [218,227,243], [180,199,231], \
                   [143,170,220], [ 47, 85,151], [ 32, 56,100]]
    __rgb_blue_dark  = [[ 68, 84,106], [214,220,229], [173,185,202], \
                        [132,151,176], [ 51, 63, 80], [ 34, 42, 53]]
    __rgb_orange = [[237,125, 49], [251,229,214], [248,203,173], \
                    [244,177,131], [197, 90, 17], [132, 60, 11]]
    __rgb_yellow = [[255,192,  0], [255,242,204], [255,230,153], \
                    [255,217,102], [191,144,  0], [126, 96,  0]]
    __rgb_green  = [[112,173, 71], [226,240,217], [197,224,180], \
                    [169,209,142], [ 84,130, 53], [ 56, 87, 35]]
                    [ 64, 64, 64], [ 38, 38, 38], [ 13, 13, 13]]
    __rgb_grey0  = [[255,255,255], [242,242,242], [217,217,217], \
                    [191,191,191], [166,166,166], [126,126,126]]
    __rgb_grey1  = [[165,165,165], [237,237,237], [219,219,219], \
                    [201,201,201], [124,124,124], [ 83, 83, 83]]
    __rgb_grey2  = [[231,230,230], [208,206,206], [175,171,171], \
                    [118,113,113], [ 59, 56, 56], [ 24, 23, 23]]
    __rgb_pink   = [[240,137,167], [246,160,198], [217,134,181], \
                    [182,101,149], [151, 71,117], [121, 45, 94]]

    rgb = {}
    pct = {}

    color_west = [[
             [ 77,  0, 10], # Bordeaux   Red
             [  0,149,182], # Bondi      Blue
             [143, 75, 40]  # Mummy      Brown
            ],[
             [  0, 49, 82], # Prussian   Blue       PB
             [  0, 47,167], # Yves Klein Blue       IKB
             [ 26, 85,153]  # Capri      Blue
            ],[
             [128,  0, 30], # Burgundy   Red
             [128,216,204], # Tiffany    Blue
             [177, 89, 35]  # Rosso Tiziano
            ]]
    color_our = [
            [[158, 46, 34],[226,176, 65],[ 66,124, 86],[ 65,138,179],[153,179,204]],
            [[206,182, 74],[218,228,230],[194,196,195],[141, 42, 97],[ 16, 20, 32]],
            [[141, 81, 44],[184,206,142],[ 79,164,133],[108, 52, 27],[ 71,146,185]]
            ]
    color_west_pct = []
    color_our_pct = []

    def __init__(self):
        for l in self.color_west :
            ll = [[(xx/255) for xx in x] for x in l]
            self.color_west_pct.append(ll)
        for l in self.color_our :
            ll = [[(xx/255) for xx in x] for x in l]
            self.color_our_pct.append(ll)
        for i in range(0, 6):
            self.rgb["blue0"] = self.__rgb_blue0
            self.rgb["blue1"] = self.__rgb_blue1
            self.rgb["bluedark"] = self.__rgb_blue_dark
            self.rgb["orange"] = self.__rgb_orange
            self.rgb["yellow"] = self.__rgb_yellow
            self.rgb["green"] = self.__rgb_green
            self.rgb["black"] = self.__rgb_black
            self.rgb["grey0"] = self.__rgb_grey0
            self.rgb["grey1"] = self.__rgb_grey1
            self.rgb["grey2"] = self.__rgb_grey2
            self.rgb["pink"] = self.__rgb_pink

            self.pct["blue0"] = [[(xx/255) for xx in x] for x in self.__rgb_blue0]
            self.pct["blue1"] = [[(xx/255) for xx in x] for x in self.__rgb_blue1]
            self.pct["bluedark"] = [[(xx/255) for xx in x] for x in self.__rgb_blue_dark]
            self.pct["orange"] = [[(xx/255) for xx in x] for x in self.__rgb_orange]
            self.pct["yellow"] = [[(xx/255) for xx in x] for x in self.__rgb_yellow]
            self.pct["green"] = [[(xx/255) for xx in x] for x in self.__rgb_green]
            self.pct["black"] = [[(xx/255) for xx in x] for x in self.__rgb_black]
            self.pct["grey0"] = [[(xx/255) for xx in x] for x in self.__rgb_grey0]
            self.pct["grey1"] = [[(xx/255) for xx in x] for x in self.__rgb_grey1]
            self.pct["grey2"] = [[(xx/255) for xx in x] for x in self.__rgb_grey2]
            self.pct["pink"] = [[(xx/255) for xx in x] for x in self.__rgb_pink]

color = pltcolor()


def get_color(cat:list):
    candi6 = [
        '#8eb3c8',
        '#dfa677',
        '#b5ccc4',
        '#688fc6',
        '#c66e60',
        '#495a4f',
    ] 
    candi2 = [
        '#dfa677',
        '#688fc6',
    ]
    candi4 = [
        '#688fc6',
        '#dfa677',
        '#b5ccc4',
        '#495a4f',
    ]
    candi5 = [
        '#b5ccc4',
        '#dfa677',
        '#688fc6',
        '#495a4f',
        '#6cc160',
    ]
    
    if len(cat)==6:
        return candi6
    elif len(cat)==2:
        return candi2
    elif len(cat)==4:
        return candi4
    elif len(cat)==5:
        return candi5

def pdfcrop(pdf_file):
    os.system(f'pdfcrop --noverbose {pdf_file} {pdf_file}')