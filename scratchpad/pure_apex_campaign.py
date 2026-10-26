import os
import sys
import csv
import time
import subprocess
from datetime import datetime

RESULTS_CSV = "benchmarks/runs/pure_apex_campaign.csv"
PHASE1_CSV = "scratchpad/results/pure_apex_phase1.csv"

# 12 Genuine A*pex instances (NO LANDMARKS)
CASES = [
    # --- Real-World Road Networks (A*pex) ---
    ('ROAD_BAY', 'bay_800_5d', 'scratchpad/maps/bay_8d_800', 800, 1, 5, 'none', 0.1, 'scratchpad/maps/bay_8d_800/apex_5d_0.1.mvh'),
    ('ROAD_BAY', 'bay_800_6d', 'scratchpad/maps/bay_8d_800', 800, 1, 6, 'none', 0.1, 'scratchpad/maps/bay_8d_800/apex_6d_0.1.mvh'),

    # --- High-Dimensional Grids (A*pex, M=6, 7, 8) ---
    ('GRID_APEX', 'grid_n8_m6_rho-0.2', 'scratchpad/grids/grid_n8_m6_rho-0.2', 1, 64, 6, -0.2, 0.05, 'scratchpad/grids/grid_n8_m6_rho-0.2/apex_0.05.mvh'),
    ('GRID_APEX', 'grid_n7_m7_rho-0.2', 'scratchpad/grids/grid_n7_m7_rho-0.2', 1, 49, 7, -0.2, 0.05, 'scratchpad/grids/grid_n7_m7_rho-0.2/apex_0.05.mvh'),
    ('GRID_APEX', 'grid_n7_m7_rho-0.4', 'scratchpad/grids/grid_n7_m7_rho-0.4', 1, 49, 7, -0.4, 0.05, 'scratchpad/grids/grid_n7_m7_rho-0.4/apex_0.05.mvh'),
    ('GRID_APEX', 'grid_n8_m7_rho-0.2', 'scratchpad/grids/grid_n8_m7_rho-0.2', 1, 64, 7, -0.2, 0.05, 'scratchpad/grids/grid_n8_m7_rho-0.2/apex_0.05.mvh'),
    ('GRID_APEX', 'grid_n6_m8_rho-0.2', 'scratchpad/grids/grid_n6_m8_rho-0.2', 1, 36, 8, -0.2, 0.05, 'scratchpad/grids/grid_n6_m8_rho-0.2/apex_0.05.mvh'),
    ('GRID_APEX', 'grid_n7_m8_rho-0.2', 'scratchpad/grids/grid_n7_m8_rho-0.2', 1, 49, 8, -0.2, 0.05, 'scratchpad/grids/grid_n7_m8_rho-0.2/apex_0.05.mvh'),
    ('GRID_APEX', 'grid_8x8_m8_rho-0.2', 'scratchpad/grids/grid_8x8_m8_rho-0.2', 1, 64, 8, -0.2, 0.05, 'scratchpad/grids/grid_8x8_m8_rho-0.2/apex_0.05.mvh'),
    ('GRID_APEX', 'grid_8x8_m8_rho-0.4', 'scratchpad/grids/grid_8x8_m8_rho-0.4', 1, 64, 8, -0.4, 0.05, 'scratchpad/grids/grid_8x8_m8_rho-0.4/apex_0.05.mvh'),
    ('GRID_APEX', 'grid_9x9_m7_rho0.0', 'scratchpad/grids/grid_9x9_m7_rho0.0', 1, 81, 7, 0.0, 0.05, 'scratchpad/grids/grid_9x9_m7_rho0.0/apex_0.05.mvh'),
    ('GRID_APEX', 'grid_9x9_m7_rho-0.4', 'scratchpad/grids/grid_9x9_m7_rho-0.4', 1, 81, 7, -0.4, 0.05, 'scratchpad/grids/grid_9x9_m7_rho-0.4/apex_0.05.mvh'),
]

def parse_proto_line(stdout: str) -> dict:
    d = {}
    for ln in stdout.splitlines():
        if "=" not in ln: continue
        for tok in ln.split("\t"):
            if "=" in tok:
                k, v = tok.split("=", 1)
                try: d[k.strip()] = int(v.strip())
                except ValueError:
                    try: d[k.strip()] = float(v.strip())
                    except ValueError: d[k.strip()] = v.strip()
    return d

def parse_maya_line(stdout: str) -> dict:
    d = {}
    for ln in stdout.splitlines():
        if "=" not in ln: continue
        for tok in ln.split("\t"):
            if "=" in tok:
                k, v = tok.split("=", 1)
                try: d[k.strip()] = float(v.strip())
                except ValueError: d[k.strip()] = v.strip()
    return d

def compare_sols(fast_sol, maya_sol):
    if not os.path.exists(fast_sol) or not os.path.exists(maya_sol):
        return "MISSING"
    try:
        with open(fast_sol, "r") as f1, open(maya_sol, "r") as f2:
            return "TRUE" if f1.read().strip() == f2.read().strip() else "FALSE"
    except Exception:
        return "ERROR"

def run_phase1():
    print("="*60)
    print("STARTING PHASE 1: FAST2 & FAST ON PURE A*PEX INSTANCES")
    print("="*60, flush=True)

    os.makedirs("scratchpad/results", exist_ok=True)
    os.makedirs("scratchpad/sols", exist_ok=True)
    
    p1_results = []
    fieldnames = [
        "domain", "instance", "map_dir", "start", "goal", "M", "rho", "eps", "mvh_path",
        "fast2_time_s", "fast_time_s", "fast2_sols", "fast2_exp", "fast_exp",
        "fast2_cmpfull", "fast_cmpfull", "fast2_cmpchk", "fast2_cmpupd"
    ]

    for domain, instance, map_dir, start, goal, M, rho, eps, mvh_path in CASES:
        print(f"\n--- [PHASE 1] {instance} (M={M}, rho={rho}, eps={eps}) ---", flush=True)
        if not os.path.exists(mvh_path):
            print(f"Skipping missing: {mvh_path}")
            continue

        sol_fast2 = f"scratchpad/sols/{instance}_fast2.sol"
        sol_fast  = f"scratchpad/sols/{instance}_fast.sol"

        # 1. FAST2 (witness=1)
        cmd_f2 = [
            "build\\Release\\proto_w.exe", map_dir, str(start), str(goal), str(M),
            mvh_path, sol_fast2, "flatH=1", "local_first=1", "witness=1", "redund=1",
            "localX=8", "targetX=8", "promoteC=64", "exact_max=1"
        ]
        t0 = time.time()
        r2 = subprocess.run(cmd_f2, capture_output=True, text=True)
        t_fast2 = time.time() - t0
        d_f2 = parse_proto_line(r2.stdout)
        print(f"   FAST2: {t_fast2:.3f}s | sols={d_f2.get('sols')} exp={d_f2.get('exp')}", flush=True)

        # 2. FAST (witness=0)
        cmd_f = [
            "build\\Release\\proto_w.exe", map_dir, str(start), str(goal), str(M),
            mvh_path, sol_fast, "flatH=1", "local_first=1", "witness=0", "redund=1",
            "localX=8", "targetX=8", "promoteC=64", "exact_max=1"
        ]
        t0 = time.time()
        r1 = subprocess.run(cmd_f, capture_output=True, text=True)
        t_fast = time.time() - t0
        d_f = parse_proto_line(r1.stdout)
        print(f"   FAST:  {t_fast:.3f}s | sols={d_f.get('sols')} exp={d_f.get('exp')}", flush=True)

        res = {
            "domain": domain, "instance": instance, "map_dir": map_dir,
            "start": start, "goal": goal, "M": M, "rho": rho, "eps": eps, "mvh_path": mvh_path,
            "fast2_time_s": round(t_fast2, 4),
            "fast_time_s": round(t_fast, 4),
            "fast2_sols": d_f2.get('sols', ''),
            "fast2_exp": d_f2.get('exp', ''),
            "fast_exp": d_f.get('exp', ''),
            "fast2_cmpfull": d_f2.get('cmpfull', ''),
            "fast_cmpfull": d_f.get('cmpfull', ''),
            "fast2_cmpchk": d_f2.get('cmpchk', ''),
            "fast2_cmpupd": d_f2.get('cmpupd', '')
        }
        p1_results.append(res)

        # Append to Phase 1 CSV
        exists = os.path.exists(PHASE1_CSV)
        with open(PHASE1_CSV, "a", newline="") as f:
            w = csv.DictWriter(f, fieldnames=fieldnames)
            if not exists: w.writeheader()
            w.writerow(res)

    return p1_results

def run_phase2(p1_results):
    print("\n" + "="*60)
    print("STARTING PHASE 2: MAYA BASELINE (SORTED SHORTEST TO LONGEST)")
    print("="*60, flush=True)

    # Sort instances by FAST2 runtime ascending!
    sorted_cases = sorted(p1_results, key=lambda x: float(x["fast2_time_s"]))

    os.makedirs("benchmarks/runs", exist_ok=True)
    fieldnames = [
        "timestamp", "domain", "instance", "M", "rho", "eps", "sols", "exp",
        "maya_time_s", "fast_time_s", "fast2_time_s",
        "speedup_fast2_vs_maya", "speedup_fast2_vs_fast",
        "maya_cmp_ch", "fast_cmpfull", "fast2_cmpfull", "cmpfull_reduction_pct",
        "fast2_cmpchk", "fast2_cmpupd", "bit_identical"
    ]

    for i, item in enumerate(sorted_cases):
        inst = item["instance"]
        f2_t = float(item["fast2_time_s"])
        f_t = float(item["fast_time_s"])
        print(f"\n--- [PHASE 2: {i+1}/{len(sorted_cases)}] {inst} (FAST2 time: {f2_t:.3f}s) ---", flush=True)

        sol_f2 = f"scratchpad/sols/{inst}_fast2.sol"
        sol_maya = f"scratchpad/sols/{inst}_maya.sol"

        objs = [str(k) for k in range(int(item["M"]))]
        cmd_maya = [
            "build\\Release\\fast_mvh.exe",
            "-a", "L_NAMOA_DR_MVH_INSTRUMENTED",
            "-m", str(item["map_dir"]),
            "-s", str(item["start"]),
            "-g", str(item["goal"]),
            "--objectives"
        ] + objs + [
            "--mvh", str(item["mvh_path"]),
            "--sol-out", str(sol_maya)
        ]

        t0 = time.time()
        rm = subprocess.run(cmd_maya, capture_output=True, text=True)
        t_maya = time.time() - t0
        d_m = parse_maya_line(rm.stdout)

        bit_id = compare_sols(sol_f2, sol_maya)
        sp_maya = (t_maya / f2_t) if f2_t > 0 else 0
        sp_fast = (f_t / f2_t) if f2_t > 0 else 0

        f_cmp = float(item["fast_cmpfull"]) if item["fast_cmpfull"] else 0
        f2_cmp = float(item["fast2_cmpfull"]) if item["fast2_cmpfull"] else 0
        red_pct = ((f_cmp - f2_cmp) / f_cmp * 100) if f_cmp > 0 else 0

        print(f"   Maya: {t_maya:.3f}s | SPEEDUP: {sp_maya:.2f}x | Bit-Identical: {bit_id}", flush=True)

        row = {
            "timestamp": datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
            "domain": item["domain"], "instance": inst, "M": item["M"],
            "rho": item["rho"], "eps": item["eps"],
            "sols": item["fast2_sols"], "exp": item["fast2_exp"],
            "maya_time_s": round(t_maya, 4),
            "fast_time_s": round(f_t, 4),
            "fast2_time_s": round(f2_t, 4),
            "speedup_fast2_vs_maya": f"{sp_maya:.2f}x",
            "speedup_fast2_vs_fast": f"{sp_fast:.2f}x",
            "maya_cmp_ch": d_m.get("cmp_chooseh", ""),
            "fast_cmpfull": item["fast_cmpfull"],
            "fast2_cmpfull": item["fast2_cmpfull"],
            "cmpfull_reduction_pct": f"{red_pct:.1f}%",
            "fast2_cmpchk": item["fast2_cmpchk"],
            "fast2_cmpupd": item["fast2_cmpupd"],
            "bit_identical": bit_id
        }

        exists = os.path.exists(RESULTS_CSV)
        with open(RESULTS_CSV, "a", newline="") as f:
            w = csv.DictWriter(f, fieldnames=fieldnames)
            if not exists: w.writeheader()
            w.writerow(row)

if __name__ == "__main__":
    # Clean previous run file if restarting
    if os.path.exists(PHASE1_CSV):
        os.remove(PHASE1_CSV)
    results = run_phase1()
    run_phase2(results)
