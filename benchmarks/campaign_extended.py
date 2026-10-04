#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
campaign_extended.py - Extended Scaling Campaign (New Runs)
Head-to-head: Maya vs. L_NAMOA_DR_MVH_FAST
Extends the prior M=6,7,8 campaign into larger grids and additional correlations
that were not covered (or had Maya timing out).

New instances:
  M=6:  N=10, rho=-0.2   (FAST failed in prior campaign - retry)
        N=10, rho=0.0, K=50,100
  M=7:  N=8,  rho=-0.2, K=25
        N=9,  rho=0.0,  K=25
        N=10, rho=0.0,  K=25
  M=8:  N=7,  rho=-0.2, K=25
        N=8,  rho=-0.2, K=25
        N=9,  rho=0.0,  K=25

Maya timeout policy: UNBOUNDED (cutoffTime=0). If Maya exceeds 3600s, record
implied lower bound and continue. Hard cap at 7200s (2 hours) per Maya instance.
"""

import io
import os
import sys
import time
import csv
import subprocess
from datetime import datetime
from typing import Dict, Any, Tuple, List

sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")
sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding="utf-8", errors="replace")

FAST_MVH_BIN  = os.path.abspath("build/Release/fast_mvh.exe")
CSV_OUT       = os.path.abspath("benchmarks/runs/fast_scaling_extended.csv")
SCRATCH_DIR   = os.path.abspath("scratchpad/deep_scaling")
MAYA_HARD_CAP = 7200   # 2-hour hard cap per Maya instance

# -----------------------------------------------------------------------
# Instance matrix
# -----------------------------------------------------------------------
INSTANCES: List[Dict] = [
    # M=6 --- cover rho=-0.2 at N=10 (FAST failed last time, retry)
    #         and add K=50,100 at N=10
    {"N": 10, "M": 6, "rho": -0.2, "K": 25},
    {"N": 10, "M": 6, "rho":  0.0, "K": 50},
    {"N": 10, "M": 6, "rho":  0.0, "K": 100},

    # M=7 --- N=8 rho=-0.2 not yet tested; push to N=9,10
    {"N":  8, "M": 7, "rho": -0.2, "K": 25},
    {"N":  9, "M": 7, "rho":  0.0, "K": 25},
    {"N": 10, "M": 7, "rho":  0.0, "K": 25},

    # M=8 --- rho=-0.2 at N=7,8; push to N=9
    {"N":  7, "M": 8, "rho": -0.2, "K": 25},
    {"N":  8, "M": 8, "rho": -0.2, "K": 25},
    {"N":  9, "M": 8, "rho":  0.0, "K": 25},
]

# -----------------------------------------------------------------------

def get_git_commit() -> str:
    try:
        return subprocess.run(["git", "rev-parse", "HEAD"],
                              capture_output=True, text=True).stdout.strip()
    except Exception:
        return "unknown"

def ensure_instance(N: int, M: int, rho: float, K: int, seed: int = 42) -> Tuple[str, str]:
    inst_dir = os.path.join(SCRATCH_DIR, f"grid_{N}x{N}_M{M}_rho{rho}")
    mvh_path = os.path.join(inst_dir, f"target_{N*N}_K{K}.mvh")
    os.makedirs(inst_dir, exist_ok=True)

    gr_files = [f for f in os.listdir(inst_dir) if f.startswith("c") and f.endswith(".gr")]
    if len(gr_files) < M:
        print(f"    Generating grid {N}x{N} M={M} rho={rho}...", flush=True)
        subprocess.run([sys.executable, "benchmarks/generators/generate_grid.py",
                        "--rows", str(N), "--cols", str(N),
                        "-M", str(M), "--rho", str(rho),
                        "--seed", str(seed), "--out-dir", inst_dir],
                       check=True, stdout=subprocess.DEVNULL)

    if not os.path.exists(mvh_path):
        print(f"    Generating MVH K={K}...", flush=True)
        subprocess.run([sys.executable, "benchmarks/generators/generate_mvh.py",
                        "--map", inst_dir,
                        "--goal", str(N * N),
                        "-K", str(K), "--seed", str(seed),
                        "--out", mvh_path],
                       check=True, stdout=subprocess.DEVNULL)

    return inst_dir, mvh_path

def parse_solver_output(stdout: str) -> Dict[str, Any]:
    metrics: Dict[str, Any] = {}
    for line in stdout.splitlines():
        if "=" in line:
            for token in line.split("\t"):
                if "=" in token:
                    k, v = token.split("=", 1)
                    try:
                        metrics[k.strip()] = float(v.strip()) if "." in v.strip() else int(v.strip())
                    except ValueError:
                        metrics[k.strip()] = v.strip()
    return metrics

def parse_sol_file(filepath: str) -> set:
    vectors = set()
    if not os.path.exists(filepath):
        return vectors
    with open(filepath, "r", encoding="utf-8", errors="replace") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            delimiter = "," if "," in line else None
            parts = line.split(delimiter)
            try:
                vec = tuple(int(p.strip()) for p in parts if p.strip())
                if vec:
                    vectors.add(vec)
            except ValueError:
                pass
    return vectors

def run_solver(inst_dir: str, mvh_path: str, N: int, M: int,
               algo: str, cutoff: int, sol_out: str) -> Tuple[bool, float, Dict[str, Any]]:
    obj_args = [str(i) for i in range(M)]
    cmd = [FAST_MVH_BIN,
           "--map", inst_dir,
           "--start", "1",
           "--goal", str(N * N),
           "--objectives", *obj_args,
           "--algorithm", algo,
           "--mvh", mvh_path,
           "--cutoffTime", str(cutoff),
           "--sol-out", sol_out]
    t0 = time.perf_counter()
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True,
                              timeout=(cutoff + 60) if cutoff > 0 else None)
        wall = time.perf_counter() - t0
        metrics = parse_solver_output(proc.stdout)
        metrics["wall_clock"] = wall
        timed_out = metrics.get("time_limit_reached", 0) == 1
        return not timed_out, wall, metrics
    except subprocess.TimeoutExpired:
        wall = time.perf_counter() - t0
        return False, wall, {"timeout": True}
    except Exception as e:
        return False, 0.0, {"error": str(e)}

def append_to_csv(row: Dict[str, Any]):
    fieldnames = [
        "timestamp", "N", "M", "rho", "K", "solutions", "expansions", "generations",
        "reinsertions", "time_fast_s", "time_maya_s", "speedup_vs_maya",
        "cmp_chooseh_fast", "cmp_chooseh_maya", "ops_reduction",
        "total_cmp_fast", "soundness", "maya_status"
    ]
    write_header = not os.path.exists(CSV_OUT) or os.path.getsize(CSV_OUT) == 0
    with open(CSV_OUT, "a", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        if write_header:
            writer.writeheader()
        writer.writerow({k: row.get(k, "") for k in fieldnames})
        f.flush()

def main():
    git = get_git_commit()
    print("=================================================================")
    print("  EXTENDED SCALING CAMPAIGN: New Instances")
    print(f"  Start: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')} | Git: {git[:12]}")
    print(f"  Machine: plugged in | Maya cap: {MAYA_HARD_CAP}s")
    print(f"  Output: {CSV_OUT}")
    print("=================================================================\n")

    total = len(INSTANCES)
    for idx, cfg in enumerate(INSTANCES, 1):
        N, M, rho, K = cfg["N"], cfg["M"], cfg["rho"], cfg["K"]

        print(f"[{idx}/{total}] === N={N}, M={M}, rho={rho}, K={K} ===")

        try:
            inst_dir, mvh_path = ensure_instance(N, M, rho, K)
        except Exception as e:
            print(f"  [ERROR] Instance generation failed: {e}")
            continue

        fast_sol = os.path.join(SCRATCH_DIR, f"sol_fast_N{N}_M{M}_rho{rho}_K{K}.txt")
        maya_sol = os.path.join(SCRATCH_DIR, f"sol_maya_N{N}_M{M}_rho{rho}_K{K}.txt")

        # -- Run FAST --
        print(f"  -> Running L_NAMOA_DR_MVH_FAST...", flush=True)
        ok_fast, time_fast, met_fast = run_solver(inst_dir, mvh_path, N, M,
                                                   "L_NAMOA_DR_MVH_FAST", 0, fast_sol)
        if not ok_fast:
            print(f"  [WARN] FAST did not complete. Skipping instance.")
            continue

        fast_sols = len(parse_sol_file(fast_sol))
        fast_cmp  = met_fast.get("cmp_chooseh", 0)
        print(f"  [FAST] {time_fast:.3f}s | Solutions: {fast_sols:,} | CmpCH: {fast_cmp:,}")

        # -- Run Maya (hard capped at MAYA_HARD_CAP) --
        print(f"  -> Running Maya (cutoff={MAYA_HARD_CAP}s)... [started {datetime.now().strftime('%H:%M:%S')}]", flush=True)
        ok_maya, time_maya, met_maya = run_solver(inst_dir, mvh_path, N, M,
                                                   "L_NAMOA_DR_MVH_INSTRUMENTED",
                                                   MAYA_HARD_CAP, maya_sol)

        cmp_maya = met_maya.get("cmp_chooseh", 0)
        ops_red  = (cmp_maya / fast_cmp) if (fast_cmp and cmp_maya) else None

        fast_vecs = parse_sol_file(fast_sol)
        maya_vecs = parse_sol_file(maya_sol)

        if ok_maya:
            soundness = "BIT_IDENTICAL" if fast_vecs == maya_vecs else \
                        f"DIVERGED (fast:{len(fast_vecs)}, maya:{len(maya_vecs)})"
            speedup = time_maya / time_fast
            speedup_str = f"{speedup:.2f}x"
            ops_str = f"{ops_red:.1f}x" if ops_red else "N/A"
            print(f"  [Maya] {time_maya:.2f}s | Soundness: {soundness}")
            print(f"  >>> SPEEDUP: {speedup_str} | OPS REDUCTION: {ops_str} <<<")
            maya_status = "OK"
        else:
            speedup = None
            speedup_str = f"> {MAYA_HARD_CAP / time_fast:.1f}x"
            soundness = "UNVERIFIED (Maya Timed Out)"
            print(f"  [Maya] TIMED OUT after {MAYA_HARD_CAP}s. Implied speedup: {speedup_str}")
            maya_status = "TIMEOUT"

        row = {
            "timestamp":        datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
            "N": N, "M": M, "rho": rho, "K": K,
            "solutions":        fast_sols,
            "expansions":       met_fast.get("num_expansion", ""),
            "generations":      met_fast.get("num_generation", ""),
            "reinsertions":     met_fast.get("num_reinsertion", ""),
            "time_fast_s":      time_fast,
            "time_maya_s":      time_maya if ok_maya else f">{MAYA_HARD_CAP}s",
            "speedup_vs_maya":  speedup if ok_maya else speedup_str,
            "cmp_chooseh_fast": fast_cmp,
            "cmp_chooseh_maya": cmp_maya if ok_maya else "",
            "ops_reduction":    f"{ops_red:.1f}" if ops_red else ("> 1000" if not ok_maya else ""),
            "total_cmp_fast":   met_fast.get("cmp_total", ""),
            "soundness":        soundness,
            "maya_status":      maya_status
        }
        append_to_csv(row)
        print(f"  Row appended to {CSV_OUT}\n")

    print("=================================================================")
    print(f"  EXTENDED CAMPAIGN COMPLETE [{datetime.now().strftime('%H:%M:%S')}]")
    print("=================================================================")

if __name__ == "__main__":
    main()
