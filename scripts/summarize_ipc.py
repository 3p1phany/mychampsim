#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import argparse, re, sys
from pathlib import Path
from collections import defaultdict

# 允许 1.23 或 1.23e-3 这样的数字
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
            try:
                # findall 可能返回 list[str] 或 list[tuple]，统一取最后一个数
                val = m[-1]
                if isinstance(val, (tuple, list)):
                    val = val[0]
                return float(val)
            except Exception:
                continue
    return None

def read_manifest(tsv_path: Path):
    rows = []
    with tsv_path.open('r', encoding='utf-8', errors='ignore') as f:
        for i, line in enumerate(f, 1):
            line = line.rstrip('\n\r')
            if not line.strip() or line.lstrip().startswith('#'):
                continue
            parts = line.split('\t')
            if parts[0] == 'benchmark':
                continue
            if len(parts) < 4:
                continue
            bench, slc, w_str, trace = parts[0], parts[1], parts[2], parts[3]
            try:
                w = float(w_str)
            except ValueError:
                continue
            rows.append((bench, slc, w))
    return rows

def main():
    ap = argparse.ArgumentParser(description="按权重汇总每个 benchmark 的加权 IPC")
    ap.add_argument("--results", required=True, help="结果目录，如 results/20250820-203125-oracle")
    ap.add_argument("--manifest", default="./benchmarks.tsv", help="benchmarks.tsv 路径")
    ap.add_argument("--out", default=None, help="输出 TSV（默认 <results>/summary.tsv）")
    ap.add_argument("--verbose", action="store_true", help="打印缺失/无法解析的切片")
    args = ap.parse_args()

    results = Path(args.results).resolve()
    manifest = Path(args.manifest).resolve()
    if not results.exists():
        print(f"[ERR] 结果目录不存在: {results}", file=sys.stderr); sys.exit(2)
    if not manifest.exists():
        print(f"[ERR] 找不到 manifest: {manifest}", file=sys.stderr); sys.exit(2)
    out_path = Path(args.out).resolve() if args.out else (results / "summary.tsv")

    entries = read_manifest(manifest)
    if not entries:
        print(f"[ERR] manifest 中无有效条目: {manifest}", file=sys.stderr); sys.exit(2)

    # 按 benchmark 聚合
    per_bench = defaultdict(list)
    for bench, slc, w in entries:
        log = results / bench / slc / "run.log"
        per_bench[bench].append((slc, w, log))

    out_lines = [("benchmark", "weighted_ipc", "slices_ok", "slices_total")]
    missing = 0

    for bench in sorted(per_bench.keys()):
        items = per_bench[bench]
        total = len(items)
        acc = 0.0
        wsum = 0.0
        ok = 0
        for slc, w, log in items:
            if not log.exists():
                missing += 1
                if args.verbose:
                    print(f"[WARN] 缺少日志: {log}", file=sys.stderr)
                continue
            ipc = parse_ipc_from_log(log)
            if ipc is None:
                missing += 1
                if args.verbose:
                    print(f"[WARN] 无法解析 IPC: {log}", file=sys.stderr)
                continue
            acc += w * ipc
            wsum += w
            ok += 1

        weighted = acc / wsum if ok > 0 and wsum > 0.0 else float('nan')
        out_lines.append((bench, f"{weighted:.6f}" if weighted == weighted else "NaN",
                          str(ok), str(total)))

    with out_path.open("w", encoding="utf-8") as f:
        for line in out_lines:
            f.write("\t".join(line) + "\n")

    print(f"完成：写入 {out_path}（benchmarks={len(out_lines)-1}）")
    if missing:
        print(f"[INFO] 有 {missing} 个切片缺日志或解析失败（已按权重和归一化）。", file=sys.stderr)

if __name__ == "__main__":
    main()

