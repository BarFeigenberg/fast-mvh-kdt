import os
import sys
import csv
import time
import subprocess
from datetime import datetime

RESULTS_CSV = "benchmarks/runs/pure_apex_campaign.csv"

# Instances 5 through 12, strictly sorted by FAST2 runtime ascending
CASES_TO_RUN = [
    {
        "domain": "GRID_APEX",
        "instance": "grid_n8_m6_rho-0.2",
        "map_dir": "scratchpad/grids/grid_n8_m6_rho-0.2",
        "start": 1, "goal": 64, "M": 6, "rho": -0.2, "eps": 0.05,
        "mvh_path": "scratchpad/grids/grid_n8_m6_rho-0.2/apex_0.05.mvh",
        "fast2_time_s": 8.2459, "fast_time_s": 11.4187,
        "sols": 104480, "exp": 293896,
        "fast_cmpfull": 1190728176, "fast2_cmpfull": 277487335,
        "fast2_cmpchk": 54224813, "fast2_cmpupd": 12588234
    },
    {
        "domain": "ROAD_BAY",
        "instance": "bay_800_6d",
        "map_dir": "scratchpad/maps/bay_8d_800",
        "start": 800, "goal": 1, "M": 6, "rho": "none", "eps": 0.1,
        "mvh_path": "scratchpad/maps/bay_8d_800/apex_6d_0.1.mvh",
        "fast2_time_s": 8.3893, "fast_time_s": 8.2753,
        "sols": 94886, "exp": 352012,
        "fast_cmpfull": 40662904, "fast2_cmpfull": 2092282,
        "fast2_cmpchk": 90200822, "fast2_cmpupd": 14053302
    },
    {
        "domain": "GRID_APEX",
        "instance": "grid_n8_m7_rho-0.2",
        "map_dir": "scratchpad/grids/grid_n8_m7_rho-0.2",
        "start": 1, "goal": 64, "M": 7, "rho": -0.2, "eps": 0.05,
        "mvh_path": "scratchpad/grids/grid_n8_m7_rho-0.2/apex_0.05.mvh",
        "fast2_time_s": 13.2402, "fast_time_s": 17.081,
        "sols": 124943, "exp": 373533,
        "fast_cmpfull": 1344621310, "fast2_cmpfull": 264202027,
        "fast2_cmpchk": 91811394, "fast2_cmpupd": 21104988
    },
    {
        "domain": "GRID_APEX",
        "instance": "grid_n7_m8_rho-0.2",
        "map_dir": "scratchpad/grids/grid_n7_m8_rho-0.2",
        "start": 1, "goal": 49, "M": 8, "rho": -0.2, "eps": 0.05,
        "mvh_path": "scratchpad/grids/grid_n7_m8_rho-0.2/apex_0.05.mvh",
        "fast2_time_s": 20.7305, "fast_time_s": 35.068,
        "sols": 175239, "exp": 445827,
        "fast_cmpfull": 5164189040, "fast2_cmpfull": 1356354060,
        "fast2_cmpchk": 122994951, "fast2_cmpupd": 35939536
    },
    {
        "domain": "GRID_APEX",
        "instance": "grid_9x9_m7_rho-0.4",
        "map_dir": "scratchpad/grids/grid_9x9_m7_rho-0.4",
        "start": 1, "goal": 81, "M": 7, "rho": -0.4, "eps": 0.05,
        "mvh_path": "scratchpad/grids/grid_9x9_m7_rho-0.4/apex_0.05.mvh",
        "fast2_time_s": 145.1816, "fast_time_s": 348.4235,
        "sols": 266224, "exp": 815838,
        "fast_cmpfull": 9712144953, "fast2_cmpfull": 1956531752,
        "fast2_cmpchk": 197781991, "fast2_cmpupd": 51476823
    },
    {
        "domain": "GRID_APEX",
        "instance": "grid_8x8_m8_rho-0.4",
        "map_dir": "scratchpad/grids/grid_8x8_m8_rho-0.4",
        "start": 1, "goal": 64, "M": 8, "rho": -0.4, "eps": 0.05,
        "mvh_path": "scratchpad/grids/grid_8x8_m8_rho-0.4/apex_0.05.mvh",
        "fast2_time_s": 217.9154, "fast_time_s": 416.6089,
        "sols": 366205, "exp": 1033695,
        "fast_cmpfull": 6481334718, "fast2_cmpfull": 1286577683,
        "fast2_cmpchk": 360708021, "fast2_cmpupd": 111588809
    },
    {
        "domain": "GRID_APEX",
        "instance": "grid_9x9_m7_rho0.0",
        "map_dir": "scratchpad/grids/grid_9x9_m7_rho0.0",
        "start": 1, "goal": 81, "M": 7, "rho": 0.0, "eps": 0.05,
        "mvh_path": "scratchpad/grids/grid_9x9_m7_rho0.0/apex_0.05.mvh",
        "fast2_time_s": 359.0082, "fast_time_s": 663.9339,
        "sols": 416597, "exp": 1305173,
        "fast_cmpfull": 19397048677, "fast2_cmpfull": 4987844231,
        "fast2_cmpchk": 375419914, "fast2_cmpupd": 95199480
    },
    {
        "domain": "GRID_APEX",
        "instance": "grid_8x8_m8_rho-0.2",
        "map_dir": "scratchpad/grids/grid_8x8_m8_rho-0.2",
        "start": 1, "goal": 64, "M": 8, "rho": -0.2, "eps": 0.05,
        "mvh_path": "scratchpad/grids/grid_8x8_m8_rho-0.2/apex_0.05.mvh",
        "fast2_time_s": 1197.7618, "fast_time_s": 9638.8392,
        "sols": 1082326, "exp": 3092096,
        "fast_cmpfull": 311457391449, "fast2_cmpfull": 59317871622,
        "fast2_cmpchk": 1412632831, "fast2_cmpupd": 515146822
    }
]

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

def update_csv_row(row_dict):
    rows = []
    fieldnames = [
        "timestamp", "domain", "instance", "M", "rho", "eps", "sols", "exp",
        "maya_time_s", "fast_time_s", "fast2_time_s",
        "speedup_fast2_vs_maya", "speedup_fast2_vs_fast",
        "maya_cmp_ch", "fast_cmpfull", "fast2_cmpfull", "cmpfull_reduction_pct",
        "fast2_cmpchk", "fast2_cmpupd", "bit_identical"
    ]
    if os.path.exists(RESULTS_CSV):
        with open(RESULTS_CSV, "r") as f:
            reader = csv.DictReader(f)
            for r in reader:
                if r["instance"] == row_dict["instance"]:
                    rows.append(row_dict)
                else:
                    rows.append(r)
    else:
        rows.append(row_dict)

    with open(RESULTS_CSV, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)

print("="*65)
print("STARTING UNBOUNDED MAYA BASELINE (instances 5-12, -t 0, NO LIMIT)")
print("="*65, flush=True)

for idx, item in enumerate(CASES_TO_RUN):
    inst = item["instance"]
    f2_t = item["fast2_time_s"]
    f_t = item["fast_time_s"]
    print(f"\n[{idx+1}/{len(CASES_TO_RUN)}] RUNNING MAYA ON: {inst} (FAST2: {f2_t:.2f}s, M={item['M']})", flush=True)

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
        "--sol-out", str(sol_maya),
        "-t", "0"  # STRICTLY UNBOUNDED / NO LIMIT!
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

    print(f"   MAYA FINISHED: {t_maya:.2f}s | SPEEDUP: {sp_maya:.2f}x | Bit-Identical: {bit_id}", flush=True)

    row = {
        "timestamp": datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
        "domain": item["domain"], "instance": inst, "M": item["M"],
        "rho": item["rho"], "eps": item["eps"],
        "sols": item["sols"], "exp": item["exp"],
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
    update_csv_row(row)

print("\n" + "="*65)
print("ALL UNBOUNDED MAYA BASELINE RUNS FINISHED SUCCESSFULLY!")
print("="*65, flush=True)
