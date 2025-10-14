#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
遍历 <root> 下的 *.champsim.trace.xz，取去掉扩展名后的“完整前缀”作为 key，
在 weights.json 中取权重，构建 benchmarks.tsv。

Usage:
  python3 build_benchmarks_from_fs.py weights.json \
      -r ~/Trace/LA \
      -o benchmarks.tsv \
      --ignore ~/Trace/LA/test \
      --skip-no-weight        #（可选）若 weights.json 缺权重则跳过该切片
"""
import argparse, json, os, re, sys
from collections import defaultdict

SUFFIX = ".champsim.trace.xz"
# 用于从前缀中提取“最后一个下划线后的数字”，用于切片排序
START_RE = re.compile(r"^(?P<head>.+)_(?P<start>\d+)$")

def is_under(path, parents):
    """path 是否位于 parents 中任一目录之下"""
    apath = os.path.abspath(path)
    for p in parents:
        try:
            if os.path.commonpath([apath, p]) == p:
                return True
        except ValueError:
            pass
    return False

def main():
    ap = argparse.ArgumentParser(description="Build benchmarks.tsv by scanning filesystem and indexing weights.json by prefix")
    ap.add_argument("weights", help="path to weights.json")
    ap.add_argument("-r", "--root", default="~/Trace/LA", help="trace root (default: %(default)s)")
    ap.add_argument("-o", "--output", default="benchmarks.tsv", help="output TSV (default: %(default)s)")
    ap.add_argument("--ignore", action="append", default=[], help="directories to ignore (repeatable)")
    ap.add_argument("--skip-no-weight", action="store_true",
                    help="skip a trace if its prefix key not found in weights.json (default: include with weight=0 and warn)")
    args = ap.parse_args()

    root = os.path.abspath(os.path.expanduser(args.root))
    weights_path = os.path.abspath(os.path.expanduser(args.weights))
    out_path = os.path.abspath(os.path.expanduser(args.output))

    # 默认忽略 <root>/test（可被 --ignore 覆盖/追加）
    ignore_dirs = [os.path.join(root, "test")]
    for ig in (args.ignore or []):
        ignore_dirs.append(os.path.abspath(os.path.expanduser(ig)))
    # 规范化去重
    ignore_dirs = sorted(set(ignore_dirs))

    # 载入权重
    with open(weights_path, "r", encoding="utf-8") as f:
        weights = json.load(f)

    print(f"[INFO] root={root}", file=sys.stderr)
    if ignore_dirs:
        print("[INFO] ignoring:", file=sys.stderr)
        for d in ignore_dirs:
            print("       " + d, file=sys.stderr)

    # 收集：benchmark_dir -> [(start_int, weight, abs_path)]
    groups = defaultdict(list)
    total = 0
    missing_weight = 0
    bad_name = 0

    for dirpath, dirnames, filenames in os.walk(root):
        # 若当前目录位于忽略列表下，剪枝
        if is_under(dirpath, ignore_dirs):
            dirnames[:] = []  # 不再向下
            continue
        # 进一步过滤将要递归的子目录
        dirnames[:] = [d for d in dirnames if not is_under(os.path.join(dirpath, d), ignore_dirs)]

        for fn in filenames:
            if not fn.endswith(SUFFIX):
                continue
            abs_path = os.path.join(dirpath, fn)
            prefix = fn[:-len(SUFFIX)]  # 完整前缀（用于查权重）
            m = START_RE.match(prefix)
            if not m:
                bad_name += 1
                print(f"[WARN] filename doesn't match '<...>_<digits>{SUFFIX}': {abs_path}", file=sys.stderr)
                # 没有可排序的 start，就当 0 处理（仍可输出）
                start_int = 0
            else:
                start_int = int(m.group("start"))

            # 查权重
            w = weights.get(prefix, None)
            if w is None:
                missing_weight += 1
                msg = f"[WARN] weight not found for prefix key '{prefix}'"
                if args.skip_no_weight:
                    print(msg + " -> SKIP", file=sys.stderr)
                    continue
                else:
                    print(msg + " -> use 0.0", file=sys.stderr)
                    w = 0.0

            # benchmark 名 = 目录相对 root
            rel_dir = os.path.relpath(dirpath, root)
            if rel_dir == ".":
                rel_dir = "."  # 若 trace 就在 root 下

            groups[rel_dir].append((start_int, float(w), abs_path))
            total += 1

    # 输出
    rows = 0
    with open(out_path, "w", encoding="utf-8") as out:
        out.write("# benchmark\tslice\tweight\ttrace_path\n")
        for bench in sorted(groups.keys()):
            entries = sorted(groups[bench], key=lambda x: x[0])
            for slice_id, (start_int, weight, path) in enumerate(entries):
                out.write(f"{bench}\t{slice_id}\t{weight:.12f}\t{path}\n")
                rows += 1

    print(f"[OK] wrote {out_path} with {rows} rows.", file=sys.stderr)
    print(f"[STAT] scanned={total}, missing_weight={missing_weight}, bad_name={bad_name}", file=sys.stderr)

if __name__ == "__main__":
    main()

