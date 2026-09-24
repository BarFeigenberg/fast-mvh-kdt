import os
import sys
import time
import subprocess
import json
import csv
import glob
import datetime

# Reuse logic from run_experiments.py
from run_experiments import execute_solver_run, get_git_commit_hash

def main():
    baseline_bin = os.path.abspath("build/baselines/bridging-mvh-dr/Release/MultivaluedHeuristicSearch.exe")
    exp_bin = os.path.abspath("benchmarks/runs/fast_mvh_exponential.exe")
    
    timeout = 60
    
    timestamp = datetime.datetime.now().strftime("%Y-%m-%d_%H%M%S")
    run_dir = os.path.join("benchmarks", "runs", f"apples_to_apples_M5_M8_{timestamp}")
    os.makedirs(run_dir, exist_ok=True)
    
    # Target topologies:
    # 10x10 (target 100), geom_n100 (target 99), geom_n200 (target 199)
    # The nodes are 0-indexed in geom? Let's check geom instances carefully. Usually start=1, goal=N. 
    # For geom_n100, N=100.
    targets = {
        "grid_10x10": 100,
        "geom_n100": 100,
        "geom_n200": 200,
        "synthetic_graph": 96
    }
    
    # Let's specify exactly which prefixes and dimensions to run
    configs = [
        ("synthetic_graph", 3),
        ("synthetic_graph", 4),
        ("synthetic_graph", 5)
    ]
    
    results = []
    
    for prefix, d in configs:
        target = targets[prefix]
        
        if prefix == "synthetic_graph":
            matches = ["baselines/bridging-mvh-dr/resources/synthetic_graph"]
        else:
            pattern = os.path.join("benchmarks", "instances", f"{prefix}_d{d}_tradeoff*")
            matches = glob.glob(pattern)
        
        # Only take up to 2 instances per config to keep it fast
        for map_dir in matches[:2]:
            if not os.path.isdir(map_dir):
                continue
                
            map_name = os.path.basename(os.path.normpath(map_dir))
            
            mvh_path = os.path.join(map_dir, f"target_{target}.mvh")
            
            # If MVH doesn't exist, try to generate it quickly!
            if not os.path.exists(mvh_path):
                print(f"[{map_name}] MVH not found. Attempting fast generation...")
                mvh_base = os.path.join(map_dir, f"target_{target}")
                obj_args = [str(i) for i in range(d)]
                cmd_gen = [
                    baseline_bin,
                    "--map", map_dir,
                    "--start", "1",
                    "--goal", str(target),
                    "--objectives", *obj_args,
                    "--algorithm", "APEX_MVH",
                    "--logging_file", mvh_base
                ]
                try:
                    subprocess.run(cmd_gen, check=True, timeout=60)
                    generated_mvh = f"{mvh_base}_apex_mvh.txt"
                    if os.path.exists(generated_mvh):
                        os.rename(generated_mvh, mvh_path)
                except Exception as e:
                    print(f"Skipping {map_name}: MVH generation failed or timed out: {e}")
                    continue
                    
            if not os.path.exists(mvh_path):
                print(f"Skipping {map_name}: MVH could not be generated.")
                continue
                
            mvh_args = ["--mvh", mvh_path]
            obj_args = [str(i) for i in range(d)]
            
            start_node = "1"
            goal_node = str(target)
            
            # 1. Baseline
            run_name_base = f"baseline_{map_name}"
            cmd_base = [
                baseline_bin,
                "--map", map_dir,
                "--start", start_node,
                "--goal", goal_node,
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
            
            print("Cooldown 2s...")
            time.sleep(2)
            
            # 2. Exponential KD-Tree
            run_name_exp = f"exponential_{map_name}"
            cmd_exp = [
                exp_bin,
                "--map", map_dir,
                "--start", start_node,
                "--goal", goal_node,
                "--objectives", *obj_args,
                "--timeout", str(timeout),
                "--sol-out", os.path.join(run_dir, f"{run_name_exp}_sols.txt"),
            ] + mvh_args
            
            print(f"[Exponential] {run_name_exp}")
            res_exp = execute_solver_run(cmd_exp, timeout + 5, run_dir, run_name_exp)
            res_exp["algorithm"] = "Exponential"
            res_exp["instance"] = map_name
            res_exp["num_objectives"] = d
            
            print("Cooldown 2s...")
            time.sleep(2)
            
            # Calculate speedup (handle timeouts and div by zero)
            time_base = res_base.get('wall_clock_s', 0.0)
            time_kdt = res_exp.get('wall_clock_s', 0.0)
            if res_base.get('timed_out') or res_exp.get('timed_out'):
                speedup = "N/A"
            elif time_kdt > 0:
                speedup = f"{time_base / time_kdt:.2f}x"
            else:
                speedup = "inf"
                
            # Keep them paired in results
            res_base["speedup"] = "N/A"
            res_exp["speedup"] = speedup
            
            results.append(res_base)
            results.append(res_exp)

    # Write summaries
    csv_file = os.path.join("benchmarks", "runs", "apples_to_apples_summary.csv")
    md_file = os.path.join("benchmarks", "runs", "apples_to_apples_summary.md")
    
    with open(md_file, "w", encoding="utf-8") as f:
        f.write("# Dimensional Scaling: Apples-to-Apples Summary\n\n")
        
        headers = ["Dimension M", "Algorithm", "Instance", "|P*|", "Time (s)", "cmpchk / checks", "Speedup (Base/KDT)", "Status"]
        f.write("| " + " | ".join(headers) + " |\n")
        f.write("|" + "|".join(["---"] * len(headers)) + "|\n")
        
        for r in results:
            m = str(r.get("num_objectives", ""))
            algo = str(r.get("algorithm", ""))
            inst = str(r.get("instance", ""))
            sols = str(r.get("num_solutions", "0"))
            t = f"{r.get('wall_clock_s', 0.0):.3f}"
            
            # cmpchk for KDT, num_global_dominance_check for Baseline
            if algo == "Baseline":
                chk = str(r.get("num_global_dominance_check", "N/A"))
            else:
                chk = str(r.get("cmpchk", "N/A"))
                
            speedup = r.get("speedup", "N/A")
            status = "TIMEOUT" if r.get("timed_out") else ("OK" if r.get("exit_code") == 0 else f"ERR({r.get('exit_code')})")
            
            row = [m, algo, inst, sols, t, chk, speedup, status]
            f.write("| " + " | ".join(row) + " |\n")
            
    print(f"\nDone! Wrote summary to {md_file}")

if __name__ == '__main__':
    sys.path.insert(0, os.path.abspath('benchmarks'))
    main()
