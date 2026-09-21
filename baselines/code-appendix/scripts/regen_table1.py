#!/usr/bin/env python3
r"""Regenerate Table 1 (tab:split) wall-clock + counts on the current build.

Runs replay_stats on the M=3..6 synthetic streams and prints LaTeX rows matching
the paper's tab:split format: t_total (ms, mean +/- 95% CI), cmp_check, cmp_update,
cmp/check. Counts are deterministic (reproduce the paper exactly); times are from
whatever build/machine this is run on.

  python scripts/regen_table1.py --bin build/replay_stats
"""
import argparse
import csv
import subprocess
import sys
import os

# (stream, M, d, K)   K matches the paper: 10 for M3,4; 5 for M5,6.
STREAMS = [
    ("data/streams/grid/m3.stream", 3, 2, 10),
    ("data/streams/grid/m4.stream", 4, 3, 10),
    ("data/streams/grid/m5.stream", 5, 4, 5),
    ("data/streams/grid/m6.stream", 6, 5, 5),
]
# paper row order and labels
ROW = [("linear", "Linear"), ("avlfast", "AVL"), ("ndinc", "ND-tree"), ("kdinc", "$k$-d")]


def run(bin_path, stream, K):
    out = subprocess.run([bin_path, stream, str(K)], capture_output=True,
                         text=True, check=True).stdout
    r = {}
    for row in csv.DictReader(out.splitlines()):
        r[row["backend"]] = dict(
            checks=int(row["checks"]),
            cc=int(row["dom_cmps_check"]), cu=int(row["dom_cmps_update"]),
            t=float(row["t_total_mean_us"]) / 1000.0,      # ms
            ci=float(row["t_total_ci95_us"]) / 1000.0)     # ms
    return r


def ms(x):
    return f"{x:.1f}" if x < 100 else f"{x:.0f}"


def cpc(cc, checks):
    x = cc / checks
    if x < 1:  return f"{x:.2f}"
    if x < 20: return f"{x:.1f}"
    return f"{x:.0f}"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", default="build/replay_stats")
    args = ap.parse_args()
    if not os.path.isfile(args.bin) and os.path.isfile(args.bin + ".exe"):
        args.bin += ".exe"          # Windows
    if not os.path.isfile(args.bin):
        sys.exit(f"binary not found: {args.bin} (build it: cmake --build build --target replay_stats)")

    print(r"% --- tab:split, regenerated on this build ---")
    for stream, M, d, K in STREAMS:
        r = run(args.bin, stream, K)
        for i, (be, label) in enumerate(ROW):
            v = r[be]
            head = f"{M} ({d})" if i == 0 else ""
            t = f"${ms(v['t'])}\\pm{ms(v['ci'])}$"
            line = (f"{head} & {label} & {t} & \\num{{{v['cc']}}} & \\num{{{v['cu']}}} "
                    f"& {cpc(v['cc'], v['checks'])} \\\\")
            print(line)
        print(r"\midrule" if M != 6 else r"\bottomrule")


if __name__ == "__main__":
    main()
