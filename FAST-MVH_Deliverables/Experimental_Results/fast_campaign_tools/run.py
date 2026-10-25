"""Same-machine campaign: reference L-NAMOA*dr-mvh vs FAST2 port vs FAST (FAST3 code).

usage: python run.py PLAN.json OUT_DIR
PLAN: {"jobs":[{"name","map","start","goal","mvh","M","eps","family",
                "solvers":{"maya":{"reps":1,"timeout":600},"fast2":{...},"fast":{...}}}]}
Repetitions of a job are interleaved across solvers. Rows are appended to results.csv.
Rep 0 solution files are kept in OUT_DIR/sols; later reps overwrite a scratch file and are hash-compared.
"""
import csv, hashlib, json, os, subprocess, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
FAST = os.path.join(ROOT, "scratchpad", "build_fast", "fast_mvh.exe")
MAYA = os.path.join(ROOT, "scratchpad", "maya_harness", "maya_run.exe")
TMP = os.path.join(HERE, "tmp_sols")
ALG = {"fast": "L_NAMOA_DR_MVH_FAST3", "fast2": "L_NAMOA_DR_MVH_FAST"}
ENV = dict(os.environ, PATH=r"C:\msys64\ucrt64\bin;" + os.environ["PATH"])


def sha(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()


def cmd_for(job, solver, sol, timeout):
    if solver == "maya":
        return [MAYA, job["map"], str(job["start"]), str(job["goal"]), job["mvh"], sol, str(timeout)]
    return [FAST, "--map", job["map"], "--start", str(job["start"]), "--goal", str(job["goal"]),
            "--algorithm", ALG[solver], "--mvh", job["mvh"], "--cutoffTime", str(timeout), "--sol-out", sol]


def main():
    plan = json.load(open(sys.argv[1]))
    out = sys.argv[2]
    os.makedirs(os.path.join(out, "sols"), exist_ok=True)
    os.makedirs(TMP, exist_ok=True)
    commit = subprocess.run(["git", "-C", ROOT, "rev-parse", "HEAD"], capture_output=True, text=True).stdout.strip()
    meta = {"commit": commit, "fast_sha256": sha(FAST), "maya_sha256": sha(MAYA),
            "plan_file": os.path.abspath(sys.argv[1]), "plan": plan, "started": time.strftime("%Y-%m-%d %H:%M:%S")}
    with open(os.path.join(out, f"meta_{time.strftime('%H%M%S')}.json"), "w") as f:
        json.dump(meta, f, indent=1)
    res = os.path.join(out, "results.csv")
    new = not os.path.exists(res)
    with open(res, "a", newline="") as fh:
        w = csv.writer(fh)
        if new:
            w.writerow(["name", "family", "M", "eps", "solver", "rep", "status", "wall_s", "sol_sha256", "cmd", "stats"])
        for job in plan["jobs"]:
            sv = job["solvers"]
            dead = set()
            for r in range(max(v["reps"] for v in sv.values())):
                for s, cfg in sv.items():
                    if r >= cfg["reps"] or s in dead:
                        continue
                    sol = (os.path.join(out, "sols", f"{job['name']}_{s}.sol") if r == 0
                           else os.path.join(TMP, f"{job['name']}_{s}.sol"))
                    c = cmd_for(job, s, sol, cfg["timeout"])
                    t0 = time.perf_counter()
                    try:
                        p = subprocess.run(c, capture_output=True, text=True, env=ENV, timeout=cfg["timeout"] + 120)
                        status, txt = ("ok" if p.returncode == 0 else f"rc{p.returncode}"), p.stdout.strip()
                    except subprocess.TimeoutExpired:
                        status, txt = "proc_timeout", ""
                    wall = time.perf_counter() - t0
                    stats = dict(kv.split("=", 1) for kv in txt.split("\t") if "=" in kv) if txt else {}
                    if stats.get("time_limit_reached") == "1":
                        status = "timeout"
                    h = sha(sol) if status == "ok" and os.path.exists(sol) else ""
                    if status != "ok":
                        dead.add(s)
                    w.writerow([job["name"], job["family"], job["M"], job["eps"], s, r, status, f"{wall:.3f}", h,
                                " ".join(c), json.dumps(stats)])
                    fh.flush()
                    print(f"{job['name']:<20} {s:<5} r{r} {status:<8} rt={stats.get('runtime_s','-'):>9} "
                          f"sols={stats.get('num_solutions','-')} exp={stats.get('num_expansion','-')} "
                          f"sha={h[:10]}", flush=True)


if __name__ == "__main__":
    main()
