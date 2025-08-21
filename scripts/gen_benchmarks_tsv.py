#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import argparse, sys, re
from pathlib import Path

TRACE_RE = re.compile(r'^(?P<prefix>.+)_(?P<start>\d+)\.champsim\.trace\.xz$')

def slice_abbr_from_start_str(start_s: str) -> str:
    return start_s[:-8] if start_s.endswith("00000000") else start_s

def parse_traces_in_dir(d: Path):
    """返回 {slice_abbr -> abs_trace_path}"""
    mapping = {}
    for p in sorted(d.glob("*.champsim.trace.xz")):
        m = TRACE_RE.match(p.name)
        if not m:
            continue
        abbr = slice_abbr_from_start_str(m.group("start"))
        mapping[abbr] = str(p.resolve())
    return mapping

def parse_simpoints(sp_path: Path):
    """simpoints: `<slice_abbr> <index>` → {index -> slice_abbr}"""
    idx2abbr = {}
    if not sp_path or not sp_path.is_file():
        return idx2abbr
    for line in sp_path.read_text(errors="ignore").splitlines():
        s = line.strip()
        if not s or s.startswith("#"): continue
        toks = s.split()
        if len(toks) < 2: continue
        abbr, idx = toks[0], toks[1]
        try:
            idx = int(idx)
        except ValueError:
            continue
        idx2abbr[idx] = abbr
    return idx2abbr

def parse_weights(w_path: Path):
    """weights: `<weight> <index>` → {index -> weight}"""
    idx2w = {}
    if not w_path or not w_path.is_file():
        return idx2w
    for line in w_path.read_text(errors="ignore").splitlines():
        s = line.strip()
        if not s or s.startswith("#"): continue
        toks = s.split()
        if len(toks) < 2: continue
        try:
            w = float(toks[0]); idx = int(toks[1])
        except ValueError:
            continue
        idx2w[idx] = w
    return idx2w

def find_nearest_meta(start_dir: Path, root: Path):
    """
    从 start_dir 一直向上到 root，分别找到“最近的” simpoints 和 weights（可来自不同层）。
    返回: (sp_path or None, sp_dir or None, wt_path or None, wt_dir or None)
    """
    cur = start_dir.resolve()
    root = root.resolve()
    sp_path = sp_dir = wt_path = wt_dir = None
    while True:
        sp = cur / "simpoints"
        wt = cur / "weights"
        if sp_path is None and sp.is_file():
            sp_path, sp_dir = sp, cur
        if wt_path is None and wt.is_file():
            wt_path, wt_dir = wt, cur
        if (sp_path is not None) and (wt_path is not None):
            break
        if cur == root:
            break
        nxt = cur.parent
        if nxt == cur:
            break
        cur = nxt
    return sp_path, sp_dir, wt_path, wt_dir

def main():
    ap = argparse.ArgumentParser(description="生成 benchmarks.tsv（支持祖先目录simpoints/weights；缺任一→均权）")
    ap.add_argument("--root", required=True, help="根目录，例如 ~/Trace/LA")
    ap.add_argument("--out", default="benchmarks.tsv", help="输出 TSV（默认 benchmarks.tsv）")
    ap.add_argument("--verbose", action="store_true", help="打印调试信息")
    args = ap.parse_args()

    root = Path(args.root).resolve()
    if not root.exists():
        print(f"根目录不存在：{root}", file=sys.stderr); sys.exit(1)

    rows = []
    trace_dirs = sorted(set(p.parent for p in root.rglob("*.champsim.trace.xz")))

    for d in trace_dirs:
        trace_map = parse_traces_in_dir(d)
        if not trace_map:
            continue

        try:
            bench = str(d.resolve().relative_to(root)).replace("\\", "/")
        except Exception:
            bench = d.name

        sp_path, sp_dir, wt_path, wt_dir = find_nearest_meta(d, root)

        if args.verbose:
            print(f"[DBG] {bench}: sp={sp_path if sp_path else '-'} (from {sp_dir if sp_dir else '-'}) ; "
                  f"wt={wt_path if wt_path else '-'} (from {wt_dir if wt_dir else '-'})", file=sys.stderr)

        # 两者都没凑齐 → 均权
        if (sp_path is None) or (wt_path is None):
            missing = []
            if sp_path is None: missing.append("simpoints")
            if wt_path is None: missing.append("weights")
            print(f"[WARN] 目录 {d}: 向上至 {root} 未凑齐 {', '.join(missing)}，该目录均权 1.0", file=sys.stderr)
            for abbr, abspath in trace_map.items():
                rows.append((bench, abbr, f"{1.0:.12f}", abspath))
            continue

        # 解析并通过“索引”关联
        idx2abbr = parse_simpoints(sp_path)
        idx2w    = parse_weights(wt_path)

        if not idx2abbr or not idx2w:
            print(f"[WARN] 目录 {d}: simpoints 或 weights 解析为空（sp来自{sp_dir}, wt来自{wt_dir}），均权 1.0", file=sys.stderr)
            for abbr, abspath in trace_map.items():
                rows.append((bench, abbr, f"{1.0:.12f}", abspath))
            continue

        used = 0
        intersect = sorted(set(idx2abbr.keys()) & set(idx2w.keys()))
        if not intersect:
            print(f"[WARN] 目录 {d}: simpoints 与 weights 索引无交集（sp来自{sp_dir}, wt来自{wt_dir}），均权 1.0", file=sys.stderr)
            for abbr, abspath in trace_map.items():
                rows.append((bench, abbr, f"{1.0:.12f}", abspath))
            continue

        for idx in intersect:
            abbr = idx2abbr[idx]
            w    = idx2w[idx]
            if abbr not in trace_map:
                # 注意：祖先 meta 可能覆盖多个子目录，本目录没有该切片就忽略
                if args.verbose:
                    print(f"[INFO] 目录 {d}: 索引 {idx} 的切片 '{abbr}' 不在本目录，已忽略", file=sys.stderr)
                continue
            rows.append((bench, abbr, f"{w:.12f}", trace_map[abbr]))
            used += 1

        if used == 0:
            print(f"[WARN] 目录 {d}: meta（sp来自{sp_dir}, wt来自{wt_dir}）未匹配到本目录任何切片，均权 1.0", file=sys.stderr)
            for abbr, abspath in trace_map.items():
                rows.append((bench, abbr, f"{1.0:.12f}", abspath))

    # 排序与输出
    rows.sort(key=lambda r: (r[0], int(r[1]) if r[1].isdigit() else r[1]))
    outp = Path(args.out)
    outp.parent.mkdir(parents=True, exist_ok=True)
    with outp.open("w", encoding="utf-8") as f:
        f.write("# benchmark\tslice\tweight\ttrace_path\n")
        for r in rows:
            f.write("\t".join(r) + "\n")

    print(f"完成：写入 {outp}（共 {len(rows)} 行）")

if __name__ == "__main__":
    main()
