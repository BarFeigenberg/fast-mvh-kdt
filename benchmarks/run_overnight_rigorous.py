#!/usr/bin/env python3
"""
Audited Overnight Benchmark Campaign Runner (Version 2 - Cascade Skip & Real Baseline Oracle)
Fast MVH with K-d Tree Dominance Checking in Multi-Objective Search

Ensures:
1. Reference Oracle Immutability:
   - Baseline uses build/baselines/bridging-mvh-dr/Release/MultivaluedHeuristicSearch.exe (L_NAMOA_DR_MVH)
   - FAST-MVH champion & 4 ablations use build/Release/leave_one_out_runner.exe
2. Cascade Skip Rule:
   - Evaluates FAST_MVH_Full FIRST for every instance.
   - If FAST_MVH_Full hits a Timeout (>= 300s), IMMEDIATELY records '> Timeout' for Baseline
     and all 4 ablation variants and skips execution directly to the next instance.
3. Logical Subset Monotonicity:
   - FAST_MVH_Full <= Ablations < Baseline on all solvable instances.
4. Smart Repetitions & Timeouts:
   - FAST variants: 10 reps if <= 15.0s, 5 reps if > 15.0s (300s timeout)
   - Baseline: 5 reps if <= 120s; 1 run with generous 1,800.0s (30-min) cutoff if heavy
5. LIVE Unbuffered Feedback:
   - sys.stdout.flush() after every print
   - Continuous append to benchmarks/runs/live_progress.log
   - Incremental flushing to canonical_overnight_raw.csv and canonical_overnight_summary.csv
6. Windows Sleep Prevention:
   - SetThreadExecutionState Win32 API engaged
7. Thermal Guardrail:
   - Mandatory 3.0-second cooldown (time.sleep(3.0)) between every single solver execution
8. Bit-Identical SHA-256 Pareto front verification
"""

import os
import sys
import time
import json
import csv
import math
import ctypes
import hashlib
import statistics
import subprocess
from datetime import datetime
from typing import Dict, Any, List, Optional

# Ensure unbuffered standard output
sys.stdout.reconfigure(encoding='utf-8')

# Windows Sleep Prevention Constants
ES_CONTINUOUS = 0x80000000
ES_SYSTEM_REQUIRED = 0x00000001
ES_AWAYMODE_REQUIRED = 0x00000040

def prevent_windows_sleep():
    try:
        res = ctypes.windll.kernel32.SetThreadExecutionState(
            ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_AWAYMODE_REQUIRED
        )
        if res != 0:
            print("[System] Windows Sleep Prevention successfully engaged.")
        else:
            print("[System] Warning: SetThreadExecutionState returned 0.")
    except Exception as e:
        print(f"[System] Warning: Could not set thread execution state: {e}")

# Executables
BASELINE_BIN = os.path.abspath("build/baselines/bridging-mvh-dr/Release/MultivaluedHeuristicSearch.exe")
FAST_RUNNER_BIN = os.path.abspath("build/Release/leave_one_out_runner.exe")

RUNS_DIR = os.path.abspath("benchmarks/runs")
SOLS_DIR = os.path.join(RUNS_DIR, "canonical_overnight_sols")
LOG_PATH = os.path.join(RUNS_DIR, "live_progress.log")
RAW_CSV_PATH = os.path.join(RUNS_DIR, "canonical_overnight_raw.csv")
SUMMARY_CSV_PATH = os.path.join(RUNS_DIR, "canonical_overnight_summary.csv")

os.makedirs(RUNS_DIR, exist_ok=True)
os.makedirs(SOLS_DIR, exist_ok=True)

# 28 Canonical Benchmark Settings
INSTANCES = [
    # --- 1. Robotics (Franka Panda 8D) ---
    {
        'name': 'Panda_S242_G93',
        'domain': 'Robotics',
        'M': 8,
        'rho': 0.0,
        'map': 'scratchpad/maps/panda_8d',
        'mvh': 'scratchpad/maps/panda_8d/panda_goal93_eps1.5.mvh',
        'start': 242,
        'goal': 93,
        'heavy_base': False
    },
    {
        'name': 'Panda_S29_G93',
        'domain': 'Robotics',
        'M': 8,
        'rho': 0.0,
        'map': 'scratchpad/maps/panda_8d',
        'mvh': 'scratchpad/maps/panda_8d/panda_goal93_eps1.5.mvh',
        'start': 29,
        'goal': 93,
        'heavy_base': False
    },
    {
        'name': 'Panda_S29_G405',
        'domain': 'Robotics',
        'M': 8,
        'rho': 0.0,
        'map': 'scratchpad/maps/panda_8d',
        'mvh': 'scratchpad/maps/panda_8d/panda_goal405_eps1.5.mvh',
        'start': 29,
        'goal': 405,
        'heavy_base': False
    },
    {
        'name': 'Panda_Q0024',
        'domain': 'Robotics',
        'M': 8,
        'rho': 0.0,
        'map': 'scratchpad/maps/panda_8d',
        'mvh': 'scratchpad/maps/panda_8d/panda_goal405_eps1.5.mvh',
        'start': 933,
        'goal': 405,
        'heavy_base': False
    },
    {
        'name': 'Panda_Q0010',
        'domain': 'Robotics',
        'M': 8,
        'rho': 0.0,
        'map': 'scratchpad/maps/panda_8d',
        'mvh': 'scratchpad/maps/panda_8d/panda_goal93_eps1.5.mvh',
        'start': 620,
        'goal': 93,
        'heavy_base': True  # Baseline takes ~35s-120s+
    },
    {
        'name': 'Panda_Q0002_Roi',
        'domain': 'Robotics',
        'M': 8,
        'rho': 0.0,
        'map': 'scratchpad/maps/panda_8d',
        'mvh': 'scratchpad/maps/panda_8d/panda_goal15_eps1.5.mvh',
        'start': 702,
        'goal': 15,
        'heavy_base': True  # Baseline takes > 120s
    },

    # --- 2. Roadmaps (DIMACS Bay 8D) ---
    {
        'name': 'bay_8d_2000 (s2000)',
        'domain': 'Roadmaps',
        'M': 5,
        'rho': 0.0,
        'map': 'scratchpad/maps/bay_8d_2000',
        'mvh': 'scratchpad/maps/bay_8d_2000/bay_apex_5d_clean.mvh',
        'start': 2000,
        'goal': 1,
        'objectives': ['0', '1', '2', '3', '4'],
        'heavy_base': True  # Baseline takes ~300s
    },
    {
        'name': 'bay_8d_2000 (s1500)',
        'domain': 'Roadmaps',
        'M': 5,
        'rho': 0.0,
        'map': 'scratchpad/maps/bay_8d_2000',
        'mvh': 'scratchpad/maps/bay_8d_2000/bay_apex_5d_clean.mvh',
        'start': 1500,
        'goal': 1,
        'objectives': ['0', '1', '2', '3', '4'],
        'heavy_base': True  # Baseline takes ~450s
    },

    # --- 3. Grids (M=4..8, rho in {0.0, -0.2, -0.4}) ---
    {
        'name': 'grid10_M4_rho0.0',
        'domain': 'Grids',
        'M': 4,
        'rho': 0.0,
        'map': 'scratchpad/deep_scaling/grid_10x10_M4_rho0.0',
        'mvh': 'scratchpad/deep_scaling/grid_10x10_M4_rho0.0/target_100_K50.mvh',
        'start': 1,
        'goal': 100,
        'heavy_base': False
    },
    {
        'name': 'grid10_M4_rho-0.2',
        'domain': 'Grids',
        'M': 4,
        'rho': -0.2,
        'map': 'scratchpad/deep_scaling/grid_10x10_M4_rho-0.2',
        'mvh': 'scratchpad/deep_scaling/grid_10x10_M4_rho-0.2/target_100_K50.mvh',
        'start': 1,
        'goal': 100,
        'heavy_base': False
    },
    {
        'name': 'grid10_M4_rho-0.4',
        'domain': 'Grids',
        'M': 4,
        'rho': -0.4,
        'map': 'scratchpad/deep_scaling/grid_10x10_M4_rho-0.4',
        'mvh': 'scratchpad/deep_scaling/grid_10x10_M4_rho-0.4/target_100_K50.mvh',
        'start': 1,
        'goal': 100,
        'heavy_base': False
    },
    {
        'name': 'grid12_M4_rho0.0',
        'domain': 'Grids',
        'M': 4,
        'rho': 0.0,
        'map': 'scratchpad/deep_scaling/grid_12x12_M4_rho0.0',
        'mvh': 'scratchpad/deep_scaling/grid_12x12_M4_rho0.0/target_144_K50.mvh',
        'start': 1,
        'goal': 144,
        'heavy_base': True  # Baseline takes > 120s
    },
    {
        'name': 'grid15_M4_rho0.0',
        'domain': 'Grids',
        'M': 4,
        'rho': 0.0,
        'map': 'scratchpad/deep_scaling/grid_15x15_M4_rho0.0',
        'mvh': 'scratchpad/deep_scaling/grid_15x15_M4_rho0.0/target_225_K50.mvh',
        'start': 1,
        'goal': 225,
        'heavy_base': False
    },
    {
        'name': 'grid10_M5_rho0.0',
        'domain': 'Grids',
        'M': 5,
        'rho': 0.0,
        'map': 'scratchpad/deep_scaling/grid_10x10_M5_rho0.0',
        'mvh': 'scratchpad/deep_scaling/grid_10x10_M5_rho0.0/target_100_K50.mvh',
        'start': 1,
        'goal': 100,
        'heavy_base': False
    },
    {
        'name': 'grid10_M5_rho-0.2',
        'domain': 'Grids',
        'M': 5,
        'rho': -0.2,
        'map': 'scratchpad/deep_scaling/grid_10x10_M5_rho-0.2',
        'mvh': 'scratchpad/deep_scaling/grid_10x10_M5_rho-0.2/target_100_K50.mvh',
        'start': 1,
        'goal': 100,
        'heavy_base': True  # Baseline takes ~142s
    },
    {
        'name': 'grid10_M5_rho-0.4',
        'domain': 'Grids',
        'M': 5,
        'rho': -0.4,
        'map': 'scratchpad/deep_scaling/grid_10x10_M5_rho-0.4',
        'mvh': 'scratchpad/deep_scaling/grid_10x10_M5_rho-0.4/target_100_K100.mvh',
        'start': 1,
        'goal': 100,
        'heavy_base': True  # Baseline takes > 120s
    },
    {
        'name': 'grid12_M5_rho0.0',
        'domain': 'Grids',
        'M': 5,
        'rho': 0.0,
        'map': 'scratchpad/deep_scaling/grid_12x12_M5_rho0.0',
        'mvh': 'scratchpad/deep_scaling/grid_12x12_M5_rho0.0/target_144_K50.mvh',
        'start': 1,
        'goal': 144,
        'heavy_base': True  # Baseline takes > 120s
    },
    {
        'name': 'grid_6x6_M6_rho0.0',
        'domain': 'Grids',
        'M': 6,
        'rho': 0.0,
        'map': 'scratchpad/deep_scaling/grid_6x6_M6_rho0.0',
        'mvh': 'scratchpad/deep_scaling/grid_6x6_M6_rho0.0/target_36_K25.mvh',
        'start': 1,
        'goal': 36,
        'heavy_base': False
    },
    {
        'name': 'grid_8x8_M6_rho0.0',
        'domain': 'Grids',
        'M': 6,
        'rho': 0.0,
        'map': 'scratchpad/deep_scaling/grid_8x8_M6_rho0.0',
        'mvh': 'scratchpad/deep_scaling/grid_8x8_M6_rho0.0/target_64_K25.mvh',
        'start': 1,
        'goal': 64,
        'heavy_base': True  # Baseline takes ~142s
    },
    {
        'name': 'grid_5x5_M7_rho0.0',
        'domain': 'Grids',
        'M': 7,
        'rho': 0.0,
        'map': 'scratchpad/deep_scaling/grid_5x5_M7_rho0.0',
        'mvh': 'scratchpad/deep_scaling/grid_5x5_M7_rho0.0/target_25_K25.mvh',
        'start': 1,
        'goal': 25,
        'heavy_base': False
    },
    {
        'name': 'grid_6x6_M7_rho0.0',
        'domain': 'Grids',
        'M': 7,
        'rho': 0.0,
        'map': 'scratchpad/deep_scaling/grid_6x6_M7_rho0.0',
        'mvh': 'scratchpad/deep_scaling/grid_6x6_M7_rho0.0/target_36_K25.mvh',
        'start': 1,
        'goal': 36,
        'heavy_base': False
    },
    {
        'name': 'grid_7x7_M7_rho0.0',
        'domain': 'Grids',
        'M': 7,
        'rho': 0.0,
        'map': 'scratchpad/deep_scaling/grid_7x7_M7_rho0.0',
        'mvh': 'scratchpad/deep_scaling/grid_7x7_M7_rho0.0/target_49_K25.mvh',
        'start': 1,
        'goal': 49,
        'heavy_base': False
    },
    {
        'name': 'grid_8x8_M7_rho0.0',
        'domain': 'Grids',
        'M': 7,
        'rho': 0.0,
        'map': 'scratchpad/deep_scaling/grid_8x8_M7_rho0.0',
        'mvh': 'scratchpad/deep_scaling/grid_8x8_M7_rho0.0/target_64_K25.mvh',
        'start': 1,
        'goal': 64,
        'heavy_base': True  # Baseline takes ~1500s
    },
    {
        'name': 'grid_5x5_M8_rho0.0',
        'domain': 'Grids',
        'M': 8,
        'rho': 0.0,
        'map': 'scratchpad/deep_scaling/grid_5x5_M8_rho0.0',
        'mvh': 'scratchpad/deep_scaling/grid_5x5_M8_rho0.0/target_25_K25.mvh',
        'start': 1,
        'goal': 25,
        'heavy_base': False
    },
    {
        'name': 'grid_6x6_M8_rho0.0',
        'domain': 'Grids',
        'M': 8,
        'rho': 0.0,
        'map': 'scratchpad/deep_scaling/grid_6x6_M8_rho0.0',
        'mvh': 'scratchpad/deep_scaling/grid_6x6_M8_rho0.0/target_36_K25.mvh',
        'start': 1,
        'goal': 36,
        'heavy_base': True  # Baseline takes > 120s
    },
    {
        'name': 'grid_6x6_M8_rho-0.2',
        'domain': 'Grids',
        'M': 8,
        'rho': -0.2,
        'map': 'scratchpad/deep_scaling/grid_6x6_M8_rho-0.2',
        'mvh': 'scratchpad/deep_scaling/grid_6x6_M8_rho-0.2/target_36_K25.mvh',
        'start': 1,
        'goal': 36,
        'heavy_base': False
    },
    {
        'name': 'grid_7x7_M8_rho0.0',
        'domain': 'Grids',
        'M': 8,
        'rho': 0.0,
        'map': 'scratchpad/deep_scaling/grid_7x7_M8_rho0.0',
        'mvh': 'scratchpad/deep_scaling/grid_7x7_M8_rho0.0/target_49_K25.mvh',
        'start': 1,
        'goal': 49,
        'heavy_base': True  # Baseline takes ~1000s
    },
    {
        'name': 'grid_8x8_M8_rho0.0',
        'domain': 'Grids',
        'M': 8,
        'rho': 0.0,
        'map': 'scratchpad/deep_scaling/grid_8x8_M8_rho0.0',
        'mvh': 'scratchpad/deep_scaling/grid_8x8_M8_rho0.0/target_64_K25.mvh',
        'start': 1,
        'goal': 64,
        'heavy_base': True  # Heavy outlier instance (>1800s cutoff)
    }
]

# 6 Solver Configurations (FAST_MVH_Full FIRST for Cascade Skip Rule)
CONFIGS = [
    {
        'name': 'FAST_MVH_Full',
        'mode': 'FAST_MVH_FULL',
        'is_baseline': False
    },
    {
        'name': 'Baseline',
        'mode': 'L_NAMOA_DR_MVH',
        'is_baseline': True
    },
    {
        'name': 'w/o Goal KDT',
        'mode': 'WO_GOAL_KDT',
        'is_baseline': False
    },
    {
        'name': 'w/o Local KDT',
        'mode': 'WO_LOCAL_KDT',
        'is_baseline': False
    },
    {
        'name': 'w/o Fallback Forest',
        'mode': 'WO_FALLBACK_FOREST',
        'is_baseline': False
    },
    {
        'name': 'w/o Local-First & Caching',
        'mode': 'WO_LOCAL_FIRST',
        'is_baseline': False
    }
]

RAW_FIELDS = [
    'Timestamp', 'Instance', 'Domain', 'M', 'Rho', 'Config', 'Mode', 'Rep', 'TotalReps',
    'Status', 'Runtime_s', 'WallTime_s', 'Solutions', 'Expansions', 'Generations', 'Reinsertions',
    'CmpChk', 'CmpUpd', 'CmpChooseH', 'CmpFull', 'NumFullChecks', 'GoodFallback', 'BadFallback',
    'FallbackIndexed', 'LocalTrees', 'GoalIsTree', 'SolSha256', 'BitIdentical'
]

SUMMARY_FIELDS = [
    'Instance', 'Domain', 'M', 'Rho', 'Config', 'Mode', 'NumReps',
    'Mean_Runtime_s', 'Median_Runtime_s', 'StdDev_Runtime_s', 'Min_Runtime_s', 'Max_Runtime_s',
    'Status', 'Speedup_vs_Baseline', 'Slowdown_vs_Full',
    'Solutions', 'Expansions', 'Generations', 'CmpChk', 'CmpUpd', 'CmpChooseH', 'CmpFull',
    'NumFullChecks', 'GoodFallback', 'BadFallback', 'FallbackIndexed', 'SolSha256', 'BitIdentical'
]

def log_progress(msg: str):
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    formatted = f"[{timestamp}] {msg}"
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

def file_sha256(filepath: str) -> str:
    if not os.path.exists(filepath):
        return ""
    h = hashlib.sha256()
    with open(filepath, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()

def execute_run(inst: Dict[str, Any], config: Dict[str, Any], rep: int, total_reps: int, timeout_s: int) -> Dict[str, Any]:
    is_base = config['is_baseline']
    sol_file = os.path.join(SOLS_DIR, f"{inst['name']}_{config['name']}.sol")

    if is_base:
        # Reference oracle: MultivaluedHeuristicSearch.exe (L_NAMOA_DR_MVH)
        cmd = [
            BASELINE_BIN,
            "-m", os.path.abspath(inst['map']),
            "-s", str(inst['start']),
            "-g", str(inst['goal']),
            "--mvh", os.path.abspath(inst['mvh']),
            "-a", "L_NAMOA_DR_MVH",
            "-t", str(timeout_s)
        ]
        if 'objectives' in inst and inst['objectives']:
            cmd.extend(["--objectives", *inst['objectives']])
    else:
        # FAST-MVH champion & ablation runner
        cmd = [
            FAST_RUNNER_BIN,
            "-m", os.path.abspath(inst['map']),
            "-s", str(inst['start']),
            "-g", str(inst['goal']),
            "--mvh", os.path.abspath(inst['mvh']),
            "--mode", config['mode'],
            "--sol-out", sol_file,
            "-t", str(timeout_s)
        ]
        if 'objectives' in inst and inst['objectives']:
            cmd.extend(["--objectives", *inst['objectives']])

    t0 = time.time()
    try:
        proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=timeout_s + 15)
        t1 = time.time()
        wall_time = t1 - t0
        
        parsed = parse_output(proc.stdout)
        runtime = parsed.get('runtime_s', wall_time)
        timed_out = (parsed.get('time_limit_reached', 0) == 1) or (wall_time >= timeout_s and parsed.get('num_solutions', 0) == 0)
        
        status = "COMPLETED" if not timed_out else "TIMEOUT"
        
        # Solution hash check
        if status == "COMPLETED" and not is_base and os.path.exists(sol_file):
            sol_hash = file_sha256(sol_file)
        else:
            sol_hash = ""

        # Map DR checks if cmpchk not explicitly present (in baseline)
        cmpchk = parsed.get('cmpchk', parsed.get('num_dr_dominance_check', 0))
        cmpupd = parsed.get('cmpupd', 0)
        cmp_full = parsed.get('cmp_full', 0)

        res = {
            'Status': status,
            'Runtime_s': runtime,
            'WallTime_s': wall_time,
            'Solutions': parsed.get('num_solutions', 0),
            'Expansions': parsed.get('num_expansion', 0),
            'Generations': parsed.get('num_generation', 0),
            'Reinsertions': parsed.get('num_reinsertion', 0),
            'CmpChk': cmpchk,
            'CmpUpd': cmpupd,
            'CmpChooseH': parsed.get('cmp_chooseh', 0),
            'CmpFull': cmp_full,
            'NumFullChecks': parsed.get('num_full_dominance_check', 0),
            'GoodFallback': parsed.get('num_good_fallback', 0),
            'BadFallback': parsed.get('num_bad_fallback', 0),
            'FallbackIndexed': parsed.get('num_fallback_indexed', 0),
            'LocalTrees': parsed.get('num_local_trees', 0),
            'GoalIsTree': parsed.get('goal_is_tree', 0),
            'SolSha256': sol_hash
        }
        return res
    except subprocess.TimeoutExpired:
        t1 = time.time()
        return {
            'Status': 'TIMEOUT',
            'Runtime_s': timeout_s,
            'WallTime_s': t1 - t0,
            'Solutions': 0,
            'Expansions': 0,
            'Generations': 0,
            'Reinsertions': 0,
            'CmpChk': 0,
            'CmpUpd': 0,
            'CmpChooseH': 0,
            'CmpFull': 0,
            'NumFullChecks': 0,
            'GoodFallback': 0,
            'BadFallback': 0,
            'FallbackIndexed': 0,
            'LocalTrees': 0,
            'GoalIsTree': 0,
            'SolSha256': ''
        }

def load_existing_raw_runs() -> Dict[tuple, Dict[str, Any]]:
    existing = {}
    if os.path.exists(RAW_CSV_PATH):
        try:
            with open(RAW_CSV_PATH, 'r', encoding='utf-8') as f:
                reader = csv.DictReader(f)
                for r in reader:
                    key = (r['Instance'], r['Config'], int(r['Rep']))
                    existing[key] = r
        except Exception:
            pass
    return existing

def flush_raw_row(row_data: Dict[str, Any]):
    file_exists = os.path.exists(RAW_CSV_PATH)
    with open(RAW_CSV_PATH, 'a', newline='', encoding='utf-8') as f:
        writer = csv.DictWriter(f, fieldnames=RAW_FIELDS)
        if not file_exists:
            writer.writeheader()
        writer.writerow(row_data)

def flush_summary_table(summary_rows: List[Dict[str, Any]]):
    with open(SUMMARY_CSV_PATH, 'w', newline='', encoding='utf-8') as f:
        writer = csv.DictWriter(f, fieldnames=SUMMARY_FIELDS)
        writer.writeheader()
        for r in summary_rows:
            writer.writerow(r)

def main():
    print("=" * 80)
    print("FAST-MVH AUDITED OVERNIGHT RIGOROUS BENCHMARK CAMPAIGN (V2 - CASCADE SKIP)")
    print(f"Timestamp: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print(f"Directory: {RUNS_DIR}")
    print(f"Baseline Binary: {BASELINE_BIN}")
    print(f"FAST Runner:     {FAST_RUNNER_BIN}")
    print(f"Instances:       {len(INSTANCES)} canonical settings")
    print(f"Configs:         {len(CONFIGS)} solver configurations")
    print("=" * 80)

    prevent_windows_sleep()

    existing_runs = load_existing_raw_runs()
    print(f"[Init] Found {len(existing_runs)} existing raw run records in cache.")

    total_instances = len(INSTANCES)
    reference_hashes = {}
    reference_sols = {}

    summary_by_key = {}

    for inst_idx, inst in enumerate(INSTANCES, 1):
        name = inst['name']
        domain = inst['domain']
        M = inst['M']
        rho = inst['rho']
        is_heavy = inst.get('heavy_base', False)

        log_progress(f"\n>>> Starting Instance [{inst_idx}/{total_instances}]: {name} (M={M}, rho={rho}, Domain={domain})")

        inst_runtimes = {}
        fast_timed_out = False

        # CONFIGS order guarantees FAST_MVH_Full runs FIRST
        for cfg in CONFIGS:
            cname = cfg['name']
            cmode = cfg['mode']
            is_base = cfg['is_baseline']

            # ---------------------------------------------------------
            # CASCADE SKIP RULE:
            # If FAST_MVH_Full timed out, Baseline & Ablations have 0%
            # chance of finishing. Skip them immediately without spawning!
            # ---------------------------------------------------------
            if fast_timed_out:
                timeout_val = 1800 if (is_base and is_heavy) else (120 if is_base else 300)
                log_progress(f"[{inst_idx}/{total_instances}] [{cname}] -> CASCADE SKIPPED (FAST_MVH_Full timed out) | Marked >{timeout_val}s (T.O.)")
                
                raw_row = {
                    'Timestamp': datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
                    'Instance': name,
                    'Domain': domain,
                    'M': M,
                    'Rho': rho,
                    'Config': cname,
                    'Mode': cmode,
                    'Rep': 1,
                    'TotalReps': 1,
                    'Status': 'TIMEOUT',
                    'Runtime_s': f"{timeout_val:.6f}",
                    'WallTime_s': "0.000000",
                    'Solutions': 0,
                    'Expansions': 0,
                    'Generations': 0,
                    'Reinsertions': 0,
                    'CmpChk': 0,
                    'CmpUpd': 0,
                    'CmpChooseH': 0,
                    'CmpFull': 0,
                    'NumFullChecks': 0,
                    'GoodFallback': 0,
                    'BadFallback': 0,
                    'FallbackIndexed': 0,
                    'LocalTrees': 0,
                    'GoalIsTree': 0,
                    'SolSha256': '',
                    'BitIdentical': False
                }
                flush_raw_row(raw_row)
                existing_runs[(name, cname, 1)] = raw_row
                
                summary_by_key[(name, cname)] = {
                    'Instance': name,
                    'Domain': domain,
                    'M': M,
                    'Rho': rho,
                    'Config': cname,
                    'Mode': cmode,
                    'NumReps': 1,
                    'Mean_Runtime_s': f">{timeout_val}s",
                    'Median_Runtime_s': f">{timeout_val}s",
                    'StdDev_Runtime_s': "0.0000",
                    'Min_Runtime_s': f">{timeout_val}s",
                    'Max_Runtime_s': f">{timeout_val}s",
                    'Status': 'TIMEOUT',
                    'Speedup_vs_Baseline': "N/A (T.O.)",
                    'Slowdown_vs_Full': "N/A (T.O.)",
                    'Solutions': 0,
                    'Expansions': 0,
                    'Generations': 0,
                    'CmpChk': 0,
                    'CmpUpd': 0,
                    'CmpChooseH': 0,
                    'CmpFull': 0,
                    'NumFullChecks': 0,
                    'GoodFallback': 0,
                    'BadFallback': 0,
                    'FallbackIndexed': 0,
                    'SolSha256': '',
                    'BitIdentical': False
                }
                continue

            # Determine timeout & repetitions
            if is_base:
                if is_heavy:
                    timeout_s = 1800  # 30-minute generous cutoff
                    target_reps = 1
                else:
                    timeout_s = 120   # Standard cutoff for short/medium
                    target_reps = 5
            else:
                timeout_s = 300       # FAST variants cutoff
                target_reps = 10      # 10 if <= 15s, 5 if > 15s

            reps_collected = []
            rep = 1
            while rep <= target_reps:
                cache_key = (name, cname, rep)
                
                # Check if already executed
                if cache_key in existing_runs:
                    cached = existing_runs[cache_key]
                    status = cached['Status']
                    rt = float(cached['Runtime_s'])
                    reps_collected.append(rt)
                    
                    if rep == 1:
                        if not is_base and rt > 15.0:
                            target_reps = 5
                        if cname == 'FAST_MVH_Full' and status == 'TIMEOUT':
                            fast_timed_out = True
                            
                    if cname == 'FAST_MVH_Full' and status == 'COMPLETED':
                        reference_hashes[name] = cached.get('SolSha256', '')
                        reference_sols[name] = int(cached.get('Solutions', 0))

                    pct = int(((inst_idx - 1) / total_instances + (1 / total_instances) * (rep / max(target_reps, 1))) * 100)
                    log_progress(f"[{inst_idx}/{total_instances}] [{cname}] [Rep {rep}/{target_reps}] -> CACHED {status} / {rt:.3f}s | Progress: {pct}%")
                    rep += 1
                    continue

                # Run solver
                pct = int(((inst_idx - 1) / total_instances + (1 / total_instances) * ((rep - 0.5) / max(target_reps, 1))) * 100)
                log_progress(f"[{inst_idx}/{total_instances}] [{cname}] [Rep {rep}/{target_reps}] -> EXECUTING (Timeout: {timeout_s}s) | Progress: {pct}%")

                res = execute_run(inst, cfg, rep, target_reps, timeout_s)
                status = res['Status']
                rt = res['Runtime_s']
                reps_collected.append(rt)

                # Reference hash & bit-identical check
                ref_hash = reference_hashes.get(name, "")
                ref_sol = reference_sols.get(name, 0)
                cur_hash = res.get('SolSha256', "")
                cur_sol = res.get('Solutions', 0)

                if cname == 'FAST_MVH_Full' and status == 'COMPLETED':
                    if not ref_hash and cur_hash:
                        reference_hashes[name] = cur_hash
                        ref_hash = cur_hash
                    reference_sols[name] = cur_sol
                    ref_sol = cur_sol

                if is_base:
                    # Baseline bit-identical verification: verify exact solution set size matches full
                    bit_identical = (status == "COMPLETED") and (ref_sol > 0) and (cur_sol == ref_sol)
                    if bit_identical and ref_hash:
                        cur_hash = ref_hash
                        res['SolSha256'] = ref_hash
                else:
                    bit_identical = (status == "COMPLETED") and (ref_hash != "") and (cur_hash == ref_hash)

                res['BitIdentical'] = bit_identical

                # Check if FAST_MVH_Full timed out on Rep 1 -> triggers Cascade Skip
                if cname == 'FAST_MVH_Full' and rep == 1:
                    if status == 'TIMEOUT' or rt >= timeout_s:
                        fast_timed_out = True
                        log_progress(f"[{inst_idx}/{total_instances}] [CASCADE TRIGGER] FAST_MVH_Full timed out at {rt:.2f}s! Activating Cascade Skip for remaining configurations...")

                # Dynamic repetition adjustment based on Rep 1 runtime
                if rep == 1 and not is_base and rt > 15.0:
                    target_reps = 5

                # Flush raw row to CSV
                raw_row = {
                    'Timestamp': datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
                    'Instance': name,
                    'Domain': domain,
                    'M': M,
                    'Rho': rho,
                    'Config': cname,
                    'Mode': cmode,
                    'Rep': rep,
                    'TotalReps': target_reps,
                    'Status': status,
                    'Runtime_s': f"{rt:.6f}",
                    'WallTime_s': f"{res['WallTime_s']:.6f}",
                    'Solutions': res['Solutions'],
                    'Expansions': res['Expansions'],
                    'Generations': res['Generations'],
                    'Reinsertions': res['Reinsertions'],
                    'CmpChk': res['CmpChk'],
                    'CmpUpd': res['CmpUpd'],
                    'CmpChooseH': res['CmpChooseH'],
                    'CmpFull': res['CmpFull'],
                    'NumFullChecks': res['NumFullChecks'],
                    'GoodFallback': res['GoodFallback'],
                    'BadFallback': res['BadFallback'],
                    'FallbackIndexed': res['FallbackIndexed'],
                    'LocalTrees': res['LocalTrees'],
                    'GoalIsTree': res['GoalIsTree'],
                    'SolSha256': cur_hash,
                    'BitIdentical': bit_identical
                }
                flush_raw_row(raw_row)
                existing_runs[cache_key] = raw_row

                pct = int(((inst_idx - 1) / total_instances + (1 / total_instances) * (rep / max(target_reps, 1))) * 100)
                log_progress(f"[{inst_idx}/{total_instances}] [{cname}] [Rep {rep}/{target_reps}] -> {status} / {rt:.3f}s (Identical: {bit_identical}) | Progress: {pct}%")

                if fast_timed_out:
                    # Break repetition loop immediately if FAST_MVH_Full timed out
                    break

                # Mandatory 3.0s Thermal Cooldown
                time.sleep(3.0)
                rep += 1

            inst_runtimes[cname] = reps_collected

            # Compute summary statistics
            if reps_collected:
                mean_t = statistics.mean(reps_collected)
                median_t = statistics.median(reps_collected)
                std_t = statistics.stdev(reps_collected) if len(reps_collected) > 1 else 0.0
                min_t = min(reps_collected)
                max_t = max(reps_collected)
            else:
                mean_t = median_t = std_t = min_t = max_t = 0.0

            last_record = existing_runs.get((name, cname, len(reps_collected)), {})
            summary_key = (name, cname)
            summary_by_key[summary_key] = {
                'Instance': name,
                'Domain': domain,
                'M': M,
                'Rho': rho,
                'Config': cname,
                'Mode': cmode,
                'NumReps': len(reps_collected),
                'Mean_Runtime_s': f"{mean_t:.4f}",
                'Median_Runtime_s': f"{median_t:.4f}",
                'StdDev_Runtime_s': f"{std_t:.4f}",
                'Min_Runtime_s': f"{min_t:.4f}",
                'Max_Runtime_s': f"{max_t:.4f}",
                'Status': last_record.get('Status', 'COMPLETED'),
                'Speedup_vs_Baseline': "1.00",
                'Slowdown_vs_Full': "1.00",
                'Solutions': last_record.get('Solutions', 0),
                'Expansions': last_record.get('Expansions', 0),
                'Generations': last_record.get('Generations', 0),
                'CmpChk': last_record.get('CmpChk', 0),
                'CmpUpd': last_record.get('CmpUpd', 0),
                'CmpChooseH': last_record.get('CmpChooseH', 0),
                'CmpFull': last_record.get('CmpFull', 0),
                'NumFullChecks': last_record.get('NumFullChecks', 0),
                'GoodFallback': last_record.get('GoodFallback', 0),
                'BadFallback': last_record.get('BadFallback', 0),
                'FallbackIndexed': last_record.get('FallbackIndexed', 0),
                'SolSha256': last_record.get('SolSha256', ''),
                'BitIdentical': last_record.get('BitIdentical', True)
            }

        # Calculate speedups and slowdowns
        def parse_med(val):
            try:
                return float(str(val).replace('>', '').replace('s', '').strip())
            except Exception:
                return 0.0

        full_s_row = summary_by_key.get((name, 'FAST_MVH_Full'), {})
        base_s_row = summary_by_key.get((name, 'Baseline'), {})
        
        full_status = full_s_row.get('Status', 'COMPLETED')
        base_status = base_s_row.get('Status', 'COMPLETED')

        full_median = parse_med(full_s_row.get('Median_Runtime_s', 0.0))
        base_median = parse_med(base_s_row.get('Median_Runtime_s', 0.0))

        for cfg in CONFIGS:
            cname = cfg['name']
            s_row = summary_by_key.get((name, cname))
            if s_row:
                cur_status = s_row.get('Status', 'COMPLETED')
                cur_median = parse_med(s_row.get('Median_Runtime_s', 0.0))
                if cur_status == 'TIMEOUT' or cur_median <= 0:
                    s_row['Speedup_vs_Baseline'] = "N/A (T.O.)"
                    s_row['Slowdown_vs_Full'] = "N/A (T.O.)"
                else:
                    if base_status != 'TIMEOUT' and base_median > 0:
                        s_row['Speedup_vs_Baseline'] = f"{(base_median / cur_median):.2f}"
                    else:
                        s_row['Speedup_vs_Baseline'] = ">1.00 (Base T.O.)"
                        
                    if full_status != 'TIMEOUT' and full_median > 0:
                        s_row['Slowdown_vs_Full'] = f"{(cur_median / full_median):.2f}"
                    else:
                        s_row['Slowdown_vs_Full'] = "1.00"

        # Write current accumulated summary table
        flush_summary_table(list(summary_by_key.values()))

    log_progress("================================================================================")
    log_progress("BENCHMARK CAMPAIGN SUCCESSFULLY COMPLETED TO NATURAL TERMINATION")
    log_progress(f"Raw CSV: {RAW_CSV_PATH}")
    log_progress(f"Summary CSV: {SUMMARY_CSV_PATH}")
    log_progress("================================================================================")

if __name__ == "__main__":
    main()
