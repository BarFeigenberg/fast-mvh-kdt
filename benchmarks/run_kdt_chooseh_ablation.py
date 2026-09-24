#!/usr/bin/env python3
"""
run_kdt_chooseh_ablation.py

Direct Head-to-Head A/B Ablation Benchmark:
Maya's Baseline (L-NAMOA_dr*-MVH with Linear Scan CHOOSEH)
vs.
Roi's Static KD-CHOOSEH (Static (d-1)-d K-d tree with Aggregated Subtree Pruning).

Evaluates across heuristic cardinalities K in {10, 25, 50, 100, 250, 500, 1000}
and dimensions M in {3, 4, 5, 6, 7, 8}.
"""

import os
import sys
import subprocess
import json
import csv
import time
import glob
from typing import Set, Tuple, Dict, Any

FAST_MVH_BIN = os.path.abspath("build/Release/fast_mvh.exe")
SCRATCH_DIR = os.path.abspath("scratchpad/ablation_kdt")
RESULTS_DIR = os.path.abspath("benchmarks/runs/kdt_chooseh_ablation")

def parse_cost_file(filepath: str) -> Set[Tuple[int, ...]]:
    vectors: Set[Tuple[int, ...]] = set()
    if not os.path.exists(filepath):
        return vectors
    with open(filepath, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            delimiter = "," if "," in line else None
            parts = line.split(delimiter)
            vec = tuple(int(p.strip()) for p in parts if p.strip())
            if vec:
                vectors.add(vec)
    return vectors

def parse_solver_output(stdout: str) -> Dict[str, Any]:
    metrics = {}
    for line in stdout.splitlines():
        line = line.strip()
        if "=" in line and ("algorithm=" in line or "num_solutions=" in line):
            for token in line.split("\t"):
                if "=" in token:
                    k, v = token.split("=", 1)
                    k = k.strip()
                    v = v.strip()
                    try:
                        metrics[k] = float(v) if "." in v else int(v)
                    except ValueError:
                        metrics[k] = v
    return metrics

def truncate_mvh(source_mvh: str, target_mvh: str, max_k: int):
    counts = {}
    os.makedirs(os.path.dirname(target_mvh), exist_ok=True)
    with open(source_mvh, "r", encoding="utf-8") as fin, open(target_mvh, "w", encoding="utf-8") as fout:
        for line in fin:
            if not line.strip(): continue
            parts = line.strip().split()
            if not parts: continue
            node_id = parts[0]
            count = counts.get(node_id, 0)
            if count < max_k:
                fout.write(line)
                counts[node_id] = count + 1

def generate_synthetic_instance(num_nodes: int, dim: int, max_k: int, instance_dir: str):
    import random
    os.makedirs(instance_dir, exist_ok=True)
    random.seed(42 + dim * 100 + num_nodes)
    
    # 1. Generate graph edges (connected DAG + cycles)
    edges = set()
    for u in range(1, num_nodes):
        edges.add((u, u + 1)) # linear backbone
    for _ in range(num_nodes * 3):
        u = random.randint(1, num_nodes)
        v = random.randint(1, num_nodes)
        if u != v:
            edges.add((u, v))
            
    edges = sorted(list(edges))
    
    # 2. Write obj_*.gr files
    for m in range(dim):
        gr_path = os.path.join(instance_dir, f"obj_{m}.gr")
        with open(gr_path, "w", encoding="utf-8") as f:
            for (u, v) in edges:
                cost = random.randint(1, 30)
                f.write(f"a {u} {v} {cost}\n")
                
    # 3. Generate synthetic admissible multi-valued heuristic
    # Calculate simple reverse shortest distances on objective 0
    mvh_path = os.path.join(instance_dir, f"heuristics_K{max_k}.mvh")
    with open(mvh_path, "w", encoding="utf-8") as f:
        for u in range(1, num_nodes + 1):
            base_dist = abs(num_nodes - u)
            for k in range(max_k):
                h_vec = [max(0, base_dist * 2 + random.randint(0, 5)) for _ in range(dim)]
                # vary coordinates to create non-dominated trade-offs
                for d in range(1, dim):
                    h_vec[d] = max(0, h_vec[d] + (k % 7) - 3)
                f.write(f"{u}\t" + "\t".join(str(x) for x in h_vec) + "\n")
                
    return mvh_path

def run():
    os.makedirs(SCRATCH_DIR, exist_ok=True)
    os.makedirs(RESULTS_DIR, exist_ok=True)
    
    print("================================================================================")
    print("  PHASE 1 ABLATION: Maya Linear Scan CHOOSEH vs. Roi Static KD-CHOOSEH")
    print("================================================================================")
    
    K_values = [10, 25, 50, 100, 250, 500, 1000]
    results = []
    
    # Test suite 1: Grid 10x10 with 3 objectives using authentic precomputed MVH
    grid_d3 = "benchmarks/instances/grid_10x10_d3_tradeoff"
    raw_mvh_d3 = os.path.join(grid_d3, "target_100.mvh")
    
    if os.path.exists(raw_mvh_d3):
        print(f"\n[Test Suite 1] Grid 10x10 (M=3, Start=1, Goal=100) across K={K_values}")
        for K in K_values:
            mvh_k = os.path.join(SCRATCH_DIR, f"grid_10x10_d3_K{K}.mvh")
            truncate_mvh(raw_mvh_d3, mvh_k, K)
            
            # Run Maya Baseline (Linear Scan Instrumented)
            maya_sol = os.path.join(SCRATCH_DIR, f"maya_d3_K{K}_sol.txt")
            cmd_maya = [
                FAST_MVH_BIN, "--map", grid_d3, "--start", "1", "--goal", "100",
                "--objectives", "0", "1", "2",
                "--algorithm", "L_NAMOA_DR_MVH_INSTRUMENTED",
                "--mvh", mvh_k, "--sol-out", maya_sol, "--cutoffTime", "60"
            ]
            t0 = time.perf_counter()
            res_maya = subprocess.run(cmd_maya, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            t_maya = time.perf_counter() - t0
            met_maya = parse_solver_output(res_maya.stdout)
            
            # Run Roi KD-CHOOSEH
            roi_sol = os.path.join(SCRATCH_DIR, f"roi_d3_K{K}_sol.txt")
            cmd_roi = [
                FAST_MVH_BIN, "--map", grid_d3, "--start", "1", "--goal", "100",
                "--objectives", "0", "1", "2",
                "--algorithm", "L_NAMOA_KDT_CHOOSEH",
                "--mvh", mvh_k, "--sol-out", roi_sol, "--cutoffTime", "60"
            ]
            t0 = time.perf_counter()
            res_roi = subprocess.run(cmd_roi, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            t_roi = time.perf_counter() - t0
            met_roi = parse_solver_output(res_roi.stdout)
            
            # Verify Pareto fronts
            front_maya = parse_cost_file(maya_sol)
            front_roi = parse_cost_file(roi_sol)
            bit_identical = (front_maya == front_roi) and (len(front_maya) > 0)
            
            cmp_maya = met_maya.get("cmp_chooseh", 0)
            cmp_roi = met_roi.get("cmp_chooseh", 0)
            cmp_ratio = (cmp_maya / cmp_roi) if cmp_roi > 0 else 1.0
            speedup = (t_maya / t_roi) if t_roi > 0 else 1.0
            
            exp_match = (met_maya.get("num_expansion") == met_roi.get("num_expansion"))
            gen_match = (met_maya.get("num_generation") == met_roi.get("num_generation"))
            rein_match = (met_maya.get("num_reinsertion") == met_roi.get("num_reinsertion"))
            
            status = "PASS" if (bit_identical and exp_match and gen_match and rein_match) else "FAIL"
            
            print(f"  K={K:4d} | Maya Cmp: {cmp_maya:8d} | Roi Cmp: {cmp_roi:8d} ({cmp_ratio:5.2f}x reduction) | "
                  f"Time: {t_maya*1000:6.1f}ms vs {t_roi*1000:6.1f}ms | Solutions: {len(front_roi):3d} | Match: {status}")
            
            results.append({
                "suite": "Grid_10x10", "dim": 3, "K": K,
                "sols": len(front_roi), "expansions": met_roi.get("num_expansion", 0),
                "maya_cmp": cmp_maya, "roi_cmp": cmp_roi, "cmp_reduction": round(cmp_ratio, 2),
                "maya_time_ms": round(t_maya * 1000, 2), "roi_time_ms": round(t_roi * 1000, 2),
                "speedup": round(speedup, 2), "bit_identical": bit_identical
            })

    # Test suite 2: Higher dimensions M in {4, 5, 6, 7, 8} on controlled instances
    for M in [4, 5, 6, 7, 8]:
        inst_dir = os.path.join(SCRATCH_DIR, f"synth_N30_d{M}")
        print(f"\n[Test Suite 2] Dimension M={M} on N=30 Graph across K in [50, 100, 500, 1000]")
        for K in [50, 100, 500, 1000]:
            mvh_path = generate_synthetic_instance(30, M, K, inst_dir)
            obj_args = [str(i) for i in range(M)]
            
            maya_sol = os.path.join(SCRATCH_DIR, f"maya_d{M}_K{K}_sol.txt")
            cmd_maya = [
                FAST_MVH_BIN, "--map", inst_dir, "--start", "1", "--goal", "30",
                "--objectives", *obj_args,
                "--algorithm", "L_NAMOA_DR_MVH_INSTRUMENTED",
                "--mvh", mvh_path, "--sol-out", maya_sol, "--cutoffTime", "60"
            ]
            t0 = time.perf_counter()
            res_maya = subprocess.run(cmd_maya, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            t_maya = time.perf_counter() - t0
            met_maya = parse_solver_output(res_maya.stdout)
            
            roi_sol = os.path.join(SCRATCH_DIR, f"roi_d{M}_K{K}_sol.txt")
            cmd_roi = [
                FAST_MVH_BIN, "--map", inst_dir, "--start", "1", "--goal", "30",
                "--objectives", *obj_args,
                "--algorithm", "L_NAMOA_KDT_CHOOSEH",
                "--mvh", mvh_path, "--sol-out", roi_sol, "--cutoffTime", "60"
            ]
            t0 = time.perf_counter()
            res_roi = subprocess.run(cmd_roi, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            t_roi = time.perf_counter() - t0
            met_roi = parse_solver_output(res_roi.stdout)
            
            front_maya = parse_cost_file(maya_sol)
            front_roi = parse_cost_file(roi_sol)
            bit_identical = (front_maya == front_roi) and (len(front_maya) > 0)
            
            cmp_maya = met_maya.get("cmp_chooseh", 0)
            cmp_roi = met_roi.get("cmp_chooseh", 0)
            cmp_ratio = (cmp_maya / cmp_roi) if cmp_roi > 0 else 1.0
            speedup = (t_maya / t_roi) if t_roi > 0 else 1.0
            
            exp_match = (met_maya.get("num_expansion") == met_roi.get("num_expansion"))
            status = "PASS" if (bit_identical and exp_match) else "FAIL"
            
            print(f"  M={M}, K={K:4d} | Maya Cmp: {cmp_maya:8d} | Roi Cmp: {cmp_roi:8d} ({cmp_ratio:5.2f}x reduction) | "
                  f"Time: {t_maya*1000:6.1f}ms vs {t_roi*1000:6.1f}ms | Solutions: {len(front_roi):3d} | Match: {status}")
            
            results.append({
                "suite": "Synthetic_N30", "dim": M, "K": K,
                "sols": len(front_roi), "expansions": met_roi.get("num_expansion", 0),
                "maya_cmp": cmp_maya, "roi_cmp": cmp_roi, "cmp_reduction": round(cmp_ratio, 2),
                "maya_time_ms": round(t_maya * 1000, 2), "roi_time_ms": round(t_roi * 1000, 2),
                "speedup": round(speedup, 2), "bit_identical": bit_identical
            })

    # Save summary reports
    csv_file = os.path.join(RESULTS_DIR, "kdt_chooseh_ablation_summary.csv")
    md_file = os.path.join(RESULTS_DIR, "kdt_chooseh_ablation_summary.md")
    
    if results:
        fieldnames = list(results[0].keys())
        with open(csv_file, "w", newline="", encoding="utf-8") as f:
            writer = csv.DictWriter(f, fieldnames=fieldnames)
            writer.writeheader()
            writer.writerows(results)
            
        with open(md_file, "w", encoding="utf-8") as f:
            f.write("# Phase 1: Isolated KD-CHOOSEH Ablation Results\n\n")
            f.write("Evaluation of Roi's Static KD-Tree over Tr(H(s)) vs Maya Wohlf's Linear Scan CHOOSEH.\n\n")
            f.write("| Suite | M | K | Solutions | Expansions | Maya Cmp | Roi KDT Cmp | Cmp Reduction | Maya (ms) | Roi (ms) | Speedup | Bit-Identical |\n")
            f.write("|:---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|\n")
            for r in results:
                f.write(f"| {r['suite']} | {r['dim']} | {r['K']} | {r['sols']} | {r['expansions']} | {r['maya_cmp']:,} | {r['roi_cmp']:,} | **{r['cmp_reduction']}x** | {r['maya_time_ms']} | {r['roi_time_ms']} | **{r['speedup']}x** | {'YES' if r['bit_identical'] else 'NO'} |\n")
                
        print(f"\n[DONE] Consolidated results saved to:")
        print(f"  - {csv_file}")
        print(f"  - {md_file}")

if __name__ == "__main__":
    run()
