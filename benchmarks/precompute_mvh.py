import os
import subprocess
import glob
import sys

def main():
    baseline_bin = os.path.abspath("build/baselines/bridging-mvh-dr/Release/MultivaluedHeuristicSearch.exe")
    if not os.path.exists(baseline_bin):
        print(f"Baseline binary not found at {baseline_bin}")
        sys.exit(1)

    instances_dir = os.path.abspath("benchmarks/instances")
    
    grids = [
        ("grid_10x10", 100),
        ("grid_15x15", 225),
        ("grid_20x20", 400),
        ("grid_25x25", 625),
        ("grid_30x30", 900)
    ]
    dims = [3, 4, 5, 6]
    
    for prefix, target in grids:
        for d in dims:
            # We look for tradeoff instances
            # The pattern is grid_XxY_dZ_tradeoff*
            pattern = os.path.join(instances_dir, f"{prefix}_d{d}_tradeoff*")
            matches = glob.glob(pattern)
            
            for map_dir in matches:
                # We only want to compute if it's a directory
                if not os.path.isdir(map_dir):
                    continue
                
                # Check if .mvh already exists
                mvh_path = os.path.join(map_dir, f"target_{target}.mvh")
                if os.path.exists(mvh_path):
                    print(f"Skipping {map_dir} - MVH already exists.")
                    continue
                
                print(f"Precomputing MVH for {map_dir} (target={target})")
                
                # We need to construct the objectives list based on d
                objectives = [str(i) for i in range(d)]
                
                # The baseline will write to `<logging_file>_apex_mvh.txt`
                # So we pass logging_file = mvh_path_base
                mvh_base = os.path.join(map_dir, f"target_{target}")
                
                cmd = [
                    baseline_bin,
                    "--map", map_dir,
                    "--start", "1",
                    "--goal", str(target),
                    "--objectives", *objectives,
                    "--algorithm", "APEX_MVH",
                    "--logging_file", mvh_base
                ]
                
                print(f"Running: {' '.join(cmd)}")
                try:
                    subprocess.run(cmd, check=True)
                    
                    # Rename the output to exactly .mvh
                    generated_mvh = f"{mvh_base}_apex_mvh.txt"
                    if os.path.exists(generated_mvh):
                        os.rename(generated_mvh, mvh_path)
                        print(f"Successfully generated {mvh_path}\n")
                    else:
                        print(f"WARNING: MVH file not found at expected path {generated_mvh}\n")
                except subprocess.CalledProcessError as e:
                    print(f"Error computing MVH for {map_dir}: {e}\n")

if __name__ == '__main__':
    main()
