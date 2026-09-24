#!/usr/bin/env python3
import os
import sys
import subprocess
import time
import csv
import itertools
import random
import glob

# Paths
FAST_MVH_BIN = os.path.abspath("build/Release/fast_mvh.exe")
SCRATCH_DIR = os.path.abspath("scratchpad/adaptive_explorer")
RESULTS_DIR = os.path.abspath("benchmarks/runs/adaptive_explorer")
CSV_PATH = os.path.join(RESULTS_DIR, "adaptive_findings.csv")
MD_PATH = os.path.join(RESULTS_DIR, "adaptive_summary.md")

os.makedirs(SCRATCH_DIR, exist_ok=True)
os.makedirs(RESULTS_DIR, exist_ok=True)

ROI_TIMEOUT = 300

def generate_grid_and_mvh(N, M, rho, K, seed=42):
    inst_dir = os.path.join(SCRATCH_DIR, f"grid_{N}x{N}_M{M}_rho{rho}")
    os.makedirs(inst_dir, exist_ok=True)
    
    if not os.path.exists(os.path.join(inst_dir, "obj_0.gr")):
        cmd = [
            sys.executable, "benchmarks/generators/generate_grid.py",
            "--rows", str(N), "--cols", str(N), "-M", str(M),
            "--rho", str(rho), "--seed", str(seed), "--out-dir", inst_dir
        ]
        subprocess.run(cmd, check=True)
        for m in range(M):
            src = os.path.join(inst_dir, f"c{m+1}.gr")
            dst = os.path.join(inst_dir, f"obj_{m}.gr")
            if os.path.exists(src):
                if os.path.exists(dst): os.remove(dst)
                os.rename(src, dst)
                
    mvh_path = os.path.join(inst_dir, f"target_{N*N}_K{K}.mvh")
    if not os.path.exists(mvh_path):
        cmd = [
            sys.executable, "benchmarks/generators/generate_mvh.py",
            "--map", inst_dir, "--goal", str(N*N), "-K", str(K),
            "--seed", str(seed), "--out", mvh_path
        ]
        subprocess.run(cmd, check=True)
        
    return inst_dir, mvh_path

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
            writer.writerow([
                "N", "M", "Rho", "K", "Solutions", "Generations",
                "Roi_Time_s", "Maya_Time_s", "Shahaf_Time_s",
                "Roi_Speedup_vs_Maya", "V3_Speedup_vs_Maya", "Roi_Cmp", "Maya_Cmp", "V3_Cmp"
            ])

def append_csv(row):
    with open(CSV_PATH, "a", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(row)

def update_md(results):
    with open(MD_PATH, "w", encoding="utf-8") as f:
        f.write("# Adaptive Exploration Findings\n\n")
        f.write("| N | M | Rho | K | Sols | V3 Time | Maya Time | Roi Time | Shahaf Time | V3 Speedup |\n")
        f.write("|:---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|\n")
        for r in results:
            rt = f"{r['Roi_Time_s']:.2f}" if isinstance(r['Roi_Time_s'], float) else r['Roi_Time_s']
            mt = f"{r['Maya_Time_s']:.2f}" if isinstance(r['Maya_Time_s'], float) else r['Maya_Time_s']
            st = f"{r['Shahaf_Time_s']:.2f}" if isinstance(r['Shahaf_Time_s'], float) else r['Shahaf_Time_s']
            su = f"**{r['Speedup']:.2f}x**" if isinstance(r['Speedup'], float) else r['Speedup']
            cr = f"**{r['Cmp_Reduction']:.2f}x**" if isinstance(r['Cmp_Reduction'], float) else r['Cmp_Reduction']
            f.write(f"| {r['N']}x{r['N']} | {r['M']} | {r['Rho']} | {r['K']} | {r['Solutions']} | {rt} | {mt} | {st} | {su} | {cr} |\n")

def run_solver(bin_path, inst_dir, mvh_path, N, M, algo, timeout):
    obj_args = [str(i) for i in range(M)]
    cmd = [
        bin_path, "--map", inst_dir, "--start", "1", "--goal", str(N*N),
        "--objectives", *obj_args, "--algorithm", algo,
        "--mvh", mvh_path, "--cutoffTime", str(timeout)
    ]
    t0 = time.perf_counter()
    try:
        # Give subprocess a little extra time before forceful kill
        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=timeout+5)
        dt = time.perf_counter() - t0
        met = parse_solver_output(res.stdout)
        if met.get("runtime_s", 0.0) >= timeout or met.get("time_limit_reached") == 1:
            return None, dt, met
        return True, dt, met
    except subprocess.TimeoutExpired:
        return None, timeout, {}

def main():
    print("=========================================================")
    print("  AUTONOMOUS ADAPTIVE EXPLORATION (Roi vs Maya vs Shahaf)")
    print("=========================================================")
    init_csv()
    
    # State space bounds
    N_bounds = (10, 50)
    M_bounds = (3, 6)
    K_bounds = (10, 200)
    Rho_bounds = (-0.9, 0.9)
    
    # Initial state
    queue = [
        (15, 3, -0.6, 100),
        (15, 3, -0.6, 150),
        (15, 3, -0.9, 100),
        (15, 3, -0.3, 150),
        (15, 3, -0.3, 200)
    ]
    visited = set()
    results = []
    
    while queue:
        state = queue.pop(0)
        if state in visited:
            continue
        visited.add(state)
        
        N, M, rho, K = state
        print(f"\n[Evaluating] N={N}, M={M}, rho={rho}, K={K}")
        sys.stdout.flush()
        
        inst_dir, mvh_path = generate_grid_and_mvh(N, M, rho, K)
        
        # 1. Run Roi
        print(f" -> Roi (Timeout {ROI_TIMEOUT}s)... ", end="", flush=True)
        roi_ok, roi_time, roi_met = run_solver(FAST_MVH_BIN, inst_dir, mvh_path, N, M, "L_NAMOA_KDT_CHOOSEH", ROI_TIMEOUT)
        
        if not roi_ok:
            print("TIMEOUT")
            append_csv([N, M, rho, K, "-", "-", "TIMEOUT", "-", "-", "-", "-", "-", "-"])
            results.append({
                "N": N, "M": M, "Rho": rho, "K": K, "Solutions": "-",
                "Roi_Time_s": "TIMEOUT", "Maya_Time_s": "-", "Shahaf_Time_s": "-",
                "Speedup": "-", "Cmp_Reduction": "-"
            })
            update_md(results)
            
            # Pivot to easier instances
            # Decrease N, decrease M, decrease K
            easier = [
                (max(N_bounds[0], N-5), M, rho, K),
                (N, max(M_bounds[0], M-1), rho, K),
                (N, M, rho, max(K_bounds[0], K-20))
            ]
            for st in easier:
                if st not in visited and st not in queue:
                    queue.append(st)
            continue
            
        print(f"SUCCESS ({roi_time:.2f}s)")
        solutions = roi_met.get("num_solutions", 0)
        generations = roi_met.get("num_generations", 0)
        
        # 2. Run Maya
        maya_timeout = max(3600, int(roi_time * 20)) # essentially no strict limit, but bounded to avoid infinite hanging
        print(f" -> Maya (Timeout {maya_timeout}s)... ", end="", flush=True)
        maya_ok, maya_time, maya_met = run_solver(FAST_MVH_BIN, inst_dir, mvh_path, N, M, "L_NAMOA_DR_MVH_INSTRUMENTED", maya_timeout)
        if not maya_ok:
            print("TIMEOUT")
            maya_time_val = "TIMEOUT"
            speedup = "-"
            maya_cmp = "-"
            cmp_red = "-"
        else:
            print(f"SUCCESS ({maya_time:.2f}s)")
            maya_time_val = maya_time
            speedup = maya_time / roi_time if roi_time > 0 else 0
            maya_cmp = maya_met.get("cmp_chooseh", 0)
            roi_cmp = roi_met.get("cmp_chooseh", 0)
            cmp_red = maya_cmp / roi_cmp if roi_cmp > 0 else 1.0
            
        # 3. Opportunistic Shahaf Check
        shahaf_time_val = "-"
        if solutions >= 20 or generations > 5000:
            print(f" -> Massive frontier detected (Sols={solutions}, Gen={generations}). Running Shahaf...", end="", flush=True)
            shahaf_timeout = min(3600, int(roi_time * 10))
            shahaf_ok, shahaf_time, shahaf_met = run_solver(FAST_MVH_BIN, inst_dir, mvh_path, N, M, "L_NAMOA_DR_MVH_KDT", shahaf_timeout)
            if not shahaf_ok:
                print("TIMEOUT")
                shahaf_time_val = "TIMEOUT"
            else:
                print(f"SUCCESS ({shahaf_time:.2f}s)")
                shahaf_time_val = shahaf_time
                
        append_csv([
            N, M, rho, K, solutions, generations,
            roi_time, maya_time_val, shahaf_time_val,
            speedup, roi_met.get("cmp_chooseh", 0), maya_cmp, cmp_red
        ])
        results.append({
            "N": N, "M": M, "Rho": rho, "K": K, "Solutions": solutions,
            "Roi_Time_s": roi_time, "Maya_Time_s": maya_time_val, "Shahaf_Time_s": shahaf_time_val,
            "Speedup": speedup, "Cmp_Reduction": cmp_red
        })
        update_md(results)
        
        # Determine next states
        # Add a mix of harder and easier, relying on the queue to spread out
        next_states = []
        if roi_time < 5.0:
            # FAST -> push much harder
            next_states.append((min(N_bounds[1], N+5), M, rho, K))
            next_states.append((N, min(M_bounds[1], M+1), rho, K))
            next_states.append((N, M, rho, min(K_bounds[1], K+25)))
            # Anti-correlated is harder
            new_rho = max(Rho_bounds[0], rho - 0.3)
            next_states.append((N, M, round(new_rho, 1), K))
        else:
            # Medium -> spread around
            next_states.append((min(N_bounds[1], N+5), M, rho, K))
            next_states.append((max(N_bounds[0], N-5), M, rho, K))
            next_states.append((N, min(M_bounds[1], M+1), rho, K))
            next_states.append((N, M, rho, min(K_bounds[1], K+10)))
            new_rho = min(Rho_bounds[1], rho + 0.3)
            next_states.append((N, M, round(new_rho, 1), K))
            
        random.shuffle(next_states) # Avoid deterministic spiraling in one parameter
        for st in next_states:
            if st not in visited and st not in queue:
                queue.append(st)

if __name__ == "__main__":
    main()
