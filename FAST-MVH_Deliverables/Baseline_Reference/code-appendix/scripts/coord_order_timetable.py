#!/usr/bin/env python3
r"""Regenerate the tab:ordertime timing table on the reference (i7) machine.

Runs the sortlinear-augmented replay_stats binary on the four streams used in
Appendix~E's runtime comparison, and prints the finished LaTeX table body:
comparisons per Check (deterministic, machine-independent) and wall-clock
speedup vs. the unsorted linear scan (self-consistent on whatever machine this
is run on). Run this on the SAME machine used for Tables 1 and 3 so every
wall-clock figure in the paper is on one machine.

Build the binary once (paper's -O2 config):
  cmake -S . -B build -DCMAKE_CXX_FLAGS="-O2"
  cmake --build build --target replay_stats

Then:
  python scripts/coord_order_timetable.py --bin build/replay_stats

Road rows require captured road streams (see README, Experiment 4) under
data/streams/road/; missing streams are skipped so the grid rows run standalone.
"""
import argparse
import csv
import subprocess
import sys
import os

# (stream path, label prefix, K repeats). n=front size is filled in from output.
STREAMS = [
    ("data/streams/grid/m5.stream",                 r"grid $M{=}5$", 5),
    ("data/streams/grid/m4.stream",                 r"grid $M{=}4$", 10),
    ("data/streams/road/COL_M5_r0.7_geo.stream",    r"COL $M5$ road", 10),
    ("data/streams/road/NY_M4_r0.7_geo.stream",     r"NY $M4$ road", 10),
]
# backends we report, in table order
COLS = ["linear", "sortlinear", "kdinc"]


def run(bin_path, stream, K):
    out = subprocess.run([bin_path, stream, str(K)], capture_output=True,
                         text=True, check=True).stdout
    r = {}
    for row in csv.DictReader(out.splitlines()):
        r[row["backend"]] = dict(
            checks=int(row["checks"]),
            cc=int(row["dom_cmps_check"]),
            t=float(row["t_total_mean_us"]),
            front=int(row["final_frontier"]))
    return r


def cpc(v):  # comparisons per check: integer at >=10, one decimal below (matches Table 1)
    x = v["cc"] / v["checks"]
    return f"{x:.0f}" if x >= 10 else f"{x:.1f}"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", default="build/replay_stats")
    args = ap.parse_args()
    if not os.path.isfile(args.bin) and os.path.isfile(args.bin + ".exe"):
        args.bin += ".exe"          # Windows
    if not os.path.isfile(args.bin):
        sys.exit(f"binary not found: {args.bin} (build it: cmake --build build --target replay_stats)")

    rows = []
    for stream, label, K in STREAMS:
        if not os.path.isfile(stream):
            sys.stderr.write(f"skipping missing stream (road rows need Experiment 4): {stream}\n")
            continue
        r = run(args.bin, stream, K)
        n = r["linear"]["front"]
        lin_t = r["linear"]["t"]
        sort_sp = lin_t / r["sortlinear"]["t"]
        kd_sp = lin_t / r["kdinc"]["t"]
        rows.append((f"{label} ($n{{=}}{n}$)",
                     cpc(r["linear"]), cpc(r["sortlinear"]), cpc(r["kdinc"]),
                     f"${sort_sp:.2f}\\times$", f"${kd_sp:.2f}\\times$"))

    print(r"% --- paste into tab:ordertime (regenerated on this machine) ---")
    print(r"\begin{tabular}{lrrrrr}")
    print(r"\toprule")
    print(r" & \multicolumn{3}{c}{comparisons / \textsc{Check}} & \multicolumn{2}{c}{speedup vs.\ Linear} \\")
    print(r"\cmidrule(lr){2-4}\cmidrule(lr){5-6}")
    print(r"Instance (front $n$) & Lin & Sort & $k$-d & Sort & $k$-d \\")
    print(r"\midrule")
    for label, lc, sc, kc, ss, ks in rows:
        print(f"{label} & {lc} & {sc} & {kc} & {ss} & {ks} \\\\")
    print(r"\bottomrule")
    print(r"\end{tabular}")


if __name__ == "__main__":
    main()
