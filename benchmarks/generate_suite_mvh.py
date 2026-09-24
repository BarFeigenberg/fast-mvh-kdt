import os
import subprocess
import glob
import sys

def main():
    baseline_bin = os.path.abspath("build/baselines/bridging-mvh-dr/Release/MultivaluedHeuristicSearch.exe")
    
    # We want to generate for 10x10 and 20x20, M in 3,4,5
    targets = {
        "grid_10x10": 100,
        "grid_20x20": 400
    }
    
    for prefix, target in targets.items():
        for d in [3, 4, 5]:
            pattern = os.path.join("benchmarks", "instances", f"{prefix}_d{d}_tradeoff*")
            matches = glob.glob(pattern)
            for map_dir in matches[:1]: # just 1 topology per dimension
                map_name = os.path.basename(os.path.normpath(map_dir))
                mvh_path = os.path.join(map_dir, f"target_{target}.mvh")
                
                if os.path.exists(mvh_path):
                    print(f"[{map_name}] MVH already exists.")
                    continue
                    
                print(f"[{map_name}] Generating full MVH... (this may take a long time)")
                mvh_base = os.path.join(map_dir, f"target_{target}")
                obj_args = [str(i) for i in range(d)]
                
                cmd_gen = [
                    baseline_bin,
                    "--map", map_dir,
                    "--start", "1",
                    "--goal", str(target),
                    "--objectives", *obj_args,
                    "--algorithm", "APEX_MVH",
                    "--logging_file", mvh_base
                ]
                
                try:
                    # No timeout! We let it run.
                    subprocess.run(cmd_gen, check=True)
                    generated_mvh = f"{mvh_base}_apex_mvh.txt"
                    if os.path.exists(generated_mvh):
                        os.rename(generated_mvh, mvh_path)
                        print(f"[{map_name}] Successfully generated MVH.")
                except Exception as e:
                    print(f"[{map_name}] Failed: {e}")

if __name__ == '__main__':
    main()
