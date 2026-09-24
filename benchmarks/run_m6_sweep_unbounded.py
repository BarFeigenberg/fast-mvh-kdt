import os
import sys
import subprocess
import time
import pandas as pd
import csv

FAST_MVH_BIN = os.path.abspath("build/Release/fast_mvh.exe")
CSV_PATH = "benchmarks/runs/adaptive_explorer/v5_breakthrough_scaling.csv"
MD_PATH = "benchmarks/runs/overnight_breakthrough_log.md"
LOG_PATH = "benchmarks/runs/adaptive_explorer/m6_unbounded_sweep.log"

def log(msg, end="\n"):
    print(msg, end=end, flush=True)
    with open(LOG_PATH, "a", encoding="utf-8") as f:
        f.write(msg + end)

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

def run_solver(inst_dir, mvh_path, N, M, algo, timeout):
    obj_args = [str(i) for i in range(M)]
    cmd = [
        FAST_MVH_BIN, "--map", inst_dir, "--start", "1", "--goal", str(N*N),
        "--objectives", *obj_args, "--algorithm", algo, "--mvh", mvh_path, "--cutoffTime", str(timeout)
    ]
    t0 = time.perf_counter()
    try:
        proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=timeout+60)
        dt = time.perf_counter() - t0
        met = parse_output(proc.stdout)
        met["wall_clock"] = dt
        reached_limit = met.get("time_limit_reached", 0) == 1 or met.get("runtime_s", 0) >= timeout
        if reached_limit:
            return False, dt, met
        return True, dt, met
    except subprocess.TimeoutExpired:
        return False, timeout, {}

def update_row_in_csv(N, M, rho, K, maya_met, v3_met, v5_met):
    df = pd.read_csv(CSV_PATH)
    mask = (df["N"] == N) & (df["M"] == M) & (df["Rho"] == rho) & (df["K"] == K)
    
    m_time = maya_met.get("runtime_s", "TIMEOUT") if maya_met else "TIMEOUT"
    v3_time = v3_met.get("runtime_s", "TIMEOUT") if v3_met else "TIMEOUT"
    v5_time = v5_met.get("runtime_s", "TIMEOUT") if v5_met else "TIMEOUT"
    
    m_cmp = maya_met.get("cmp_chooseh", "-") if maya_met else "-"
    v3_cmp = v3_met.get("cmp_chooseh", "-") if v3_met else "-"
    v5_cmp = v5_met.get("cmp_chooseh", "-") if v5_met else "-"
    
    sols = maya_met.get("num_solutions") or v3_met.get("num_solutions") or v5_met.get("num_solutions") or "-"
    gen = maya_met.get("num_generation") or v3_met.get("num_generation") or v5_met.get("num_generation") or "-"
    
    v3_sp = round(float(m_time) / float(v3_time), 2) if isinstance(m_time, (int, float)) and isinstance(v3_time, (int, float)) else "-"
    v5_sp = round(float(m_time) / float(v5_time), 2) if isinstance(m_time, (int, float)) and isinstance(v5_time, (int, float)) else "-"
    
    if mask.any():
        idx = df[mask].index[0]
        df.at[idx, "Solutions"] = sols
        df.at[idx, "Generations"] = gen
        df.at[idx, "V3_Time_s"] = v3_time
        df.at[idx, "V5_Time_s"] = v5_time
        df.at[idx, "Maya_Time_s"] = m_time
        df.at[idx, "V3_Speedup"] = v3_sp
        df.at[idx, "V5_Speedup"] = v5_sp
        df.at[idx, "V3_Cmp"] = v3_cmp
        df.at[idx, "V5_Cmp"] = v5_cmp
        df.at[idx, "Maya_Cmp"] = m_cmp
    else:
        new_row = {
            "N": N, "M": M, "Rho": rho, "K": K,
            "Solutions": sols, "Generations": gen,
            "V3_Time_s": v3_time, "V5_Time_s": v5_time, "Maya_Time_s": m_time,
            "V3_Speedup": v3_sp, "V5_Speedup": v5_sp,
            "V3_Cmp": v3_cmp, "V5_Cmp": v5_cmp, "Maya_Cmp": m_cmp
        }
        df = pd.concat([df, pd.DataFrame([new_row])], ignore_index=True)
        
    df.to_csv(CSV_PATH, index=False)
    log(f"Updated CSV for N={N}, M={M}, rho={rho}, K={K}: Maya={m_time}s, V3={v3_time}s, V5={v5_time}s, V5_Speedup={v5_sp}x")

def regenerate_plots():
    try:
        subprocess.run([sys.executable, "benchmarks/plot_all_dimensions.py"], check=True, stdout=subprocess.DEVNULL)
        log("Regenerated speedup plots successfully.")
    except Exception as e:
        log(f"Plot regeneration error: {e}")

def main():
    log("=============================================================")
    log("  TASK 3 (UNBOUNDED): HIGH-DIMENSIONAL SCALING (M=6)")
    log("=============================================================")
    
    # Part 1: Complete Maya & V3 for Instance 1 (N=8, M=6, rho=0.0, K=50)
    inst_dir_1 = os.path.abspath("scratchpad/deep_scaling/grid_8x8_M6_rho0.0")
    mvh_path_1 = os.path.join(inst_dir_1, "target_64_K50.mvh")
    v5_met_1 = {"runtime_s": 382.14, "cmp_chooseh": 78925265, "num_solutions": 71191, "num_generation": 179319}
    
    log("\n[1/12 Unbounded] Completing Instance 1 (N=8, M=6, rho=0.0, K=50)...")
    log("  -> Maya_Linear (unbounded up to 3600s)...", end=" ")
    m_ok_1, _, m_met_1 = run_solver(inst_dir_1, mvh_path_1, 8, 6, "L_NAMOA_DR_MVH_INSTRUMENTED", 3600)
    if m_ok_1:
        log(f"SUCCESS in {m_met_1['runtime_s']:.2f}s (Cmp: {m_met_1.get('cmp_chooseh', 0):,}, Sols: {m_met_1.get('num_solutions')})")
    else:
        log("TIMEOUT")
        
    log("  -> V3_ExpandOnly (unbounded up to 3600s)...", end=" ")
    v3_ok_1, _, v3_met_1 = run_solver(inst_dir_1, mvh_path_1, 8, 6, "L_NAMOA_KDT_V3", 3600)
    if v3_ok_1:
        log(f"SUCCESS in {v3_met_1['runtime_s']:.2f}s (Cmp: {v3_met_1.get('cmp_chooseh', 0):,}, Sols: {v3_met_1.get('num_solutions')})")
    else:
        log("TIMEOUT")
        
    update_row_in_csv(8, 6, 0.0, 50, m_met_1 if m_ok_1 else {}, v3_met_1 if v3_ok_1 else {}, v5_met_1)
    regenerate_plots()
    
    # Part 2: Remaining M=6 instances
    remaining_queue = [
        (8, 6, 0.0, 100),
        (8, 6, -0.2, 50), (8, 6, -0.2, 100),
        (10, 6, 0.0, 50), (10, 6, 0.0, 100),
        (10, 6, -0.2, 50), (10, 6, -0.2, 100),
        (12, 6, 0.0, 50), (12, 6, 0.0, 100),
        (12, 6, -0.2, 50), (12, 6, -0.2, 100)
    ]
    
    for idx, (N, M, rho, K) in enumerate(remaining_queue, 2):
        inst_dir = os.path.abspath(f"scratchpad/deep_scaling/grid_{N}x{N}_M{M}_rho{rho}")
        mvh_path = os.path.join(inst_dir, f"target_{N*N}_K{K}.mvh")
        
        if not os.path.exists(mvh_path):
            log(f"[{idx}/12] Missing instance: N={N}, M={M}, rho={rho}, K={K}")
            continue
            
        log(f"\n[{idx}/12] Evaluating N={N}, M={M}, rho={rho}, K={K}...")
        
        # 1. Run V5 first (timeout 1800s)
        log("  -> V5_DualTree (timeout 1800s)...", end=" ")
        v5_ok, _, v5_met = run_solver(inst_dir, mvh_path, N, M, "L_NAMOA_KDT_V5", 1800)
        if v5_ok:
            log(f"SUCCESS in {v5_met['runtime_s']:.2f}s (Cmp: {v5_met.get('cmp_chooseh', 0):,}, Sols: {v5_met.get('num_solutions')})")
        else:
            log("TIMEOUT")
            
        # 2. If V5 succeeds, grant Maya and V3 unbounded window up to 3600s
        maya_timeout = 3600 if v5_ok else 600
        v3_timeout = 3600 if v5_ok else 600
        
        log(f"  -> Maya_Linear (timeout {maya_timeout}s)...", end=" ")
        m_ok, _, m_met = run_solver(inst_dir, mvh_path, N, M, "L_NAMOA_DR_MVH_INSTRUMENTED", maya_timeout)
        if m_ok:
            log(f"SUCCESS in {m_met['runtime_s']:.2f}s (Cmp: {m_met.get('cmp_chooseh', 0):,}, Sols: {m_met.get('num_solutions')})")
        else:
            log("TIMEOUT")
            
        log(f"  -> V3_ExpandOnly (timeout {v3_timeout}s)...", end=" ")
        v3_ok, _, v3_met = run_solver(inst_dir, mvh_path, N, M, "L_NAMOA_KDT_V3", v3_timeout)
        if v3_ok:
            log(f"SUCCESS in {v3_met['runtime_s']:.2f}s (Cmp: {v3_met.get('cmp_chooseh', 0):,}, Sols: {v3_met.get('num_solutions')})")
        else:
            log("TIMEOUT")
            
        update_row_in_csv(N, M, rho, K, m_met if m_ok else {}, v3_met if v3_ok else {}, v5_met if v5_ok else {})
        regenerate_plots()
        
    log("\nM=6 SUITE FULLY COMPLETED.")

if __name__ == "__main__":
    main()
