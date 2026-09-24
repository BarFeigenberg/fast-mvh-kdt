#!/usr/bin/env python3
import os
import sys
import subprocess
import time
import csv

FAST_MVH_BIN = os.path.abspath("build/Release/fast_mvh.exe")
RESULTS_DIR = os.path.abspath("benchmarks/runs/variants_eval")
os.makedirs(RESULTS_DIR, exist_ok=True)
MD_PATH = os.path.join(RESULTS_DIR, "variants_summary.md")

ALGOS = [
    ("Maya_Linear", "L_NAMOA_DR_MVH_INSTRUMENTED"),
    ("Roi_Original", "L_NAMOA_KDT_CHOOSEH"),
    ("V1_ShortCircuit", "L_NAMOA_KDT_V1"),
    ("V2_BottomUp", "L_NAMOA_KDT_V2"),
    ("V3_ExpandOnly", "L_NAMOA_KDT_V3")
]

def generate_mvh(N, M, rho, K, seed=42):
    inst_dir = os.path.abspath(f"scratchpad/adaptive_explorer/grid_{N}x{N}_M{M}_rho{rho}")
    mvh_path = os.path.join(inst_dir, f"target_{N*N}_K{K}.mvh")
    if not os.path.exists(mvh_path):
        print(f"Generating MVH K={K}...")
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

def run():
    print("=========================================================")
    print("  VARIANTS BENCHMARK EVALUATION")
    print("=========================================================")
    
    configs = [
        (15, 3, -0.3, 50),
        (15, 3, -0.3, 75),
        (15, 3, -0.3, 100),
        (15, 3, -0.6, 75)
    ]
    
    with open(MD_PATH, "w", encoding="utf-8") as f:
        f.write("# Variants Benchmark Evaluation\n\n")
        f.write("| Instance | K | Algorithm | Time (s) | Speedup (vs Maya) | Cmp Checks | Cmp Red |\n")
        f.write("|:---|:---:|:---|:---:|:---:|:---:|:---:|\n")
    
    for N, M, rho, K in configs:
        inst_dir, mvh_path = generate_mvh(N, M, rho, K)
        obj_args = [str(i) for i in range(M)]
        
        print(f"\nEvaluating N={N}x{N}, M={M}, rho={rho}, K={K}")
        
        maya_time = None
        maya_cmp = None
        
        for name, algo in ALGOS:
            cmd = [
                FAST_MVH_BIN, "--map", inst_dir, "--start", "1", "--goal", str(N*N),
                "--objectives", *obj_args, "--algorithm", algo,
                "--mvh", mvh_path, "--cutoffTime", "300"
            ]
            
            t0 = time.perf_counter()
            try:
                res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=310)
                dt = time.perf_counter() - t0
                met = parse_solver_output(res.stdout)
                
                if met.get("runtime_s", 0.0) >= 300 or met.get("time_limit_reached") == 1:
                    print(f" -> {name}: TIMEOUT")
                    row = f"| {N}x{N} r{rho} | {K} | {name} | TIMEOUT | - | - | - |\n"
                else:
                    sols = met.get("num_solutions", 0)
                    cmp_chk = met.get("cmp_chooseh", 0)
                    print(f" -> {name}: {dt:.2f}s (Cmp: {cmp_chk}, Sols: {sols})")
                    
                    if name == "Maya_Linear":
                        maya_time = dt
                        maya_cmp = cmp_chk
                        spdup = "1.00x"
                        cmp_red = "1.00x"
                    else:
                        spdup = f"{maya_time / dt:.2f}x" if maya_time else "-"
                        cmp_red = f"{maya_cmp / cmp_chk:.2f}x" if maya_cmp and cmp_chk > 0 else "-"
                        
                    row = f"| {N}x{N} r{rho} | {K} | {name} | {dt:.2f} | **{spdup}** | {cmp_chk} | **{cmp_red}** |\n"
                    
                with open(MD_PATH, "a", encoding="utf-8") as f:
                    f.write(row)
                    
            except subprocess.TimeoutExpired:
                print(f" -> {name}: TIMEOUT (Subprocess)")
                with open(MD_PATH, "a", encoding="utf-8") as f:
                    f.write(f"| {N}x{N} r{rho} | {K} | {name} | TIMEOUT | - | - | - |\n")

if __name__ == "__main__":
    run()
