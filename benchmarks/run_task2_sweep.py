import os
import sys
import subprocess
import time
import csv

FAST_MVH_BIN = os.path.abspath("build/Release/fast_mvh.exe")
CSV_PATH = "benchmarks/runs/adaptive_explorer/v5_breakthrough_scaling.csv"
MD_PATH = "benchmarks/runs/overnight_breakthrough_log.md"
TASK2_LOG = "benchmarks/runs/adaptive_explorer/task2_multidim_sweep.log"

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

def log(msg, end="\n"):
    print(msg, end=end, flush=True)
    with open(TASK2_LOG, "a", encoding="utf-8") as f:
        f.write(msg + end)

def run_solver(inst_dir, mvh_path, N, M, algo, timeout):
    obj_args = [str(i) for i in range(M)]
    cmd = [
        FAST_MVH_BIN, "--map", inst_dir, "--start", "1", "--goal", str(N*N),
        "--objectives", *obj_args, "--algorithm", algo, "--mvh", mvh_path, "--cutoffTime", str(timeout)
    ]
    t0 = time.perf_counter()
    try:
        proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=timeout+30)
        dt = time.perf_counter() - t0
        met = parse_output(proc.stdout)
        met["wall_clock"] = dt
        reached_limit = met.get("time_limit_reached", 0) == 1 or met.get("runtime_s", 0) >= timeout
        if reached_limit:
            return False, dt, met
        return True, dt, met
    except subprocess.TimeoutExpired:
        return False, timeout, {}

def append_csv(row):
    with open(CSV_PATH, "a", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(row)

def append_md(row):
    with open(MD_PATH, "a", encoding="utf-8") as f:
        v3t = f"{row['V3_Time_s']:.2f}" if isinstance(row['V3_Time_s'], float) else row['V3_Time_s']
        v5t = f"{row['V5_Time_s']:.2f}" if isinstance(row['V5_Time_s'], float) else row['V5_Time_s']
        mt = f"{row['Maya_Time_s']:.2f}" if isinstance(row['Maya_Time_s'], float) else row['Maya_Time_s']
        su3 = f"**{row['V3_Speedup']:.2f}x**" if isinstance(row['V3_Speedup'], float) else row['V3_Speedup']
        su5 = f"**{row['V5_Speedup']:.2f}x**" if isinstance(row['V5_Speedup'], float) else row['V5_Speedup']
        f.write(f"| {row['N']}x{row['N']} | {row['M']} | {row['Rho']} | {row['K']} | {row['Sols']} | {row['Gen']} | {v3t} | {v5t} | {mt} | {su3} | {su5} | {row['V3_Cmp']} | {row['V5_Cmp']} | {row['Maya_Cmp']} |\n")

def run_suite(include_v5=True, timeout=600):
    log("=============================================================")
    log("  TASK 2: MULTI-DIMENSIONAL STRESS TESTING (M=4, 5)")
    log("=============================================================")
    
    # Priority ordered instances: N=10 first (fast verification), then N=12, then N=15
    queue = [
        # N=10, M=4 (baseline sweep)
        (10, 4, 0.0, 50), (10, 4, 0.0, 100), (10, 4, 0.0, 200),
        (10, 4, -0.2, 50), (10, 4, -0.2, 100), (10, 4, -0.2, 200),
        (10, 4, -0.4, 50), (10, 4, -0.4, 100), (10, 4, -0.4, 200),
        
        # N=10, M=5 (high dimensional geometric index test)
        (10, 5, 0.0, 50), (10, 5, 0.0, 100), (10, 5, 0.0, 200),
        (10, 5, -0.2, 50), (10, 5, -0.2, 100), (10, 5, -0.2, 200),
        (10, 5, -0.4, 50), (10, 5, -0.4, 100), (10, 5, -0.4, 200),
        
        # N=12, M=4 (moderate scaling)
        (12, 4, 0.0, 50), (12, 4, 0.0, 100), (12, 4, 0.0, 200),
        (12, 4, -0.2, 50), (12, 4, -0.2, 100), (12, 4, -0.2, 200),
        (12, 4, -0.4, 50), (12, 4, -0.4, 100), (12, 4, -0.4, 200),
        
        # N=12, M=5 (5D scaling)
        (12, 5, 0.0, 50), (12, 5, 0.0, 100),
        (12, 5, -0.2, 50), (12, 5, -0.2, 100),
        
        # N=15, M=4 (heavy multi-objective frontier)
        (15, 4, 0.0, 50), (15, 4, 0.0, 100),
        (15, 4, -0.2, 50), (15, 4, -0.2, 100),
        
        # N=15, M=5
        (15, 5, 0.0, 50), (15, 5, 0.0, 100)
    ]
    
    total = len(queue)
    for idx, (N, M, rho, K) in enumerate(queue, 1):
        inst_dir = os.path.abspath(f"scratchpad/deep_scaling/grid_{N}x{N}_M{M}_rho{rho}")
        mvh_path = os.path.join(inst_dir, f"target_{N*N}_K{K}.mvh")
        
        if not os.path.exists(mvh_path):
            log(f"[{idx}/{total}] Skipping missing instance N={N}, M={M}, rho={rho}, K={K}")
            continue
            
        log(f"\n[{idx}/{total}] Evaluating N={N}, M={M}, rho={rho}, K={K} (Timeout {timeout}s)...")
        
        # 1. Run Maya Baseline
        log("  -> Maya_Linear...", end=" ")
        maya_ok, maya_dt, maya_met = run_solver(inst_dir, mvh_path, N, M, "L_NAMOA_DR_MVH_INSTRUMENTED", timeout)
        if maya_ok:
            log(f"SUCCESS in {maya_met['runtime_s']:.2f}s (Cmp: {maya_met.get('cmp_chooseh', 0):,}, Sols: {maya_met.get('num_solutions')})")
        else:
            log("TIMEOUT")
            
        # 2. Run V3_ExpandOnly
        log("  -> V3_ExpandOnly...", end=" ")
        v3_ok, v3_dt, v3_met = run_solver(inst_dir, mvh_path, N, M, "L_NAMOA_KDT_V3", timeout)
        if v3_ok:
            log(f"SUCCESS in {v3_met['runtime_s']:.2f}s (Cmp: {v3_met.get('cmp_chooseh', 0):,}, Sols: {v3_met.get('num_solutions')})")
        else:
            log("TIMEOUT")
            
        # 3. Run V5_DualTree (if enabled)
        if include_v5:
            log("  -> V5_DualTree...", end=" ")
            v5_ok, v5_dt, v5_met = run_solver(inst_dir, mvh_path, N, M, "L_NAMOA_KDT_V5", timeout)
            if v5_ok:
                log(f"SUCCESS in {v5_met['runtime_s']:.2f}s (Cmp: {v5_met.get('cmp_chooseh', 0):,}, Sols: {v5_met.get('num_solutions')})")
            else:
                log("TIMEOUT")
        else:
            v5_ok = False
            v5_met = {}
            
        # Log results
        v3_time = v3_met.get("runtime_s", "TIMEOUT") if v3_ok else "TIMEOUT"
        v5_time = v5_met.get("runtime_s", "TIMEOUT") if v5_ok else "TIMEOUT"
        maya_time = maya_met.get("runtime_s", "TIMEOUT") if maya_ok else "TIMEOUT"
        
        v3_speedup = "-"
        if v3_ok and maya_ok:
            v3_speedup = maya_met["runtime_s"] / v3_met["runtime_s"]
            
        v5_speedup = "-"
        if v5_ok and maya_ok:
            v5_speedup = maya_met["runtime_s"] / v5_met["runtime_s"]
            
        row = {
            "N": N, "M": M, "Rho": rho, "K": K,
            "Sols": maya_met.get("num_solutions", v3_met.get("num_solutions", "-")),
            "Gen": maya_met.get("num_generation", v3_met.get("num_generation", "-")),
            "V3_Time_s": v3_time,
            "V5_Time_s": v5_time,
            "Maya_Time_s": maya_time,
            "V3_Speedup": v3_speedup,
            "V5_Speedup": v5_speedup,
            "V3_Cmp": v3_met.get("cmp_chooseh", "-"),
            "V5_Cmp": v5_met.get("cmp_chooseh", "-"),
            "Maya_Cmp": maya_met.get("cmp_chooseh", "-")
        }
        
        append_csv([
            row["N"], row["M"], row["Rho"], row["K"],
            row["Sols"], row["Gen"],
            row["V3_Time_s"], row["V5_Time_s"], row["Maya_Time_s"],
            f"{row['V3_Speedup']:.2f}" if isinstance(row['V3_Speedup'], float) else row['V3_Speedup'],
            f"{row['V5_Speedup']:.2f}" if isinstance(row['V5_Speedup'], float) else row['V5_Speedup'],
            row["V3_Cmp"], row["V5_Cmp"], row["Maya_Cmp"]
        ])
        append_md(row)

if __name__ == "__main__":
    run_suite(include_v5=True, timeout=600)
