import os, subprocess, time

cases = [
    # (name, map_dir, start, goal, M, mvh_path, sol_fast2, sol_maya)
    ("grid_n8_m7_rho-0.2", "scratchpad/grids/grid_n8_m7_rho-0.2", 1, 64, 7, "scratchpad/grids/grid_n8_m7_rho-0.2/apex_0.05.mvh", "scratchpad/sols/grid_n8m7_fast2.sol", "scratchpad/sols/grid_n8m7_maya.sol"),
    ("grid_n7_m8_rho-0.2", "scratchpad/grids/grid_n7_m8_rho-0.2", 1, 49, 8, "scratchpad/grids/grid_n7_m8_rho-0.2/apex_0.05.mvh", "scratchpad/sols/grid_n7m8_fast2.sol", "scratchpad/sols/grid_n7m8_maya.sol")
]

for name, d, s, g, M, mvh, f2_sol, m_sol in cases:
    print(f"\n==========================================", flush=True)
    print(f"Running Maya on {name} (M={M})...", flush=True)
    print(f"==========================================", flush=True)
    objs = " ".join(str(i) for i in range(M))
    cmd = f"build/Release/fast_mvh.exe -a L_NAMOA_DR_MVH_INSTRUMENTED -m {d} -s {s} -g {g} --objectives {objs} --mvh {mvh} --sol-out {m_sol} -t 3600"
    
    t0 = time.time()
    try:
        r = subprocess.run(cmd, shell=True, capture_output=True, text=True, timeout=3630)
        wall = time.time() - t0
        print(f"Result stdout:\n{r.stdout.strip()}", flush=True)
        print(f"Total Process Wall: {wall:.2f}s", flush=True)
    except subprocess.TimeoutExpired:
        print("Maya TIMED OUT (>3600s)!", flush=True)

    # Check bit-identity if both files exist
    if os.path.exists(m_sol) and os.path.exists(f2_sol):
        with open(m_sol) as f1, open(f2_sol) as f2:
            s1 = sorted([l.strip() for l in f1 if l.strip()])
            s2 = sorted([l.strip() for l in f2 if l.strip()])
            print(f"Maya Sols: {len(s1)}, FAST2 Sols: {len(s2)}, Bit-Identical: {s1 == s2}", flush=True)
