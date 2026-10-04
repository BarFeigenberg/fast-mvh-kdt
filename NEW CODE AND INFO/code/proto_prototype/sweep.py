"""Sweep proto configs over instances, verify vs reference Maya solutions, write CSV.
usage: sweep.py <out_csv> <cfgfile> <inst_spec>...   inst_spec = dir:mvh:M:goal:refsol:refexp:refgen
cfgfile: one config per line: name|key=val key=val
"""
import subprocess, sys, csv, os, hashlib
EXE = os.environ.get("PROTO_EXE", os.path.join(os.path.dirname(__file__), "proto.exe"))

def parse(line):
    d = {}
    for t in line.strip().split("\t"):
        if "=" in t:
            k, v = t.split("=", 1); d[k] = v
    return d

def main():
    out_csv, cfgfile = sys.argv[1], sys.argv[2]
    reps = int(os.environ.get("REPS", "1"))
    cfgs = []
    for ln in open(cfgfile):
        ln = ln.strip()
        if not ln or ln.startswith("#"): continue
        name, kv = ln.split("|", 1); cfgs.append((name.strip(), kv.split()))
    new = not os.path.exists(out_csv)
    f = open(out_csv, "a", newline=""); w = None
    for spec in sys.argv[3:]:
        d, mvh, M, goal, refsol, refexp, refgen = spec.split("*")
        ref_hash = hashlib.md5(open(refsol, "rb").read()).hexdigest()
        inst = os.path.basename(d.rstrip("\\/")) + "/" + os.path.basename(mvh)
        for rep in range(reps):
            for name, kv in cfgs:
                sol = out_csv + ".tmp.sol"
                r = subprocess.run([EXE, d, "1", goal, M, mvh, sol] + kv, capture_output=True, text=True)
                ls = r.stdout.strip().splitlines()
                m = parse(ls[-1]) if ls else {}
                ok = bool(m) and hashlib.md5(open(sol, "rb").read()).hexdigest() == ref_hash and m.get("exp") == refexp and m.get("gen") == refgen
                row = {"instance": inst, "cfg": name, "rep": rep, "identical": ok, **m}
                if w is None:
                    w = csv.DictWriter(f, fieldnames=list(row.keys()), extrasaction="ignore")
                    if new: w.writeheader()
                w.writerow(row); f.flush()
                print(f"{inst:40s} {name:28s} wall={m.get('wall')} ident={ok} ch%={m.get('p_ch_gen')}/{m.get('p_ch_re')} loc%={m.get('p_local')} full%={m.get('p_full')} upd%={m.get('p_update')}", flush=True)

main()

