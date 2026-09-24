import os
import sys
import subprocess
import time
import pandas as pd

FAST_MVH_BIN = os.path.abspath("build/Release/fast_mvh.exe")
CSV_PATH = "benchmarks/runs/adaptive_explorer/v5_breakthrough_scaling.csv"
LOG_PATH = "benchmarks/runs/adaptive_explorer/step1_unbounded_5d.log"

def log(msg):
    print(msg, flush=True)
    with open(LOG_PATH, "a", encoding="utf-8") as f:
        f.write(msg + "\n")

def parse_output(stdout):
    met = {}
    for line in stdout.splitlines():
        if "=" in line:
            for tok in line.split("\t"):
                if "=" in tok:
                    k, v = tok.split("=", 1)
                    try:
                        met[k.strip()] = float(v.strip()) if "." in v else int(v.strip())
                    except:
                        pass
    return met

def run_one(N, M, rho, K, algo, timeout=7200):
    inst_dir = os.path.abspath(f"scratchpad/deep_scaling/grid_{N}x{N}_M{M}_rho{rho}")
    mvh_path = os.path.join(inst_dir, f"target_{N*N}_K{K}.mvh")
    obj_args = [str(i) for i in range(M)]
    cmd = [
        FAST_MVH_BIN, "--map", inst_dir, "--start", "1", "--goal", str(N*N),
        "--objectives", *obj_args, "--algorithm", algo, "--mvh", mvh_path, "--cutoffTime", str(timeout)
    ]
    log(f"Starting {algo} on N={N}, M={M}, rho={rho}, K={K} (Timeout {timeout}s)...")
    t0 = time.perf_counter()
    proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    dt = time.perf_counter() - t0
    met = parse_output(proc.stdout)
    met["wall_clock"] = dt
    log(f"-> FINISHED in {met.get('runtime_s', dt):.2f}s (Cmp: {met.get('cmp_chooseh', 0):,}, Sols: {met.get('num_solutions')})")
    return met

def update_csv_cell(N, M, rho, K, algo, met):
    df = pd.read_csv(CSV_PATH)
    # Find row
    mask = (df["N"] == N) & (df["M"] == M) & (df["Rho"] == rho) & (df["K"] == K)
    if not mask.any():
        log(f"Warning: row N={N}, M={M}, rho={rho}, K={K} not found in {CSV_PATH}")
        return
        
    idx = df[mask].index[0]
    runtime = met.get("runtime_s", met["wall_clock"])
    cmp_cnt = met.get("cmp_chooseh", 0)
    sols = met.get("num_solutions")
    gen = met.get("num_generation")
    
    if sols is not None:
        df.at[idx, "Solutions"] = sols
    if gen is not None:
        df.at[idx, "Generations"] = gen
        
    if algo == "L_NAMOA_KDT_V3":
        df.at[idx, "V3_Time_s"] = round(runtime, 3)
        df.at[idx, "V3_Cmp"] = cmp_cnt
    elif algo == "L_NAMOA_DR_MVH_INSTRUMENTED":
        df.at[idx, "Maya_Time_s"] = round(runtime, 3)
        df.at[idx, "Maya_Cmp"] = cmp_cnt
        
    # Recompute speedups if both times are numeric
    try:
        m_time = float(df.at[idx, "Maya_Time_s"])
        v3_time = float(df.at[idx, "V3_Time_s"])
        df.at[idx, "V3_Speedup"] = round(m_time / v3_time, 2)
    except:
        pass
        
    try:
        m_time = float(df.at[idx, "Maya_Time_s"])
        v5_time = float(df.at[idx, "V5_Time_s"])
        df.at[idx, "V5_Speedup"] = round(m_time / v5_time, 2)
    except:
        pass
        
    df.to_csv(CSV_PATH, index=False)
    log(f"Updated CSV row {idx}: {df.loc[idx].to_dict()}")

def main():
    log("=============================================================")
    log("  STEP 1: UNBOUNDED RESOLUTION OF 5D TIMEOUT INSTANCES")
    log("=============================================================")
    
    # 1. Instance 10: N=10, M=5, rho=0.0, K=50 -> V3
    log("\n[1/4] Running V3 on Instance 10 (N=10, M=5, rho=0.0, K=50)...")
    met1 = run_one(10, 5, 0.0, 50, "L_NAMOA_KDT_V3")
    update_csv_cell(10, 5, 0.0, 50, "L_NAMOA_KDT_V3", met1)
    
    # 2. Instance 11: N=10, M=5, rho=0.0, K=100 -> Maya
    log("\n[2/4] Running Maya on Instance 11 (N=10, M=5, rho=0.0, K=100)...")
    met2 = run_one(10, 5, 0.0, 100, "L_NAMOA_DR_MVH_INSTRUMENTED")
    update_csv_cell(10, 5, 0.0, 100, "L_NAMOA_DR_MVH_INSTRUMENTED", met2)
    
    # 3. Instance 13: N=10, M=5, rho=-0.2, K=50 -> Maya
    log("\n[3/4] Running Maya on Instance 13 (N=10, M=5, rho=-0.2, K=50)...")
    met3 = run_one(10, 5, -0.2, 50, "L_NAMOA_DR_MVH_INSTRUMENTED")
    update_csv_cell(10, 5, -0.2, 50, "L_NAMOA_DR_MVH_INSTRUMENTED", met3)
    
    # 4. Instance 13: N=10, M=5, rho=-0.2, K=50 -> V3
    log("\n[4/4] Running V3 on Instance 13 (N=10, M=5, rho=-0.2, K=50)...")
    met4 = run_one(10, 5, -0.2, 50, "L_NAMOA_KDT_V3")
    update_csv_cell(10, 5, -0.2, 50, "L_NAMOA_KDT_V3", met4)
    
    log("\nSTEP 1 COMPLETE: All 5D instances resolved with exact unbounded values.")

if __name__ == "__main__":
    main()
