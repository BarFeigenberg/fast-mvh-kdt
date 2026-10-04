import os, sys, subprocess, time, csv, glob

TIMEOUT_MAYA = 3600  # 1 hour per Maya run
TIMEOUT_FAST = 600   # 10 minutes per FAST run

RESULTS_CSV = "benchmarks/runs/24h_high_dim_campaign.csv"
os.makedirs("benchmarks/runs", exist_ok=True)

# Header
fieldnames = [
    "timestamp", "domain", "instance", "M", "rho", "eps", "sols",
    "exp", "gen", "reins", "maya_time_s", "fast_time_s", "fast2_time_s",
    "speedup_fast2_vs_maya", "speedup_fast2_vs_fast",
    "maya_cmp_ch", "fast_cmpfull", "fast2_cmpfull", "cmpfull_reduction_pct",
    "fast2_cmpchk", "fast2_cmpupd", "bit_identical", "maya_status"
]

if not os.path.exists(RESULTS_CSV):
    with open(RESULTS_CSV, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()

def parse_maya_line(stdout):
    d = {}
    for line in stdout.splitlines():
        if line.startswith("algorithm="):
            for part in line.strip().split("\t"):
                if "=" in part:
                    k, v = part.split("=", 1)
                    d[k] = v
            break
    return d

def parse_proto_line(stdout):
    d = {}
    for line in stdout.splitlines():
        if line.startswith("sols="):
            for part in line.strip().split("\t"):
                if "=" in part:
                    k, v = part.split("=", 1)
                    d[k] = v
            break
    return d

def compare_solutions(file_maya, file_fast2):
    if not os.path.exists(file_maya) or not os.path.exists(file_fast2):
        return False
    with open(file_maya) as f1, open(file_fast2) as f2:
        s1 = sorted([l.strip() for l in f1 if l.strip()])
        s2 = sorted([l.strip() for l in f2 if l.strip()])
        return s1 == s2

def run_case(domain, name, map_dir, start, goal, M, rho, eps, mvh_path):
    print(f"\n=======================================================", flush=True)
    print(f"RUNNING: {name} (M={M}, rho={rho}, eps={eps})", flush=True)
    print(f"=======================================================", flush=True)

    if not os.path.exists(mvh_path):
        print(f"ERROR: Heuristic file {mvh_path} not found! Skipping.", flush=True)
        return

    sol_maya = f"scratchpad/sols/{name}_maya.sol"
    sol_fast = f"scratchpad/sols/{name}_fast.sol"
    sol_fast2 = f"scratchpad/sols/{name}_fast2.sol"
    os.makedirs("scratchpad/sols", exist_ok=True)

    # 1. Run FAST2 (witness=1)
    print("-> Running FAST2 (witness=1)...", flush=True)
    cmd_fast2 = f"build/Release/proto_w.exe {map_dir} {start} {goal} {M} {mvh_path} {sol_fast2} flatH=1 local_first=1 witness=1 redund=1 localX=8 targetX=8 promoteC=64 exact_max=1"
    t0 = time.time()
    try:
        r2 = subprocess.run(cmd_fast2, shell=True, capture_output=True, text=True, timeout=TIMEOUT_FAST)
        t_fast2 = time.time() - t0
        d_fast2 = parse_proto_line(r2.stdout)
    except subprocess.TimeoutExpired:
        t_fast2 = TIMEOUT_FAST
        d_fast2 = {"wall": f">{TIMEOUT_FAST}", "sols": "TIMEOUT"}

    print(f"   FAST2: {t_fast2:.3f}s | sols={d_fast2.get('sols')} exp={d_fast2.get('exp')} cmpfull={d_fast2.get('cmpfull')}", flush=True)

    # 2. Run FAST (witness=0)
    print("-> Running FAST (witness=0)...", flush=True)
    cmd_fast = f"build/Release/proto_w.exe {map_dir} {start} {goal} {M} {mvh_path} {sol_fast} flatH=1 local_first=1 witness=0 redund=1 localX=8 targetX=8 promoteC=64 exact_max=1"
    t0 = time.time()
    try:
        r1 = subprocess.run(cmd_fast, shell=True, capture_output=True, text=True, timeout=TIMEOUT_FAST)
        t_fast = time.time() - t0
        d_fast = parse_proto_line(r1.stdout)
    except subprocess.TimeoutExpired:
        t_fast = TIMEOUT_FAST
        d_fast = {"wall": f">{TIMEOUT_FAST}", "sols": "TIMEOUT"}

    print(f"   FAST:  {t_fast:.3f}s | sols={d_fast.get('sols')} exp={d_fast.get('exp')} cmpfull={d_fast.get('cmpfull')}", flush=True)

    # 3. Run Maya Baseline
    print("-> Running Maya Baseline...", flush=True)
    objs = " ".join(str(i) for i in range(M))
    cmd_maya = f"build/Release/fast_mvh.exe -a L_NAMOA_DR_MVH_INSTRUMENTED -m {map_dir} -s {start} -g {goal} --objectives {objs} --mvh {mvh_path} --sol-out {sol_maya} -t {TIMEOUT_MAYA}"
    t0 = time.time()
    maya_status = "OK"
    try:
        rm = subprocess.run(cmd_maya, shell=True, capture_output=True, text=True, timeout=TIMEOUT_MAYA + 30)
        t_maya = time.time() - t0
        d_maya = parse_maya_line(rm.stdout)
        if "time_limit_reached=1" in rm.stdout:
            maya_status = "TIMEOUT"
            t_maya = TIMEOUT_MAYA
    except subprocess.TimeoutExpired:
        t_maya = TIMEOUT_MAYA
        maya_status = "TIMEOUT"
        d_maya = {}

    print(f"   Maya:  {t_maya:.3f}s | status={maya_status} cmp_chooseh={d_maya.get('cmp_chooseh', 'N/A')}", flush=True)

    # Compare bit-identity
    bit_ident = False
    if maya_status == "OK" and os.path.exists(sol_maya) and os.path.exists(sol_fast2):
        bit_ident = compare_solutions(sol_maya, sol_fast2)
        print(f"   Soundness: {'BIT_IDENTICAL' if bit_ident else 'MISMATCH'}", flush=True)

    # Compute speedup
    speedup_maya = (t_maya / t_fast2) if (t_fast2 > 0 and maya_status == "OK") else (t_maya / max(t_fast2, 0.001))
    speedup_wit = (t_fast / t_fast2) if (t_fast2 > 0 and t_fast > 0) else 1.0

    cmpfull_red = 0.0
    try:
        cf1 = float(d_fast.get("cmpfull", 0))
        cf2 = float(d_fast2.get("cmpfull", 0))
        if cf1 > 0:
            cmpfull_red = (cf1 - cf2) / cf1 * 100.0
    except:
        pass

    row = {
        "timestamp": time.strftime("%Y-%m-%d %H:%M:%S"),
        "domain": domain,
        "instance": name,
        "M": M,
        "rho": rho,
        "eps": eps,
        "sols": d_fast2.get("sols", d_maya.get("num_solutions", "")),
        "exp": d_fast2.get("exp", d_maya.get("num_expansion", "")),
        "gen": d_fast2.get("gen", d_maya.get("num_generation", "")),
        "reins": d_fast2.get("reins", d_maya.get("num_reinsertion", "")),
        "maya_time_s": f"{t_maya:.4f}",
        "fast_time_s": f"{t_fast:.4f}",
        "fast2_time_s": f"{t_fast2:.4f}",
        "speedup_fast2_vs_maya": f"{speedup_maya:.2f}x",
        "speedup_fast2_vs_fast": f"{speedup_wit:.2f}x",
        "maya_cmp_ch": d_maya.get("cmp_chooseh", ""),
        "fast_cmpfull": d_fast.get("cmpfull", ""),
        "fast2_cmpfull": d_fast2.get("cmpfull", ""),
        "cmpfull_reduction_pct": f"{cmpfull_red:.1f}%",
        "fast2_cmpchk": d_fast2.get("cmpchk", ""),
        "fast2_cmpupd": d_fast2.get("cmpupd", ""),
        "bit_identical": "TRUE" if bit_ident else ("TIMEOUT" if maya_status == "TIMEOUT" else "FALSE"),
        "maya_status": maya_status
    }

    with open(RESULTS_CSV, "a", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writerow(row)

    print(f"-> Logged result to {RESULTS_CSV}: Speedup={row['speedup_fast2_vs_maya']} (FAST2 vs Maya)", flush=True)

if __name__ == "__main__":
    cases = [
        # --- Real Road Networks (Bay Area 800-node core) ---
        ('ROAD_BAY', 'bay_800_5d', 'scratchpad/maps/bay_8d_800', 800, 1, 5, 'none', 0.1, 'scratchpad/maps/bay_8d_800/apex_5d_0.1.mvh'),
        ('ROAD_BAY', 'bay_800_6d', 'scratchpad/maps/bay_8d_800', 800, 1, 6, 'none', 0.1, 'scratchpad/maps/bay_8d_800/apex_6d_0.1.mvh'),
        ('ROAD_BAY', 'bay_800_7d', 'scratchpad/maps/bay_8d_800', 800, 1, 7, 'none', 0.1, 'scratchpad/maps/bay_8d_800/apex_7d_0.1.mvh'),
        ('ROAD_BAY', 'bay_800_8d', 'scratchpad/maps/bay_8d_800', 800, 1, 8, 'none', 0.1, 'scratchpad/maps/bay_8d_800/apex_8d_0.1.mvh'),

        # --- High-Dimensional Genuine A*pex Grids (Medium Scale) ---
        ('GRID_APEX', 'grid_n6_m8_rho-0.2', 'scratchpad/grids/grid_n6_m8_rho-0.2', 1, 36, 8, -0.2, 0.05, 'scratchpad/grids/grid_n6_m8_rho-0.2/apex_0.05.mvh'),
        ('GRID_APEX', 'grid_n7_m7_rho-0.2', 'scratchpad/grids/grid_n7_m7_rho-0.2', 1, 49, 7, -0.2, 0.05, 'scratchpad/grids/grid_n7_m7_rho-0.2/apex_0.05.mvh'),
        ('GRID_APEX', 'grid_n7_m7_rho-0.4', 'scratchpad/grids/grid_n7_m7_rho-0.4', 1, 49, 7, -0.4, 0.05, 'scratchpad/grids/grid_n7_m7_rho-0.4/apex_0.05.mvh'),
        ('GRID_APEX', 'grid_n8_m6_rho-0.2', 'scratchpad/grids/grid_n8_m6_rho-0.2', 1, 64, 6, -0.2, 0.05, 'scratchpad/grids/grid_n8_m6_rho-0.2/apex_0.05.mvh'),

        # --- Deep Scaling Genuine A*pex Grids (Heavy Scale) ---
        ('GRID_APEX', 'grid_n7_m8_rho-0.2', 'scratchpad/grids/grid_n7_m8_rho-0.2', 1, 49, 8, -0.2, 0.05, 'scratchpad/grids/grid_n7_m8_rho-0.2/apex_0.05.mvh'),
        ('GRID_APEX', 'grid_n8_m7_rho-0.2', 'scratchpad/grids/grid_n8_m7_rho-0.2', 1, 64, 7, -0.2, 0.05, 'scratchpad/grids/grid_n8_m7_rho-0.2/apex_0.05.mvh'),
        ('GRID_APEX', 'grid_n8_m8_rho0.0',  'scratchpad/grids/grid_n8_m8_rho0.0',  1, 64, 8, 0.0,  0.05, 'scratchpad/grids/grid_n8_m8_rho0.0/apex_0.05.mvh'),
        ('GRID_APEX', 'grid_n9_m6_rho-0.2', 'scratchpad/grids/grid_n9_m6_rho-0.2', 1, 81, 6, -0.2, 0.05, 'scratchpad/grids/grid_n9_m6_rho-0.2/apex_0.05.mvh'),
        ('GRID_APEX', 'grid_n10_m6_rho0.0', 'scratchpad/grids/grid_n10_m6_rho0.0', 1, 100, 6, 0.0, 0.05, 'scratchpad/grids/grid_n10_m6_rho0.0/apex_0.05.mvh'),
    ]

    for c in cases:
        run_case(*c)
