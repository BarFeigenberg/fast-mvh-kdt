import csv
import os
import time
import subprocess
from datetime import datetime

PHASE1_CSV = "scratchpad/results/phase1_fast_unbounded.csv"
FINAL_CSV = "benchmarks/runs/no_timeout_campaign.csv"

def parse_maya_line(stdout: str) -> dict:
    d = {}
    for ln in stdout.splitlines():
        if "=" not in ln: continue
        for tok in ln.split("\t"):
            if "=" in tok:
                k, v = tok.split("=", 1)
                try:
                    d[k.strip()] = float(v.strip())
                except ValueError:
                    d[k.strip()] = v.strip()
    return d

def compare_sols(fast_sol, maya_sol):
    if not os.path.exists(fast_sol) or not os.path.exists(maya_sol):
        return "MISSING_FILES"
    try:
        with open(fast_sol, "r") as f1, open(maya_sol, "r") as f2:
            return "TRUE" if f1.read() == f2.read() else "FALSE"
    except Exception:
        return "ERROR"

def run_maya_phase2():
    if not os.path.exists(PHASE1_CSV):
        print("ERROR: Phase 1 CSV not found.")
        return

    # Read and sort instances by FAST2 time (shortest to longest)
    instances = []
    with open(PHASE1_CSV, "r") as f:
        reader = csv.DictReader(f)
        for row in reader:
            instances.append(row)
    
    instances.sort(key=lambda x: float(x["fast2_time_s"]))

    print(f"Loaded {len(instances)} instances from Phase 1. Sorted by FAST2 time.")

    file_exists = os.path.exists(FINAL_CSV)
    fieldnames = [
        "timestamp", "domain", "instance", "M", "rho", "eps", 
        "sols", "exp", "maya_time_s", "fast_time_s", "fast2_time_s", 
        "speedup_fast2_vs_maya", "speedup_fast2_vs_fast", 
        "maya_cmp_ch", "fast_cmpfull", "fast2_cmpfull", "cmpfull_reduction_pct", 
        "fast2_cmpchk", "fast2_cmpupd", "bit_identical"
    ]

    for i, row in enumerate(instances):
        instance = row["instance"]
        domain = row["domain"]
        M = int(row["M"])
        mvh_path = row["mvh_path"]
        fast2_time = float(row["fast2_time_s"])
        fast_time = float(row["fast_time_s"])
        
        print(f"\n{'='*55}\nPHASE 2 [{i+1}/{len(instances)}]: {instance} (FAST2 time: {fast2_time:.3f}s)\n{'='*55}", flush=True)

        if domain == "ROAD_BAY":
            start, goal = 800, 1
            map_dir = "scratchpad/maps/bay_8d_800"
        else:
            start, goal = 1, int(row["sols"]) if "sols" in row and row["sols"] else 64 # Approximation, Maya gets -s 1 -g GOAL
            # Actually, grid apex generator usually uses goal = N*N or (N+1)*(N+1)
            # Let's read the grid size from instance name, or just hardcode for grids:
            if "n7_m7" in instance: goal = 49
            elif "n8_m8" in instance: goal = 64
            elif "8x8" in instance: goal = 64
            elif "9x9" in instance: goal = 81
            map_dir = mvh_path.rsplit('/', 1)[0] # The directory containing the mvh

        sol_fast2 = f"scratchpad/sols/{instance}_fast2.sol"
        sol_maya = f"scratchpad/sols/{instance}_maya.sol"

        print(f"-> Running Maya Baseline on {instance}...", flush=True)
        objs = " ".join(str(o) for o in range(M))
        
        cmd_maya = [
            "build\\Release\\fast_mvh.exe",
            "-a", "L_NAMOA_DR_MVH_INSTRUMENTED",
            "-m", str(map_dir),
            "-s", str(start),
            "-g", str(goal),
            "--objectives"
        ] + objs.split() + [
            "--mvh", str(mvh_path),
            "--sol-out", str(sol_maya)
        ]

        t0 = time.time()
        # No timeout! shell=False
        rm = subprocess.run(cmd_maya, capture_output=True, text=True)
        t_maya = time.time() - t0
        d_maya = parse_maya_line(rm.stdout)

        bit_ident = compare_sols(sol_fast2, sol_maya)
        
        maya_time = t_maya
        speedup = maya_time / fast2_time if fast2_time > 0 else 0
        speedup_fast = fast_time / fast2_time if fast2_time > 0 else 0
        
        f_cmpfull = float(row["fast_cmpfull"]) if row["fast_cmpfull"] else 0
        f2_cmpfull = float(row["fast2_cmpfull"]) if row["fast2_cmpfull"] else 0
        red_pct = ((f_cmpfull - f2_cmpfull) / f_cmpfull * 100) if f_cmpfull > 0 else 0

        print(f"   Maya: {maya_time:.3f}s | Speedup: {speedup:.2f}x | Identical: {bit_ident}", flush=True)

        final_row = {
            "timestamp": datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
            "domain": domain, "instance": instance, "M": M, "rho": row["rho"], "eps": row["eps"],
            "sols": row["fast2_sols"], "exp": row["fast2_exp"],
            "maya_time_s": round(maya_time, 4),
            "fast_time_s": round(fast_time, 4),
            "fast2_time_s": round(fast2_time, 4),
            "speedup_fast2_vs_maya": f"{speedup:.2f}x",
            "speedup_fast2_vs_fast": f"{speedup_fast:.2f}x",
            "maya_cmp_ch": d_maya.get("cmp_chooseh", ""),
            "fast_cmpfull": row["fast_cmpfull"],
            "fast2_cmpfull": row["fast2_cmpfull"],
            "cmpfull_reduction_pct": f"{red_pct:.1f}%",
            "fast2_cmpchk": row["fast2_cmpchk"],
            "fast2_cmpupd": row["fast2_cmpupd"],
            "bit_identical": bit_ident
        }

        with open(FINAL_CSV, "a", newline="") as f:
            writer = csv.DictWriter(f, fieldnames=fieldnames)
            if not file_exists:
                writer.writeheader()
                file_exists = True
            writer.writerow(final_row)

if __name__ == "__main__":
    run_maya_phase2()
