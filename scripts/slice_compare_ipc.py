#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
对比两种策略在“所有切片”上的 IPC（不加权）
- 输入为两次运行的 results 目录（如 results/20250820-203125-oracle）
- 从 <results>/<benchmark>/<slice>/run.log 里解析 IPC（取日志中“最后一次匹配”的 IPC）
- 只比较两侧都存在且 IPC 有效的 (benchmark, slice)
- 输出列：benchmark, slice, ipc_A, ipc_B, delta(B-A), pct_change(%), speedup(B/A)
- 末尾附 __GEOMEAN__ 的 speedup（几何平均，不加权）

用法示例：
python3 scripts/compare_ipc_slices.py \
  --a results/20250820-203125-oracle \
  --b results/20250820-223945-close \
  --a-label oracle --b-label close \
  --out results/compare_slices_oracle_vs_close.tsv
"""

import argparse
import math
import re
import sys
from pathlib import Path

NUM = r'([0-9]*\.?[0-9]+(?:[eE][+-]?[0-9]+)?)'
IPC_PATTERNS = [
    re.compile(rf'CPU\s*\d+\s+cumulative\s+IPC\s*[:=]\s*{NUM}', re.IGNORECASE),
    re.compile(rf'Finished.*?cumulative\s+IPC\s*[:=]\s*{NUM}', re.IGNORECASE),
    re.compile(rf'overall\s+IPC\s*[:=]\s*{NUM}', re.IGNORECASE),
    re.compile(rf'\bIPC\b[^0-9\-+]*{NUM}', re.IGNORECASE),  # 兜底
]

def parse_ipc_from_log(log_path: Path):
    try:
        text = log_path.read_text(errors="ignore")
    except Exception:
        return None
    for pat in IPC_PATTERNS:
        m = pat.findall(text)
        if m:
            val = m[-1]
            if isinstance(val, (tuple, list)):
                val = val[0]
            try:
                return float(val)
            except Exception:
                continue
    return None

def scan_results(results_dir: Path):
    """
    扫描 results 目录，返回 {(benchmark, slice): ipc_float 或 None}
    - benchmark 是相对 results_dir 的多级路径（去掉最后一级 slice）
    - slice 是 run.log 上级目录名
    """
    mapping = {}
    for log in results_dir.rglob("run.log"):
        # rel = <benchmark>/<slice>
        rel = log.parent.relative_to(results_dir)
        if len(rel.parts) < 2:
            # 结构异常，跳过
            continue
        slc = rel.name
        bench = rel.parent.as_posix()  # 保持与 benchmarks.tsv 的风格一致（多级路径用/）
        ipc = parse_ipc_from_log(log)
        mapping[(bench, slc)] = ipc
    return mapping

def natural_key_slice(s):
    # 尝试按整数切片排序，否则按字符串
    try:
        return (0, int(s))
    except Exception:
        return (1, s)

def geomean(values):
    vals = [v for v in values if v > 0 and math.isfinite(v)]
    if not vals:
        return float('nan')
    return math.exp(sum(math.log(v) for v in vals) / len(vals))

def main():
    ap = argparse.ArgumentParser(description="逐切片 IPC 对比（不加权）")
    ap.add_argument("--a", required=True, help="A 结果目录，例如 results/20250820-203125-oracle")
    ap.add_argument("--b", required=True, help="B 结果目录，例如 results/20250820-223945-close")
    ap.add_argument("--a-label", default="A", help="A 的标签（列名用）")
    ap.add_argument("--b-label", default="B", help="B 的标签（列名用）")
    ap.add_argument("--out", default="compare_slices.tsv", help="输出 TSV 路径")
    ap.add_argument("--verbose", action="store_true", help="打印缺失/解析失败的切片")
    args = ap.parse_args()

    a_dir = Path(args.a).resolve()
    b_dir = Path(args.b).resolve()
    if not a_dir.exists() or not b_dir.exists():
        print(f"[ERR] 结果目录不存在：{a_dir if not a_dir.exists() else b_dir}", file=sys.stderr)
        sys.exit(2)

    a_map = scan_results(a_dir)
    b_map = scan_results(b_dir)

    # 只比较两侧都存在的 (bench, slice)
    inter = sorted(set(a_map.keys()) & set(b_map.keys()),
                   key=lambda k: (k[0], natural_key_slice(k[1])))

    headers = [
        "benchmark", "slice",
        f"ipc_{args.a_label}", f"ipc_{args.b_label}",
        "delta(B-A)", "pct_change(%)", "speedup(B/A)"
    ]
    out_path = Path(args.out).resolve()
    out_path.parent.mkdir(parents=True, exist_ok=True)

    rows = []
    speedups = []
    skipped = 0

    for (bench, slc) in inter:
        ipc_a = a_map.get((bench, slc))
        ipc_b = b_map.get((bench, slc))
        if not (isinstance(ipc_a, (int, float)) and isinstance(ipc_b, (int, float))
                and math.isfinite(ipc_a) and math.isfinite(ipc_b) and ipc_a > 0):
            skipped += 1
            if args.verbose:
                why = []
                if ipc_a is None or not math.isfinite(ipc_a): why.append("A无效")
                if ipc_b is None or not math.isfinite(ipc_b): why.append("B无效")
                if ipc_a is not None and math.isfinite(ipc_a) and ipc_a <= 0: why.append("A<=0")
                print(f"[WARN] 跳过 {bench}/{slc}：" + ",".join(why), file=sys.stderr)
            continue
        delta = ipc_b - ipc_a
        pct = (delta / ipc_a) * 100.0
        spd = ipc_b / ipc_a
        rows.append((
            bench, slc,
            f"{ipc_a:.6f}", f"{ipc_b:.6f}",
            f"{delta:.6f}", f"{pct:.2f}", f"{spd:.6f}"
        ))
        speedups.append(spd)

    with out_path.open("w", encoding="utf-8") as f:
        f.write("\t".join(headers) + "\n")
        for r in rows:
            f.write("\t".join(r) + "\n")
        g = geomean(speedups)
        f.write("\t".join(["__GEOMEAN__", "", "", "", "", "", f"{g:.6f}" if math.isfinite(g) else "NaN"]) + "\n")

    print(f"完成：写入 {out_path}（对齐 {len(rows)} 个切片；跳过 {skipped} 个）")
    if not rows:
        print("[INFO] 没有可比较的切片。请检查两侧 results 目录结构是否一致。", file=sys.stderr)

if __name__ == "__main__":
    main()

