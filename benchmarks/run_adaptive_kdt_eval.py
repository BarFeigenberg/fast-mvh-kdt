#!/usr/bin/env python3
import os
import sys
import subprocess
import time
import csv

FAST_MVH_BIN = os.path.abspath("build/Release/fast_mvh.exe")
SCRATCH_DIR = os.path.abspath("scratchpad/adaptive_eval")
RESULTS_DIR = os.path.abspath("benchmarks/runs/adaptive_kdt_eval")
CSV_PATH = os.path.join(RESULTS_DIR, "adaptive_kdt_eval.csv")

os.makedirs(SCRATCH_DIR, exist_ok=True)
os.makedirs(RESULTS_DIR, exist_ok=True)

def generate_grid_and_mvh(N, M, rho, K, seed=42):
    inst_dir = os.path.join(SCRATCH_DIR, f"grid_{N}x{N}_M{M}_rho{rho}")
    os.makedirs(inst_dir, exist_ok=True)
    
    if not os.path.exists(os.path.join(inst_dir, "obj_0.gr")):
        subprocess.run([
            sys.executable, "benchmarks/generators/generate_grid.py",
            "--rows", str(N), "--cols", str(N), "-M", str(M),
            "--rho", str(rho), "--seed", str(seed), "--out-dir", inst_dir
        ], check=True)
        for m in range(M):
            src = os.path.join(inst_dir, f"c{m+1}.gr")
            dst = os.path.join(inst_dir, f"obj_{m}.gr")
            if os.path.exists(src):
                if os.path.exists(dst): os.remove(dst)
                os.rename(src, dst)
    
    mvh_path = os.path.join(inst_dir, f"target_{N*N}_K{K}.mvh")
    if not os.path.exists(mvh_path):
        subprocess.run([
            sys.executable, "benchmarks/generators/generate_mvh.py",
            "--map", inst_dir, "--goal", str(N*N), "-K", str(K),
            "--seed", str(seed), "--out", mvh_path
        ], check=True)
        
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
            writer.writerow(["N", "M", "Rho", "K", "Solutions", "Roi_Time_s", "Maya_Time_s", "Speedup", "Roi_Cmp", "Maya_Cmp", "Cmp_Reduction", "Action"])
            
def append_csv(row):
    with open(CSV_PATH, "a", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(row)

def run_solver(algo, inst_dir, N, M, mvh_path, timeout):
    obj_args = [str(i) for i in range(M)]
    sol_out = os.path.join(SCRATCH_DIR, f"sol_{algo}_{N}_{M}.txt")
    if os.path.exists(sol_out): os.remove(sol_out)
    cmd = [
        FAST_MVH_BIN, "--map", inst_dir, "--start", "1", "--goal", str(N*N),
        "--objectives", *obj_args, "--algorithm", algo,
        "--mvh", mvh_path, "--sol-out", sol_out, "--cutoffTime", str(timeout)
    ]
    t0 = time.perf_counter()
    try:
        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=timeout+5)
        t = time.perf_counter() - t0
        met = parse_solver_output(res.stdout)
        if met.get("runtime_s", 0.0) >= timeout or met.get("time_limit_reached") == 1:
            return None, sol_out, t
        if met.get("num_solutions", 0) == 0:
            if met.get("runtime_s", 0.0) >= timeout:
                return None, sol_out, t
        return met, sol_out, t
    except subprocess.TimeoutExpired:
        return None, sol_out, timeout

def run():
    print("================================================================================")
    print("  ADAPTIVE KDT EVALUATION")
    print("  Note: Using L_NAMOA_DR_MVH_INSTRUMENTED for Maya's Baseline to capture cmp_chooseh.")
    print("================================================================================")
    
    init_csv()
    
    Ms = [3, 4, 5]
    rhos = [0.0, -0.1, -0.3]
    Ks = [25, 50, 100]
    
    for M in Ms:
        for rho in rhos:
            for K in Ks:
                print(f"\n[Config] M={M}, rho={rho}, K={K}")
                
                N = 15
                direction = 0 # 0=start, -1=down, 1=up
                best_N_found = False
                
                while True:
                    inst_dir, mvh_path = generate_grid_and_mvh(N, M, rho, K)
                    print(f"  -> Testing N={N} (Timeout 180s)...", end="", flush=True)
                    
                    met_roi, sol_roi, t_roi = run_solver("L_NAMOA_KDT_CHOOSEH", inst_dir, N, M, mvh_path, timeout=180)
                    
                    sols_roi = 0
                    if met_roi:
                        sols_roi = len(parse_cost_file(sol_roi))
                        
                    if not met_roi or sols_roi == 0:
                        action = f"Timeout/0 sols ({t_roi:.1f}s). Stepping down."
                        print(f" {action}")
                        if direction == 1:
                            print("     Hit wall going up. We are done with this config.")
                            break
                        direction = -1
                        N -= 3
                        if N < 6:
                            print("     Grid too small. Aborting config.")
                            break
                        continue
                    else:
                        if t_roi < 30.0 and direction != -1:
                            action = f"Fast ({t_roi:.1f}s). Stepping up."
                            print(f" {action}")
                            direction = 1
                            N += 3
                            if N > 30:
                                print("     Max grid reached. Will use this.")
                                best_N_found = True
                            else:
                                continue
                        else:
                            print(f" Good runtime ({t_roi:.1f}s, {sols_roi} sols). Selected N={N}.")
                            best_N_found = True
                            
                    if best_N_found:
                        maya_timeout = 180
                        if t_roi > 60: maya_timeout = 360
                        
                        print(f"  -> Running Maya (Timeout {maya_timeout}s)...", end="", flush=True)
                        met_maya, sol_maya, t_maya = run_solver("L_NAMOA_DR_MVH_INSTRUMENTED", inst_dir, N, M, mvh_path, timeout=maya_timeout)
                        
                        maya_time_str = round(t_maya, 2)
                        speedup = round(t_maya / t_roi, 2)
                        cmp_maya = "-"
                        cmp_ratio = "-"
                        if not met_maya:
                            print(f" TIMEOUT.")
                            maya_time_str = "TIMEOUT"
                            speedup = f">={round(maya_timeout / t_roi, 2)}"
                            action = "Maya Timed out"
                        else:
                            print(f" SUCCESS ({t_maya:.2f}s).")
                            cmp_maya = met_maya.get("cmp_chooseh", 0)
                            cmp_roi = met_roi.get("cmp_chooseh", 0)
                            cmp_ratio = round(cmp_maya / cmp_roi, 2) if cmp_roi > 0 else 1.0
                            action = "Success"
                            
                            front_roi = parse_cost_file(sol_roi)
                            front_maya = parse_cost_file(sol_maya)
                            if front_roi != front_maya:
                                print("  !!! MISMATCH in Pareto fronts !!!")
                            
                        print(f"     [RESULT] N={N}, M={M}, rho={rho}, K={K} | Sols: {sols_roi} | Roi: {t_roi:.1f}s | Maya: {maya_time_str}s | Speedup: {speedup}x")
                        row = [N, M, rho, K, sols_roi, round(t_roi, 2), maya_time_str, speedup, met_roi.get("cmp_chooseh", 0), cmp_maya, cmp_ratio, action]
                        append_csv(row)
                        break

if __name__ == "__main__":
    run()
