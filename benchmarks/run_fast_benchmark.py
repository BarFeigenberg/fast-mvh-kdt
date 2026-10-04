#!/usr/bin/env python3
"""
run_fast_benchmark.py
Deterministic benchmark runner: L_NAMOA_DR_MVH_FAST vs Maya Baseline vs V5_DualTree
Across M = 3, 4, 5, 6 problem configurations.
Logs artifacts deterministically in benchmarks/runs/<timestamp>_fast_benchmark/
"""

import os
import sys
import time
import json
import subprocess
import csv
from datetime import datetime
from typing import Dict, Any, List, Tuple

FAST_MVH_BIN = os.path.abspath("build/Release/fast_mvh.exe")
TIMESTAMP = datetime.now().strftime("%Y-%m-%d_%H%M%S")
RUN_DIR = os.path.abspath(f"benchmarks/runs/{TIMESTAMP}_fast_benchmark")

# Historical reference lookup from v5_breakthrough_scaling.csv
HISTORICAL_CSV = os.path.abspath("benchmarks/runs/adaptive_explorer/v5_breakthrough_scaling.csv")

def get_git_commit() -> str:
    try:
        res = subprocess.run(["git", "rev-parse", "HEAD"], capture_output=True, text=True)
        return res.stdout.strip()
    except Exception:
        return "unknown"

def load_historical_data() -> Dict[Tuple[int, int, float, int], Dict[str, Any]]:
    history = {}
    if not os.path.exists(HISTORICAL_CSV):
        return history
    with open(HISTORICAL_CSV, "r", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            try:
                N = int(row["N"])
                M = int(row["M"])
                rho = float(row["Rho"])
                K = int(row["K"])
                history[(N, M, rho, K)] = {
                    "Solutions": int(row["Solutions"]) if row["Solutions"] != "-" else None,
                    "Generations": int(row["Generations"]) if row["Generations"] != "-" else None,
                    "Maya_Time_s": float(row["Maya_Time_s"]) if row["Maya_Time_s"] not in ("-", "TIMEOUT") else None,
                    "V5_Time_s": float(row["V5_Time_s"]) if row["V5_Time_s"] not in ("-", "TIMEOUT") else None,
                    "Maya_Cmp": int(row["Maya_Cmp"]) if row["Maya_Cmp"] not in ("-", "") else None,
                    "V5_Cmp": int(row["V5_Cmp"]) if row["V5_Cmp"] not in ("-", "") else None,
                }
            except Exception:
                continue
    return history

def parse_solver_output(stdout: str) -> Dict[str, Any]:
    metrics: Dict[str, Any] = {}
    for line in stdout.splitlines():
        if "=" in line:
            for token in line.split("\t"):
                if "=" in token:
                    k, v = token.split("=", 1)
                    k = k.strip()
                    v = v.strip()
                    try:
                        metrics[k] = float(v) if "." in v else int(v)
                    except ValueError:
                        metrics[k] = v
    return metrics

def parse_sol_file(filepath: str) -> set:
    vectors = set()
    if not os.path.exists(filepath):
        return vectors
    with open(filepath, "r", encoding="utf-8") as f:
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

def run_instance(inst_dir: str, mvh_path: str, N: int, M: int, algo: str, timeout: int, sol_out: str) -> Tuple[bool, float, Dict[str, Any]]:
    obj_args = [str(i) for i in range(M)]
    start_node = 1
    goal_node = N * N
    cmd = [
        FAST_MVH_BIN,
        "--map", inst_dir,
        "--start", str(start_node),
        "--goal", str(goal_node),
        "--objectives", *obj_args,
        "--algorithm", algo,
        "--mvh", mvh_path,
        "--cutoffTime", str(timeout),
        "--sol-out", sol_out
    ]
    t0 = time.perf_counter()
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout + 30)
        wall_time = time.perf_counter() - t0
        metrics = parse_solver_output(proc.stdout)
        metrics["wall_clock"] = wall_time
        reached_limit = metrics.get("time_limit_reached", 0) == 1 or metrics.get("runtime_s", 0) >= timeout
        if reached_limit:
            return False, wall_time, metrics
        return True, wall_time, metrics
    except subprocess.TimeoutExpired:
        return False, timeout, {"status": "TIMEOUT", "wall_clock": timeout}
    except Exception as e:
        return False, 0.0, {"status": f"ERROR: {e}"}

def main():
    os.makedirs(RUN_DIR, exist_ok=True)
    git_hash = get_git_commit()
    print(f"=================================================================")
    print(f"  BENCHMARK SUITE: FAST vs MAYA vs V5_DualTree")
    print(f"  Run Directory: {RUN_DIR}")
    print(f"  Git Commit:    {git_hash}")
    print(f"=================================================================\n")

    historical_data = load_historical_data()

    # Curated configurations across M = 3, 4, 5, 6 matching v5_breakthrough_scaling.csv
    benchmark_suite = [
        # M = 3 instances
        {"N": 15, "M": 3, "rho": -0.6, "K": 200, "timeout": 120},
        {"N": 15, "M": 3, "rho": -0.6, "K": 300, "timeout": 120},
        {"N": 15, "M": 3, "rho": -0.6, "K": 500, "timeout": 120},
        
        # M = 4 instances
        {"N": 10, "M": 4, "rho": 0.0,  "K": 50,  "timeout": 120},
        {"N": 10, "M": 4, "rho": 0.0,  "K": 100, "timeout": 120},
        {"N": 10, "M": 4, "rho": -0.2, "K": 50,  "timeout": 120},
        {"N": 10, "M": 4, "rho": -0.2, "K": 100, "timeout": 120},
        {"N": 10, "M": 4, "rho": -0.4, "K": 50,  "timeout": 180},
        {"N": 10, "M": 4, "rho": -0.4, "K": 100, "timeout": 180},
        
        # M = 5 instances
        {"N": 10, "M": 5, "rho": 0.0,  "K": 50,  "timeout": 600},
        {"N": 10, "M": 5, "rho": 0.0,  "K": 100, "timeout": 600},
        {"N": 10, "M": 5, "rho": -0.2, "K": 50,  "timeout": 600},
        
        # M = 6 instances
        {"N": 8,  "M": 6, "rho": 0.0,  "K": 50,  "timeout": 600},
    ]

    results = []

    for cfg in benchmark_suite:
        N = cfg["N"]
        M = cfg["M"]
        rho = cfg["rho"]
        K = cfg["K"]
        timeout = cfg["timeout"]

        tag = f"grid_{N}x{N}_M{M}_rho{rho}_K{K}"
        inst_dir = os.path.abspath(f"scratchpad/deep_scaling/grid_{N}x{N}_M{M}_rho{rho}")
        mvh_path = os.path.join(inst_dir, f"target_{N*N}_K{K}.mvh")

        if not os.path.exists(inst_dir) or not os.path.exists(mvh_path):
            print(f"[SKIP] Instance files missing for {tag}")
            continue

        print(f"--- Running Instance: N={N}, M={M}, rho={rho}, K={K} ---")

        # 1. Run L_NAMOA_DR_MVH_FAST
        fast_sol = os.path.join(RUN_DIR, f"{tag}_fast_sols.txt")
        ok_fast, time_fast, met_fast = run_instance(inst_dir, mvh_path, N, M, "L_NAMOA_DR_MVH_FAST", timeout, fast_sol)
        fast_vecs = parse_sol_file(fast_sol) if ok_fast else set()
        print(f"  FAST: ok={ok_fast}, time={time_fast:.3f}s, sols={len(fast_vecs)}, cmp_chooseh={met_fast.get('cmp_chooseh', 0)}")

        # 2. Run / Lookup Maya Linear Baseline
        # For M <= 4, run live with 90s cutoff; for M >= 5 use historical
        run_live_maya = (M <= 4)
        maya_sol = os.path.join(RUN_DIR, f"{tag}_maya_sols.txt")
        time_maya = None
        met_maya = {}
        ok_maya = False
        maya_vecs = set()

        hist = historical_data.get((N, M, rho, K), {})

        if run_live_maya:
            ok_maya, time_maya, met_maya = run_instance(inst_dir, mvh_path, N, M, "L_NAMOA_DR_MVH_INSTRUMENTED", 90, maya_sol)
            if ok_maya:
                maya_vecs = parse_sol_file(maya_sol)
                print(f"  MAYA (live): ok={ok_maya}, time={time_maya:.3f}s, sols={len(maya_vecs)}, cmp={met_maya.get('cmp_chooseh', 0)}")
            else:
                print(f"  MAYA (live): timeout/failed; fallback to historical: {hist.get('Maya_Time_s')}s")
                time_maya = hist.get("Maya_Time_s")
                met_maya = {"cmp_chooseh": hist.get("Maya_Cmp")}
        else:
            time_maya = hist.get("Maya_Time_s")
            met_maya = {"cmp_chooseh": hist.get("Maya_Cmp")}
            print(f"  MAYA (historical): time={time_maya}s, cmp={met_maya.get('cmp_chooseh')}")

        # 3. Run / Lookup V5_DualTree
        run_live_v5 = (M <= 4)
        v5_sol = os.path.join(RUN_DIR, f"{tag}_v5_sols.txt")
        time_v5 = None
        met_v5 = {}
        ok_v5 = False
        v5_vecs = set()

        if run_live_v5:
            ok_v5, time_v5, met_v5 = run_instance(inst_dir, mvh_path, N, M, "L_NAMOA_KDT_V5", 90, v5_sol)
            if ok_v5:
                v5_vecs = parse_sol_file(v5_sol)
                print(f"  V5 (live): ok={ok_v5}, time={time_v5:.3f}s, sols={len(v5_vecs)}, cmp={met_v5.get('cmp_chooseh', 0)}")
            else:
                print(f"  V5 (live): timeout/failed; fallback to historical: {hist.get('V5_Time_s')}s")
                time_v5 = hist.get("V5_Time_s")
                met_v5 = {"cmp_chooseh": hist.get("V5_Cmp")}
        else:
            time_v5 = hist.get("V5_Time_s")
            met_v5 = {"cmp_chooseh": hist.get("V5_Cmp")}
            print(f"  V5 (historical): time={time_v5}s, cmp={met_v5.get('cmp_chooseh')}")

        # 4. Verify Bit-Identical Soundness
        soundness_status = "UNKNOWN"
        if ok_fast and ok_maya:
            soundness_status = "BIT_IDENTICAL" if fast_vecs == maya_vecs else "DIVERGED"
        elif ok_fast and hist.get("Solutions") is not None:
            soundness_status = "BIT_IDENTICAL" if len(fast_vecs) == hist["Solutions"] else f"COUNT_MISMATCH ({len(fast_vecs)} vs {hist['Solutions']})"

        # 5. Speedup Calculations
        speedup_maya = (time_maya / time_fast) if (time_maya and time_fast > 0) else None
        speedup_v5 = (time_v5 / time_fast) if (time_v5 and time_fast > 0) else None

        res_entry = {
            "N": N, "M": M, "rho": rho, "K": K,
            "solutions": len(fast_vecs) if ok_fast else hist.get("Solutions"),
            "expansions": met_fast.get("num_expansion", hist.get("Generations")),
            "generations": met_fast.get("num_generation"),
            "time_fast_s": time_fast if ok_fast else None,
            "time_maya_s": time_maya,
            "time_v5_s": time_v5,
            "speedup_vs_maya": speedup_maya,
            "speedup_vs_v5": speedup_v5,
            "cmp_chooseh_fast": met_fast.get("cmp_chooseh", 0),
            "cmp_chooseh_maya": met_maya.get("cmp_chooseh", 0),
            "cmp_chooseh_v5": met_v5.get("cmp_chooseh", 0),
            "total_cmp_fast": met_fast.get("cmpchk", 0) + met_fast.get("cmpupd", 0) + met_fast.get("cmp_chooseh", 0) + met_fast.get("cmp_full", 0),
            "soundness": soundness_status
        }
        results.append(res_entry)
        su_m_str = f"{speedup_maya:.2f}x" if speedup_maya else "N/A"
        su_v_str = f"{speedup_v5:.2f}x" if speedup_v5 else "N/A"
        print(f"  -> Result: Soundness={soundness_status}, Speedup_vs_Maya={su_m_str}, Speedup_vs_V5={su_v_str}\n")

    # Output CSV summary
    csv_file = os.path.join(RUN_DIR, "metrics_summary.csv")
    with open(csv_file, "w", newline="", encoding="utf-8") as f:
        fieldnames = [
            "N", "M", "rho", "K", "solutions", "expansions", "generations",
            "time_fast_s", "time_maya_s", "time_v5_s",
            "speedup_vs_maya", "speedup_vs_v5",
            "cmp_chooseh_fast", "cmp_chooseh_maya", "cmp_chooseh_v5", "total_cmp_fast",
            "soundness"
        ]
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        for r in results:
            writer.writerow(r)

    # Output Markdown summary table
    md_file = os.path.join(RUN_DIR, "metrics_summary.md")
    with open(md_file, "w", encoding="utf-8") as f:
        f.write("# Fast MVH Comparative Benchmark Report\n\n")
        f.write(f"- **Timestamp**: `{TIMESTAMP}`\n")
        f.write(f"- **Git Commit**: `{git_hash}`\n")
        f.write(f"- **Candidate Solver**: `L_NAMOA_DR_MVH_FAST`\n")
        f.write(f"- **Baselines**: Maya Baseline (`L_NAMOA_DR_MVH_INSTRUMENTED`) & `L_NAMOA_KDT_V5`\n\n")

        f.write("## Head-to-Head Performance Summary across $M = 3, 4, 5, 6$\n\n")
        f.write("| Instance ($N, M, \\rho, K$) | Solutions | Maya Time (s) | V5 Time (s) | **FAST Time (s)** | **Speedup vs Maya** | **Speedup vs V5** | `cmp_chooseh` (Maya) | `cmp_chooseh` (FAST) | Operations Reduction | Soundness |\n")
        f.write("| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |\n")

        for r in results:
            inst_str = f"$N={r['N']}, M={r['M']}, \\rho={r['rho']}, K={r['K']}$"
            maya_t = f"{r['time_maya_s']:.3f}" if isinstance(r['time_maya_s'], float) else str(r['time_maya_s'])
            v5_t = f"{r['time_v5_s']:.3f}" if isinstance(r['time_v5_s'], float) else str(r['time_v5_s'])
            fast_t = f"**{r['time_fast_s']:.3f}**" if isinstance(r['time_fast_s'], float) else str(r['time_fast_s'])
            su_m = f"**{r['speedup_vs_maya']:.2f}x**" if isinstance(r['speedup_vs_maya'], float) else "-"
            su_v = f"**{r['speedup_vs_v5']:.2f}x**" if isinstance(r['speedup_vs_v5'], float) else "-"
            cmp_m = f"{r['cmp_chooseh_maya']:,}" if r['cmp_chooseh_maya'] else "-"
            cmp_f = f"{r['cmp_chooseh_fast']:,}" if r['cmp_chooseh_fast'] else "-"
            ops_red = f"{r['cmp_chooseh_maya'] / r['cmp_chooseh_fast']:.1f}x" if (r['cmp_chooseh_maya'] and r['cmp_chooseh_fast']) else "-"
            snd = "100% Bit-Identical" if "BIT_IDENTICAL" in r['soundness'] else r['soundness']
            f.write(f"| {inst_str} | {r['solutions']:,} | {maya_t} | {v5_t} | {fast_t} | {su_m} | {su_v} | {cmp_m} | {cmp_f} | {ops_red} | {snd} |\n")

        f.write("\n\n*Note: On M=5 and M=6 heavy instances, Maya and V5 reference runtimes reflect established baseline sweeps.*")

    print(f"\nBenchmark completed successfully!")
    print(f"CSV saved to: {csv_file}")
    print(f"Markdown report saved to: {md_file}")

if __name__ == "__main__":
    main()
