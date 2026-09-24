import os
import sys
import subprocess
import time
import csv

FAST_MVH_BIN = os.path.abspath("build/Release/fast_mvh.exe")
CSV_PATH = "benchmarks/runs/adaptive_explorer/v5_breakthrough_scaling.csv"
TASK1_LOG = "benchmarks/runs/adaptive_explorer/task1_maya_unbounded.log"

V5_RESULTS = {
    100: {"time": 1109.09, "cmp": 712355753, "gen": 1782858, "sols": 10580},
    200: {"time": 1130.22, "cmp": 712355753, "gen": 1782858, "sols": 10580},
    400: {"time": 1134.41, "cmp": 712355753, "gen": 1782858, "sols": 10580}
}

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

def log(msg):
    print(msg, flush=True)
    with open(TASK1_LOG, "a", encoding="utf-8") as f:
        f.write(msg + "\n")

def run_maya_unbounded(K, timeout=7200):
    N, M, rho = 20, 3, -0.3
    inst_dir = os.path.abspath(f"scratchpad/deep_scaling/grid_{N}x{N}_M{M}_rho{rho}")
    mvh_path = os.path.join(inst_dir, f"target_{N*N}_K{K}.mvh")
    
    obj_args = [str(i) for i in range(M)]
    cmd = [
        FAST_MVH_BIN, "--map", inst_dir, "--start", "1", "--goal", str(N*N),
        "--objectives", *obj_args, "--algorithm", "L_NAMOA_DR_MVH_INSTRUMENTED",
        "--mvh", mvh_path, "--cutoffTime", str(timeout)
    ]
    
    log(f"[{time.strftime('%Y-%m-%d %H:%M:%S')}] Starting Maya Unbounded on N=20, M=3, rho=-0.3, K={K} (Timeout {timeout}s)...")
    t0 = time.perf_counter()
    try:
        proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=timeout+60)
        dt = time.perf_counter() - t0
        met = parse_output(proc.stdout)
        met["wall_clock"] = dt
        
        reached_limit = met.get("time_limit_reached", 0) == 1 or met.get("runtime_s", 0) >= timeout
        if reached_limit:
            log(f"[{time.strftime('%Y-%m-%d %H:%M:%S')}] Maya TIMEOUT after {dt:.2f}s!")
            return False, met
        else:
            log(f"[{time.strftime('%Y-%m-%d %H:%M:%S')}] Maya SUCCESS in {met['runtime_s']:.2f}s (Wall-clock: {dt:.2f}s)!")
            log(f"  Solutions: {met.get('num_solutions')}, Generations: {met.get('num_generation')}")
            log(f"  Dominance Comparisons: {met.get('cmp_chooseh'):,}")
            
            v5_t = V5_RESULTS[K]["time"]
            speedup = met["runtime_s"] / v5_t
            log(f"  ==> EXACT V5 SPEEDUP: {speedup:.2f}x (Maya {met['runtime_s']:.2f}s vs V5 {v5_t:.2f}s)")
            return True, met
    except subprocess.TimeoutExpired:
        log(f"[{time.strftime('%Y-%m-%d %H:%M:%S')}] Maya Subprocess TimeoutExpired after {timeout}s!")
        return False, {}

def update_csv_and_md(K, maya_met):
    N, M, rho = 20, 3, -0.3
    v5_data = V5_RESULTS[K]
    maya_time = maya_met.get("runtime_s", "TIMEOUT")
    maya_cmp = maya_met.get("cmp_chooseh", "-")
    speedup = f"{maya_time / v5_data['time']:.2f}x" if isinstance(maya_time, (int, float)) else "-"
    
    # Read existing CSV
    rows = []
    updated = False
    with open(CSV_PATH, "r", newline="", encoding="utf-8") as f:
        reader = csv.reader(f)
        header = next(reader)
        rows.append(header)
        for r in reader:
            if len(r) >= 14 and r[0] == str(N) and r[1] == str(M) and r[2] == str(rho) and r[3] == str(K):
                # Update row
                r[8] = f"{maya_time:.3f}" if isinstance(maya_time, float) else str(maya_time)
                r[10] = str(speedup)
                r[13] = str(maya_cmp)
                updated = True
            rows.append(r)
            
    if updated:
        with open(CSV_PATH, "w", newline="", encoding="utf-8") as f:
            writer = csv.writer(f)
            writer.writerows(rows)
        log(f"Updated CSV entry for N={N}, M={M}, rho={rho}, K={K}")

def main():
    log("=============================================================")
    log("  TASK 1: UNBOUNDED MAYA HEAD-TO-HEAD BENCHMARK (N=20, M=3)")
    log("=============================================================")
    
    for K in [100, 200, 400]:
        ok, met = run_maya_unbounded(K, timeout=7200)
        if ok:
            update_csv_and_md(K, met)

if __name__ == "__main__":
    main()
