# scripts/plot_road.py
# Two-panel road replay benchmark across real DIMACS maps (NY, BAY, COL).
# Reads build/road_replay.csv (one row per captured/replayed stream).
#   LEFT  : dominance-CHECK reduction  check_red_avl = AVL/kdinc  vs frontier (log-log).
#           Deterministic and exact -- the structural pruning advantage.
#   RIGHT : replay WALL-CLOCK reduction wall_red_avl = AVL/kdinc  vs frontier (x log).
# Markers by map {NY:o, BAY:s, COL:^}; geo = filled, synth = open/red.
# Dashed y=1 parity line on both panels; legend on the right panel.
import csv, sys
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

# Larger fonts for print legibility at column width.
plt.rcParams.update({
    "font.size": 13, "axes.labelsize": 14, "axes.titlesize": 14,
    "xtick.labelsize": 12, "ytick.labelsize": 12, "legend.fontsize": 11,
})

MARK  = {"NY": "o", "BAY": "s", "COL": "^"}
COLOR = {"NY": "tab:blue", "BAY": "tab:orange", "COL": "tab:green"}

def main(path="build/road_replay.csv"):
    rows = list(csv.DictReader(open(path)))
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(7.4, 3.3))

    seen = set()
    for r in rows:
        mp = r["map"]; mode = r["mode"]
        marker = MARK.get(mp, "o")
        try:
            x = float(r["frontier"]); yc = float(r["check_red_avl"]); yw = float(r["wall_red_avl"])
        except (ValueError, KeyError):
            continue
        if x != x or yc != yc:  # NaN guard
            continue
        is_geo = (mode == "geo")
        # Fixed colour per map for geo; random (synth) costs drawn as open red markers.
        if is_geo:
            face, edge = COLOR.get(mp, "tab:gray"), COLOR.get(mp, "tab:gray")
        else:
            face, edge = "none", "red"
        # one legend entry per (map, mode)
        key = (mp, mode)
        lbl = None
        if key not in seen:
            seen.add(key)
            lbl = mp if is_geo else "%s costs (random)" % mp
        ax1.scatter(x, yc, marker=marker, s=64,
                    facecolors=face, edgecolors=edge, linewidths=1.3, alpha=0.9)
        ax2.scatter(x, yw, marker=marker, s=64,
                    facecolors=face, edgecolors=edge, linewidths=1.3, alpha=0.9, label=lbl)

    for ax in (ax1, ax2):
        ax.set_xscale("log")
        ax.set_xlabel("final frontier size")
        ax.axhline(1.0, color="gray", lw=0.9, ls="--")
    ax1.set_yscale("log")
    ax1.set_ylabel("dom-check reduction")
    ax2.set_ylabel("wall-clock reduction")
    ax2.legend(loc="upper left", framealpha=0.9)
    fig.tight_layout()
    out = "docs/report/figs/road_scatter.png"
    fig.savefig(out, dpi=140, bbox_inches="tight")
    print("wrote %s with %d points" % (out, len(rows)))

if __name__ == "__main__":
    main(*sys.argv[1:])
