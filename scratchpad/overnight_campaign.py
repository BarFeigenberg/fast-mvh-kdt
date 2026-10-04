import os
import subprocess
import time
import csv

TIMEOUT = 10800 # 3 hours

CASES = [
    # Shorter runs (lower dimensions, easier correlations)
    ("bay_2k_5d", "scratchpad/maps/bay_8d_2000", 2000, 1, 5, "scratchpad/maps/bay_8d_2000/apex_5d_0.05.mvh"),
    ("bay_2k_6d", "scratchpad/maps/bay_8d_2000", 2000, 1, 6, "scratchpad/maps/bay_8d_2000/apex_6d_0.05.mvh"),
    ("grid_n8_m6_rho-0.2", "scratchpad/grids/grid_n8_m6_rho-0.2", 1, 64, 6, "scratchpad/grids/grid_n8_m6_rho-0.2/apex_0.05.mvh"),
    
    # Medium runs (7D)
    ("bay_2k_7d", "scratchpad/maps/bay_8d_2000", 2000, 1, 7, "scratchpad/maps/bay_8d_2000/apex_7d_0.05.mvh"),
    ("grid_n7_m7_rho-0.2", "scratchpad/grids/grid_n7_m7_rho-0.2", 1, 49, 7, "scratchpad/grids/grid_n7_m7_rho-0.2/apex_0.05.mvh"),
    
    # Longer runs (8D and hard anti-correlations)
    ("bay_2k_8d", "scratchpad/maps/bay_8d_2000", 2000, 1, 8, "scratchpad/maps/bay_8d_2000/apex_8d_0.05.mvh"),
    ("grid_n6_m8_rho-0.2", "scratchpad/grids/grid_n6_m8_rho-0.2", 1, 36, 8, "scratchpad/grids/grid_n6_m8_rho-0.2/apex_0.05.mvh"),
    ("grid_n7_m7_rho-0.4", "scratchpad/grids/grid_n7_m7_rho-0.4", 1, 49, 7, "scratchpad/grids/grid_n7_m7_rho-0.4/apex_0.05.mvh"),
]

def parse_maya(stdout):
    res = {}
    for line in stdout.split('\n'):
        if line.startswith("algorithm="):
            parts = line.strip().split('\t')
            for p in parts:
                if "=" in p:
                    k, v = p.split("=")
                    res[k] = v
            break
    if not res: return {"wall": ">10800", "sols": "TIMEOUT"}
    return {
        "wall": res.get("runtime_s", ""),
        "sols": res.get("num_solutions", ""),
        "exp": res.get("num_expansion", ""),
        "gen": res.get("num_generation", ""),
        "cmpchk": res.get("num_dr_dominance_check", ""), 
    }

def parse_proto(stdout):
    res = {}
    for line in stdout.split('\n'):
        if line.startswith("sols="):
            parts = line.strip().split('\t')
            for p in parts:
                if "=" in p:
                    k, v = p.split("=")
                    res[k] = v
            break
    if not res: return {"wall": "ERR", "sols": "ERR"}
    return res

os.makedirs("scratchpad/results", exist_ok=True)
csv_file = "scratchpad/results/overnight_apex_results.csv"

with open(csv_file, "w", newline='') as f:
    writer = csv.writer(f)
    writer.writerow(["Name", "Solver", "Wall", "Sols", "Expansions", "Generations", "cmpchk", "cmpupd"])

for name, map_dir, start, goal, M, mvh in CASES:
    print(f"\n--- Running {name} ---")
    
    print("Running Maya...")
    objs = " ".join(str(i) for i in range(M))
    cmd_maya = f"build/Release/fast_mvh.exe -a L_NAMOA_DR_MVH_INSTRUMENTED -m {map_dir} -s {start} -g {goal} --objectives {objs} --mvh {mvh} -t {TIMEOUT}"
    
    maya_data = {"wall": ">10800", "sols": "TIMEOUT"}
    try:
        start_time = time.time()
        p = subprocess.run(cmd_maya, shell=True, capture_output=True, text=True, timeout=TIMEOUT + 10)
        wall = time.time() - start_time
        if "algorithm=" in p.stdout:
            maya_data = parse_maya(p.stdout)
        elif "time_limit_reached=1" in p.stdout:
            maya_data = {"wall": f">{TIMEOUT}", "sols": "TIMEOUT"}
    except subprocess.TimeoutExpired:
        maya_data = {"wall": f">{TIMEOUT}", "sols": "TIMEOUT"}
        
    print(f"Maya: wall={maya_data.get('wall')} sols={maya_data.get('sols')}")
    with open(csv_file, "a", newline='') as f:
        writer = csv.writer(f)
        writer.writerow([name, "Maya", maya_data.get("wall"), maya_data.get("sols"), maya_data.get("exp"), maya_data.get("gen"), "", ""])
        
    print("Running FAST (tcache=0)...")
    cmd_fast = f"build/Release/proto_w.exe {map_dir} {start} {goal} {M} {mvh} NUL flatH=1 local_first=1 witness=1 redund=1 localX=8 targetX=8 promoteC=64 exact_max=1 tcache=0"
    try:
        p = subprocess.run(cmd_fast, shell=True, capture_output=True, text=True, timeout=TIMEOUT)
        fast_data = parse_proto(p.stdout)
    except subprocess.TimeoutExpired:
        fast_data = {"wall": f">{TIMEOUT}", "sols": "TIMEOUT"}
        
    print(f"FAST: wall={fast_data.get('wall')} sols={fast_data.get('sols')}")
    with open(csv_file, "a", newline='') as f:
        writer = csv.writer(f)
        writer.writerow([name, "FAST", fast_data.get("wall"), fast_data.get("sols"), fast_data.get("exp"), fast_data.get("gen"), fast_data.get("cmpchk"), fast_data.get("cmpupd")])
        
    print("Running FAST2 (tcache=2)...")
    cmd_fast2 = f"build/Release/proto_w.exe {map_dir} {start} {goal} {M} {mvh} NUL flatH=1 local_first=1 witness=1 redund=1 localX=8 targetX=8 promoteC=64 exact_max=1 tcache=2"
    try:
        p = subprocess.run(cmd_fast2, shell=True, capture_output=True, text=True, timeout=TIMEOUT)
        fast2_data = parse_proto(p.stdout)
    except subprocess.TimeoutExpired:
        fast2_data = {"wall": f">{TIMEOUT}", "sols": "TIMEOUT"}
        
    print(f"FAST2: wall={fast2_data.get('wall')} sols={fast2_data.get('sols')}")
    with open(csv_file, "a", newline='') as f:
        writer = csv.writer(f)
        writer.writerow([name, "FAST2", fast2_data.get("wall"), fast2_data.get("sols"), fast2_data.get("exp"), fast2_data.get("gen"), fast2_data.get("cmpchk"), fast2_data.get("cmpupd")])

print("\nAll done.")
