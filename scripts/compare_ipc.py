#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
对比两次运行的 benchmark IPC：
- 输入可以是 results 目录或直接是 summary.tsv 文件
- 仅比较两侧都存在且 IPC 有效的 benchmark
- 输出列：benchmark, ipc_A, ipc_B, delta(B-A), pct_change(%), speedup(B/A)
- 末尾附带 GEOMEAN 的 speedup（几何平均）

用法示例：
python3 compare_ipc.py --a results/20250820-203125-oracle --b results/20250820-231500-close \
  --a-label oracle --b-label close --out compare_oracle_vs_close.tsv
"""

import argparse
import math
import sys
from pathlib import Path

def resolve_summary_path(p: Path) -> Path:
    p = p.resolve()
    if p.is_dir():
        p = p / "summary.tsv"
    return p

def parse_summary(summary_path: Path) -> dict:
    """
    读取 summary.tsv，返回 {benchmark: ipc(float)}。
    兼容列名 'weighted_ipc' 或 'ipc'；忽略 NaN/空值。
    """
    if not summary_path.exists():
        print(f"[ERR] 找不到 {summary_path}", file=sys.stderr)
        sys.exit(2)

    bench2ipc = {}
    with summary_path.open("r", encoding="utf-8", errors="ignore") as f:
        header = None
        for i, line in enumerate(f, 1):
            line = line.strip("\n\r")
            if not line.strip():
                continue
            if line.startswith("#"):
                continue
            parts = line.split("\t")
            if header is None:
                header = [c.strip() for c in parts]
                # 找到 IPC 列
                try:
                    ipc_idx = header.index("weighted_ipc")
                except ValueError:
                    try:
                        ipc_idx = header.index("ipc")
                    except ValueError:
                        if len(header) >= 2:
                            ipc_idx = 1  # 兜底：第二列
                        else:
                            print(f"[ERR] {summary_path} 表头不合法：{header}", file=sys.stderr)
                            sys.exit(2)
                try:
                    bench_idx = header.index("benchmark")
                except ValueError:
                    bench_idx = 0
                continue

            # 数据行
            if len(parts) <= max(bench_idx, ipc_idx):
                continue
            bench = parts[bench_idx]
            ipc_raw = parts[ipc_idx]
            if ipc_raw in ("NaN", "nan", "", None):
                continue
            try:
                ipc = float(ipc_raw)
            except Exception:
                continue
            if not math.isfinite(ipc):
                continue
            bench2ipc[bench] = ipc
    return bench2ipc

def geomean(values):
    # 几何平均，加一点点稳定性处理
    vals = [v for v in values if v > 0 and math.isfinite(v)]
    if not vals:
        return float('nan')
    # 防止乘积下溢，改用 log 求和
    s = sum(math.log(v) for v in vals)
    return math.exp(s / len(vals))

def main():
    ap = argparse.ArgumentParser(description="对比两次运行（A vs B）的 benchmark IPC")
    ap.add_argument("--a", required=True, help="A 路径（results 目录或 summary.tsv）")
    ap.add_argument("--b", required=True, help="B 路径（results 目录或 summary.tsv）")
    ap.add_argument("--a-label", default="A", help="A 的标签（用于输出列名）")
    ap.add_argument("--b-label", default="B", help="B 的标签（用于输出列名）")
    ap.add_argument("--out", default="compare.tsv", help="输出 TSV（默认 compare.tsv）")
    args = ap.parse_args()

    a_path = resolve_summary_path(Path(args.a))
    b_path = resolve_summary_path(Path(args.b))

    a = parse_summary(a_path)
    b = parse_summary(b_path)

    inter = sorted(set(a.keys()) & set(b.keys()))
    if not inter:
        print("[ERR] 两次运行没有可对齐的 benchmark；请检查输入。", file=sys.stderr)
        sys.exit(3)

    out_path = Path(args.out).resolve()
    out_path.parent.mkdir(parents=True, exist_ok=True)

    # 列头
    headers = [
        "benchmark",
        f"ipc_{args.a_label}",
        f"ipc_{args.b_label}",
        "delta(B-A)",
        "pct_change(%)",
        "speedup(B/A)"
    ]

    # 计算
    rows = []
    speedups = []
    skipped = 0
    for bench in inter:
        ipc_a = a.get(bench, float('nan'))
        ipc_b = b.get(bench, float('nan'))
        if not (math.isfinite(ipc_a) and math.isfinite(ipc_b) and ipc_a > 0):
            skipped += 1
            continue
        delta = ipc_b - ipc_a
        pct = (delta / ipc_a) * 100.0
        spd = ipc_b / ipc_a
        rows.append((
            bench,
            f"{ipc_a:.6f}",
            f"{ipc_b:.6f}",
            f"{delta:.6f}",
            f"{pct:.2f}",
            f"{spd:.6f}",
        ))
        speedups.append(spd)

    # 写文件
    with out_path.open("w", encoding="utf-8") as f:
        f.write("\t".join(headers) + "\n")
        for r in rows:
            f.write("\t".join(r) + "\n")
        # 最后一行写 GEOMEAN（仅 speedup）
        g = geomean(speedups)
        f.write("\t".join([
            "__GEOMEAN__",
            "", "", "", "",
            f"{g:.6f}" if math.isfinite(g) else "NaN"
        ]) + "\n")

    print(f"完成：写入 {out_path}（对齐 {len(rows)} 项；跳过 {skipped} 项）")
    print(f"几何平均加速比（{args.b_label}/{args.a_label}）：{geomean(speedups):.6f}" if speedups else "无有效项目可计算几何平均")

if __name__ == "__main__":
    main()

