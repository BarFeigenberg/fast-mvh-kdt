import os
import sys
import time
import subprocess
import glob
import datetime

# Reuse logic from run_experiments.py
from run_experiments import execute_solver_run

def main():
    baseline_bin = os.path.abspath("build/baselines/bridging-mvh-dr/Release/MultivaluedHeuristicSearch.exe")
    exp_bin = os.path.abspath("benchmarks/runs/fast_mvh_exponential.exe")
    
    timeout = 120 # generous timeout since heuristic is fast to load
    
    timestamp = datetime.datetime.now().strftime("%Y-%m-%d_%H%M%S")
    run_dir = os.path.join("benchmarks", "runs", f"K_sweep_{timestamp}")
    os.makedirs(run_dir, exist_ok=True)
    
    targets = {
        "grid_10x10": 100,
        "grid_20x20": 400
    }
    
    K_values = [10, 20, 50, 100]
    
    configs = [
        ("grid_10x10", 3),
        ("grid_10x10", 4)
    ]
    
    results = []
    
    for prefix, d in configs:
        target = targets[prefix]
        pattern = os.path.join("benchmarks", "instances", f"{prefix}_d{d}_tradeoff*")
        matches = glob.glob(pattern)
        
        for map_dir in matches[:1]:
            map_name = os.path.basename(os.path.normpath(map_dir))
            full_mvh = os.path.join(map_dir, f"target_{target}.mvh")
            
            if not os.path.exists(full_mvh):
                print(f"Skipping {map_name}: Full MVH {full_mvh} not yet precomputed.")
                continue
                
            for K in K_values:
                k_mvh = os.path.join(map_dir, f"target_{target}_K{K}.mvh")
                
                # Truncate if it doesn't exist
                if not os.path.exists(k_mvh):
                    subprocess.run([sys.executable, "benchmarks/truncate_mvh.py", full_mvh, k_mvh, str(K)], check=True)
                
                mvh_args = ["--mvh", k_mvh]
                obj_args = [str(i) for i in range(d)]
                
                # 1. Baseline
                run_name_base = f"base_{map_name}_K{K}"
                cmd_base = [
                    baseline_bin,
                    "--map", map_dir,
                    "--start", "1",
                    "--goal", str(target),
                    "--algorithm", "L_NAMOA_DR_MVH",
                    "--objectives", *obj_args,
                    "--cutoffTime", str(timeout),
                    "--logging_file", os.path.join(run_dir, f"{run_name_base}_log"),
                ] + mvh_args
                
                print(f"\n[Baseline] {run_name_base}")
                res_base = execute_solver_run(cmd_base, timeout + 5, run_dir, run_name_base)
                res_base["algorithm"] = "Baseline"
                res_base["instance"] = map_name
                res_base["num_objectives"] = d
                res_base["K"] = K
                
                time.sleep(2)
                
                # 2. Exponential KD-Tree
                run_name_exp = f"exp_{map_name}_K{K}"
                cmd_exp = [
                    exp_bin,
                    "--map", map_dir,
                    "--start", "1",
                    "--goal", str(target),
                    "--objectives", *obj_args,
                    "--timeout", str(timeout),
                    "--sol-out", os.path.join(run_dir, f"{run_name_exp}_sols.txt"),
                ] + mvh_args
                
                print(f"[Exponential] {run_name_exp}")
                res_exp = execute_solver_run(cmd_exp, timeout + 5, run_dir, run_name_exp)
                res_exp["algorithm"] = "Exponential"
                res_exp["instance"] = map_name
                res_exp["num_objectives"] = d
                res_exp["K"] = K
                
                time.sleep(2)
                
                time_base = res_base.get('wall_clock_s', 0.0)
                time_kdt = res_exp.get('wall_clock_s', 0.0)
                if res_base.get('timed_out') or res_exp.get('timed_out'):
                    speedup = "N/A"
                elif time_kdt > 0:
                    speedup = f"{time_base / time_kdt:.2f}x"
                else:
                    speedup = "inf"
                    
                res_base["speedup"] = "N/A"
                res_exp["speedup"] = speedup
                
                results.append(res_base)
                results.append(res_exp)

    md_file = os.path.join("benchmarks", "runs", "K_sweep_summary.md")
    with open(md_file, "w", encoding="utf-8") as f:
        f.write("# Heuristic Cardinality Scaling (|H(s)| <= K)\n\n")
        
        headers = ["Dimension M", "Instance", "K", "Algorithm", "|P*|", "Time (s)", "cmpchk/checks", "Speedup (Base/KDT)"]
        f.write("| " + " | ".join(headers) + " |\n")
        f.write("|" + "|".join(["---"] * len(headers)) + "|\n")
        
        for r in results:
            m = str(r.get("num_objectives", ""))
            inst = str(r.get("instance", ""))
            k_val = str(r.get("K", ""))
            algo = str(r.get("algorithm", ""))
            sols = str(r.get("num_solutions", "0"))
            t = f"{r.get('wall_clock_s', 0.0):.3f}"
            chk = str(r.get("num_global_dominance_check", "N/A")) if algo == "Baseline" else str(r.get("cmpchk", "N/A"))
            speedup = r.get("speedup", "N/A")
            
            row = [m, inst, k_val, algo, sols, t, chk, speedup]
            f.write("| " + " | ".join(row) + " |\n")
            
    print(f"\nDone! Wrote summary to {md_file}")

if __name__ == '__main__':
    sys.path.insert(0, os.path.abspath('benchmarks'))
    main()
