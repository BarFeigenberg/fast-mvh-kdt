#!/usr/bin/env python3
"""
overnight_campaign.py  —  Fully Independent Overnight Scaling Campaign
=======================================================================
Launched via pythonw.exe — completely detached from any shell, agent, or
network. All output is written directly to log files inside this script.
WiFi disconnection, agent disconnection, browser closed: nothing stops it.
Only Windows sleep/shutdown can interrupt it.

RESUME: reads fast_scaling_overnight.csv at startup, skips done instances.
HEARTBEAT: writes overnight_status.txt every 10 min from a background thread.
NO TIMEOUTS: every solver call runs to full completion.
"""

import os, sys, csv, time, subprocess, threading
from datetime import datetime

# ── All output goes to log file — no stdout dependency whatsoever ─────────────
RUNS     = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "benchmarks", "runs")
RUNS     = os.path.abspath(RUNS)
os.makedirs(RUNS, exist_ok=True)
TS       = datetime.now().strftime("%Y-%m-%d_%H%M%S")
LOG_PATH = os.path.join(RUNS, f"overnight_{TS}.log")
_lf      = open(LOG_PATH, "w", encoding="utf-8", buffering=1)
# Redirect stdout/stderr into the log file so pythonw has nothing to manage
sys.stdout = _lf
sys.stderr = _lf

# ── Paths ─────────────────────────────────────────────────────────────────────
BASE     = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
BIN      = os.path.join(BASE, "build", "Release", "fast_mvh.exe")
PYTHON   = os.path.join(os.path.dirname(sys.executable.replace("pythonw", "python")), "python.exe")
if not os.path.exists(PYTHON):
    PYTHON = "python"
SCRATCH  = os.path.join(BASE, "scratchpad", "deep_scaling")
OUT_CSV  = os.path.join(RUNS, "fast_scaling_overnight.csv")
STATUS   = os.path.join(RUNS, "overnight_status.txt")

# ── Instance matrix — EASY FIRST ─────────────────────────────────────────────
# Ordered by expected difficulty (FAST time as proxy).
# Resume logic skips anything already in the CSV.
INSTANCES = [
    # Currently running — skipped by resume once Maya finishes
    ( 9, 8,  0.0, 25),

    # ── EASY: M=6 N=10 rho sweep (FAST ~58s known, Maya ~6655s known) ───────
    (10, 6, -0.2, 25),
    (10, 6, -0.4, 25),
    (10, 6, -0.6, 25),

    # ── MEDIUM: M=6 N=11 rho sweep (FAST ~200-400s est) ─────────────────────
    (11, 6,  0.0, 25),
    (11, 6, -0.2, 25),
    (11, 6, -0.4, 25),
    (11, 6, -0.6, 25),

    # ── MEDIUM: M=8 N=9 remaining rhos (FAST ~874s known) ───────────────────
    ( 9, 8, -0.2, 25),
    ( 9, 8, -0.4, 25),
    ( 9, 8, -0.6, 25),

    # ── HARD: M=7 N=9 rho sweep (FAST ~1000-3000s est) ──────────────────────
    ( 9, 7,  0.0, 25),
    ( 9, 7, -0.2, 25),
    ( 9, 7, -0.4, 25),
    ( 9, 7, -0.6, 25),

    # ── HARDER: M=6 N=12 ─────────────────────────────────────────────────────
    (12, 6,  0.0, 25),
    (12, 6, -0.2, 25),
    (12, 6, -0.4, 25),

    # ── HARDER: M=8 N=10 ─────────────────────────────────────────────────────
    (10, 8,  0.0, 25),
    (10, 8, -0.2, 25),
    (10, 8, -0.4, 25),

    # ── HARDEST: M=7 N=10 ────────────────────────────────────────────────────
    (10, 7,  0.0, 25),
    (10, 7, -0.2, 25),
    (10, 7, -0.4, 25),

    # ── EXTREME: M=6 N=13 ────────────────────────────────────────────────────
    (13, 6,  0.0, 25),
    (13, 6, -0.2, 25),
]

FIELDS = ["timestamp","N","M","rho","K","solutions","expansions","generations",
          "reinsertions","time_fast_s","time_maya_s","speedup_vs_maya",
          "cmp_chooseh_fast","cmp_chooseh_maya","ops_reduction","soundness","maya_status"]

# ── Heartbeat state ───────────────────────────────────────────────────────────
_hb = {"instance":"starting","phase":"init","since":datetime.now(),
       "done":0,"total":len(INSTANCES),"stop":threading.Event()}

def _heartbeat_loop():
    while not _hb["stop"].wait(timeout=600):
        elapsed = (datetime.now() - _hb["since"]).total_seconds()
        try:
            with open(STATUS, "w", encoding="utf-8") as f:
                f.write(f"=== OVERNIGHT CAMPAIGN STATUS ===\n")
                f.write(f"Alive     : {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
                f.write(f"Progress  : {_hb['done']} / {_hb['total']} instances done\n")
                f.write(f"Current   : {_hb['instance']}\n")
                f.write(f"Phase     : {_hb['phase']}\n")
                f.write(f"In phase  : {elapsed/3600:.2f}h ({elapsed:.0f}s)\n")
                f.write(f"Log       : {LOG_PATH}\n")
                f.write(f"CSV       : {OUT_CSV}\n")
        except Exception:
            pass

# ── Logging ───────────────────────────────────────────────────────────────────
def log(msg=""):
    line = f"[{datetime.now().strftime('%H:%M:%S')}] {msg}"
    _lf.write(line + "\n")
    _lf.flush()

# ── Resume ────────────────────────────────────────────────────────────────────
def get_done() -> set:
    done = set()
    if not os.path.exists(OUT_CSV):
        return done
    try:
        with open(OUT_CSV, "r", encoding="utf-8", errors="replace") as f:
            for row in csv.DictReader(f):
                try:
                    done.add((int(row["N"]),int(row["M"]),float(row["rho"]),int(row["K"])))
                except Exception:
                    pass
    except Exception:
        pass
    return done

def write_row(row: dict):
    new = not os.path.exists(OUT_CSV) or os.path.getsize(OUT_CSV) == 0
    with open(OUT_CSV, "a", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=FIELDS)
        if new: w.writeheader()
        w.writerow({k: row.get(k,"") for k in FIELDS})
        f.flush()

# ── Solver ────────────────────────────────────────────────────────────────────
def parse_metrics(out: str) -> dict:
    m = {}
    for ln in out.splitlines():
        if "=" not in ln: continue
        for tok in ln.split("\t"):
            if "=" in tok:
                k,v = tok.split("=",1)
                try: m[k.strip()] = float(v.strip()) if "." in v.strip() else int(v.strip())
                except ValueError: m[k.strip()] = v.strip()
    return m

def parse_sols(path: str) -> set:
    vecs = set()
    if not os.path.exists(path): return vecs
    with open(path,"r",encoding="utf-8",errors="replace") as f:
        for ln in f:
            ln = ln.strip()
            if not ln or ln.startswith("#"): continue
            delim = "," if "," in ln else None
            try:
                v = tuple(int(p.strip()) for p in ln.split(delim) if p.strip())
                if v: vecs.add(v)
            except ValueError: pass
    return vecs

def run_solver(inst_dir, mvh, N, M, algo, sol_out):
    cmd = [BIN,"--map",inst_dir,"--start","1","--goal",str(N*N),
           "--objectives",*[str(i) for i in range(M)],
           "--algorithm",algo,"--mvh",mvh,"--cutoffTime","0","--sol-out",sol_out]
    t0 = time.perf_counter()
    try:
        p = subprocess.run(cmd, capture_output=True, text=True)
        wall = time.perf_counter() - t0
        met = parse_metrics(p.stdout); met["wall"] = wall
        return True, wall, met
    except Exception as e:
        return False, 0.0, {"error": str(e)}

def ensure(N, M, rho, K, seed=42):
    d = os.path.join(SCRATCH, f"grid_{N}x{N}_M{M}_rho{rho}")
    mvh = os.path.join(d, f"target_{N*N}_K{K}.mvh")
    os.makedirs(d, exist_ok=True)
    grs = [f for f in os.listdir(d) if f.startswith("c") and f.endswith(".gr")]
    if len(grs) < M:
        log(f"  Generating {N}x{N} grid M={M} rho={rho}...")
        subprocess.run([PYTHON,
                        os.path.join(BASE,"benchmarks","generators","generate_grid.py"),
                        "--rows",str(N),"--cols",str(N),"-M",str(M),
                        "--rho",str(rho),"--seed",str(seed),"--out-dir",d],
                       check=True, capture_output=True)
    if not os.path.exists(mvh):
        log(f"  Generating MVH K={K}...")
        subprocess.run([PYTHON,
                        os.path.join(BASE,"benchmarks","generators","generate_mvh.py"),
                        "--map",d,"--goal",str(N*N),"-K",str(K),
                        "--seed",str(seed),"--out",mvh],
                       check=True, capture_output=True)
    return d, mvh

# ── Main ──────────────────────────────────────────────────────────────────────
def main():
    threading.Thread(target=_heartbeat_loop, daemon=True).start()

    log("=================================================================")
    log("  OVERNIGHT CAMPAIGN v3  —  pythonw / Fully WiFi-Independent")
    log(f"  Started  : {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    log(f"  Binary   : {BIN}")
    log(f"  Instances: {len(INSTANCES)}  |  Timeouts: NONE")
    log(f"  Heartbeat: {STATUS}  (every 10 min)")
    log(f"  CSV      : {OUT_CSV}")
    log(f"  Log      : {LOG_PATH}")
    log("=================================================================")
    log()

    already_done = get_done()
    skipped = sum(1 for i in INSTANCES if (i[0],i[1],i[2],i[3]) in already_done)
    if skipped:
        log(f"RESUME: {skipped}/{len(INSTANCES)} already in CSV — skipping.")
    log()

    total = len(INSTANCES)
    done_count = skipped

    for idx, (N, M, rho, K) in enumerate(INSTANCES, 1):
        if (N, M, rho, K) in already_done:
            log(f"[{idx}/{total}] SKIP (done): N={N} M={M} rho={rho}")
            continue

        _hb["instance"] = f"N={N} M={M} rho={rho} K={K}"
        _hb["phase"]    = "setup"
        _hb["since"]    = datetime.now()

        log(f"[{idx}/{total}] {'='*54}")
        log(f"[{idx}/{total}]  N={N} ({N}x{N}={N*N} nodes)  M={M}  rho={rho}  K={K}")
        log(f"[{idx}/{total}] {'='*54}")

        try:
            d, mvh = ensure(N, M, rho, K)
        except Exception as e:
            log(f"  [ERROR] Generation failed: {e}")
            continue

        fast_sol = os.path.join(SCRATCH, f"sol_fast_N{N}_M{M}_rho{rho}_K{K}.txt")
        maya_sol = os.path.join(SCRATCH, f"sol_maya_N{N}_M{M}_rho{rho}_K{K}.txt")

        # FAST
        _hb["phase"] = "FAST running"; _hb["since"] = datetime.now()
        log(f"  -> FAST [started {datetime.now().strftime('%H:%M:%S')}]")
        ok_f, tf, mf = run_solver(d, mvh, N, M, "L_NAMOA_DR_MVH_FAST", fast_sol)
        if not ok_f:
            log(f"  [FAST] ERROR: {mf.get('error')}. Skipping.")
            continue
        fv = parse_sols(fast_sol)
        fc = mf.get("cmp_chooseh", 0)
        log(f"  [FAST] {tf:.2f}s | {len(fv):,} solutions | CmpCH: {fc:,}")

        # Maya
        _hb["phase"] = "Maya running"; _hb["since"] = datetime.now()
        log(f"  -> Maya [started {datetime.now().strftime('%H:%M:%S')}]")
        ok_m, tm, mm = run_solver(d, mvh, N, M, "L_NAMOA_DR_MVH_INSTRUMENTED", maya_sol)
        mv = parse_sols(maya_sol)
        mc = mm.get("cmp_chooseh", 0)

        if ok_m and fv == mv:   sound = "BIT_IDENTICAL"
        elif ok_m:               sound = f"DIVERGED fast:{len(fv)} maya:{len(mv)}"
        else:                    sound = "MAYA_ERROR"

        speedup = tm / tf if ok_m else None
        ops     = (mc / fc) if (ok_m and fc and mc) else None
        log(f"  [Maya] {tm:.2f}s | {sound}")
        if speedup: log(f"  >>> SPEEDUP: {speedup:.2f}x | OPS: {ops:.1f}x <<<")

        write_row({
            "timestamp":        datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
            "N":N,"M":M,"rho":rho,"K":K,
            "solutions":        len(fv),
            "expansions":       mf.get("num_expansion",""),
            "generations":      mf.get("num_generation",""),
            "reinsertions":     mf.get("num_reinsertion",""),
            "time_fast_s":      tf,
            "time_maya_s":      tm,
            "speedup_vs_maya":  f"{speedup:.2f}" if speedup else "",
            "cmp_chooseh_fast": fc,
            "cmp_chooseh_maya": mc,
            "ops_reduction":    f"{ops:.1f}" if ops else "",
            "soundness":        sound,
            "maya_status":      "OK" if ok_m else "ERROR",
        })
        done_count += 1
        _hb["done"] = done_count
        log(f"  Saved. ({done_count}/{total} done)\n")

    _hb["stop"].set()
    log("=================================================================")
    log(f"  CAMPAIGN COMPLETE  [{datetime.now().strftime('%H:%M:%S')}]")
    log("=================================================================")
    with open(STATUS,"w",encoding="utf-8") as f:
        f.write(f"COMPLETE at {datetime.now()}\n{done_count}/{total} instances done.\n")
    _lf.close()

if __name__ == "__main__":
    try:
        main()
    except Exception as e:
        import traceback
        _lf.write(f"\n[FATAL] {e}\n{traceback.format_exc()}\n")
        _lf.close()
        sys.exit(1)
