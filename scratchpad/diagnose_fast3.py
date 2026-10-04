import os
import time
import subprocess

# Let's run Case 8: grid_n7_m8_rho-0.2 with proto_w (FAST2) vs fast_mvh (FAST) vs fast_mvh (FAST3)
map_dir = "scratchpad/grids/grid_n7_m8_rho-0.2"
mvh = "scratchpad/grids/grid_n7_m8_rho-0.2/apex_0.05.mvh"

print("--- 1. Testing proto_w (FAST2) ---")
cmd_proto = [
    "build\\Release\\proto_w.exe", map_dir, "1", "49", "8",
    mvh, "scratchpad/sols/diag_proto.sol",
    "flatH=1", "local_first=1", "witness=1", "redund=1",
    "localX=8", "targetX=8", "promoteC=64", "exact_max=1"
]
t0 = time.time()
res_proto = subprocess.run(cmd_proto, capture_output=True, text=True)
print(f"proto_w time: {time.time()-t0:.3f}s")
for line in res_proto.stdout.splitlines():
    if "sols=" in line:
        print("proto_w output:", line[:200])

print("\n--- 2. Testing fast_mvh FAST (FAST2 in main C++) ---")
cmd_fast = [
    "build\\Release\\fast_mvh.exe",
    "-m", map_dir, "-s", "1", "-g", "49",
    "--objectives", "0", "1", "2", "3", "4", "5", "6", "7",
    "--mvh", mvh,
    "-a", "L_NAMOA_DR_MVH_FAST",
    "-t", "120"
]
t0 = time.time()
res_fast = subprocess.run(cmd_fast, capture_output=True, text=True)
print(f"fast_mvh FAST time: {time.time()-t0:.3f}s")
print(res_fast.stdout.strip())

print("\n--- 3. Testing fast_mvh FAST3 ---")
cmd_fast3 = [
    "build\\Release\\fast_mvh.exe",
    "-m", map_dir, "-s", "1", "-g", "49",
    "--objectives", "0", "1", "2", "3", "4", "5", "6", "7",
    "--mvh", mvh,
    "-a", "L_NAMOA_DR_MVH_FAST3",
    "-t", "300"
]
t0 = time.time()
res_fast3 = subprocess.run(cmd_fast3, capture_output=True, text=True)
print(f"fast_mvh FAST3 time: {time.time()-t0:.3f}s")
print(res_fast3.stdout.strip())
