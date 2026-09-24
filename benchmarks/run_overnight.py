import os
import csv
import subprocess
import time
import datetime
import itertools

def parse_solver_stdout(stdout):
    metrics = {}
    for line in stdout.splitlines():
        line = line.strip()
        if "=" in line:
            for token in line.split("\t"):
                if "=" in token:
                    k, v = token.split("=", 1)
                    k, v = k.strip(), v.strip()
                    try:
                        metrics[k] = float(v) if "." in v else int(v)
                    except ValueError:
                        metrics[k] = v
    return metrics

def run_solver(bin_path, map_dir, M, K, algo, timeout):
    goal = int(map_dir.split("grid")[1].split("x")[0])**2
    cmd = [
        bin_path,
        "--map", map_dir,
        "--start", "1",
        "--goal", str(goal),
        "--objectives", *[str(i) for i in range(M)],
        "--algorithm", algo,
        "--mvh", os.path.join(map_dir, f"mvh_K{K}.mvh"),
        "--cutoffTime", str(timeout)
    ]
    start = time.time()
    timed_out = False
    stdout_text = ""
    try:
        proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=timeout + 2, errors="replace")
        stdout_text = proc.stdout
    except subprocess.TimeoutExpired as te:
        timed_out = True
        try:
            if te.stdout:
                stdout_text = te.stdout.decode("utf-8", errors="replace") if isinstance(te.stdout, bytes) else str(te.stdout)
        except Exception:
            pass
    except Exception as e:
        timed_out = True
        stdout_text = ""
        
    duration = time.time() - start
    metrics = parse_solver_stdout(stdout_text)
    
    # If the C++ process hit its own internal timeout, it reports time_limit_reached=1
    if metrics.get("time_limit_reached") == 1:
        timed_out = True
        
    metrics["wall_clock_s"] = duration
    metrics["timed_out"] = timed_out
    return metrics

def main():
    bin_path = "build/Release/fast_mvh.exe"
    
    passes = [60, 300, 900, 1800]
    
    Ns = [10, 20, 30, 40, 50]
    Ms = [3, 4, 5, 6, 7, 8]
    rhos = [0.0, -0.3, -0.6]
    Ks = [10, 25, 50, 100]
    algos = ["L_NAMOA_DR_MVH_INSTRUMENTED", "L_NAMOA_DR_MVH_KDT", "L_NAMOA_KDT_CHOOSEH"]
    
    tasks = []
    for N, M, rho in itertools.product(Ns, Ms, rhos):
        map_dir = f"benchmarks/instances/overnight/grid{N}x{N}_M{M}_rho{rho}"
        for K, algo in itertools.product(Ks, algos):
            tasks.append({
                "N": N, "M": M, "rho": rho, "K": K,
                "algo": algo, "map_dir": map_dir
            })
            
    csv_file = "benchmarks/runs/overnight_results.csv"
    os.makedirs("benchmarks/runs", exist_ok=True)
    
    fields = ["N", "M", "rho", "K", "algo", "pass_idx", "timeout_limit", 
              "timed_out", "wall_clock_s", "num_solutions", "num_expansion", 
              "num_generation", "cmpchk", "cmpupd", "cmp_chooseh", "time_limit_reached"]
              
    completed_runs = {}
    if os.path.exists(csv_file):
        with open(csv_file, "r", encoding="utf-8") as f:
            reader = csv.DictReader(f)
            for row in reader:
                try:
                    key = (int(row["N"]), int(row["M"]), float(row["rho"]), int(row["K"]), row["algo"], int(row["pass_idx"]))
                    completed_runs[key] = (row.get("timed_out", "False") == "True")
                except Exception:
                    pass
    else:
        with open(csv_file, "w", newline="", encoding="utf-8") as f:
            writer = csv.DictWriter(f, fieldnames=fields)
            writer.writeheader()

    remaining_tasks = tasks
    
    for pass_idx, timeout in enumerate(passes):
        print(f"\n=== STARTING PASS {pass_idx+1} (Timeout: {timeout}s) - {len(remaining_tasks)} tasks ===")
        next_remaining = []
        
        for i, t in enumerate(remaining_tasks):
            run_key = (t["N"], t["M"], float(t["rho"]), t["K"], t["algo"], pass_idx + 1)
            if run_key in completed_runs:
                if completed_runs[run_key]:
                    next_remaining.append(t)
                continue

            print(f"[{i+1}/{len(remaining_tasks)}] N={t['N']} M={t['M']} rho={t['rho']} K={t['K']} Algo={t['algo']} ... ", end="", flush=True)
            
            if not os.path.exists(t["map_dir"]):
                print("SKIPPED (Map missing)")
                continue
                
            try:
                res = run_solver(bin_path, t["map_dir"], t["M"], t["K"], t["algo"], timeout)
            except Exception as e:
                res = {"timed_out": True, "wall_clock_s": 0.0, "error": str(e)}

            print(f"{'TIMEOUT' if res.get('timed_out') else 'SUCCESS'} ({res.get('wall_clock_s', 0.0):.1f}s) Expansions: {res.get('num_expansion', 'N/A')}")
            
            # Write to CSV immediately
            row = {
                "N": t["N"], "M": t["M"], "rho": t["rho"], "K": t["K"],
                "algo": t["algo"], "pass_idx": pass_idx + 1, "timeout_limit": timeout,
                "timed_out": res.get("timed_out", False),
                "wall_clock_s": res.get("wall_clock_s", 0.0),
                "num_solutions": res.get("num_solutions", ""),
                "num_expansion": res.get("num_expansion", ""),
                "num_generation": res.get("num_generation", ""),
                "cmpchk": res.get("cmpchk", res.get("num_dr_dominance_check", "")),
                "cmpupd": res.get("cmpupd", ""),
                "cmp_chooseh": res.get("cmp_chooseh", ""),
                "time_limit_reached": res.get("time_limit_reached", "")
            }
            with open(csv_file, "a", newline="", encoding="utf-8") as f:
                writer = csv.DictWriter(f, fieldnames=fields)
                writer.writerow(row)
                
            if res.get("timed_out", False):
                next_remaining.append(t)
                
        remaining_tasks = next_remaining
        if not remaining_tasks:
            break
            
    print("\n=== PIPELINE FINISHED ===")

if __name__ == "__main__":
    main()
