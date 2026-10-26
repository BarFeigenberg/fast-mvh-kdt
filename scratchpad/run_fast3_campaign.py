import os
import sys
import time
import subprocess
import csv

CASES = [
    # 1. bay_800_5d
    ('ROAD_BAY', 'bay_800_5d', 'scratchpad/maps/bay_8d_800', 800, 1, 5, 'Road Network', 0.1, 'scratchpad/maps/bay_8d_800/apex_5d_0.1.mvh'),
    # 2. grid_n7_m7_rho-0.2
    ('GRID_APEX', 'grid_n7_m7_rho-0.2', 'scratchpad/grids/grid_n7_m7_rho-0.2', 1, 49, 7, -0.2, 0.05, 'scratchpad/grids/grid_n7_m7_rho-0.2/apex_0.05.mvh'),
    # 3. grid_n6_m8_rho-0.2
    ('GRID_APEX', 'grid_n6_m8_rho-0.2', 'scratchpad/grids/grid_n6_m8_rho-0.2', 1, 36, 8, -0.2, 0.05, 'scratchpad/grids/grid_n6_m8_rho-0.2/apex_0.05.mvh'),
    # 4. grid_n7_m7_rho-0.4
    ('GRID_APEX', 'grid_n7_m7_rho-0.4', 'scratchpad/grids/grid_n7_m7_rho-0.4', 1, 49, 7, -0.4, 0.05, 'scratchpad/grids/grid_n7_m7_rho-0.4/apex_0.05.mvh'),
    # 5. grid_n8_m6_rho-0.2
    ('GRID_APEX', 'grid_n8_m6_rho-0.2', 'scratchpad/grids/grid_n8_m6_rho-0.2', 1, 64, 6, -0.2, 0.05, 'scratchpad/grids/grid_n8_m6_rho-0.2/apex_0.05.mvh'),
    # 6. bay_800_6d
    ('ROAD_BAY', 'bay_800_6d', 'scratchpad/maps/bay_8d_800', 800, 1, 6, 'Road Network', 0.1, 'scratchpad/maps/bay_8d_800/apex_6d_0.1.mvh'),
    # 7. grid_n8_m7_rho-0.2
    ('GRID_APEX', 'grid_n8_m7_rho-0.2', 'scratchpad/grids/grid_n8_m7_rho-0.2', 1, 64, 7, -0.2, 0.05, 'scratchpad/grids/grid_n8_m7_rho-0.2/apex_0.05.mvh'),
    # 8. grid_n7_m8_rho-0.2
    ('GRID_APEX', 'grid_n7_m8_rho-0.2', 'scratchpad/grids/grid_n7_m8_rho-0.2', 1, 49, 8, -0.2, 0.05, 'scratchpad/grids/grid_n7_m8_rho-0.2/apex_0.05.mvh'),
    # 9. grid_9x9_m7_rho-0.4
    ('GRID_APEX', 'grid_9x9_m7_rho-0.4', 'scratchpad/grids/grid_9x9_m7_rho-0.4', 1, 81, 7, -0.4, 0.05, 'scratchpad/grids/grid_9x9_m7_rho-0.4/apex_0.05.mvh'),
    # 10. grid_8x8_m8_rho-0.4
    ('GRID_APEX', 'grid_8x8_m8_rho-0.4', 'scratchpad/grids/grid_8x8_m8_rho-0.4', 1, 64, 8, -0.4, 0.05, 'scratchpad/grids/grid_8x8_m8_rho-0.4/apex_0.05.mvh'),
    # 11. grid_9x9_m7_rho0.0
    ('GRID_APEX', 'grid_9x9_m7_rho0.0', 'scratchpad/grids/grid_9x9_m7_rho0.0', 1, 81, 7, 0.0, 0.05, 'scratchpad/grids/grid_9x9_m7_rho0.0/apex_0.05.mvh'),
    # 12. grid_8x8_m8_rho-0.2
    ('GRID_APEX', 'grid_8x8_m8_rho-0.2', 'scratchpad/grids/grid_8x8_m8_rho-0.2', 1, 64, 8, -0.2, 0.05, 'scratchpad/grids/grid_8x8_m8_rho-0.2/apex_0.05.mvh'),
]

def parse_line(stdout: str) -> dict:
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

def compare_sols(f1, f2):
    if not os.path.exists(f1) or not os.path.exists(f2):
        return "MISSING"
    with open(f1) as fp1, open(f2) as fp2:
        l1 = fp1.readlines()
        l2 = fp2.readlines()
    if l1 == l2:
        return "EXACT"
    if set(l1) == set(l2):
        return "UNORDERED_MATCH"
    return "MISMATCH"

CSV_OUT = "scratchpad/results/fast3_apex_campaign.csv"
os.makedirs("scratchpad/results", exist_ok=True)
os.makedirs("scratchpad/sols", exist_ok=True)

fieldnames = [
    "idx", "instance", "domain", "M", "rho", "eps", "sols", "exp", "gen", 
    "fast3_time_s", "runtime_s", "cmpchk", "cmpupd", "cmp_chooseh", "cmp_full",
    "num_full_dom", "num_good_fallback", "num_bad_fallback", "match_fast2"
]

with open(CSV_OUT, "w", newline="") as fp:
    writer = csv.DictWriter(fp, fieldnames=fieldnames)
    writer.writeheader()

print("Starting FAST3 benchmark run across all 12 instances...", flush=True)
for idx, (domain, instance, map_dir, start, goal, M, rho, eps, mvh_path) in enumerate(CASES, 1):
    sol_out = f"scratchpad/sols/{instance}_fast3.sol"
    cmd = [
        "build\\Release\\fast_mvh.exe",
        "-m", map_dir,
        "-s", str(start),
        "-g", str(goal),
        "--objectives"
    ] + [str(i) for i in range(M)] + [
        "--mvh", mvh_path,
        "-a", "L_NAMOA_DR_MVH_FAST3",
        "-t", "3600",
        "--sol-out", sol_out
    ]
    t0 = time.time()
    res = subprocess.run(cmd, capture_output=True, text=True)
    t_elapsed = time.time() - t0
    
    d = parse_line(res.stdout)
    sol_ref = f"scratchpad/sols/{instance}_fast2.sol"
    cmp_res = compare_sols(sol_out, sol_ref)
    
    row = {
        "idx": idx,
        "instance": instance,
        "domain": domain,
        "M": M,
        "rho": rho,
        "eps": eps,
        "sols": d.get("num_solutions"),
        "exp": d.get("num_expansion"),
        "gen": d.get("num_generation"),
        "fast3_time_s": round(t_elapsed, 4),
        "runtime_s": round(d.get("runtime_s", 0), 4),
        "cmpchk": d.get("cmpchk"),
        "cmpupd": d.get("cmpupd"),
        "cmp_chooseh": d.get("cmp_chooseh"),
        "cmp_full": d.get("cmp_full"),
        "num_full_dom": d.get("num_full_dominance_check"),
        "num_good_fallback": d.get("num_good_fallback"),
        "num_bad_fallback": d.get("num_bad_fallback"),
        "match_fast2": cmp_res
    }
    with open(CSV_OUT, "a", newline="") as fp:
        writer = csv.DictWriter(fp, fieldnames=fieldnames)
        writer.writerow(row)
    
    print(f"[{idx}/12] {instance} (M={M}, rho={rho}) -> Time: {t_elapsed:.3f}s (inner {d.get('runtime_s', 0):.3f}s) | Sols: {d.get('num_solutions')} | Exp: {d.get('num_expansion')} | cmp_full: {d.get('cmp_full')} | Match: {cmp_res}", flush=True)

print("FAST3 benchmark campaign finished!", flush=True)
