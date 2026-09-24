#!/usr/bin/env python3
import os
import sys
import subprocess
import time
import csv
import itertools
import random

# Paths
FAST_MVH_BIN = os.path.abspath("build/Release/fast_mvh.exe")
SCRATCH_DIR = os.path.abspath("scratchpad/deep_eval")
RESULTS_DIR = os.path.abspath("benchmarks/runs/roi_vs_maya_deep_eval")
CSV_PATH = os.path.join(RESULTS_DIR, "roi_vs_maya_deep_eval.csv")
MD_PATH = os.path.join(RESULTS_DIR, "roi_vs_maya_deep_eval.md")

os.makedirs(SCRATCH_DIR, exist_ok=True)
os.makedirs(RESULTS_DIR, exist_ok=True)

def generate_grid_and_mvh(N, M, rho, K, seed=42):
    inst_dir = os.path.join(SCRATCH_DIR, f"grid_{N}x{N}_M{M}_rho{rho}")
    os.makedirs(inst_dir, exist_ok=True)
    
    # 1. Generate grid if not exists
    if not os.path.exists(os.path.join(inst_dir, "obj_0.gr")):
        cmd = [
            sys.executable, "benchmarks/generators/generate_grid.py",
            "--rows", str(N), "--cols", str(N), "-M", str(M),
            "--rho", str(rho), "--seed", str(seed), "--out-dir", inst_dir
        ]
        subprocess.run(cmd, check=True)
        # Rename c1.gr to obj_0.gr etc for solver compatibility
        for m in range(M):
            src = os.path.join(inst_dir, f"c{m+1}.gr")
            dst = os.path.join(inst_dir, f"obj_{m}.gr")
            if os.path.exists(src):
                if os.path.exists(dst):
                    os.remove(dst)
                os.rename(src, dst)
    
    # 2. Generate MVH
    mvh_path = os.path.join(inst_dir, f"target_{N*N}_K{K}.mvh")
    if not os.path.exists(mvh_path):
        cmd = [
            sys.executable, "benchmarks/generators/generate_mvh.py",
            "--map", inst_dir, "--goal", str(N*N), "-K", str(K),
            "--seed", str(seed), "--out", mvh_path
        ]
        subprocess.run(cmd, check=True)
        
    return inst_dir, mvh_path

def parse_cost_file(filepath: str):
    vectors = set()
    if not os.path.exists(filepath): return vectors
    with open(filepath, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"): continue
            delimiter = "," if "," in line else None
            parts = line.split(delimiter)
            vec = tuple(int(p.strip()) for p in parts if p.strip())
            if vec: vectors.add(vec)
    return vectors

def parse_solver_output(stdout: str):
    metrics = {}
    for line in stdout.splitlines():
        line = line.strip()
        if "=" in line and ("algorithm=" in line or "num_solutions=" in line):
            for token in line.split("\t"):
                if "=" in token:
                    k, v = token.split("=", 1)
                    try:
                        metrics[k.strip()] = float(v.strip()) if "." in v else int(v.strip())
                    except ValueError:
                        pass
    return metrics

def init_csv():
    if not os.path.exists(CSV_PATH):
        with open(CSV_PATH, "w", newline="", encoding="utf-8") as f:
            writer = csv.writer(f)
            writer.writerow(["N", "M", "Rho", "K", "Solutions", "Roi_Time_s", "Maya_Time_s", "Speedup", "Roi_Cmp", "Maya_Cmp", "Cmp_Reduction"])
            
def append_csv(row):
    with open(CSV_PATH, "a", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(row)

def update_md(results):
    with open(MD_PATH, "w", encoding="utf-8") as f:
        f.write("# Roi vs Maya: Deep Evaluation (Asymmetric Timeouts)\n\n")
        f.write("| N | M | Rho | K | Solutions | Roi Time (s) | Maya Time (s) | Speedup | Cmp Reduction |\n")
        f.write("|:---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|\n")
        for r in results:
            maya_time = f"{r['Maya_Time_s']:.2f}" if isinstance(r['Maya_Time_s'], float) else r['Maya_Time_s']
            roi_time = f"{r['Roi_Time_s']:.2f}" if isinstance(r['Roi_Time_s'], float) else r['Roi_Time_s']
            speedup = f"**{r['Speedup']:.2f}x**" if isinstance(r['Speedup'], float) else r['Speedup']
            cmp_red = f"**{r['Cmp_Reduction']:.2f}x**" if isinstance(r['Cmp_Reduction'], float) else r['Cmp_Reduction']
            f.write(f"| {r['N']}x{r['N']} | {r['M']} | {r['Rho']} | {r['K']} | {r['Solutions']} | {roi_time} | {maya_time} | {speedup} | {cmp_red} |\n")

def run():
    print("================================================================================")
    print("  ROI vs MAYA DEEP EVALUATION (Asymmetric Timeouts)")
    print("================================================================================")
    
    init_csv()
    results = []
    
    Ns = [20, 25, 30]
    Ms = [4, 5, 6]
    rhos = [-0.3, -0.6]
    Ks = [25, 50, 100]
    
    for N, M, rho, K in itertools.product(Ns, Ms, rhos, Ks):
        print(f"\nEvaluating N={N}, M={M}, rho={rho}, K={K}")
        inst_dir, mvh_path = generate_grid_and_mvh(N, M, rho, K)
        
        obj_args = [str(i) for i in range(M)]
        roi_sol = os.path.join(SCRATCH_DIR, f"roi_N{N}_M{M}_rho{rho}_K{K}_sol.txt")
        maya_sol = os.path.join(SCRATCH_DIR, f"maya_N{N}_M{M}_rho{rho}_K{K}_sol.txt")
        
        # 1. Run Roi (Timeout 600s)
        cmd_roi = [
            FAST_MVH_BIN, "--map", inst_dir, "--start", "1", "--goal", str(N*N),
            "--objectives", *obj_args, "--algorithm", "L_NAMOA_KDT_CHOOSEH",
            "--mvh", mvh_path, "--sol-out", roi_sol, "--cutoffTime", "600"
        ]
        
        print("  -> Running Roi's KD-CHOOSEH (Timeout 600s)...", end="", flush=True)
        t0 = time.perf_counter()
        roi_timeout = False
        try:
            res_roi = subprocess.run(cmd_roi, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=610)
            t_roi = time.perf_counter() - t0
            met_roi = parse_solver_output(res_roi.stdout)
            
            # Check if internal timeout reached
            if met_roi.get("runtime", 0.0) >= 600.0 or "time_limit_reached" in res_roi.stdout:
                roi_timeout = True
        except subprocess.TimeoutExpired:
            roi_timeout = True
            
        if roi_timeout:
            print(" TIMEOUT_ROI (>600s). Skipping Maya.")
            row = [N, M, rho, K, "-", "TIMEOUT", "-", "-", "-", "-", "-"]
            append_csv(row)
            results.append({"N": N, "M": M, "Rho": rho, "K": K, "Solutions": "-", "Roi_Time_s": "TIMEOUT", "Maya_Time_s": "-", "Speedup": "-", "Cmp_Reduction": "-"})
            update_md(results)
            continue
            
        print(f" SUCCESS ({t_roi:.2f}s).")
        
        # 2. Run Maya (Timeout 1200s)
        cmd_maya = [
            FAST_MVH_BIN, "--map", inst_dir, "--start", "1", "--goal", str(N*N),
            "--objectives", *obj_args, "--algorithm", "L_NAMOA_DR_MVH_INSTRUMENTED",
            "--mvh", mvh_path, "--sol-out", maya_sol, "--cutoffTime", "1200"
        ]
        
        print("  -> Running Maya's Baseline (Timeout 1200s)...", end="", flush=True)
        t0 = time.perf_counter()
        maya_timeout = False
        try:
            res_maya = subprocess.run(cmd_maya, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=1210)
            t_maya = time.perf_counter() - t0
            met_maya = parse_solver_output(res_maya.stdout)
            if met_maya.get("runtime", 0.0) >= 1200.0 or "time_limit_reached" in res_maya.stdout:
                maya_timeout = True
        except subprocess.TimeoutExpired:
            maya_timeout = True
            
        if maya_timeout:
            print(" TIMEOUT_MAYA (>1200s).")
            maya_time_str = "TIMEOUT"
            speedup = ">= " + str(round(1200.0 / t_roi, 2))
            cmp_maya = "-"
            cmp_ratio = "-"
            solutions = len(parse_cost_file(roi_sol))
        else:
            print(f" SUCCESS ({t_maya:.2f}s).")
            maya_time_str = round(t_maya, 2)
            speedup = round(t_maya / t_roi, 2)
            cmp_maya = met_maya.get("cmp_chooseh", 0)
            cmp_roi = met_roi.get("cmp_chooseh", 0)
            cmp_ratio = round(cmp_maya / cmp_roi, 2) if cmp_roi > 0 else 1.0
            
            # Verify correctness
            front_roi = parse_cost_file(roi_sol)
            front_maya = parse_cost_file(maya_sol)
            if front_roi != front_maya:
                print("  !!! MISMATCH in Pareto fronts !!!")
            solutions = len(front_roi)
            
        row = [N, M, rho, K, solutions, round(t_roi, 2), maya_time_str, speedup, met_roi.get("cmp_chooseh", 0), cmp_maya, cmp_ratio]
        append_csv(row)
        
        results.append({
            "N": N, "M": M, "Rho": rho, "K": K, 
            "Solutions": solutions, "Roi_Time_s": round(t_roi, 2), 
            "Maya_Time_s": maya_time_str, "Speedup": speedup, 
            "Cmp_Reduction": cmp_ratio
        })
        update_md(results)
        
if __name__ == "__main__":
    run()
