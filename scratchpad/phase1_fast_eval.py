import os
import csv
import sys
import time
import subprocess
from datetime import datetime

RESULTS_CSV = "scratchpad/results/phase1_fast_unbounded.csv"

def parse_proto_line(stdout: str) -> dict:
    d = {}
    for ln in stdout.splitlines():
        if "=" not in ln: continue
        for tok in ln.split("\t"):
            if "=" in tok:
                k, v = tok.split("=", 1)
                try:
                    d[k.strip()] = int(v.strip())
                except ValueError:
                    try:
                        d[k.strip()] = float(v.strip())
                    except ValueError:
                        d[k.strip()] = v.strip()
    return d

def run_case(domain, instance, map_dir, start, goal, M, rho, eps, mvh_path):
    print(f"\n{'='*55}\nRUNNING PHASE 1: {instance} (M={M}, rho={rho}, eps={eps})\n{'='*55}", flush=True)

    if not os.path.exists(mvh_path):
        print(f"ERROR: Heuristic missing {mvh_path}. Skipping.")
        return

    os.makedirs("scratchpad/sols", exist_ok=True)
    sol_fast2 = f"scratchpad/sols/{instance}_fast2.sol"
    sol_fast  = f"scratchpad/sols/{instance}_fast.sol"

    # --- FAST2 (witness=1) ---
    print("-> Running FAST2 (witness=1)...", flush=True)
    cmd_fast2 = [
        "build\\Release\\proto_w.exe",
        str(map_dir), str(start), str(goal), str(M), str(mvh_path), str(sol_fast2),
        "flatH=1", "local_first=1", "witness=1", "redund=1", 
        "localX=8", "targetX=8", "promoteC=64", "exact_max=1"
    ]
    
    t0 = time.time()
    # No timeout! shell=False
    r2 = subprocess.run(cmd_fast2, capture_output=True, text=True)
    t_fast2 = time.time() - t0
    d_fast2 = parse_proto_line(r2.stdout)
    
    print(f"   FAST2: {t_fast2:.3f}s | sols={d_fast2.get('sols')} exp={d_fast2.get('exp')} cmpfull={d_fast2.get('cmpfull')}", flush=True)

    # --- FAST (witness=0) ---
    print("-> Running FAST (witness=0)...", flush=True)
    cmd_fast = [
        "build\\Release\\proto_w.exe",
        str(map_dir), str(start), str(goal), str(M), str(mvh_path), str(sol_fast),
        "flatH=1", "local_first=1", "witness=0", "redund=1", 
        "localX=8", "targetX=8", "promoteC=64", "exact_max=1"
    ]
    
    t0 = time.time()
    r1 = subprocess.run(cmd_fast, capture_output=True, text=True)
    t_fast = time.time() - t0
    d_fast = parse_proto_line(r1.stdout)
    
    print(f"   FAST:  {t_fast:.3f}s | sols={d_fast.get('sols')} exp={d_fast.get('exp')} cmpfull={d_fast.get('cmpfull')}", flush=True)

    # Logging
    file_exists = os.path.exists(RESULTS_CSV)
    fieldnames = [
        "timestamp", "domain", "instance", "M", "rho", "eps", 
        "fast2_time_s", "fast_time_s", 
        "fast2_sols", "fast2_exp", "fast_exp",
        "fast2_cmpfull", "fast_cmpfull",
        "fast2_cmpchk", "fast2_cmpupd",
        "mvh_path"
    ]

    row = {
        "timestamp": datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
        "domain": domain, "instance": instance, "M": M, "rho": rho, "eps": eps,
        "fast2_time_s": round(t_fast2, 4),
        "fast_time_s": round(t_fast, 4),
        "fast2_sols": d_fast2.get('sols', ''),
        "fast2_exp": d_fast2.get('exp', ''),
        "fast_exp": d_fast.get('exp', ''),
        "fast2_cmpfull": d_fast2.get('cmpfull', ''),
        "fast_cmpfull": d_fast.get('cmpfull', ''),
        "fast2_cmpchk": d_fast2.get('cmpchk', ''),
        "fast2_cmpupd": d_fast2.get('cmpupd', ''),
        "mvh_path": mvh_path
    }

    with open(RESULTS_CSV, "a", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        if not file_exists:
            writer.writeheader()
        writer.writerow(row)

if __name__ == "__main__":
    cases = [
        ('GRID_APEX', 'grid_n7_m7_rho-0.4', 'scratchpad/grids/grid_n7_m7_rho-0.4', 1, 49, 7, -0.4, 0.05, 'scratchpad/grids/grid_n7_m7_rho-0.4/apex_0.05.mvh'),
        ('GRID_APEX', 'grid_n8_m8_rho0.0', 'scratchpad/grids/grid_n8_m8_rho0.0', 1, 64, 8, 0.0, 0.05, 'scratchpad/grids/grid_n8_m8_rho0.0/apex_0.05.mvh'),
        ('GRID_APEX', 'grid_8x8_m8_rho-0.2', 'scratchpad/grids/grid_8x8_m8_rho-0.2', 1, 64, 8, -0.2, 0.05, 'scratchpad/grids/grid_8x8_m8_rho-0.2/apex_0.05.mvh'),
        ('GRID_APEX', 'grid_8x8_m8_rho-0.4', 'scratchpad/grids/grid_8x8_m8_rho-0.4', 1, 64, 8, -0.4, 0.05, 'scratchpad/grids/grid_8x8_m8_rho-0.4/apex_0.05.mvh'),
        ('GRID_APEX', 'grid_9x9_m7_rho0.0', 'scratchpad/grids/grid_9x9_m7_rho0.0', 1, 81, 7, 0.0, 0.05, 'scratchpad/grids/grid_9x9_m7_rho0.0/apex_0.05.mvh'),
        ('GRID_APEX', 'grid_9x9_m7_rho-0.4', 'scratchpad/grids/grid_9x9_m7_rho-0.4', 1, 81, 7, -0.4, 0.05, 'scratchpad/grids/grid_9x9_m7_rho-0.4/apex_0.05.mvh'),
        ('ROAD_BAY', 'bay_800_7d', 'scratchpad/maps/bay_8d_800', 800, 1, 7, 'none', 0.1, 'scratchpad/maps/bay_8d_800/apex_7d_0.1.mvh'),
        ('ROAD_BAY', 'bay_800_8d', 'scratchpad/maps/bay_8d_800', 800, 1, 8, 'none', 0.1, 'scratchpad/maps/bay_8d_800/apex_8d_0.1.mvh'),
    ]

    for c in cases:
        run_case(*c)
