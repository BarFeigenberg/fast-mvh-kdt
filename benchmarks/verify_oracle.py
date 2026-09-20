#!/usr/bin/env python3
"""
verify_oracle.py - Oracle Validation and Bit-Identical Pareto Front Verification

Verifies that the accelerated K-d tree search solver produces Pareto-optimal
frontiers that are 100% bit-identical to the Maya Wohlf reference baseline oracle.
"""

import argparse
import json
import os
import subprocess
import sys
from typing import List, Set, Tuple


def parse_cost_file(filepath: str) -> Set[Tuple[int, ...]]:
    """Parse a file of comma- or space-delimited cost vectors."""
    vectors: Set[Tuple[int, ...]] = set()
    if not os.path.exists(filepath):
        raise FileNotFoundError(f"Solution file not found: {filepath}")

    with open(filepath, "r", encoding="utf-8") as f:
        for line_num, line in enumerate(f, 1):
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            # Handle comma or space separation
            delimiter = "," if "," in line else None
            parts = line.split(delimiter)
            try:
                vec = tuple(int(p.strip()) for p in parts if p.strip())
                if vec:
                    vectors.add(vec)
            except ValueError as e:
                raise ValueError(
                    f"Malformed cost vector on line {line_num} in {filepath}: '{line}'"
                ) from e
    return vectors


def compare_frontiers(
    oracle_front: Set[Tuple[int, ...]],
    test_front: Set[Tuple[int, ...]],
    instance_id: str = "instance",
) -> bool:
    """
    Compare two Pareto frontiers for exact bit-identical vector equality.
    Prints detailed diagnostics if differences exist.
    """
    print(f"=== Oracle Verification for: {instance_id} ===")
    print(f"Oracle front size:    {len(oracle_front)} vectors")
    print(f"Test candidate size:  {len(test_front)} vectors")

    if oracle_front == test_front:
        print("[SUCCESS] Bit-identical match! 100% vector equality verified.")
        return True

    missing_in_test = oracle_front - test_front
    spurious_in_test = test_front - oracle_front

    print("\n[FAILURE] Pareto frontiers diverge!")
    if missing_in_test:
        print(f"\nMissing vectors in candidate front ({len(missing_in_test)}):")
        for vec in sorted(missing_in_test)[:20]:
            print(f"  - {vec}")
        if len(missing_in_test) > 20:
            print(f"  ... and {len(missing_in_test) - 20} more.")

    if spurious_in_test:
        print(f"\nSpurious (dominated or invalid) vectors in candidate front ({len(spurious_in_test)}):")
        for vec in sorted(spurious_in_test)[:20]:
            print(f"  + {vec}")
        if len(spurious_in_test) > 20:
            print(f"  ... and {len(spurious_in_test) - 20} more.")

    return False


def run_command(cmd: List[str], timeout: int) -> subprocess.CompletedProcess:
    """Execute command with timeout and return process result."""
    print(f"Executing: {' '.join(cmd)}")
    return subprocess.run(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        timeout=timeout,
        check=True,
    )


def main():
    parser = argparse.ArgumentParser(
        description="Verify bit-identical Pareto fronts between baseline oracle and fast_mvh."
    )
    # Direct file comparison mode
    parser.add_argument("--oracle-file", help="Path to oracle solution vector file")
    parser.add_argument("--test-file", help="Path to test solution vector file")

    # Binary execution mode
    parser.add_argument("--oracle-bin", help="Path to baseline oracle executable")
    parser.add_argument("--test-bin", help="Path to fast_mvh test executable")
    parser.add_argument("--map", help="Directory path to graph instance map")
    parser.add_argument("--start", type=int, help="Start node index")
    parser.add_argument("--goal", type=int, help="Goal node index")
    parser.add_argument("--objectives", nargs="+", type=int, default=[0, 1, 2], help="Objective indices")
    parser.add_argument("--mvh", help="Path to multi-valued heuristic file")
    parser.add_argument("--timeout", type=int, default=120, help="Per-solver timeout in seconds")
    parser.add_argument("--scratch-dir", default="scratchpad", help="Directory for temporary solution outputs")

    args = parser.parse_args()

    # Case A: Direct file comparison
    if args.oracle_file and args.test_file:
        try:
            oracle_front = parse_cost_file(args.oracle_file)
            test_front = parse_cost_file(args.test_file)
            is_identical = compare_frontiers(
                oracle_front, test_front, instance_id=f"{args.oracle_file} vs {args.test_file}"
            )
            sys.exit(0 if is_identical else 1)
        except Exception as e:
            print(f"[ERROR] Verification failed: {e}", file=sys.stderr)
            sys.exit(2)

    # Case B: Execution mode
    if args.test_bin and args.map and args.start is not None and args.goal is not None:
        os.makedirs(args.scratch_dir, exist_ok=True)
        oracle_out = os.path.join(args.scratch_dir, "oracle_sol.txt")
        test_out = os.path.join(args.scratch_dir, "test_sol.txt")

        # Run Test Binary
        obj_args = [str(o) for o in args.objectives]
        test_cmd = [
            args.test_bin,
            "--map", args.map,
            "--start", str(args.start),
            "--goal", str(args.goal),
            "--objectives", *obj_args,
            "--sol-out", test_out,
        ]
        if args.mvh:
            test_cmd.extend(["--mvh", args.mvh])

        try:
            run_command(test_cmd, args.timeout)
        except subprocess.TimeoutExpired:
            print(f"[ERROR] Test binary timed out after {args.timeout}s", file=sys.stderr)
            sys.exit(2)
        except subprocess.CalledProcessError as e:
            print(f"[ERROR] Test binary failed (code {e.returncode}):\n{e.stderr}", file=sys.stderr)
            sys.exit(2)

        # Run Oracle Binary (if provided)
        if args.oracle_bin:
            oracle_cmd = [
                args.oracle_bin,
                "--map", args.map,
                "--start", str(args.start),
                "--goal", str(args.goal),
                "--algorithm", "L_NAMOA_DR_MVH",
                "--objectives", *obj_args,
                "--logging_file", os.path.join(args.scratch_dir, "oracle"),
            ]
            if args.mvh:
                oracle_cmd.extend(["--mvh", args.mvh])

            try:
                run_command(oracle_cmd, args.timeout)
            except subprocess.TimeoutExpired:
                print(f"[ERROR] Oracle binary timed out after {args.timeout}s", file=sys.stderr)
                sys.exit(2)
            except subprocess.CalledProcessError as e:
                print(f"[ERROR] Oracle binary failed (code {e.returncode}):\n{e.stderr}", file=sys.stderr)
                sys.exit(2)

        try:
            oracle_front = parse_cost_file(oracle_out)
            test_front = parse_cost_file(test_out)
            is_identical = compare_frontiers(
                oracle_front, test_front, instance_id=f"map={args.map}, {args.start}->{args.goal}"
            )
            sys.exit(0 if is_identical else 1)
        except Exception as e:
            print(f"[ERROR] Comparison failed: {e}", file=sys.stderr)
            sys.exit(2)

    parser.print_help()
    sys.exit(2)


if __name__ == "__main__":
    main()
