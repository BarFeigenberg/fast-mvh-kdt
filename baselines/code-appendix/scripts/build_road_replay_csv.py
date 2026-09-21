#!/usr/bin/env python3
# scripts/build_road_replay_csv.py
# Parse build/road_replay_raw.log (replay_stats CSV blocks interleaved with
# "### MARK ..." instance markers written by scripts/run_road_matrix.ps1) into a
# tidy one-row-per-stream CSV build/road_replay.csv.
#
# Each block looks like:
#   ### MARK map=NY M=4 rho=0.5 mode=geo stream=NY_M4_r0.5_geo.stream K=5
#   backend,dim,checks,updates,t_total_mean_us,t_total_ci95_us,dom_cmps_check,dom_cmps_update,final_frontier
#   linear,3,...
#   avlfast,3,...
#   kdinc,3,...
#
# kdinc is the reference backend; reductions are AVL/kdinc and linear/kdinc.
import sys, os, re

def parse(raw_path):
    rows = []
    cur = None          # current MARK dict
    backends = {}       # backend name -> parsed field dict
    def flush():
        if cur is None or "kdinc" not in backends:
            return
        lin = backends.get("linear"); avl = backends.get("avlfast"); kd = backends["kdinc"]
        nd = backends.get("ndinc")
        def f(d, k): return float(d[k]) if d is not None else float("nan")
        def rr(num, den):
            return (num / den) if (den not in (0, 0.0) and den == den) else float("nan")
        frontier = int(float(kd["final_frontier"]))
        checks   = int(float(kd["checks"]))
        updates  = int(float(kd["updates"]))
        ops      = checks + updates
        cc_lin, cc_avl, cc_kd = f(lin,"dom_cmps_check"), f(avl,"dom_cmps_check"), f(kd,"dom_cmps_check")
        cu_lin, cu_avl, cu_kd = f(lin,"dom_cmps_update"), f(avl,"dom_cmps_update"), f(kd,"dom_cmps_update")
        t_lin, t_avl, t_kd    = f(lin,"t_total_mean_us"), f(avl,"t_total_mean_us"), f(kd,"t_total_mean_us")
        ci_lin, ci_avl, ci_kd = f(lin,"t_total_ci95_us"), f(avl,"t_total_ci95_us"), f(kd,"t_total_ci95_us")
        # ndinc (incremental ND-tree) row; NaN-safe if the backend is absent from a block.
        cc_nd = f(nd,"dom_cmps_check"); cu_nd = f(nd,"dom_cmps_update")
        t_nd  = f(nd,"t_total_mean_us"); ci_nd = f(nd,"t_total_ci95_us")
        rows.append({
            "map": cur["map"], "M": cur["M"], "rho": cur["rho"], "mode": cur["mode"],
            "frontier": frontier, "ops": ops, "checks": checks, "updates": updates,
            "t_lin_us": t_lin, "t_avl_us": t_avl, "t_kd_us": t_kd, "t_nd_us": t_nd,
            "ci_lin": ci_lin, "ci_avl": ci_avl, "ci_kd": ci_kd, "ci_nd": ci_nd,
            "cc_lin": cc_lin, "cc_avl": cc_avl, "cc_kd": cc_kd, "cc_nd": cc_nd,
            "cu_lin": cu_lin, "cu_avl": cu_avl, "cu_kd": cu_kd, "cu_nd": cu_nd,
            "check_red_avl": rr(cc_avl, cc_kd), "check_red_lin": rr(cc_lin, cc_kd),
            "upd_red_avl": rr(cu_avl, cu_kd),   "upd_red_lin": rr(cu_lin, cu_kd),
            "wall_red_avl": rr(t_avl, t_kd),    "wall_red_lin": rr(t_lin, t_kd),
            # ndinc relative to kdinc: >1 means kdinc does LESS work than ndinc; <1 means ndinc wins.
            "check_kd_vs_nd": rr(cc_nd, cc_kd), "upd_kd_vs_nd": rr(cu_nd, cu_kd),
            "wall_kd_vs_nd": rr(t_nd, t_kd),
        })

    hdr = None
    for line in open(raw_path):
        line = line.rstrip("\n").strip()
        if not line:
            continue
        if line.startswith("### MARK"):
            flush()
            cur = {}
            for kv in re.findall(r"(\w+)=([^\s]+)", line[len("### MARK"):]):
                cur[kv[0]] = kv[1]
            backends = {}
            hdr = None
            continue
        if line.startswith("backend,"):
            hdr = line.split(",")
            continue
        if hdr is not None:
            parts = line.split(",")
            if len(parts) == len(hdr) and parts[0] in ("linear", "avlfast", "kdinc", "ndinc"):
                backends[parts[0]] = dict(zip(hdr, parts))
    flush()
    return rows

COLS = ["map","M","rho","mode","frontier","ops","checks","updates",
        "t_lin_us","t_avl_us","t_kd_us","t_nd_us","ci_lin","ci_avl","ci_kd","ci_nd",
        "cc_lin","cc_avl","cc_kd","cc_nd","cu_lin","cu_avl","cu_kd","cu_nd",
        "check_red_avl","check_red_lin","upd_red_avl","upd_red_lin",
        "wall_red_avl","wall_red_lin",
        "check_kd_vs_nd","upd_kd_vs_nd","wall_kd_vs_nd"]

def fmt(v):
    if isinstance(v, float):
        if v != v:  # NaN
            return "nan"
        if v == int(v) and abs(v) < 1e15:
            return str(int(v))
        return "%.4f" % v
    return str(v)

def main(raw="build/road_replay_nd_raw.log", out="build/road_replay.csv"):
    rows = parse(raw)
    rows.sort(key=lambda r: (r["map"], int(r["M"]), float(r["rho"]), r["mode"]))
    with open(out, "w", newline="") as fh:
        fh.write(",".join(COLS) + "\n")
        for r in rows:
            fh.write(",".join(fmt(r[c]) for c in COLS) + "\n")
    print("wrote %s with %d rows" % (out, len(rows)))

if __name__ == "__main__":
    main(*sys.argv[1:])
