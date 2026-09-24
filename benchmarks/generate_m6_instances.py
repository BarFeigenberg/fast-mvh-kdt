import os
import sys
import subprocess
import time

GEN_SCRIPT = os.path.abspath("benchmarks/generators/generate_mvh.py")

def generate_inst(N, M, rho, K, seed=42):
    inst_dir = os.path.abspath(f"scratchpad/deep_scaling/grid_{N}x{N}_M{M}_rho{rho}")
    mvh_path = os.path.join(inst_dir, f"target_{N*N}_K{K}.mvh")
    if not os.path.exists(mvh_path):
        os.makedirs(inst_dir, exist_ok=True)
        if not any(f.endswith(".gr") for f in os.listdir(inst_dir) if os.path.isfile(os.path.join(inst_dir, f))) or not os.listdir(inst_dir):
            cmd_grid = [
                sys.executable, "benchmarks/generators/generate_grid.py",
                "--rows", str(N), "--cols", str(N),
                "-M", str(M), "--rho", str(rho),
                "--seed", str(seed), "--out-dir", inst_dir
            ]
            subprocess.run(cmd_grid, check=True, stdout=subprocess.DEVNULL)
            
        cmd_mvh = [
            sys.executable, GEN_SCRIPT, "--map", inst_dir,
            "--goal", str(N*N), "-K", str(K),
            "--seed", str(seed), "--out", mvh_path
        ]
        subprocess.run(cmd_mvh, check=True, stdout=subprocess.DEVNULL)
    return inst_dir, mvh_path

def main():
    print("Generating M=6 benchmark instances...")
    m6_configs = [
        (8, 6, 0.0, [50, 100]),
        (8, 6, -0.2, [50, 100]),
        (10, 6, 0.0, [50, 100]),
        (10, 6, -0.2, [50, 100]),
        (12, 6, 0.0, [50, 100]),
        (12, 6, -0.2, [50, 100])
    ]
    
    count = 0
    for N, M, rho, k_list in m6_configs:
        for K in k_list:
            t0 = time.time()
            idir, mpath = generate_inst(N, M, rho, K)
            count += 1
            print(f"[{count}] Generated N={N}, M={M}, rho={rho}, K={K} ({time.time()-t0:.2f}s)")
            
    print(f"\nSuccessfully generated {count} instances for M=6.")

if __name__ == "__main__":
    main()
