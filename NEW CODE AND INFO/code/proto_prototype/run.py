"""Run harness algorithms on instances; verify bit-identity vs MAYA; emit CSV rows.
usage: run.py <exe> <out_dir> <algos,comma> <inst_dir>:<mvh>:<M>:<goal> ... [--timeout S]
"""
import os, subprocess, sys, csv, time

def parse(line):
    d = {}
    for tok in line.strip().split("\t"):
        if "=" in tok:
            k, v = tok.split("=", 1)
            d[k] = v
    return d

def main():
    args = sys.argv[1:]
    timeout = 600
    if "--timeout" in args:
        i = args.index("--timeout"); timeout = int(args[i + 1]); del args[i:i + 2]
    reps = 1
    if "--reps" in args:
        i = args.index("--reps"); reps = int(args[i + 1]); del args[i:i + 2]
    exe, out_dir, algos = args[0], args[1], args[2].split(",")
    os.makedirs(out_dir, exist_ok=True)
    rows = []
    csv_path = os.path.join(out_dir, "results.csv")
    new = not os.path.exists(csv_path)
    with open(csv_path, "a", newline="") as fcsv:
        w = csv.writer(fcsv)
        if new:
            w.writerow(["instance", "mvh", "algo", "rep", "sols", "exp", "gen", "reins", "full", "cmp", "wall", "identical"])
        for spec in args[3:]:
            inst, mvh, M, goal = spec.rsplit(":", 3)
            name = os.path.basename(inst.rstrip("\\/")) + "_" + os.path.splitext(os.path.basename(mvh))[0]
            ref = None
            for algo in algos:
                for rep in range(reps):
                    sol = os.path.join(out_dir, f"{name}_{algo}.sol")
                    cmd = [exe, inst, "1", goal, M, mvh, algo, sol]
                    try:
                        r = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
                        ls = r.stdout.strip().splitlines(); d = parse(ls[-1]) if ls else {"wall": f"CRASH{r.returncode}"}
                    except subprocess.TimeoutExpired:
                        d = {"wall": "TIMEOUT"}
                    ident = ""
                    if "sols" in d:
                        sig = (open(sol).read(), d["exp"], d["gen"])
                        if algo == algos[0] and rep == 0:
                            ref = sig
                        ident = "REF" if algo == algos[0] else str(ref is not None and sig == ref)
                    row = [name, os.path.basename(mvh), algo, rep, d.get("sols"), d.get("exp"), d.get("gen"),
                           d.get("reins"), d.get("full"), d.get("cmp"), d.get("wall"), ident]
                    w.writerow(row); fcsv.flush()
                    print("\t".join(map(str, row)), flush=True)

if __name__ == "__main__":
    main()

