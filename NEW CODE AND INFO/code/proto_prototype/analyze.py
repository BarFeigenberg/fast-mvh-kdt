"""Summarize sweep CSV: median wall per (instance,cfg), relative to a reference cfg; check identity.
usage: analyze.py <csv> [ref_cfg] [extra_cols,comma]
"""
import csv, sys, statistics
from collections import defaultdict

path = sys.argv[1]
ref = sys.argv[2] if len(sys.argv) > 2 else None
extra = sys.argv[3].split(",") if len(sys.argv) > 3 else []
rows = list(csv.DictReader(open(path)))
walls = defaultdict(list); ident = defaultdict(set); ex = {}
order_i, order_c = [], []
for r in rows:
    k = (r["instance"], r["cfg"])
    if r["instance"] not in order_i: order_i.append(r["instance"])
    if r["cfg"] not in order_c: order_c.append(r["cfg"])
    try: walls[k].append(float(r["wall"]))
    except: pass
    ident[k].add(r["identical"])
    ex[k] = r
for inst in order_i:
    print(f"\n### {inst}")
    agg = min if __import__("os").environ.get("AGG", "min") == "min" else statistics.median
    base = agg(walls[(inst, ref)]) if ref and walls.get((inst, ref)) else None
    print("| cfg | wall (s, min) | min | max | n | vs " + (ref or "-") + " | identical | " + " | ".join(extra) + " |")
    print("|---|---|---|---|---|---|---|" + "---|" * len(extra))
    for c in order_c:
        w = walls.get((inst, c))
        if not w: continue
        m = agg(w)
        sp = f"{base / m:.2f}x" if base else "-"
        e = " | ".join(ex[(inst, c)].get(x, "") for x in extra)
        print(f"| {c} | {m:.3f} | {min(w):.3f} | {max(w):.3f} | {len(w)} | {sp} | {'/'.join(sorted(ident[(inst, c)]))} | {e} |")

