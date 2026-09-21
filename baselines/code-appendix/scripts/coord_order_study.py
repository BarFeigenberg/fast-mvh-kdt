#!/usr/bin/env python3
"""Auxiliary study: coordinate-order sensitivity of frontier backends.

For each captured op-stream we replay every permutation of the d projected
coordinate axes through the existing replay_stats binary and record the exact,
deterministic dominance-comparison counts per backend. Permuting the axes
changes the lexicographic key (AVL) and the k-d split-axis cycle (k-d, ND-tree),
but is (by construction) invariant for the linear scan's vector-level count.

Backends reported by replay_stats: linear, avlfast, kdinc, ndinc.
Metric: dom_cmps_check and dom_cmps_update (exact, compiler-independent).

Usage:
  python scripts/coord_order_study.py --bin build/replay_coordorder \
      --streams <f1> <f2> ... --out results/coord_order.csv
"""
import argparse
import csv
import itertools
import math
import os
import statistics
import subprocess
import sys
import tempfile

BACKENDS = ["linear", "sortlinear", "avlfast", "kdinc", "ndinc"]


def read_stream(path):
    """Return (dim, list[(tag, [floats])])."""
    ops = []
    with open(path, "r") as f:
        header = f.readline().split()
        assert header[0] == "DIM", f"bad header in {path}: {header}"
        dim = int(header[1])
        for line in f:
            line = line.strip()
            if not line:
                continue
            parts = line.split()
            tag = parts[0]
            vec = parts[1:]  # keep as strings to preserve exact formatting
            ops.append((tag, vec))
    return dim, ops


def write_permuted(path, dim, ops, perm):
    """Write a stream with each vector's coordinates reordered by perm.

    new_vec[i] = old_vec[perm[i]]  ->  perm is the new axis order.
    """
    with open(path, "w") as f:
        f.write(f"DIM {dim}\n")
        for tag, vec in ops:
            f.write(tag)
            for i in perm:
                f.write(" " + vec[i])
            f.write("\n")


def run_replay(bin_path, stream_path, K=1):
    """Run replay_stats, return {backend: (checks, updates, cc, cu, final)}."""
    out = subprocess.run(
        [bin_path, stream_path, str(K)],
        capture_output=True, text=True, check=True,
    ).stdout
    rows = {}
    reader = csv.DictReader(out.splitlines())
    for r in reader:
        rows[r["backend"]] = dict(
            checks=int(r["checks"]),
            updates=int(r["updates"]),
            cc=int(r["dom_cmps_check"]),
            cu=int(r["dom_cmps_update"]),
            final=int(r["final_frontier"]),
        )
    return rows


def summarize(values):
    """min, max, mean, std (sample), and max/min ratio."""
    vmin, vmax = min(values), max(values)
    mean = statistics.mean(values)
    std = statistics.stdev(values) if len(values) > 1 else 0.0
    ratio = (vmax / vmin) if vmin > 0 else (float("inf") if vmax > 0 else 1.0)
    return vmin, vmax, mean, std, ratio


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", required=True)
    ap.add_argument("--streams", nargs="+", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--max-perms", type=int, default=0,
                    help="if >0 and d! exceeds it, sample this many random perms (seeded)")
    ap.add_argument("--tmp", default=None, help="scratch dir for permuted streams")
    args = ap.parse_args()

    args.bin = os.path.abspath(args.bin)
    if not os.path.isfile(args.bin) and os.path.isfile(args.bin + ".exe"):
        args.bin += ".exe"          # Windows
    if not os.path.isfile(args.bin):
        sys.exit(f"replay binary not found: {args.bin}")
    os.makedirs(os.path.dirname(args.out) or ".", exist_ok=True)
    tmpdir = args.tmp or tempfile.mkdtemp(prefix="coordperm_")
    os.makedirs(tmpdir, exist_ok=True)

    fieldnames = [
        "stream", "dim", "backend", "metric", "n_perms",
        "identity", "min", "max", "mean", "std", "max_min_ratio",
    ]
    with open(args.out, "w", newline="") as fout:
        w = csv.DictWriter(fout, fieldnames=fieldnames)
        w.writeheader()

        for spath in args.streams:
            name = os.path.basename(spath)
            dim, ops = read_stream(spath)
            perms = list(itertools.permutations(range(dim)))
            identity = tuple(range(dim))
            if args.max_perms and len(perms) > args.max_perms:
                import random
                rng = random.Random(12345)
                perms = [identity] + rng.sample(
                    [p for p in perms if p != identity], args.max_perms - 1)
            # collect per-backend/metric across perms
            acc = {b: {"cc": [], "cu": []} for b in BACKENDS}
            ident_vals = {b: {"cc": None, "cu": None} for b in BACKENDS}
            print(f"[{name}] dim={dim}, {len(perms)} permutations", flush=True)
            for pi, perm in enumerate(perms):
                ppath = os.path.join(tmpdir, f"{name}.p{pi}.stream")
                write_permuted(ppath, dim, ops, perm)
                rows = run_replay(args.bin, ppath, K=1)
                os.remove(ppath)
                for b in BACKENDS:
                    acc[b]["cc"].append(rows[b]["cc"])
                    acc[b]["cu"].append(rows[b]["cu"])
                    if perm == identity:
                        ident_vals[b]["cc"] = rows[b]["cc"]
                        ident_vals[b]["cu"] = rows[b]["cu"]
                print(f"  perm {pi+1}/{len(perms)} {perm} done", flush=True)

            for b in BACKENDS:
                for metric in ("cc", "cu"):
                    vals = acc[b][metric]
                    vmin, vmax, mean, std, ratio = summarize(vals)
                    w.writerow(dict(
                        stream=name, dim=dim, backend=b,
                        metric="check" if metric == "cc" else "update",
                        n_perms=len(perms),
                        identity=ident_vals[b][metric],
                        min=vmin, max=vmax, mean=round(mean, 2),
                        std=round(std, 2), max_min_ratio=round(ratio, 4),
                    ))
            fout.flush()
    print(f"wrote {args.out}")


if __name__ == "__main__":
    main()
