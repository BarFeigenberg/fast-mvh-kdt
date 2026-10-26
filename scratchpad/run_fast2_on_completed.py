#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Run FAST2 (L_NAMOA_DR_MVH_FAST) across previously completed benchmark instances.
Validates 100% bit-identical soundness against stored Maya oracle solution files,
measures fresh FAST2 wall-clock runtime, and computes exact speedup ratios.
"""

import os
import sys
import time
import subprocess
from datetime import datetime

BIN = os.path.abspath("build/Release/fast_mvh.exe")
SCRATCH = os.path.abspath("scratchpad/deep_scaling")

# Completed instances list with known Maya times (seconds)
# (N, M, rho, K, maya_time_s)
INSTANCES = [
    # M = 6
    (6, 6,  0.0, 25, 0.6077),
    (6, 6, -0.2, 25, 0.9051),
    (6, 6,  0.0, 50, 0.6944),
    (8, 6,  0.0, 25, 272.181),
    (8, 6, -0.2, 25, 256.253),
    (8, 6,  0.0, 50, 187.301),
    (10, 6, 0.0, 25, 6655.261),  # Maya ~1.85h

    # M = 7
    (5, 7,  0.0, 25, 0.7995),
    (5, 7, -0.2, 25, 3.6410),
    (6, 7,  0.0, 25, 37.020),
    (6, 7, -0.2, 25, 43.144),
    (6, 7,  0.0, 50, 38.787),
    (7, 7,  0.0, 25, 1001.158),
    (8, 7,  0.0, 25, 6813.046),  # Maya ~1.89h

    # M = 8
    (5, 8,  0.0, 25, 1.2212),
    (5, 8, -0.2, 25, 0.9456),
    (6, 8,  0.0, 25, 25.048),
    (6, 8, -0.2, 25, 35.485),
    (6, 8,  0.0, 50, 30.689),
    (7, 8,  0.0, 25, 214.231),
    (8, 8,  0.0, 25, 8995.451),  # Maya ~2.50h
    (9, 8,  0.0, 25, 246953.938), # Maya ~68.60h (2.86 days)
]

def parse_metrics(stdout: str) -> dict:
    m = {}
    for ln in stdout.splitlines():
        if "=" not in ln:
            continue
        for tok in ln.split("\t"):
            if "=" in tok:
                k, v = tok.split("=", 1)
                k = k.strip()
                v = v.strip()
                try:
                    m[k] = float(v) if "." in v else int(v)
                except ValueError:
                    m[k] = v
    return m

def parse_solutions(filepath: str) -> set:
    sols = set()
    if not os.path.exists(filepath):
        return sols
    with open(filepath, "r", encoding="utf-8") as f:
        for ln in f:
            ln = ln.strip()
            if not ln or ln.startswith("#"):
                continue
            delim = "," if "," in ln else None
            try:
                vec = tuple(int(x.strip()) for x in ln.split(delim) if x.strip())
                if vec:
                    sols.add(vec)
            except ValueError:
                pass
    return sols

def main():
    print("=" * 80)
    print("  FAST2 VALIDATION & BENCHMARK ON ESTABLISHED INSTANCES")
    print(f"  Time: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print(f"  Binary: {BIN}")
    print("=" * 80)

    results = []

    for idx, (N, M, rho, K, tm) in enumerate(INSTANCES, 1):
        inst_dir = os.path.join(SCRATCH, f"grid_{N}x{N}_M{M}_rho{rho}")
        mvh_path = os.path.join(inst_dir, f"target_{N*N}_K{K}.mvh")
        sol_out = os.path.join(SCRATCH, f"fresh_fast2_sol_N{N}_M{M}_rho{rho}_K{K}.txt")
        
        # Check reference solution
        ref_maya = os.path.join(SCRATCH, f"sol_maya_N{N}_M{M}_rho{rho}_K{K}.txt")
        if not os.path.exists(ref_maya):
            ref_maya = os.path.join(SCRATCH, f"sol_maya_unbounded_N{N}_M{M}_rho{rho}_K{K}.txt")

        print(f"\n[{idx}/{len(INSTANCES)}] N={N} ({N}x{N}) | M={M} | rho={rho} | K={K} ...")

        if not os.path.exists(inst_dir) or not os.path.exists(mvh_path):
            print(f"  [ERROR] Instance files missing at {inst_dir}")
            continue

        cmd = [
            BIN,
            "--map", inst_dir,
            "--start", "1",
            "--goal", str(N * N),
            "--objectives", *[str(i) for i in range(M)],
            "--algorithm", "L_NAMOA_DR_MVH_FAST",
            "--mvh", mvh_path,
            "--cutoffTime", "0",
            "--sol-out", sol_out
        ]

        t0 = time.perf_counter()
        proc = subprocess.run(cmd, capture_output=True, text=True)
        tf = time.perf_counter() - t0

        if proc.returncode != 0:
            print(f"  [FAIL] Error running FAST2: {proc.stderr}")
            continue

        met = parse_metrics(proc.stdout)
        num_sols = met.get("num_solutions", 0)
        exp = met.get("num_expansion", 0)

        # Check soundness
        sols_fast = parse_solutions(sol_out)
        sols_maya = parse_solutions(ref_maya) if os.path.exists(ref_maya) else set()

        if sols_maya:
            if sols_fast == sols_maya:
                soundness = "BIT_IDENTICAL"
            else:
                soundness = f"MISMATCH (FAST:{len(sols_fast)}, Maya:{len(sols_maya)})"
        else:
            soundness = "ORACLE_NOT_FOUND"

        speedup = (tm / tf) if tf > 0 else 0.0

        print(f"  -> FAST2 Time: {tf:.4f}s | Maya Time: {tm:.2f}s | Speedup: {speedup:8.2f}x")
        print(f"  -> Solutions : {len(sols_fast):,} | Soundness: {soundness}")

        results.append({
            "N": N, "M": M, "rho": rho, "K": K,
            "sols": len(sols_fast),
            "exp": exp,
            "fast2_time_s": tf,
            "maya_time_s": tm,
            "speedup": speedup,
            "soundness": soundness
        })

    print("\n" + "=" * 80)
    print("  SUMMARY TABLE (FAST2 vs MAYA)")
    print("=" * 80)
    print(f"{'M':>2} | {'N':>2} | {'rho':>4} | {'Sols':>9} | {'FAST2 (s)':>11} | {'Maya (s)':>12} | {'Speedup':>10} | {'Soundness':>14}")
    print("-" * 80)
    for r in results:
        maya_str = f"{r['maya_time_s']:.2f}s"
        if r['maya_time_s'] > 3600:
            maya_str = f"{r['maya_time_s']/3600:.2f}h"
        fast_str = f"{r['fast2_time_s']:.3f}s"
        if r['fast2_time_s'] > 60:
            fast_str = f"{r['fast2_time_s']/60:.2f}m"
        print(f"{r['M']:2d} | {r['N']:2d} | {r['rho']:4.1f} | {r['sols']:9,d} | {fast_str:>11} | {maya_str:>12} | {r['speedup']:9.2f}x | {r['soundness']:>14}")
    print("=" * 80)

if __name__ == "__main__":
    main()
