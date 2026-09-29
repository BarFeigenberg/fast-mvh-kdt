#!/usr/bin/env python3
"""Summarize coordinate-order sensitivity CSVs from coord_order_study.py.

Produces (a) a per-backend headline across all streams in each domain and
(b) a per-stream table, using the max/min dominance-comparison ratio across all
axis permutations as the sensitivity measure (1.0 == perfectly order-invariant).
"""
import argparse
import csv
from collections import defaultdict

BACKEND_LABEL = {
    "linear": "Linear (unsorted)",
    "sortlinear": "Linear (lex-sorted)",
    "avlfast": "AVL (lexicographic)",
    "kdinc": "k-d (incremental)",
    "ndinc": "ND-tree (incremental)",
}
ORDER = ["linear", "sortlinear", "avlfast", "kdinc", "ndinc"]


def load(paths):
    rows = []
    for p in paths:
        with open(p) as f:
            for r in csv.DictReader(f):
                for k in ("dim", "n_perms", "identity", "min", "max", "mean", "std", "max_min_ratio"):
                    r[k] = float(r[k])
                rows.append(r)
    return rows


def headline(rows, domain):
    print(f"\n=== {domain}: coordinate-order sensitivity (max/min comparison ratio across all axis permutations) ===")
    print(f"{'backend':<24}{'metric':<8}{'#streams':>9}{'ratio=1?':>10}{'max ratio':>11}{'median ratio':>14}")
    for b in ORDER:
        for metric in ("check", "update"):
            rr = [r for r in rows if r["backend"] == b and r["metric"] == metric]
            if not rr:
                continue
            ratios = sorted(r["max_min_ratio"] for r in rr)
            all_one = all(abs(x - 1.0) < 1e-9 for x in ratios)
            med = ratios[len(ratios) // 2]
            print(f"{BACKEND_LABEL[b]:<24}{metric:<8}{len(rr):>9}"
                  f"{('yes' if all_one else 'no'):>10}{max(ratios):>11.3f}{med:>14.3f}")


def per_stream(rows, domain):
    print(f"\n--- {domain}: per-stream check-comparison max/min ratio across axis orders ---")
    streams = sorted({r["stream"] for r in rows}, key=lambda s: (rows_dim(rows, s), s))
    print(f"{'stream':<28}{'dim':>4}{'perms':>7}{'lin':>7}{'sortlin':>9}{'AVL':>8}{'k-d':>8}{'ND':>7}")
    for s in streams:
        def ratio(b):
            m = [r for r in rows if r["stream"] == s and r["backend"] == b and r["metric"] == "check"]
            return m[0]["max_min_ratio"] if m else float("nan")
        any_r = next(r for r in rows if r["stream"] == s)
        print(f"{s:<28}{int(any_r['dim']):>4}{int(any_r['n_perms']):>7}"
              f"{ratio('linear'):>7.3f}{ratio('sortlinear'):>9.3f}"
              f"{ratio('avlfast'):>8.3f}{ratio('kdinc'):>8.3f}{ratio('ndinc'):>7.3f}")


def scalar_table(rows):
    """Two-granularity view from a scalar CSV (metrics dom_cmps_* / coord_cmps_*)."""
    print("\n=== Two-granularity sensitivity (max/min ratio across all axis permutations) ===")
    print("dom = vector-level dominance checks (paper metric); coord = scalar coordinate comparisons")
    print(f"{'stream':<26}{'dim':>4}  {'backend':<20}{'dom(chk)':>9}{'coord(chk)':>11}{'dom(upd)':>9}{'coord(upd)':>11}")
    streams = sorted({r["stream"] for r in rows}, key=lambda s: (rows_dim(rows, s), s))
    for s in streams:
        for b in ORDER:
            def g(metric):
                m = [r for r in rows if r["stream"] == s and r["backend"] == b and r["metric"] == metric]
                return m[0]["max_min_ratio"] if m else float("nan")
            d = int(next(r["dim"] for r in rows if r["stream"] == s))
            print(f"{s:<26}{d:>4}  {BACKEND_LABEL[b]:<20}"
                  f"{g('dom_cmps_check'):>9.3f}{g('coord_cmps_check'):>11.3f}"
                  f"{g('dom_cmps_update'):>9.3f}{g('coord_cmps_update'):>11.3f}")
        print()


def rows_dim(rows, s):
    return int(next(r["dim"] for r in rows if r["stream"] == s))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--synth")
    ap.add_argument("--road")
    ap.add_argument("--scalar")
    args = ap.parse_args()
    if args.synth:
        rows = load([args.synth])
        headline(rows, "Synthetic grid")
        per_stream(rows, "Synthetic grid")
    if args.road:
        rows = load([args.road])
        headline(rows, "Real road networks")
        per_stream(rows, "Real road networks")
    if args.scalar:
        scalar_table(load([args.scalar]))


if __name__ == "__main__":
    main()
