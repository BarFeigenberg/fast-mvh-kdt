#!/usr/bin/env python3
"""
run_experiments.py - Benchmark Execution and Deterministic Metric Logging Driver

Executes multi-objective search experiments across benchmark instances,
enforces timeouts, isolates all run artifacts into deterministic run directories,
and produces structured Markdown and CSV comparison tables.
"""

import argparse
import csv
import datetime
import json
import os
import re
import subprocess
import sys
from typing import Any, Dict, List, Optional


def get_git_commit_hash() -> str:
    """Return current git commit hash, or 'uncommitted'."""
    try:
        res = subprocess.run(
            ["git", "rev-parse", "HEAD"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=True,
        )
        return res.stdout.strip()
    except Exception:
        return "unknown_or_uncommitted"


def parse_solver_stdout(stdout: str) -> Dict[str, Any]:
    """
    Parse key=value metrics emitted by solvers on stdout.
    Example line:
    algorithm=L_NAMOA_DR_MVH\tsource=1\ttarget=20\tnum_solutions=4\tnum_expansion=152...
    """
    metrics: Dict[str, Any] = {}
    for line in stdout.splitlines():
        line = line.strip()
        if "=" in line:
            tokens = line.split("\t")
            for token in tokens:
                if "=" in token:
                    k, v = token.split("=", 1)
                    k = k.strip()
                    v = v.strip()
                    # Type conversion
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


def execute_solver_run(
    cmd: List[str], timeout_s: int, output_dir: str, run_name: str
) -> Dict[str, Any]:
    """Execute solver binary, save stdout/stderr, and parse output metrics."""
    stdout_file = os.path.join(output_dir, f"{run_name}_stdout.log")
    stderr_file = os.path.join(output_dir, f"{run_name}_stderr.log")

    start_time = datetime.datetime.now()
    timed_out = False
    exit_code = 0
    stdout_text = ""
    stderr_text = ""

    try:
        proc = subprocess.run(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            timeout=timeout_s,
        )
        stdout_text = proc.stdout
        stderr_text = proc.stderr
        exit_code = proc.returncode
    except subprocess.TimeoutExpired as te:
        timed_out = True
        exit_code = -1
        stdout_text = te.stdout or "" if isinstance(te.stdout, str) else ""
        stderr_text = f"Execution timed out after {timeout_s} seconds."

    duration_s = (datetime.datetime.now() - start_time).total_seconds()

    with open(stdout_file, "w", encoding="utf-8") as f:
        f.write(stdout_text)
    with open(stderr_file, "w", encoding="utf-8") as f:
        f.write(stderr_text)

    metrics = parse_solver_stdout(stdout_text)
    metrics["run_name"] = run_name
    metrics["timed_out"] = timed_out
    metrics["exit_code"] = exit_code
    metrics["wall_clock_s"] = duration_s

    return metrics


def write_summary_tables(results: List[Dict[str, Any]], run_dir: str):
    """Write markdown and CSV summary comparison tables."""
    csv_file = os.path.join(run_dir, "metrics_summary.csv")
    md_file = os.path.join(run_dir, "metrics_summary.md")

    # Define standard field order
    standard_fields = [
        "run_name",
        "algorithm",
        "instance",
        "source",
        "target",
        "num_objectives",
        "num_solutions",
        "wall_clock_s",
        "runtime_s",
        "num_expansion",
        "num_generation",
        "cmpchk",
        "cmpupd",
        "num_dr_dominance_check",
        "num_full_dominance_check",
        "num_global_dominance_check",
        "num_good_fallback",
        "num_bad_fallback",
        "num_reinsertion",
        "timed_out",
        "exit_code",
    ]

    all_keys = set()
    for r in results:
        all_keys.update(r.keys())

    ordered_fields = [f for f in standard_fields if f in all_keys]
    remaining_fields = sorted(list(all_keys - set(ordered_fields)))
    final_fields = ordered_fields + remaining_fields

    # 1. Write CSV
    with open(csv_file, "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=final_fields)
        writer.writeheader()
        for row in results:
            writer.writerow({k: row.get(k, "") for k in final_fields})

    # 2. Write Markdown Table
    with open(md_file, "w", encoding="utf-8") as f:
        f.write("# Benchmark Evaluation Summary\n\n")
        f.write(f"- Run Directory: `{run_dir}`\n")
        f.write(f"- Generated: {datetime.datetime.now().isoformat()}\n\n")

        # Table Header
        headers = [
            "Method / Run",
            "Instance",
            "Objectives",
            "Sols",
            "Time (s)",
            "Expansions",
            "Generations",
            "cmpchk",
            "cmpupd",
            "Good FB",
            "Bad FB",
            "Status",
        ]
        f.write("| " + " | ".join(headers) + " |\n")
        f.write("| " + " | ".join([":---"] * len(headers)) + " |\n")

        for r in results:
            run_name = str(r.get("run_name", "unknown"))
            inst = str(r.get("instance", "N/A"))
            objs = str(r.get("num_objectives", "N/A"))
            sols = str(r.get("num_solutions", "0"))
            t = f"{r.get('wall_clock_s', 0.0):.3f}"
            exp = str(r.get("num_expansion", "0"))
            gen = str(r.get("num_generation", "0"))
            chk = str(r.get("cmpchk", r.get("num_dr_dominance_check", "N/A")))
            upd = str(r.get("cmpupd", "N/A"))
            g_fb = str(r.get("num_good_fallback", "0"))
            b_fb = str(r.get("num_bad_fallback", "0"))
            status = "TIMEOUT" if r.get("timed_out") else ("OK" if r.get("exit_code") == 0 else f"ERR({r.get('exit_code')})")

            row_cols = [run_name, inst, objs, sols, t, exp, gen, chk, upd, g_fb, b_fb, status]
            f.write("| " + " | ".join(row_cols) + " |\n")

    print(f"\n[OUTPUT] Metric tables generated:\n  - {csv_file}\n  - {md_file}")


def main():
    parser = argparse.ArgumentParser(
        description="Run multi-objective benchmark experiments with isolated deterministic logging."
    )
    parser.add_argument("--baseline-bin", help="Path to reference baseline binary")
    parser.add_argument("--kdt-bin", help="Path to fast_mvh binary")
    parser.add_argument("--maps", nargs="+", required=True, help="Path(s) to instance map directories")
    parser.add_argument("--queries", nargs="+", default=["1 20"], help="Start and goal pairs, e.g. '1 20' '1 50'")
    parser.add_argument("--objectives", nargs="+", type=int, default=[0, 1, 2], help="Objective coordinates")
    parser.add_argument("--timeout", type=int, default=60, help="Per-run timeout in seconds")
    parser.add_argument("--tag", default="experiment", help="Descriptive tag for run")

    args = parser.parse_args()

    # Create timestamped run directory
    timestamp = datetime.datetime.now().strftime("%Y-%m-%d_%H%M%S")
    run_dir = os.path.join("benchmarks", "runs", f"{timestamp}_{args.tag}")
    os.makedirs(run_dir, exist_ok=True)

    # Save run metadata
    run_info = {
        "timestamp": timestamp,
        "git_commit": get_git_commit_hash(),
        "cli_arguments": sys.argv,
        "args_parsed": vars(args),
    }
    with open(os.path.join(run_dir, "run_info.json"), "w", encoding="utf-8") as f:
        json.dump(run_info, f, indent=2)

    print(f"=== Starting Benchmark Suite ===")
    print(f"Artifacts isolated to: {run_dir}")
    print(f"Git commit:            {run_info['git_commit']}")

    results: List[Dict[str, Any]] = []

    for map_dir in args.maps:
        map_name = os.path.basename(os.path.normpath(map_dir))
        for q in args.queries:
            parts = q.strip().split()
            if len(parts) != 2:
                continue
            start_node, goal_node = parts[0], parts[1]
            obj_args = [str(o) for o in args.objectives]
            obj_count = len(args.objectives)

            # 1. Run Baseline (if available)
            if args.baseline_bin and os.path.exists(args.baseline_bin):
                run_name = f"baseline_{map_name}_s{start_node}_g{goal_node}_d{obj_count}"
                sol_file = os.path.join(run_dir, f"{run_name}_sols.txt")
                cmd = [
                    args.baseline_bin,
                    "--map", map_dir,
                    "--start", start_node,
                    "--goal", goal_node,
                    "--algorithm", "L_NAMOA_DR_MVH",
                    "--objectives", *obj_args,
                    "--cutoffTime", str(args.timeout),
                    "--logging_file", os.path.join(run_dir, f"{run_name}_log"),
                ]
                print(f"\nRunning Baseline: {run_name}")
                res = execute_solver_run(cmd, args.timeout + 5, run_dir, run_name)
                res["instance"] = map_name
                res["source"] = start_node
                res["target"] = goal_node
                res["num_objectives"] = obj_count
                results.append(res)

            # 2. Run Fast MVH (KD-Tree)
            if args.kdt_bin and os.path.exists(args.kdt_bin):
                run_name = f"kdt_{map_name}_s{start_node}_g{goal_node}_d{obj_count}"
                sol_file = os.path.join(run_dir, f"{run_name}_sols.txt")
                cmd = [
                    args.kdt_bin,
                    "--map", map_dir,
                    "--start", start_node,
                    "--goal", goal_node,
                    "--objectives", *obj_args,
                    "--timeout", str(args.timeout),
                    "--sol-out", sol_file,
                ]
                print(f"\nRunning Fast MVH: {run_name}")
                res = execute_solver_run(cmd, args.timeout + 5, run_dir, run_name)
                res["instance"] = map_name
                res["source"] = start_node
                res["target"] = goal_node
                res["num_objectives"] = obj_count
                results.append(res)

    write_summary_tables(results, run_dir)
    print(f"\n[FINISHED] Benchmark sweep complete.")


if __name__ == "__main__":
    main()
