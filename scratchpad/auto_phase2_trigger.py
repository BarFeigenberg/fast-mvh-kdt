import os
import sys
import time
import subprocess

TARGET_PID = 29084
PHASE1_CSV = "scratchpad/results/phase1_fast_unbounded.csv"
LOG_FILE = "scratchpad/results/auto_phase2.log"

def log(msg):
    t = time.strftime("%Y-%m-%d %H:%M:%S")
    line = f"[{t}] {msg}\n"
    print(line, end="", flush=True)
    with open(LOG_FILE, "a", encoding="utf-8") as f:
        f.write(line)

log(f"Auto-trigger started. Monitoring Phase 1 PID {TARGET_PID}...")

# Check if process is alive
def is_pid_alive(pid):
    try:
        # Use tasklist on Windows
        r = subprocess.run(["tasklist", "/FI", f"PID eq {pid}"], capture_output=True, text=True)
        return str(pid) in r.stdout
    except Exception:
        return False

# Loop until Phase 1 is done
while True:
    alive = is_pid_alive(TARGET_PID)
    if not alive:
        log("Phase 1 process has exited! Launching Phase 2 automatically now...")
        break
    time.sleep(10)

# Launch Phase 2
log("Starting python scratchpad/phase2_maya_eval.py...")
cmd = [sys.executable, "scratchpad/phase2_maya_eval.py"]
with open("scratchpad/results/phase2_maya.log", "w", encoding="utf-8") as out:
    proc = subprocess.Popen(cmd, stdout=out, stderr=subprocess.STDOUT)
    log(f"Phase 2 launched with PID {proc.pid}. Waiting for completion...")
    proc.wait()
    log(f"Phase 2 finished with exit code {proc.returncode}!")
