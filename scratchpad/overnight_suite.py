#!/usr/bin/env python3
"""
overnight_suite.py - Continuous Autonomous Benchmark Suite with RAM Protection

Executes multi-objective benchmark queries across varying dimensions and graph topologies.
Monitors memory consumption in real time to prevent system RAM overdose.
Logs deterministic metrics to benchmarks/runs/ and benchmarks/golden_results/.
Runs continuously across expanding graph sizes and seeds.
"""

import csv
import datetime
import json
import os
import subprocess
import sys
import time
from typing import Any, Dict, List, Optional
import psutil

# Safe Resource Thresholds
MAX_PROCESS_RAM_MB = 1500.0   # 1.5 GB cap per solver subprocess
MIN_SYSTEM_FREE_RAM_MB = 400.0 # Emergency stop if whole system available RAM drops below 400 MB
DEFAULT_TIMEOUT_S = 120       # 120 seconds timeout per query

def get_git_commit_hash() -> str:
    try:
        res = subprocess.run(
            ["git", "rev-parse", "HEAD"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=True
        )
        return res.stdout.strip()
    except Exception:
        return "unknown"

def parse_solver_stdout(stdout: str) -> Dict[str, Any]:
    metrics: Dict[str, Any] = {}
    for line in stdout.splitlines():
        line = line.strip()
        if "=" in line:
            tokens = line.split("\t")
            for token in tokens:
                if "=" in token:
                    k, v = token.split("=", 1)
                    k, v = k.strip(), v.strip()
                    try:
                        if "." in v:
                            metrics[k] = float(v)
                        else:
                            metrics[k] = int(v)
                    except ValueError:
                        if v.lower() == "true":
                            metrics[k] = True
                        elif v.lower() == "false":
                            metrics[k] = False
                        else:
                            metrics[k] = v
    return metrics

def update_heartbeat(status_file: str, data: Dict[str, Any]):
    try:
        vm = psutil.virtual_memory()
        data["system_ram_total_gb"] = round(vm.total / (1024**3), 2)
        data["system_ram_avail_gb"] = round(vm.available / (1024**3), 2)
        data["system_ram_pct"] = vm.percent
        data["system_cpu_pct"] = psutil.cpu_percent(interval=None)
        data["heartbeat_timestamp"] = datetime.datetime.now().isoformat()
        with open(status_file, "w", encoding="utf-8") as f:
            json.dump(data, f, indent=2)
    except Exception as e:
        print(f"[WARN] Failed to write heartbeat: {e}", file=sys.stderr)

def execute_with_ram_guard(
    cmd: List[str],
    timeout_s: int,
    stdout_file: str,
    stderr_file: str
) -> Dict[str, Any]:
    start_time = time.time()
    ram_exceeded = False
    timed_out = False
    exit_code = 0
    max_rss_mb = 0.0

    proc = subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )

    try:
        ps_proc = psutil.Process(proc.pid)
    except Exception:
        ps_proc = None

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

        # Check memory
        if ps_proc is not None:
            try:
                mem_info = ps_proc.memory_info()
                rss_mb = mem_info.rss / (1024 * 1024)
                if rss_mb > max_rss_mb:
                    max_rss_mb = rss_mb

                # 1. Process RAM cap
                if rss_mb > MAX_PROCESS_RAM_MB:
                    ram_exceeded = True
                    proc.kill()
                    break

                # 2. Host free memory emergency guard
                vm = psutil.virtual_memory()
                avail_mb = vm.available / (1024 * 1024)
                if avail_mb < MIN_SYSTEM_FREE_RAM_MB:
                    ram_exceeded = True
                    proc.kill()
                    break
            except (psutil.NoSuchProcess, psutil.AccessDenied):
                break

        time.sleep(0.20)

    stdout_text, stderr_text = proc.communicate()
    duration_s = time.time() - start_time

    if ram_exceeded:
        stderr_text += f"\n[ERROR] Terminated: Exceeded RAM limit (proc_rss={max_rss_mb:.1f}MB, sys_avail={psutil.virtual_memory().available/(1024*1024):.1f}MB)."
        exit_code = -2
    elif timed_out:
        stderr_text += f"\n[ERROR] Terminated: Execution timed out after {timeout_s} seconds."
        exit_code = -1

    with open(stdout_file, "w", encoding="utf-8") as f:
        f.write(stdout_text)
    with open(stderr_file, "w", encoding="utf-8") as f:
        f.write(stderr_text)

    metrics = parse_solver_stdout(stdout_text)
    metrics["wall_clock_s"] = duration_s
    metrics["max_rss_mb"] = max_rss_mb
    metrics["timed_out"] = timed_out
    metrics["ram_exceeded"] = ram_exceeded
    metrics["exit_code"] = exit_code
    return metrics

def append_to_summary_tables(result: Dict[str, Any], run_dir: str, all_results: List[Dict[str, Any]]):
    csv_file = os.path.join(run_dir, "metrics_summary.csv")
    md_file = os.path.join(run_dir, "metrics_summary.md")

    standard_fields = [
        "timestamp", "run_name", "algorithm", "instance", "source", "target",
        "num_objectives", "num_solutions", "wall_clock_s", "runtime_s", "max_rss_mb",
        "num_expansion", "num_generation", "cmpchk", "cmpupd",
        "num_dr_dominance_check", "num_full_dominance_check", "num_global_dominance_check",
        "num_good_fallback", "num_bad_fallback", "num_reinsertion",
        "timed_out", "ram_exceeded", "exit_code"
    ]

    all_keys = set()
    for r in all_results:
        all_keys.update(r.keys())
    ordered_fields = [f for f in standard_fields if f in all_keys]
    remaining = sorted(list(all_keys - set(ordered_fields)))
    fields = ordered_fields + remaining

    with open(csv_file, "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fields)
        writer.writeheader()
        for r in all_results:
            writer.writerow({k: r.get(k, "") for k in fields})

    with open(md_file, "w", encoding="utf-8") as f:
        f.write("# Overnight Continuous Benchmark Suite Summary\n\n")
        f.write(f"- Updated: {datetime.datetime.now().isoformat()}\n")
        f.write(f"- Total Queries Completed: {len(all_results)}\n\n")
        
        headers = [
            "Method / Algorithm", "Instance", "Dims", "Sols", "Time (s)",
            "RAM (MB)", "Expansions", "Generations", "cmpchk (DR)", "Full Chk",
            "Good FB", "Bad FB", "Status"
        ]
        f.write("| " + " | ".join(headers) + " |\n")
        f.write("| " + " | ".join([":---"] * len(headers)) + " |\n")

        # Display last 200 rows in markdown to keep it clean
        display_results = all_results[-200:]
        for r in display_results:
            algo = str(r.get("algorithm", r.get("run_name", "N/A")))
            inst = str(r.get("instance", "N/A"))
            dims = str(r.get("num_objectives", "N/A"))
            sols = str(r.get("num_solutions", "0"))
            t = f"{r.get('wall_clock_s', 0.0):.3f}"
            ram = f"{r.get('max_rss_mb', 0.0):.1f}"
            exp = str(r.get("num_expansion", "0"))
            gen = str(r.get("num_generation", "0"))
            chk = str(r.get("num_dr_dominance_check", r.get("cmpchk", "N/A")))
            fchk = str(r.get("num_full_dominance_check", "0"))
            g_fb = str(r.get("num_good_fallback", "0"))
            b_fb = str(r.get("num_bad_fallback", "0"))

            if r.get("ram_exceeded"):
                status = "RAM_EXCEEDED"
            elif r.get("timed_out"):
                status = "TIMEOUT"
            elif r.get("exit_code") == 0:
                status = "OK"
            else:
                status = f"ERR({r.get('exit_code')})"

            row = [algo, inst, dims, sols, t, ram, exp, gen, chk, fchk, g_fb, b_fb, status]
            f.write("| " + " | ".join(row) + " |\n")

def build_workload_queue(cycle: int = 1) -> List[tuple]:
    workload = []
    synth_dir = "baselines/bridging-mvh-dr/resources/synthetic_graph"
    
    # 1. Baseline Synthetic Graph suite
    workload.extend([
        (synth_dir, 1, 20, [0, 1, 4], ["L_NAMOA_DR_MVH", "NAMOA_DR", "NAMOA"], 30),
        (synth_dir, 10, 77, [0, 1, 3, 4], ["L_NAMOA_DR_MVH", "NAMOA_DR", "NAMOA"], 30),
        (synth_dir, 43, 96, [0, 1, 2, 3, 4], ["L_NAMOA_DR_MVH", "NAMOA_DR", "NAMOA"], 30),
        (synth_dir, 1, 96, [0, 1, 2, 3, 4], ["L_NAMOA_DR_MVH", "NAMOA_DR", "NAMOA"], 30),
        (synth_dir, 15, 85, [0, 1, 2, 3, 4], ["L_NAMOA_DR_MVH", "NAMOA_DR", "NAMOA"], 30),
    ])

    # 2. Grids 10x10 (100 nodes, 3, 4, 5 objectives)
    for d in [3, 4, 5]:
        map_path = f"benchmarks/instances/grid_10x10_d{d}_tradeoff"
        objs = list(range(d))
        algos = ["L_NAMOA_DR_MVH", "NAMOA_DR", "NAMOA"]
        workload.extend([
            (map_path, 1, 100, objs, algos, 60),
            (map_path, 1, 50, objs, algos, 60),
            (map_path, 10, 91, objs, algos, 60),
            (map_path, 25, 75, objs, algos, 60),
        ])

    # 3. Geometric Graphs 100 nodes (3, 4, 5 objectives)
    for d in [3, 4, 5]:
        map_path = f"benchmarks/instances/geom_n100_d{d}_tradeoff"
        objs = list(range(d))
        algos = ["L_NAMOA_DR_MVH", "NAMOA_DR", "NAMOA"]
        workload.extend([
            (map_path, 1, 100, objs, algos, 60),
            (map_path, 5, 95, objs, algos, 60),
            (map_path, 20, 80, objs, algos, 60),
        ])

    # 4. Grids 15x15 (225 nodes, 3, 4, 5 objectives)
    for d in [3, 4, 5]:
        map_path = f"benchmarks/instances/grid_15x15_d{d}_tradeoff"
        objs = list(range(d))
        algos = ["L_NAMOA_DR_MVH", "NAMOA_DR"]
        workload.extend([
            (map_path, 1, 225, objs, algos, 90),
            (map_path, 15, 211, objs, algos, 90),
            (map_path, 50, 175, objs, algos, 90),
        ])

    # 5. Geometric Graphs 250 nodes (3, 4 objectives)
    for d in [3, 4]:
        map_path = f"benchmarks/instances/geom_n250_d{d}_tradeoff"
        objs = list(range(d))
        algos = ["L_NAMOA_DR_MVH", "NAMOA_DR"]
        workload.extend([
            (map_path, 1, 250, objs, algos, 90),
            (map_path, 10, 240, objs, algos, 90),
        ])

    # 6. Grid 20x20 (400 nodes, 3, 4 objectives)
    for d in [3, 4]:
        map_path = f"benchmarks/instances/grid_20x20_d{d}_tradeoff"
        objs = list(range(d))
        algos = ["L_NAMOA_DR_MVH", "NAMOA_DR"]
        workload.extend([
            (map_path, 1, 400, objs, algos, 120),
            (map_path, 20, 381, objs, algos, 120),
            (map_path, 100, 300, objs, algos, 120),
        ])

    # 7. Grid 25x25 (625 nodes, 3 objectives: tradeoff and uniform)
    for corr in ["tradeoff", "uniform"]:
        map_path = f"benchmarks/instances/grid_25x25_d3_{corr}"
        objs = [0, 1, 2]
        algos = ["L_NAMOA_DR_MVH", "NAMOA_DR"]
        workload.extend([
            (map_path, 1, 625, objs, algos, 120),
            (map_path, 25, 601, objs, algos, 120),
            (map_path, 1, 300, objs, algos, 90),
        ])

    # 8. Geometric Graphs 500 nodes (3 objectives)
    map_path = "benchmarks/instances/geom_n500_d3_tradeoff"
    objs = [0, 1, 2]
    algos = ["L_NAMOA_DR_MVH", "NAMOA_DR"]
    workload.extend([
        (map_path, 1, 500, objs, algos, 150),
        (map_path, 50, 450, objs, algos, 150),
        (map_path, 10, 200, objs, algos, 90),
    ])

    # 9. Large Grids 30x30, 40x40, 50x50 (3 objectives)
    for sz in [30, 40, 50]:
        map_path = f"benchmarks/instances/grid_{sz}x{sz}_d3_tradeoff"
        num_n = sz * sz
        objs = [0, 1, 2]
        algos = ["L_NAMOA_DR_MVH", "NAMOA_DR"]
        workload.extend([
            (map_path, 1, num_n, objs, algos, 180),
            (map_path, 1, num_n // 2, objs, algos, 120),
            (map_path, sz, num_n - sz + 1, objs, algos, 180),
            (map_path, 10, num_n - 10, objs, algos, 150),
        ])

    return workload

def main():
    solver_bin = os.path.abspath("build/baselines/bridging-mvh-dr/Release/MultivaluedHeuristicSearch.exe")
    if not os.path.exists(solver_bin):
        print(f"[FATAL] Solver binary not found: {solver_bin}", file=sys.stderr)
        sys.exit(1)

    timestamp = datetime.datetime.now().strftime("%Y-%m-%d_%H%M%S")
    run_dir = os.path.join("benchmarks", "runs", f"{timestamp}_overnight_continuous")
    os.makedirs(run_dir, exist_ok=True)
    status_file = os.path.join("scratchpad", "runner_status.json")

    run_info = {
        "start_time": timestamp,
        "git_commit": get_git_commit_hash(),
        "solver_bin": solver_bin,
        "max_process_ram_mb": MAX_PROCESS_RAM_MB,
        "min_system_free_ram_mb": MIN_SYSTEM_FREE_RAM_MB,
        "default_timeout_s": DEFAULT_TIMEOUT_S
    }
    with open(os.path.join(run_dir, "run_info.json"), "w", encoding="utf-8") as f:
        json.dump(run_info, f, indent=2)

    print(f"=== OVERNIGHT CONTINUOUS BENCHMARK SUITE STARTED ===")
    print(f"Artifact directory: {run_dir}")

    all_results: List[Dict[str, Any]] = []
    completed = 0
    cycle = 1
    start_suite_time = time.time()

    # Continuous loop across workload generations
    while True:
        workload = build_workload_queue(cycle)
        total_tasks_in_cycle = sum(len(entry[4]) for entry in workload)
        print(f"\n--- Starting Workload Cycle {cycle} ({len(workload)} cases, {total_tasks_in_cycle} solver runs) ---", flush=True)

        for map_path, s, g, objs, algos, timeout_s in workload:
            map_name = os.path.basename(os.path.normpath(map_path))
            d = len(objs)
            obj_strs = [str(x) for x in objs]

            for algo in algos:
                completed += 1
                run_name = f"{algo}_{map_name}_s{s}_g{g}_d{d}_c{cycle}"
                stdout_file = os.path.join(run_dir, f"{run_name}_stdout.log")
                stderr_file = os.path.join(run_dir, f"{run_name}_stderr.log")

                update_heartbeat(status_file, {
                    "cycle": cycle,
                    "run_dir": run_dir,
                    "current_run": run_name,
                    "algorithm": algo,
                    "instance": map_name,
                    "source": s,
                    "goal": g,
                    "objectives": objs,
                    "completed_tasks": completed - 1,
                    "elapsed_suite_s": time.time() - start_suite_time
                })

                cmd = [
                    solver_bin,
                    "--map", map_path,
                    "--start", str(s),
                    "--goal", str(g),
                    "--algorithm", algo,
                    "--objectives", *obj_strs,
                    "--cutoffTime", str(timeout_s)
                ]

                print(f"[Cycle {cycle} | Run #{completed}] {run_name} (timeout={timeout_s}s)...", flush=True)
                res = execute_with_ram_guard(cmd, timeout_s + 5, stdout_file, stderr_file)

                res["timestamp"] = datetime.datetime.now().isoformat()
                res["cycle"] = cycle
                res["run_name"] = run_name
                res["algorithm"] = algo
                res["instance"] = map_name
                res["source"] = s
                res["target"] = g
                res["num_objectives"] = d

                all_results.append(res)
                append_to_summary_tables(res, run_dir, all_results)

                if res.get("exit_code") == 0 and not res.get("timed_out") and not res.get("ram_exceeded"):
                    sols = res.get("num_solutions", 0)
                    exp = res.get("num_expansion", 0)
                    gen = res.get("num_generation", 0)
                    chk = res.get("num_dr_dominance_check", 0)
                    print(f"    -> OK: sols={sols}, exp={exp}, gen={gen}, chk={chk}, wall={res['wall_clock_s']:.2f}s, ram={res['max_rss_mb']:.1f}MB", flush=True)
                    
                    golden_file = os.path.join("benchmarks", "golden_results", f"golden_{map_name}_s{s}_g{g}_d{d}.json")
                    if not os.path.exists(golden_file) and algo == "L_NAMOA_DR_MVH":
                        with open(golden_file, "w", encoding="utf-8") as gf:
                            json.dump(res, gf, indent=2)
                else:
                    status_str = "RAM_EXCEEDED" if res.get("ram_exceeded") else ("TIMEOUT" if res.get("timed_out") else f"EXIT_{res.get('exit_code')}")
                    print(f"    -> {status_str}: wall={res['wall_clock_s']:.2f}s, ram={res['max_rss_mb']:.1f}MB", flush=True)

        cycle += 1

if __name__ == "__main__":
    main()
