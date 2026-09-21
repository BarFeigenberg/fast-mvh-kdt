# scripts/validate_cost_model.py
# Validates the average-case model: fits comparisons/Check ~ n^a vs frontier size n for each
# backend. Expect linear ~1; kd/kdinc clearly sub-linear. Writes build/cost_model.png.
import csv, sys
import numpy as np
def load(path):
    with open(path) as f: return list(csv.DictReader(f))
def fit_exponent(sizes, cmps):
    x = np.log(np.array(sizes, float)); y = np.log(np.array(cmps, float))
    a, _ = np.polyfit(x, y, 1); return a
def main(path="build/avg_sweep.csv"):
    rows = load(path); summary = {}
    series = {}
    for backend in ("linear","kd","kdinc","avlfast"):
        pts = [(float(r["final_size"]), float(r["dom_cmps_check"])/max(1.0,float(r["checks"])))
               for r in rows if r["backend"]==backend and float(r["final_size"])>1
               and float(r["dom_cmps_check"])>0]
        if not pts: continue
        sizes, cpc = zip(*sorted(pts)); series[backend]=(sizes,cpc)
        summary[backend] = fit_exponent(sizes, cpc)
    print("fitted cmp/check vs n exponents:", {k: round(v,3) for k,v in summary.items()})
    try:
        import matplotlib; matplotlib.use("Agg"); import matplotlib.pyplot as plt
        plt.rcParams.update({
            "font.size": 13, "axes.labelsize": 15, "xtick.labelsize": 12,
            "ytick.labelsize": 12, "legend.fontsize": 12,
        })
        disp = {"linear": "Linear", "avlfast": "AVL", "kdinc": "k-d", "kd": "k-d (batch)"}
        fig, ax = plt.subplots(figsize=(6.2, 4.4))
        for b,(sizes,cpc) in series.items():
            ax.scatter(sizes,cpc,s=22,label=f"{disp.get(b,b)} ($a$={summary[b]:.2f})")
        ax.set_xscale("log"); ax.set_yscale("log")
        ax.set_xlabel("frontier size $n$"); ax.set_ylabel("dom.\\ comparisons / Check"); ax.legend()
        fig.tight_layout()
        fig.savefig("build/cost_model.png", dpi=140); print("wrote build/cost_model.png")
    except Exception as e:
        print("plot skipped:", e)
    ok = summary.get("linear",0) > 0.7
    for g in ("kd","kdinc"):
        if g in summary and not (summary[g] < 0.7): ok = False
    if not ok: print("WARN: exponents not in expected regime (linear>0.7, kd/kdinc<0.7)", file=sys.stderr)
    sys.exit(0)
if __name__=="__main__": main(*sys.argv[1:])
