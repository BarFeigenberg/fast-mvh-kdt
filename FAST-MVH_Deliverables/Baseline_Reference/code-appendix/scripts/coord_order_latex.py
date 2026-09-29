#!/usr/bin/env python3
"""Emit the objective-ordering sensitivity appendix table (comparisons/check).

Reads the coord-order sweep CSVs (from coord_order_study.py) and, dividing the
per-permutation dominance-check totals by the fixed number of Check operations
in each stream, reports best / worst / mean+/-std comparisons-per-check across
all d! axis orderings, per backend. Also verifies the headline claim: the k-d
tree's worst ordering vs. every baseline's best ordering.
"""
import argparse
import csv

# Number of Check operations per captured stream (constant across permutations);
# counted as lines beginning with 'C' in each stream file.
N_CHECKS = {
    "m3.stream": 27233, "m4.stream": 237999, "m5.stream": 534949, "m6.stream": 604836,
    "COL_M4_r0.6_geo.stream": 25977, "NY_M4_r0.7_geo.stream": 66921,
    "BAY_M4_r0.5_geo.stream": 299275, "BAY_M5_r0.7_geo.stream": 59294,
    "COL_M5_r0.7_geo.stream": 268296, "NY_M5_r0.6_geo.stream": 3076608,
    "NY_M5_r0.7_synth.stream": 2615410,
}
LABEL = {"linear": "Linear (unsorted)", "sortlinear": "Sorted linear",
         "avlfast": "AVL (lex)", "kdinc": "$k$-d (ours)", "ndinc": "ND-tree"}
ORDER = ["linear", "sortlinear", "avlfast", "kdinc", "ndinc"]
BASELINES = ["linear", "avlfast", "ndinc"]  # the paper's baselines (excl. sortlinear)


def fmt(x):
    if x < 10:   return f"{x:.2f}"
    if x < 100:  return f"{x:.1f}"
    return f"{x:.0f}"


def load(path):
    rows = {}
    with open(path) as f:
        for r in csv.DictReader(f):
            if r["metric"] != "check":
                continue
            s = r["stream"]; n = N_CHECKS[s]
            rows.setdefault(s, {})[r["backend"]] = dict(
                dim=int(r["dim"]),
                best=float(r["min"]) / n, worst=float(r["max"]) / n,
                mean=float(r["mean"]) / n, std=float(r["std"]) / n)
    return rows


def emit_table(rows, streams, caption, label):
    print(r"\begin{table}[t]\centering\footnotesize")
    print(r"\setlength{\tabcolsep}{4pt}")
    print(r"\begin{tabular}{llrrr}")
    print(r"\toprule")
    print(r"$M$ & Backend & best & worst & mean\,$\pm$\,std \\")
    print(r"\midrule")
    for s in streams:
        d = rows[s][ORDER[0]]["dim"]; M = d + 1
        for i, b in enumerate(ORDER):
            v = rows[s][b]
            mm = f"${fmt(v['mean'])}\\pm{fmt(v['std'])}$"
            head = f"{M}" if i == 0 else ""
            print(f"{head} & {LABEL[b]} & {fmt(v['best'])} & {fmt(v['worst'])} & {mm} \\\\")
        if s != streams[-1]:
            print(r"\midrule")
    print(r"\bottomrule")
    print(r"\end{tabular}")
    print(f"\\caption{{{caption}}}")
    print(f"\\label{{{label}}}")
    print(r"\end{table}")


def check_claim(rows):
    print("\n% ---- claim verification (k-d worst vs others' best) ----")
    for s, bk in rows.items():
        kd_worst = bk["kdinc"]["worst"]
        base_best = min(bk[b]["best"] for b in BASELINES)
        all_best = min(bk[b]["best"] for b in ORDER if b != "kdinc")
        print(f"% {s:26s} kd_worst={kd_worst:9.2f} | baselines_best={base_best:9.2f} "
              f"({'OK' if kd_worst < base_best else 'FAIL'}) | "
              f"all(incl sortlin)_best={all_best:9.2f} ({'OK' if kd_worst < all_best else 'FAIL'})")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--synth", required=True)
    ap.add_argument("--road", help="optional; road streams for the claim verification")
    args = ap.parse_args()
    rows = {}
    rows.update(load(args.synth))
    if args.road:
        rows.update(load(args.road))

    synth = [s for s in ["m3.stream", "m4.stream", "m5.stream", "m6.stream"] if s in rows]
    emit_table(rows, synth,
               r"Objective-ordering sensitivity on the synthetic grid (the streams of "
               r"Table~\ref{tab:split}), replayed under all $d!$ coordinate orderings. "
               r"Comparisons per \textsc{Check}; best/worst/mean over orderings. The total-order "
               r"indices swing widely while the $k$-d tree stays nearly flat, and its \emph{worst} "
               r"ordering still uses fewer comparisons than any baseline's \emph{best}.",
               "tab:ordersynth")
    check_claim(rows)


if __name__ == "__main__":
    main()
