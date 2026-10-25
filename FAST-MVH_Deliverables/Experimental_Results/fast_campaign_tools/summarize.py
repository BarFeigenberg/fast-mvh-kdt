"""Summarize results.csv -> summary.csv / summary.md in the run dir.  usage: python summarize.py RUN_DIR"""
import csv, json, os, sys
from collections import defaultdict

run = sys.argv[1]
rows = list(csv.DictReader(open(os.path.join(run, "results.csv"))))
by = defaultdict(lambda: defaultdict(list))
meta = {}
for r in rows:
    st = json.loads(r["stats"]) if r["stats"] else {}
    by[r["name"]][r["solver"]].append((r, st))
    meta[r["name"]] = (r["family"], r["M"], r["eps"])

out = []
for name, sv in by.items():
    fam, M, eps = meta[name]
    rec = {"name": name, "family": fam, "M": M, "eps": eps}
    hashes = set()
    for s in ("maya", "fast2", "fast"):
        runs = sv.get(s, [])
        ok = [(r, st) for r, st in runs if r["status"] == "ok"]
        rec[f"{s}_n"] = len(ok)
        rec[f"{s}_status"] = runs[0][0]["status"] if runs else ""
        if ok:
            ts = sorted(float(st["runtime_s"]) for _, st in ok)
            rec[f"{s}_min"] = ts[0]
            rec[f"{s}_med"] = ts[len(ts) // 2]
            st = ok[0][1]
            rec["sols"] = st["num_solutions"]; rec["exp"] = st["num_expansion"]
            for r, _ in ok:
                if r["sol_sha256"]:
                    hashes.add(r["sol_sha256"])
            for k in ("cmpchk", "cmpupd", "cmp_chooseh", "cmp_full", "num_full_dominance_check",
                      "num_good_fallback", "num_bad_fallback", "num_fallback_indexed", "num_frontier_trees",
                      "num_reinsertion", "num_generation"):
                if k in st:
                    rec[f"{s}_{k}"] = st[k]
        elif runs:
            st = runs[0][1]
            rec[f"{s}_min"] = None
            if st:
                rec[f"{s}_partial"] = f"{st.get('runtime_s')}s/{st.get('num_solutions')}sols"
    rec["identical"] = len(hashes) == 1
    if rec.get("maya_min") and rec.get("fast_min"):
        rec["speedup"] = rec["maya_min"] / rec["fast_min"]
    if rec.get("fast2_min") and rec.get("fast_min"):
        rec["f2_over_f"] = rec["fast2_min"] / rec["fast_min"]
    out.append(rec)

keys = sorted({k for r in out for k in r}, key=lambda k: (k not in ("name", "family", "M", "eps", "sols", "exp"), k))
with open(os.path.join(run, "summary.csv"), "w", newline="") as f:
    w = csv.DictWriter(f, keys); w.writeheader(); w.writerows(out)
cols = ["name", "M", "eps", "sols", "exp", "maya_min", "fast2_min", "fast_min", "speedup", "f2_over_f",
        "fast2_cmp_full", "fast_cmp_full", "fast_num_fallback_indexed", "identical"]
with open(os.path.join(run, "summary.md"), "w") as f:
    f.write("| " + " | ".join(cols) + " |\n|" + "---|" * len(cols) + "\n")
    for r in out:
        f.write("| " + " | ".join(
            (f"{r[c]:.3f}" if isinstance(r.get(c), float) else str(r.get(c, ""))) for c in cols) + " |\n")
print(open(os.path.join(run, "summary.md")).read())
