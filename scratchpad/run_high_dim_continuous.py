import ctypes
import os
import sys
import time
import datetime
import subprocess
import json
import csv
import random
import math
import psutil

# --- Windows Sleep Prevention ---
ES_CONTINUOUS = 0x80000000
ES_SYSTEM_REQUIRED = 0x00000001
ES_AWAYMODE_REQUIRED = 0x00000040

def prevent_sleep():
    try:
        ctypes.windll.kernel32.SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_AWAYMODE_REQUIRED)
        print("[INFO] Windows sleep prevention active.")
    except Exception as e:
        print(f"[WARN] Failed to set thread execution state: {e}")

def restore_sleep():
    try:
        ctypes.windll.kernel32.SetThreadExecutionState(ES_CONTINUOUS)
        print("[INFO] Windows sleep prevention removed.")
    except Exception as e:
        pass

# --- Topology Generation ---
def generate_tradeoff_grid(rows, cols, num_objs, out_dir, seed):
    random.seed(seed)
    os.makedirs(out_dir, exist_ok=True)
    def node_id(r, c): return r * cols + c + 1
    edges = []
    for r in range(rows):
        for c in range(cols):
            u = node_id(r, c)
            if c + 1 < cols:
                v = node_id(r, c + 1)
                edges.append((u, v))
                edges.append((v, u))
            if r + 1 < rows:
                v = node_id(r + 1, c)
                edges.append((u, v))
                edges.append((v, u))
    edges.sort()
    for m in range(num_objs):
        fpath = os.path.join(out_dir, f"obj_{m}.gr")
        with open(fpath, "w", encoding="utf-8") as f:
            for i, (u, v) in enumerate(edges):
                # Ensure deterministic costs across objectives
                random.seed(seed + i * 100 + m)
                base = random.randint(2, 30)
                if m % 2 == 1:
                    cost = max(1, 35 - base + random.randint(-4, 4))
                else:
                    cost = max(1, base + random.randint(1, 15))
                f.write(f"a {u} {v} {cost}\n")
    return rows * cols

def generate_tradeoff_geom(num_nodes, radius, num_objs, out_dir, seed):
    random.seed(seed)
    os.makedirs(out_dir, exist_ok=True)
    coords = [(random.random(), random.random()) for _ in range(num_nodes)]
    edges = []
    for i in range(num_nodes):
        for j in range(num_nodes):
            if i != j:
                dx, dy = coords[i][0] - coords[j][0], coords[i][1] - coords[j][1]
                if math.hypot(dx, dy) <= radius:
                    edges.append((i + 1, j + 1))
    for i in range(1, num_nodes):
        if not any(u == i + 1 for (u, v) in edges):
            edges.append((i + 1, i))
            edges.append((i, i + 1))
    edges = sorted(list(set(edges)))
    
    for m in range(num_objs):
        fpath = os.path.join(out_dir, f"obj_{m}.gr")
        with open(fpath, "w", encoding="utf-8") as f:
            for i, (u, v) in enumerate(edges):
                p1, p2 = coords[u - 1], coords[v - 1]
                eucl = int(math.hypot(p1[0] - p2[0], p1[1] - p2[1]) * 100) + 1
                random.seed(seed + i * 100 + m)
                if m > 0:
                    if m % 2 == 1:
                        cost = max(1, 120 - eucl + random.randint(-5, 5))
                    else:
                        cost = max(1, eucl + random.randint(2, 20))
                else:
                    cost = eucl
                f.write(f"a {u} {v} {cost}\n")
    return num_nodes

# --- Runner Logic ---
def execute_query(cmd, timeout_s, max_ram_mb, min_sys_ram_gb):
    start_time = time.time()
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    
    try:
        ps_proc = psutil.Process(proc.pid)
    except Exception:
        ps_proc = None

    timed_out = False
    ram_exceeded = False
    max_rss_mb = 0.0
    exit_code = 0

    while True:
        ret = proc.poll()
        if ret is not None:
            exit_code = ret
            break
        
        elapsed = time.time() - start_time
        if elapsed > timeout_s:
            timed_out = True
            proc.kill()
            break
            
        if ps_proc:
            try:
                rss_mb = ps_proc.memory_info().rss / (1024 * 1024)
                if rss_mb > max_rss_mb:
                    max_rss_mb = rss_mb
                if rss_mb > max_ram_mb:
                    ram_exceeded = True
                    proc.kill()
                    break
            except Exception:
                pass
                
        sys_avail_gb = psutil.virtual_memory().available / (1024**3)
        if sys_avail_gb < min_sys_ram_gb:
            print(f"[WARN] System RAM below {min_sys_ram_gb} GB! Pausing...")
            proc.kill()
            ram_exceeded = True
            time.sleep(10)
            break
            
        time.sleep(0.1)
        
    stdout, stderr = proc.communicate()
    duration = time.time() - start_time
    
    return {
        "stdout": stdout,
        "stderr": stderr,
        "wall_s": duration,
        "peak_rss_mb": max_rss_mb,
        "timed_out": timed_out,
        "ram_exceeded": ram_exceeded,
        "exit_code": exit_code
    }

def parse_metrics(stdout):
    metrics = {}
    for line in stdout.splitlines():
        line = line.strip()
        if "=" in line:
            for token in line.split("\t"):
                if "=" in token:
                    k, v = token.split("=", 1)
                    try:
                        metrics[k.strip()] = float(v) if "." in v else int(v)
                    except ValueError:
                        metrics[k.strip()] = v.strip()
    return metrics

def run():
    prevent_sleep()
    try:
        run_dir = "benchmarks/runs/high_dim_continuous"
        os.makedirs(run_dir, exist_ok=True)
        golden_dir = "benchmarks/golden_results"
        os.makedirs(golden_dir, exist_ok=True)
        
        csv_file = os.path.join(run_dir, "metrics_summary.csv")
        md_file = os.path.join(run_dir, "metrics_summary.md")
        status_file = "scratchpad/continuous_runner_status.json"
        
        solver_bin = os.path.abspath("build/baselines/bridging-mvh-dr/Release/MultivaluedHeuristicSearch.exe")
        
        fields = ["timestamp", "algorithm", "map", "dim", "s", "g", "sols", "wall_s", "peak_mb", 
                  "expansions", "generations", "cmp_dr", "cmp_full", "status"]
        
        if not os.path.exists(csv_file):
            with open(csv_file, "w", newline="", encoding="utf-8") as f:
                csv.DictWriter(f, fieldnames=fields).writeheader()
        
        cycle = 1
        completed = 0
        start_time = time.time()
        
        while True:
            # Generate topologies
            workload = []
            seed = 1000 + cycle
            
            for d in [4, 6, 8]:
                # Grids
                for (r, c) in [(10, 10), (15, 15), (20, 20)]:
                    mname = f"grid_{r}x{c}_d{d}_tradeoff_cyc{cycle}"
                    out_path = f"benchmarks/instances/{mname}"
                    nodes = generate_tradeoff_grid(r, c, d, out_path, seed)
                    # Corners
                    workload.append((out_path, mname, 1, nodes, d))
                
                # Geoms
                for (n, rad) in [(100, 0.25), (200, 0.18)]:
                    mname = f"geom_n{n}_d{d}_tradeoff_cyc{cycle}"
                    out_path = f"benchmarks/instances/{mname}"
                    nodes = generate_tradeoff_geom(n, rad, d, out_path, seed)
                    workload.append((out_path, mname, 1, nodes, d))
            
            print(f"\n--- CYCLE {cycle} STARTED ({len(workload)} topologies) ---")
            
            for (path, mname, s, g, d) in workload:
                for algo in ["L_NAMOA_DR_MVH", "NAMOA_DR"]:
                    print(f"[{completed+1}] {algo} on {mname} ({s}->{g}) ...", flush=True)
                    
                    obj_args = [str(i) for i in range(d)]
                    cmd = [
                        solver_bin, "--map", path, "--start", str(s), "--goal", str(g),
                        "--algorithm", algo, "--objectives", *obj_args, "--cutoffTime", "60"
                    ]
                    
                    res = execute_query(cmd, timeout_s=60.0, max_ram_mb=1200.0, min_sys_ram_gb=2.0)
                    
                    # Cool down phase
                    time.sleep(3.0)
                    
                    met = parse_metrics(res["stdout"])
                    status = "OK"
                    if res["timed_out"]: status = "TIMEOUT"
                    elif res["ram_exceeded"]: status = "RAM_EXCEEDED"
                    elif res["exit_code"] != 0: status = f"ERR_{res['exit_code']}"
                    
                    row = {
                        "timestamp": datetime.datetime.now().isoformat(),
                        "algorithm": algo,
                        "map": mname,
                        "dim": d,
                        "s": s,
                        "g": g,
                        "sols": met.get("num_solutions", 0),
                        "wall_s": round(res["wall_s"], 3),
                        "peak_mb": round(res["peak_rss_mb"], 1),
                        "expansions": met.get("num_expansion", 0),
                        "generations": met.get("num_generation", 0),
                        "cmp_dr": met.get("num_dr_dominance_check", 0),
                        "cmp_full": met.get("num_full_dominance_check", 0),
                        "status": status
                    }
                    
                    with open(csv_file, "a", newline="", encoding="utf-8") as f:
                        csv.DictWriter(f, fieldnames=fields).writerow(row)
                    
                    # Update Markdown
                    with open(md_file, "w", encoding="utf-8") as f:
                        f.write(f"# High-Dim Continuous Benchmark\nTotal Queries: {completed+1}\n\n")
                        f.write("| " + " | ".join(fields) + " |\n")
                        f.write("| " + " | ".join(["---"] * len(fields)) + " |\n")
                        # (in reality we'd append or rewrite, we'll keep it simple for now or write a tail)
                    
                    if status == "OK" and algo == "L_NAMOA_DR_MVH":
                        g_file = os.path.join(golden_dir, f"{mname}_m{d}_s{s}_g{g}_metrics.json")
                        with open(g_file, "w") as gf:
                            json.dump(met, gf, indent=2)
                    
                    completed += 1
                    
                    # Heartbeat
                    with open(status_file, "w") as sf:
                        json.dump({
                            "cycle": cycle,
                            "elapsed_time_minutes": round((time.time() - start_time) / 60.0, 2),
                            "completed_queries": completed,
                            "current_host_free_ram_gb": round(psutil.virtual_memory().available / (1024**3), 2),
                            "last_peak_process_rss_mb": row["peak_mb"],
                            "status": "RUNNING"
                        }, sf, indent=2)
                    
                    # Stop after 2 queries for the initial telemetry
                    if completed == 2 and "--initial-only" in sys.argv:
                        print("Initial telemetry complete.")
                        return

            cycle += 1

    finally:
        restore_sleep()

if __name__ == "__main__":
    run()
