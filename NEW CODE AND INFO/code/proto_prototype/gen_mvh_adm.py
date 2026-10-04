"""Admissible landmark differential MVH.
Same construction as benchmarks/generators/generate_mvh.py, but Dijkstra runs on the graph the
solver actually searches: Maya's parser adds the inverse of every edge (AdjacencyMatrix(..., inverse=true)),
so the search graph is symmetric. On a symmetric graph |d(s,l) - d(g,l)| <= d(s,g) (triangle inequality).
usage: gen_mvh_adm.py --map DIR --goal G -K K --seed S --out FILE
"""
import os, argparse, random, heapq, glob


def load(dir_path):
    files = sorted(glob.glob(os.path.join(dir_path, "*.gr")))
    M = len(files)
    adj = {}
    n = 0
    per = [dict() for _ in range(M)]
    for k, fp in enumerate(files):
        with open(fp) as f:
            for line in f:
                if line.startswith("a "):
                    _, u, v, c = line.split()
                    u, v, c = int(u), int(v), int(c)
                    n = max(n, u, v)
                    for a, b in ((u, v), (v, u)):
                        key = (a, b)
                        per[k].setdefault(key, []).append(c)
    # edges are aligned line-by-line across objective files in Maya's parser; each directed line is one
    # multi-edge carrying a cost vector. For per-objective shortest paths, the min over parallel edges is exact.
    adjk = []
    for k in range(M):
        a = {}
        for (u, v), cs in per[k].items():
            a.setdefault(u, []).append((v, min(cs)))
        adjk.append(a)
    return n, M, adjk


def dijkstra(a, n, s):
    dist = [float("inf")] * (n + 1)
    dist[s] = 0
    pq = [(0, s)]
    while pq:
        d, u = heapq.heappop(pq)
        if d > dist[u]:
            continue
        for v, w in a.get(u, ()):
            nd = d + w
            if nd < dist[v]:
                dist[v] = nd
                heapq.heappush(pq, (nd, v))
    return dist


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("--map", required=True); p.add_argument("--goal", type=int, required=True)
    p.add_argument("-K", type=int, required=True); p.add_argument("--seed", type=int, default=123)
    p.add_argument("--out", required=True)
    a = p.parse_args()
    random.seed(a.seed)
    n, M, adjk = load(a.map)
    lms = random.sample(range(1, n + 1), min(a.K, n))
    D = [[dijkstra(adjk[k], n, l) for k in range(M)] for l in lms]
    with open(a.out, "w") as f:
        for s in range(1, n + 1):
            for i in range(len(lms)):
                hv = []
                ok = True
                for k in range(M):
                    ds, dg = D[i][k][s], D[i][k][a.goal]
                    if ds == float("inf") or dg == float("inf"):
                        ok = False; break
                    hv.append(max(0, int(abs(ds - dg))))
                if ok:
                    f.write(str(s) + "".join(f"\t{x}" for x in hv) + "\n")
