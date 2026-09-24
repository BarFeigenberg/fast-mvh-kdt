import os
import sys
import subprocess
import time
import csv

CSV_PATH = "benchmarks/runs/adaptive_explorer/v5_breakthrough_scaling.csv"
MD_PATH = "benchmarks/runs/overnight_breakthrough_log.md"
FAST_MVH_BIN = os.path.abspath("build/Release/fast_mvh.exe")
GEN_SCRIPT = os.path.abspath("benchmarks/generators/generate_mvh.py")

os.makedirs("benchmarks/runs/adaptive_explorer", exist_ok=True)

def init_files():
    if not os.path.exists(CSV_PATH):
        with open(CSV_PATH, "w", newline="", encoding="utf-8") as f:
            writer = csv.writer(f)
            writer.writerow([
                "N", "M", "Rho", "K", "Solutions", "Generations",
                "V3_Time_s", "V5_Time_s", "Maya_Time_s", 
                "V3_Speedup", "V5_Speedup",
                "V3_Cmp", "V5_Cmp", "Maya_Cmp"
            ])
            
    if not os.path.exists(MD_PATH):
        with open(MD_PATH, "w", encoding="utf-8") as f:
            f.write("# V5 Dual-Tree Breakthrough Scaling Log\n\n")
            f.write("Tracking the scaling advantage of V5 (Dual Tree) and V3 vs Maya.\n\n")
            f.write("| Grid | M | Rho | K | Sols | Gen | V3 Time | V5 Time | Maya Time | V3 Speedup | V5 Speedup | V3 Cmp | V5 Cmp | Maya Cmp |\n")
            f.write("|:---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|\n")

def append_csv(row):
    with open(CSV_PATH, "a", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(row)

def append_md(r):
    v3t = f"{r['V3_Time_s']:.2f}" if isinstance(r['V3_Time_s'], float) else r['V3_Time_s']
    v5t = f"{r['V5_Time_s']:.2f}" if isinstance(r['V5_Time_s'], float) else r['V5_Time_s']
    mt = f"{r['Maya_Time_s']:.2f}" if isinstance(r['Maya_Time_s'], float) else r['Maya_Time_s']
    su3 = f"**{r['V3_Speedup']:.2f}x**" if isinstance(r['V3_Speedup'], float) else r['V3_Speedup']
    su5 = f"**{r['V5_Speedup']:.2f}x**" if isinstance(r['V5_Speedup'], float) else r['V5_Speedup']
    
    with open(MD_PATH, "a", encoding="utf-8") as f:
        f.write(f"| {r['N']}x{r['N']} | {r['M']} | {r['Rho']} | {r['K']} | {r['Sols']} | {r['Gen']} | {v3t} | {v5t} | {mt} | {su3} | {su5} | {r['V3_Cmp']} | {r['V5_Cmp']} | {r['Maya_Cmp']} |\n")

def generate_inst(N, M, rho, K, seed=42):
    inst_dir = os.path.abspath(f"scratchpad/deep_scaling/grid_{N}x{N}_M{M}_rho{rho}")
    mvh_path = os.path.join(inst_dir, f"target_{N*N}_K{K}.mvh")
    if not os.path.exists(mvh_path):
        os.makedirs(inst_dir, exist_ok=True)
        if not any(f.endswith(".gr") for f in os.listdir(inst_dir) if os.path.isfile(os.path.join(inst_dir, f))) or not os.listdir(inst_dir):
            cmd_grid = [
                sys.executable, "benchmarks/generators/generate_grid.py",
                "--rows", str(N), "--cols", str(N),
                "-M", str(M), "--rho", str(rho),
                "--seed", str(seed), "--out-dir", inst_dir
            ]
            subprocess.run(cmd_grid, check=True, stdout=subprocess.DEVNULL)
            
        cmd_mvh = [
            sys.executable, GEN_SCRIPT, "--map", inst_dir,
            "--goal", str(N*N), "-K", str(K),
            "--seed", str(seed), "--out", mvh_path
        ]
        subprocess.run(cmd_mvh, check=True, stdout=subprocess.DEVNULL)
    return inst_dir, mvh_path

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
        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=timeout+10)
        dt = time.perf_counter() - t0
        met = parse_output(res.stdout)
        if met.get("time_limit_reached") == 1 or met.get("runtime_s", 0) >= timeout:
            return False, dt, met
        met["real_time"] = dt
        return True, dt, met
    except subprocess.TimeoutExpired:
        return False, timeout, {}

def main():
    init_files()
    
    queue = [
        (15, 3, -0.6, 200),
        (15, 3, -0.6, 300),
        (15, 3, -0.6, 500),
        (20, 3, -0.3, 100),
        (20, 3, -0.3, 200),
        (20, 3, -0.3, 400),
        (25, 3, -0.3, 100),
        (25, 3, -0.3, 200),
        (15, 4, -0.3, 100),
        (15, 4, -0.3, 200)
    ]
    
    TIMEOUT = 1200
    
    print("=========================================================")
    print("  OVERNIGHT DUAL-TREE BREAKTHROUGH EXPLORATION (V5 vs Maya)")
    print("=========================================================")
    
    for state in queue:
        N, M, rho, K = state
        print(f"\\n[Evaluating] N={N}, M={M}, rho={rho}, K={K}")
        sys.stdout.flush()
        
        try:
            inst_dir, mvh_path = generate_inst(N, M, rho, K)
        except Exception as e:
            print(f"Failed to generate: {e}")
            continue
            
        print(f" -> V3_ExpandOnly (Timeout {TIMEOUT}s)... ", end="", flush=True)
        v3_ok, v3_time, v3_met = run_solver(inst_dir, mvh_path, N, M, "L_NAMOA_KDT_V3", TIMEOUT)
        if v3_ok: print(f"SUCCESS ({v3_met['runtime_s']:.2f}s)")
        else: print("TIMEOUT")

        print(f" -> V5_DualTree (Timeout {TIMEOUT}s)... ", end="", flush=True)
        v5_ok, v5_time, v5_met = run_solver(inst_dir, mvh_path, N, M, "L_NAMOA_KDT_V5", TIMEOUT)
        if v5_ok: print(f"SUCCESS ({v5_met['runtime_s']:.2f}s)")
        else: print("TIMEOUT")
        
        if v3_ok or v5_ok:
            best_time = min(v3_met.get('runtime_s', TIMEOUT), v5_met.get('runtime_s', TIMEOUT))
            maya_timeout = min(TIMEOUT, int(best_time * 5) + 60)
            print(f" -> Maya_Linear (Timeout {maya_timeout}s)... ", end="", flush=True)
            maya_ok, maya_time, maya_met = run_solver(inst_dir, mvh_path, N, M, "L_NAMOA_DR_MVH_INSTRUMENTED", maya_timeout)
            if maya_ok: print(f"SUCCESS ({maya_met['runtime_s']:.2f}s)")
            else: print("TIMEOUT")
        else:
            maya_met = {}
            maya_ok = False
            
        row = {
            "N": N, "M": M, "Rho": rho, "K": K,
            "Sols": v5_met.get("num_solutions", v3_met.get("num_solutions", "-")),
            "Gen": v5_met.get("num_generation", v3_met.get("num_generation", "-")),
            "V3_Time_s": v3_met.get("runtime_s", "TIMEOUT") if not v3_ok else v3_met.get("runtime_s"),
            "V5_Time_s": v5_met.get("runtime_s", "TIMEOUT") if not v5_ok else v5_met.get("runtime_s"),
            "Maya_Time_s": maya_met.get("runtime_s", "TIMEOUT") if not maya_ok else maya_met.get("runtime_s"),
            "V3_Cmp": v3_met.get("cmp_chooseh", "-"),
            "V5_Cmp": v5_met.get("cmp_chooseh", "-"),
            "Maya_Cmp": maya_met.get("cmp_chooseh", "-")
        }
        
        if v3_ok and maya_ok:
            row["V3_Speedup"] = maya_met["runtime_s"] / v3_met["runtime_s"]
        elif v3_ok and not maya_ok:
            row["V3_Speedup"] = f">{(maya_timeout / v3_met['runtime_s']):.1f}x"
        else:
            row["V3_Speedup"] = "-"

        if v5_ok and maya_ok:
            row["V5_Speedup"] = maya_met["runtime_s"] / v5_met["runtime_s"]
        elif v5_ok and not maya_ok:
            row["V5_Speedup"] = f">{(maya_timeout / v5_met['runtime_s']):.1f}x"
        else:
            row["V5_Speedup"] = "-"
            
        append_csv([
            row["N"], row["M"], row["Rho"], row["K"],
            row["Sols"], row["Gen"],
            row["V3_Time_s"], row["V5_Time_s"], row["Maya_Time_s"], 
            row["V3_Speedup"], row["V5_Speedup"],
            row["V3_Cmp"], row["V5_Cmp"], row["Maya_Cmp"]
        ])
        append_md(row)

if __name__ == "__main__":
    main()
