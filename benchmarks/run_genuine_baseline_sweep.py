#!/usr/bin/env python3
"""
Targeted Genuine Baseline Oracle Sweep
Fast MVH with K-d Tree Dominance Checking in Multi-Objective Search

Executes ONLY the genuine reference oracle:
  build/baselines/bridging-mvh-dr/Release/MultivaluedHeuristicSearch.exe -a L_NAMOA_DR_MVH
Across the 28 canonical instances with Cascade Skip and 300.0s timeout.
"""

import os
import sys
import time
import csv
import subprocess
from datetime import datetime
from typing import Dict, Any

sys.stdout.reconfigure(encoding='utf-8')
sys.path.insert(0, os.path.abspath("."))

BASELINE_BIN = os.path.abspath("build/baselines/bridging-mvh-dr/Release/MultivaluedHeuristicSearch.exe")
OUT_CSV = os.path.abspath("benchmarks/runs/genuine_baseline_results.csv")
LOG_PATH = os.path.abspath("benchmarks/runs/live_baseline_progress.log")

from benchmarks.run_overnight_rigorous import INSTANCES

# Instances where FAST_MVH_Full timed out (>300s) -> Cascade Skip Rule applies
CASCADE_SKIP_INSTANCES = {
    'Panda_Q0002_Roi',
    'bay_8d_2000 (s1500)'
}

TIMEOUT_S = 300  # 5-minute timeout per instance

FIELDS = [
    'Timestamp', 'Instance', 'Domain', 'M', 'Rho', 'Config', 'Status',
    'Runtime_s', 'WallTime_s', 'Solutions', 'Expansions', 'Generations',
    'NumFullChecks', 'CmpDr', 'GoodFallback', 'BadFallback', 'Reinsertions'
]

def log_progress(msg: str):
    ts = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    formatted = f"[{ts}] {msg}"
    print(formatted)
    sys.stdout.flush()
    try:
        with open(LOG_PATH, "a", encoding="utf-8") as f:
            f.write(formatted + "\n")
    except Exception:
        pass

def parse_output(stdout: str) -> Dict[str, Any]:
    res = {}
    for line in stdout.splitlines():
        line = line.strip()
        if "=" in line:
            for token in line.split("\t"):
                if "=" in token:
                    k, v = token.split("=", 1)
                    k, v = k.strip(), v.strip()
                    try:
                        res[k] = float(v) if ("." in v or "e" in v.lower()) else int(v)
                    except ValueError:
                        res[k] = v
    return res

def flush_row(row: Dict[str, Any]):
    exists = os.path.exists(OUT_CSV)
    with open(OUT_CSV, 'a', newline='', encoding='utf-8') as f:
        w = csv.DictWriter(f, fieldnames=FIELDS)
        if not exists:
            w.writeheader()
        w.writerow(row)

def main():
    print("=" * 80)
    print("TARGETED GENUINE BASELINE ORACLE SWEEP")
    print(f"Timestamp:       {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print(f"Baseline Binary: {BASELINE_BIN}")
    print(f"Output CSV:      {OUT_CSV}")
    print(f"Timeout:         {TIMEOUT_S}s per instance")
    print("=" * 80)

    total = len(INSTANCES)

    for idx, inst in enumerate(INSTANCES, 1):
        name = inst['name']
        domain = inst['domain']
        M = inst['M']
        rho = inst['rho']

        # Check Cascade Skip
        if name in CASCADE_SKIP_INSTANCES:
            log_progress(f"[{idx}/{total}] [{name}] Baseline -> CASCADE SKIPPED (FAST_MVH_Full timed out) | Marked >{TIMEOUT_S}s (T.O.)")
            row = {
                'Timestamp': datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
                'Instance': name,
                'Domain': domain,
                'M': M,
                'Rho': rho,
                'Config': 'Baseline',
                'Status': 'TIMEOUT',
                'Runtime_s': f"{TIMEOUT_S:.4f}",
                'WallTime_s': "0.0000",
                'Solutions': 0,
                'Expansions': 0,
                'Generations': 0,
                'NumFullChecks': 0,
                'CmpDr': 0,
                'GoodFallback': 0,
                'BadFallback': 0,
                'Reinsertions': 0
            }
            flush_row(row)
            continue

        log_progress(f"[{idx}/{total}] [{name}] Baseline -> EXECUTING (MultivaluedHeuristicSearch.exe, Timeout: {TIMEOUT_S}s) ...")

        cmd = [
            BASELINE_BIN,
            "-m", os.path.abspath(inst['map']),
            "-s", str(inst['start']),
            "-g", str(inst['goal']),
            "--mvh", os.path.abspath(inst['mvh']),
            "-a", "L_NAMOA_DR_MVH",
            "-t", str(TIMEOUT_S)
        ]
        if 'objectives' in inst and inst['objectives']:
            cmd.extend(["--objectives", *inst['objectives']])

        t0 = time.time()
        try:
            proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=TIMEOUT_S + 15)
            t1 = time.time()
            wall_time = t1 - t0

            parsed = parse_output(proc.stdout)
            runtime = parsed.get('runtime_s', wall_time)
            timed_out = (parsed.get('time_limit_reached', 0) == 1) or (wall_time >= TIMEOUT_S and parsed.get('num_solutions', 0) == 0)

            status = "COMPLETED" if not timed_out else "TIMEOUT"
            sols = parsed.get('num_solutions', 0)
            exp = parsed.get('num_expansion', 0)
            gen = parsed.get('num_generation', 0)

            row = {
                'Timestamp': datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
                'Instance': name,
                'Domain': domain,
                'M': M,
                'Rho': rho,
                'Config': 'Baseline',
                'Status': status,
                'Runtime_s': f"{runtime:.4f}",
                'WallTime_s': f"{wall_time:.4f}",
                'Solutions': sols,
                'Expansions': exp,
                'Generations': gen,
                'NumFullChecks': parsed.get('num_full_dominance_check', 0),
                'CmpDr': parsed.get('num_dr_dominance_check', 0),
                'GoodFallback': parsed.get('num_good_fallback', 0),
                'BadFallback': parsed.get('num_bad_fallback', 0),
                'Reinsertions': parsed.get('num_reinsertion', 0)
            }
            flush_row(row)
            log_progress(f"[{idx}/{total}] [{name}] Baseline -> {status} / {runtime:.3f}s | Exp: {exp:,} | Sols: {sols:,}")

        except subprocess.TimeoutExpired:
            t1 = time.time()
            row = {
                'Timestamp': datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
                'Instance': name,
                'Domain': domain,
                'M': M,
                'Rho': rho,
                'Config': 'Baseline',
                'Status': 'TIMEOUT',
                'Runtime_s': f"{TIMEOUT_S:.4f}",
                'WallTime_s': f"{(t1 - t0):.4f}",
                'Solutions': 0,
                'Expansions': 0,
                'Generations': 0,
                'NumFullChecks': 0,
                'CmpDr': 0,
                'GoodFallback': 0,
                'BadFallback': 0,
                'Reinsertions': 0
            }
            flush_row(row)
            log_progress(f"[{idx}/{total}] [{name}] Baseline -> TIMEOUT (> {TIMEOUT_S}s)")

        # Mandatory 3.0s thermal cooldown between runs
        time.sleep(3.0)

    log_progress("================================================================================")
    log_progress("GENUINE BASELINE SWEEP COMPLETED SUCCESSFULLY")
    log_progress(f"Output CSV: {OUT_CSV}")
    log_progress("================================================================================")

if __name__ == "__main__":
    main()
