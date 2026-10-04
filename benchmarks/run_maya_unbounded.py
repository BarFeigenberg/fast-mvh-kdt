#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
run_maya_unbounded.py
Re-runs (or recovers) Maya with ZERO timeout on instances where she previously
timed out in the 3600s-capped campaign.

Pending instances:
  1. N=10, M=6, rho=0.0, K=25   -- FAST=58.57s, Maya solution on disk but time lost
  2. N=8,  M=7, rho=0.0, K=25   -- FAST=264.58s, Maya never ran
  3. N=8,  M=8, rho=0.0, K=25   -- FAST=73.72s,  Maya never ran
"""

import io
import os
import sys
import time
import csv
import subprocess
from datetime import datetime
from typing import Dict, Any, Tuple

# Force UTF-8 output to avoid Windows cp1252 encoding errors
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")
sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding="utf-8", errors="replace")

FAST_MVH_BIN = os.path.abspath("build/Release/fast_mvh.exe")
CSV_OUT = os.path.abspath("benchmarks/runs/fast_scaling_up_to_8D.csv")
SCRATCH_DIR = os.path.abspath("scratchpad/deep_scaling")

TARGETS = [
    # N=10 M=6: Maya solution already on disk from prior crashed run; re-run to get clean time
    {"N": 10, "M": 6, "rho": 0.0, "K": 25,  "fast_time": 58.565,   "fast_sols": 133699,  "fast_cmp": 68459210},
    # N=8 M=7: Maya never ran
    {"N":  8, "M": 7, "rho": 0.0, "K": 25,  "fast_time": 264.584,  "fast_sols": 192162,  "fast_cmp": 54288160},
    # N=8 M=8: Maya never ran
    {"N":  8, "M": 8, "rho": 0.0, "K": 25,  "fast_time": 73.715,   "fast_sols": 369351,  "fast_cmp": 201597168},
]

def parse_solver_output(stdout: str) -> Dict[str, Any]:
    metrics: Dict[str, Any] = {}
    for line in stdout.splitlines():
        if "=" in line:
            for token in line.split("\t"):
                if "=" in token:
                    k, v = token.split("=", 1)
                    try:
                        metrics[k.strip()] = float(v.strip()) if "." in v else int(v.strip())
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
                continue
    return vectors

def run_maya_unbounded(inst_dir: str, mvh_path: str, N: int, M: int, sol_out: str) -> Tuple[bool, float, Dict[str, Any]]:
    """Run Maya with cutoffTime=0 (unlimited). No subprocess timeout."""
    obj_args = [str(i) for i in range(M)]
    cmd = [
        FAST_MVH_BIN,
        "--map", inst_dir,
        "--start", "1",
        "--goal", str(N * N),
        "--objectives", *obj_args,
        "--algorithm", "L_NAMOA_DR_MVH_INSTRUMENTED",
        "--mvh", mvh_path,
        "--cutoffTime", "0",
        "--sol-out", sol_out
    ]
    t0 = time.perf_counter()
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True)
        wall = time.perf_counter() - t0
        metrics = parse_solver_output(proc.stdout)
        metrics["wall_clock"] = wall
        timed_out = metrics.get("time_limit_reached", 0) == 1
        return (not timed_out), wall, metrics
    except Exception as e:
        return False, 0.0, {"error": str(e)}

def run_fast(inst_dir: str, mvh_path: str, N: int, M: int, sol_out: str) -> Tuple[bool, float, Dict[str, Any]]:
    """Run FAST solver to get a fresh solution file for bit-comparison."""
    obj_args = [str(i) for i in range(M)]
    cmd = [
        FAST_MVH_BIN,
        "--map", inst_dir,
        "--start", "1",
        "--goal", str(N * N),
        "--objectives", *obj_args,
        "--algorithm", "L_NAMOA_DR_MVH_FAST",
        "--mvh", mvh_path,
        "--cutoffTime", "0",
        "--sol-out", sol_out
    ]
    t0 = time.perf_counter()
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True)
        wall = time.perf_counter() - t0
        metrics = parse_solver_output(proc.stdout)
        return True, wall, metrics
    except Exception as e:
        return False, 0.0, {"error": str(e)}

def append_to_csv(row: Dict[str, Any]):
    fieldnames = [
        "timestamp", "N", "M", "rho", "K", "solutions", "expansions", "generations", "reinsertions",
        "time_fast_s", "time_maya_s", "speedup_vs_maya",
        "cmp_chooseh_fast", "cmp_chooseh_maya", "ops_reduction",
        "total_cmp_fast", "soundness", "maya_status"
    ]
    with open(CSV_OUT, "a", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writerow({k: row.get(k, "") for k in fieldnames})
        f.flush()

def main():
    print("=================================================================")
    print("  UNBOUNDED MAYA RE-RUN: Recovering Exact Speedup Ratios")
    print(f"  Start Time: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print("  Maya cutoffTime = 0 (unlimited) | Machine: plugged in")
    print("=================================================================\n")

    for idx, cfg in enumerate(TARGETS, 1):
        N, M, rho, K = cfg["N"], cfg["M"], cfg["rho"], cfg["K"]
        fast_time_ref = cfg["fast_time"]
        fast_cmp_ref  = cfg["fast_cmp"]

        inst_dir  = os.path.join(SCRATCH_DIR, f"grid_{N}x{N}_M{M}_rho{rho}")
        mvh_path  = os.path.join(inst_dir, f"target_{N*N}_K{K}.mvh")
        fast_sol  = os.path.join(SCRATCH_DIR, f"sol_fast_N{N}_M{M}_rho{rho}_K{K}.txt")
        maya_sol  = os.path.join(SCRATCH_DIR, f"sol_maya_unbounded_N{N}_M{M}_rho{rho}_K{K}.txt")

        print(f"[{idx}/{len(TARGETS)}] === N={N}, M={M}, rho={rho}, K={K} ===")
        print(f"    FAST reference: {fast_time_ref:.3f}s | {cfg['fast_sols']:,} solutions")

        # Step 1: ensure FAST solution file exists for bit-comparison
        if not os.path.exists(fast_sol):
            print("    -> Running FAST to generate reference solution file...")
            ok_f, ft, _ = run_fast(inst_dir, mvh_path, N, M, fast_sol)
            if ok_f:
                print(f"    [FAST done in {ft:.3f}s]")
            else:
                print("    [WARN] FAST failed. Skipping soundness check.")
        else:
            print(f"    -> FAST solution file already exists ({os.path.getsize(fast_sol):,} bytes)")

        # Step 2: run Maya with unlimited timeout
        print(f"    -> Running Maya (UNLIMITED)... [started {datetime.now().strftime('%H:%M:%S')}]", flush=True)
        t_start_str = datetime.now().strftime('%H:%M:%S')
        ok_maya, time_maya, met_maya = run_maya_unbounded(inst_dir, mvh_path, N, M, maya_sol)
        t_end_str = datetime.now().strftime('%H:%M:%S')

        # Step 3: soundness check
        fast_vecs = parse_sol_file(fast_sol)
        maya_vecs = parse_sol_file(maya_sol)

        cmp_maya = met_maya.get("cmp_chooseh", 0) or met_maya.get("cmpCH", 0)
        ops_red  = (cmp_maya / fast_cmp_ref) if (cmp_maya and fast_cmp_ref) else None

        if ok_maya and fast_vecs and maya_vecs:
            soundness = "BIT_IDENTICAL" if fast_vecs == maya_vecs else \
                        f"DIVERGED (fast:{len(fast_vecs)}, maya:{len(maya_vecs)})"
        elif ok_maya:
            soundness = f"MAYA_OK_NO_FAST_REF (sol count: {len(maya_vecs):,})"
        else:
            soundness = "MAYA_FAILED"

        speedup = time_maya / fast_time_ref if fast_time_ref > 0 else None

        print(f"\n    [RESULT] Maya completed in {time_maya:.2f}s ({t_start_str} -> {t_end_str})")
        print(f"    Solutions: {len(maya_vecs):,} | Soundness: {soundness}")
        if speedup:
            print(f"    >>> EXACT SPEEDUP: {speedup:.2f}x | OPS REDUCTION: {ops_red:.1f}x <<<")
        print()

        row = {
            "timestamp":        datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
            "N": N, "M": M, "rho": rho, "K": K,
            "solutions":        len(maya_vecs) if ok_maya else cfg["fast_sols"],
            "expansions":       met_maya.get("num_expansion", ""),
            "generations":      met_maya.get("num_generation", ""),
            "reinsertions":     met_maya.get("num_reinsertion", ""),
            "time_fast_s":      fast_time_ref,
            "time_maya_s":      time_maya,
            "speedup_vs_maya":  speedup,
            "cmp_chooseh_fast": fast_cmp_ref,
            "cmp_chooseh_maya": cmp_maya,
            "ops_reduction":    ops_red,
            "total_cmp_fast":   "",
            "soundness":        soundness + " (UNBOUNDED_RERUN)",
            "maya_status":      "OK_UNBOUNDED" if ok_maya else "FAILED"
        }
        append_to_csv(row)
        print(f"    Row appended to {CSV_OUT}\n")

    print("=================================================================")
    print(f"  ALL UNBOUNDED RE-RUNS COMPLETE  [{datetime.now().strftime('%H:%M:%S')}]")
    print("=================================================================")

if __name__ == "__main__":
    main()
