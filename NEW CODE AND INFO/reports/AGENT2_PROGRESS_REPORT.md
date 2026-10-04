# Agent 2 — Progress Report: Integration-Layer Audit of Maya / Shahaf / Roi (MVH + K-d trees)

Status: **experiments stopped on request** (the final head-to-head run was killed while on
`grid10_M5_r-0.3_s42 / A50`; see §6). No file under `src/` or `baselines/` was modified.
All code lives in `scratchpad/`; the proposed `src/` change set exists only as a staged patch
(`scratchpad/proposal/proposal_clean.patch`) awaiting approval.

## 0. Context for reproducibility

| item | value |
|---|---|
| base commit (all measurements) | `8870a825d0b5c10b16d8928eed259ce6a9f2cfb5` (branch `barfe-aicahub-mvh-kdtree-perf-audit`) |
| baseline submodule | `baselines/bridging-mvh-dr` @ `0a2f9ea158a138ac9d40d0d84a15e64d5b66a7e1` (read-only) |
| hardware | Intel i5-1135G7 laptop (4C, 1.25 MB L2/core, 8 MB L3), Windows, 15.7 GB RAM |
| compiler | `python -m ziglang c++` (clang 21.1, libc++) — no MSVC/Boost on the machine |
| flags | `-std=c++20 -O2 -DNDEBUG -march=native -Wl,--stack,536870912` (`scratchpad/perf/build.ps1`, `build_proto.ps1`) |
| Boost | header shims in `scratchpad/shim/boost/` (`filesystem` -> `std::filesystem`, `algorithm::split`) |
| timing protocol | process HIGH priority, affinity core 2 (`PROTO_AFFINITY=4`), configs interleaved per repetition, **min** of reps reported (run-to-run spread on this laptop is up to 2x from thermal throttling; medians were too noisy) |
| correctness protocol | every row: solution file MD5 + `num_expansion` + `num_generation` compared against Maya's `L_NAMOA_DR_MVH` built by the same compiler |

Binaries: `scratchpad/perf/harness.cpp` (Maya + every existing `src/` solver + proposed `L_NAMOA_DR_MVH_FAST`),
`scratchpad/perf/proto.hpp` + `proto_main.cpp` (policy-configurable prototype, `key=value` CLI).
Sweep drivers: `sweep.py` (proto), `run.py` / `final_eval.py` (harness), `analyze.py`, `prof_table.py`, `make_tables.py`.

Instances (all start = 1, goal = N·N or 100 for the `benchmarks/instances` grid):

| id | graph (generator, seed 42) | M | MVH | Maya wall (s) |
|---|---|---|---|---|
| g10M3-A50 | `grid10_M3_r-0.6` | 3 | admissible landmark, K=50 | 0.17 |
| g15M3-A100 | `grid15_M3_r-0.3` | 3 | adm. K=100 | 1.15 |
| g15M3-A200 | `grid15_M3_r-0.6` | 3 | adm. K=200 | 1.39 |
| g10M4-A{10,25,50,100} | `grid10_M4_r-0.6` | 4 | adm. K=10/25/50/100 | 5.0–9.6 |
| g12M4-A50 | `grid12_M4_r0.0` | 4 | adm. K=50 | 11.0 |
| g10M5-A50 | `grid10_M5_r-0.3` | 5 | adm. K=50 | 77.9–87.4 |
| g12M5-A25 | `grid12_M5_r0.0` | 5 | adm. K=25 | 365.9 |
| g10M6-A25 | `grid10_M6_r0.3` | 6 | adm. K=25 | 72.6 |
| apex-g10M4 | `benchmarks/instances/grid_10x10_d4_tradeoff_cyc1` | 4 | Maya's `APEX_MVH` (146,904 vectors, avg 1,469 per state) | 2.17 |
| apex-g10M3 | `grid10_M3_r-0.6` | 3 | `APEX_MVH` (31,413 vectors) | 0.07 |
| apex-g8M5 | `grid8_M6_r0.0`, objectives 0..4 | 5 | `APEX_MVH` (98,949 vectors) | 1.79 |
| legacy K50 | `grid10_M{3,4,5}` | 3–5 | **inadmissible** `benchmarks/generators/generate_mvh.py` K=50 | 0.27 / 13.2 / 181 |

Graphs: `benchmarks/generators/generate_grid.py --rows N --cols N -M M --rho r --seed 42` via `scratchpad/perf/gen.py`.
Admissible MVH: `scratchpad/perf/gen_mvh_adm.py --map DIR --goal G -K K --seed 42` (see §1.3).
APEX MVH: `harness.exe DIR 1 GOAL M apex-dump:FILE NONE`.

---

## 1. Findings that change the premises of the task

### 1.1 Correctness defects in the existing `src/` solvers
| solver | defect | evidence |
|---|---|---|
| `L_NAMOA_KDT_V1` | reads `node_mvh[start_idx]` with `start_idx == |H(s)|` on reinsertion of the last heuristic -> access violation | crash (exit `0xC0000005`) on legacy g10M4-K50 (`scratchpad/runs/smoke`) |
| `L_NAMOA_KDT_V2` (`choose_h_bottom_up`) | returns a **synthetic** vector (`Tr` = subtree `N.min`, `h_1 = 0`) that is not in H(s); changes f = g + h and the OPEN order | 43,420 vs 41,129 expansions on legacy g10M3-K50 — **not bit-identical** |
| V5 / `L_NAMOA_DR_MVH_KDT` | `max_g0` only recomputed on rebuild (stale-high) -> more DR fallbacks | 54,009 vs 51,198 full checks; decisions still identical (see §1.4) |
| `choose_h_dual` | 3 `std::vector` heap allocations per visited node; no `min_idx` child order | code audit |
| `DynamicFrontierKDTree` | node = 3 × `size_t[MAX_DIM=10]` + ptrs ≈ 272 B regardless of M | code audit (layout effect measured in §2.10: not significant) |

Fixes for the first, second and fourth are in the staged patch (not applied).

### 1.2 Benchmark MVH used by previous sweeps is inadmissible
`generate_mvh.py` runs Dijkstra on the **directed** `.gr` edges, but Maya's parser builds
`AdjacencyMatrix(..., inverse=true)`, i.e. the solver searches the symmetrised multigraph. The
differential bound |d(s,l) − d(goal,l)| is then not a lower bound. Proof by counterexample on
`grid10_M4_r-0.6`: zero heuristic (true front) = **12,667** solutions; legacy K10 → 8,592, K50 → 11,444,
K100 → 11,510. With `gen_mvh_adm.py` (Dijkstra on the symmetric graph) every K gives 12,667.
All conclusions below use admissible or APEX MVHs; the legacy K50 files are kept only to compare with earlier results.
Also: on a 10×10 grid K is capped at 100 landmarks (K100 ≡ K200).

### 1.3 Landmark MVHs are ~80–86 % redundant
For 82–86 % of vectors h_j there is an earlier h_i with Tr(h_i) ≤ Tr(h_j) (e.g. 3,652 / 4,450 for g10M4-A50).
APEX MVHs are antichains (0 % redundant). This dominates the behaviour of any H(s) index.

### 1.4 LOCALDOMCHECK is implementation-independent
Maya's LOCALDOMCHECK(s, g) returns true **iff** some expanded path p at s (p ∈ G_cl(s)) satisfies p ≤ g in all
coordinates: if a Tr-dominator exists and max g_1 ≤ g_1 it dominates fully; otherwise the fallback scans G_cl(s),
which contains every point ever inserted into G_cl^Tr(s) (and a removed point's remover also Tr-dominates g). Hence
any exact structure for the Tr set (flat, K-d, stale or exact max) and any witness shortcut yields identical decisions.
Only the fallback counters differ. This is what allows the witness and order-swap optimisations below.

### 1.5 Where the time actually goes (Maya-equivalent flat prototype, `proto.exe` defaults)
g10M4-K50: CHOOSEH 40 %, LOCALDOMCHECK 35 % (full-D fallback 25 %), update 5 %, global check 4 %, rest ≈ OPEN/allocation.
g10M5-K50: CHOOSEH 52 %, local 33 % (fallback 25 %). apex-g10M4: CHOOSEH 70 % + 12 %.
Simply storing G_cl^Tr, G_cl and Tr(H(s)) contiguously (no `vector<vector>` / `NodePtr` chasing) with the *same*
algorithm: g10M4-K50 3.5 s vs Maya 13.5 s; g10M5-K50 62 s vs 181 s (bit-identical).

---

## 2. Mechanisms implemented in the prototype (`proto::Cfg`) and the evidence for each

CSV/log locations are relative to `scratchpad/runs/`. "E-n" refers to the sweep ids below.

| sweep | file | what |
|---|---|---|
| correctness | `correct_m3*.csv`, `correct_apex.csv`, `correct_c4.csv`, `correct_c5.csv` | every config vs Maya |
| E1 | `e1.csv`, `e1.log` | frontier structure × fixed threshold X (context: flatH + local_first + witness) |
| prof | `prof_tables.txt` | per-size cycles/op for flat vs K-d vs bucket K-d (M3, M4, M5, apex) |
| E2 / E2b | `e2.csv`, `e2b_pad.csv`, `e2b_compact.csv` | adaptive promotion (promoteC) vs fixed X; padded vs compact node |
| E3 / E3b / E3c | `e3.csv`, `e3b.csv`, `e3c.csv` | CHOOSEH strategies, hX, leafB, T-structure interplay, ordering |
| E4 | `e4.csv` | leave-one-out ablation of the hybrid |
| final (partial) | `final_eval_partial/results.csv` | harness head-to-head (killed) |

### 2.1 `localX`, `targetX` — flat → K-d promotion threshold X (Version 1, Shahaf)
*Problem.* A K-d tree per state costs build/insert/rebuild overhead and pointer traversal; most states are tiny.
*Design.* `Front<D>`: contiguous D-strided array; one pass per update removes Tr-weakly-dominated points **and**
recomputes max g_1 exactly; promotion is a one-time O(n log n) build from the flat points.
*Evidence.*
* Per-size cost (`prof_tables.txt`, cycles per op):
  * local query crossover: M3 none up to the largest local size (≤ 64; parity), M4 ≈ [64,128) (flat 454 vs bucket 379),
    M5 ≈ [32,64) (flat 396 vs K-d 325), apex-M4 ≈ [128,256).
  * target-frontier (T) query crossover: M4 ≈ 16–32, M5 ≈ 4–8, but apex-M4 only at [512,1024) — in the APEX regime T
    queries are mostly dominated early, which flat scans answer quickly.
  * update crossover: ≈ 128–256 for M4, M5, apex (flat compaction is cheap).
* In-situ (E1/E2, min of 3–5): for M ≥ 4 wall time is flat for X ∈ [0, 64] (differences ≤ ~5 %, inside noise) and
  degrades for X ≥ 128–512 (g10M4-A50: X=512 1.29x vs X=0 1.90x over flat). For **M = 3 every X that actually builds
  trees is a regression** (g15M3-A200: X=0 0.85–0.89x of flat; X ≥ 64 builds 0 trees → parity).
* **Conclusion: the optimal X is dimension- and workload-dependent — low (≈ 8–32) for M ≥ 4 and "never" for M = 3.** A
  single fixed X (Shahaf's appendix uses TAU = 32) is harmful at M = 3 (fixedX16: 0.89x on g15M3-A200, E4).

### 2.2 `promoteC` — cost-model promotion (kept)
*Problem.* §2.1 shows the right X depends on M and on the query outcome mix, not only on n.
*Design.* Each flat frontier tracks the running mean number of point comparisons per query (halved every 4,096
queries); promote when size ≥ `localX` (=8) **and** mean scan ≥ C.
*Evidence (E2).* C ∈ {8,16,32,64,128}: C = 32–64 is within noise of the best fixed X on every M ≥ 4 instance
(g10M5-A50: C64 3.83x vs best fixed 3.99x over flat), and it **automatically avoids the M = 3 regression**
(C ≥ 64 → 0 trees on g15M3-A200, 0.95–0.98x ≈ parity; APEX T stays flat where flat is better). C = 128 is too late.
*Chosen:* `localX = targetX = 8, promoteC = 64`.

### 2.3 `bk` — bucket K-d tree leaf capacity (discarded)
*Design.* Internal nodes = (box, split); points in contiguous fixed-capacity leaf buckets, in-place compaction on delete.
*Evidence (E1, E2, prof).* bk8/16/32 never beat the point-per-node K-d consistently (g10M4-A50: bk16 X0 1.66x vs K-d 1.90x;
M5 ±5 %). Fewer comparisons (bk8 cmpchk 4.1 M vs 8.2 M) but more work per update. **Discarded**; note that an early
run in `correct_c4.csv` that seemed to favour "bk" was invalid (integration edit had not applied — caught by audit).

### 2.4 `rb_div`, `rb_slack` — K-d rebuild policy (kept at src default)
`total > built + built/4 + 64` (current `src/`) vs Shahaf's original `total > 2·built + 8` (`kd_X0_rb2x`, E1): not better (apex-g10M4 1.46x vs 1.65x over flat; the g15M3 row, 0.54x, sits in a noisy
block of that sweep where several configs dropped to 0.54x, so it is not used as evidence). **Kept** the `src/` rule.

### 2.5 `exact_max` — exact vs stale max g_1 (kept exact)
Exact bookkeeping (count of live points at the max; O(n) recompute only when the last one dies) vs stale-until-rebuild:
decisions identical (§1.4); stale only adds fallbacks. E1: exact 1.90/1.82/2.74x vs stale 1.78/1.41/2.98x (noise-level).
**Kept exact** (cheaper fallbacks, deterministic counters).

### 2.6 `witness` — full-D witness in LOCALDOMCHECK (kept, strong)
*Design.* While scanning G_cl^Tr(s) for a Tr-dominator, return immediately if a point also has p_1 ≤ g_1 (then g is
dominated by an expanded path — same decision as Maya by §1.4); otherwise fall back as Maya does.
*Evidence.* Bad fallbacks drop 207 vs 81,285 (g10M4-A50), 82 vs 119,642 (M5). Removing it costs 1.03–1.36x (E4, Table D).

### 2.7 `local_first` — LOCALDOMCHECK before CHOOSEH at generation (kept, strong)
Both are side-effect-free predicates, so "push iff CHOOSEH ≠ ∅ ∧ ¬LOCALDOMCHECK" is order-independent; the pushed node,
its heuristic index and the OPEN sequence are unchanged. CHOOSEH calls drop 4.2x (g10M4: 1,243,670 → 297,332;
apex-g10M4: 252,312 → 84,512). Removing it costs 1.19–2.40x (E4). This single reordering removes most of the work that
Roi's tree was designed to accelerate (see §2.9).

### 2.8 `flatH`, `redund`, `suffix_ideal`, `fastpath` — cheap CHOOSEH mechanisms
* `flatH` (contiguous Tr(H(s))): 0.98–1.26x (E4 `no_flatH_redund`). Kept.
* `redund` (redundancy pointers r_j = 1 + max{i < j : Tr(h_i) ≤ Tr(h_j)}; skip h_j once h_i is refuted; at reinsertion
  h[start−1] is known refuted by the global check): cmp_chooseh 10.4 M → 1.6 M (g10M4-A50). Wall effect small once the
  other mechanisms are on (0.94–1.09x, E4) but large in the Maya-order context (flatT_lin_r_id 1.26–1.30x, E3b).
  The O(K²) build cost 26.6 % of runtime on APEX until a 48-element antichain probe was added (skips the pass when the
  prefix has no redundancy). Kept (with probe).
* `suffix_ideal` (componentwise min of Tr(h_j), j ≥ start; one Dom query proves "no valid heuristic left"): prunes
  134k–144k of 194k empty calls on apex-g10M4 in Maya order, but when it fails it costs a full non-dominated query;
  net 0.95–1.12x alone, 0.83–1.08x combined with `redund` (E3). Not in the final config.
* `fastpath` (test h[start] before any tree): in the local-first context 86 % of landmark calls and 21 % of APEX calls
  return h[start]. It is mandatory for top-down trees (lexTD without it: 0.85x on g10M4-A50; roi without it 0.89x on
  g15M3) and implicit in bottom-up traversal (first test is h[start]).

### 2.9 `chooseh` variants (linear / lexTD / lexBU / roi), `hX`, `leafB` — Version 2, Roi
*Cost model derived and confirmed.* Every CHOOSEH that returns an index must certify it with one **non-dominated**
query Dom(T, Tr(g)+Tr(h)) (full traversal of T, or a Rule-2 accept which is equally a non-dominated query). A heuristic
tree can only save the *refutation* queries, which on a flat T are cheap (early exit). Rule 1 (discard) queries on
loose boxes are small q = Tr(g)+N.min that are usually **not** dominated → full-T cost when they fail. Therefore:
* with a flat T, trees lose badly on APEX (lexBU 0.34x, roi 0.62x of linear, E3b) and win modestly on landmark sets
  (lexBU+fp+ideal+redund 1.37x/1.43x, M4/M5);
* with a K-d T those failed small-q queries are cheap (lo-corner pruning), and Roi's spatial tree wins on APEX
  (1.62x, E3b; 1.2–1.9x across the three APEX instances, E3c). **The Roi–Shahaf synergy is real, but only in this direction.**
*Top-down vs bottom-up.* In an index-ordered tree bottom-up traversal visits only right siblings of the path from
leaf(start) (boxes cover only indices ≥ start, tested in index order, first accept/leaf hit is final); top-down tests
ancestors whose boxes include the refuted prefix. Measured: lexBU 1.25x vs lexTD 1.02x (g10M4, flat T), 1.29x vs 0.99x (M5).
In Roi's *spatial* tree a bottom-up walk has no meaning for index order (ancestors of leaf(start) do not bound [start, K)),
which is exactly why `src` V2 had to fabricate a heuristic. The spatial tree needs top-down + fast path + index pruning
(`max_idx < start`, `min_idx ≥ best`) + children in `min_idx` order.
*`filterT`* (pass shrinking candidate subsets of T down the tree): 24 s vs 1.9 s on apex-g10M4 — filtering destroys
the early exit. **Discarded.**
*Threshold hX (the "≈ 50" hypothesis).*
* Maya-order context: **lower is better** — hX = 0 is best on all three APEX instances (1.89x, 1.20x, 1.56x vs linear);
  hX = 64 already loses 3–10 % (1.69x / 1.11x / 1.52x), hX = 1024 most of the gain (1.22x / 0.98x / 1.32x) (E3c).`n  This contradicts "build only above ~50".
* Local-first context: the tree is **slower than the best linear configuration at every hX** on apex-g8M5 (linear
  0.287 s vs best tree 0.326 s) and apex-g10M3 (linear with flat T 0.052 s vs best tree 0.062 s); on apex-g10M4 only
  hX = 1024 is slightly ahead (0.610 vs 0.689 s, within noise). Tree build alone is 7–9 % of runtime.
* Landmark MVHs (K ≤ 200): no hX gives a significant gain over linear + redundancy pointers (E3: all within ±10 %).
* `leafB` ∈ {4, 8, 16, 32}: 8–32 equivalent; 4 is worst (E3c).

### 2.10 Layout: `PAD_NODES` (compile-time) — padded (MAX_DIM = 10) vs dimension-sized nodes
E2b: no measurable difference (g10M4 0.945 vs 1.088 s, M5 3.89 vs 3.86 s; noise dominates). The frontier trees fit in
L2/L3 at these sizes. The "cache-friendly layout" hypothesis is **not supported** here.

### 2.11 Other knobs
* `branchless` dominance kernel: no gain (correct_m3). Discarded.
* `lastwit` (retest last dominator first): cmp_chooseh −70 % but wall 0.92–1.31x (E4). Discarded.
* `gcl_prune` (keep only the full-D Pareto set of G_cl(s)): **harmful** 1.13–3.28x slower (E4; removal pass on every
  expansion). Discarded.

---

## 3. Current best-known configuration

Prototype (`proto.exe`): `flatH=1 local_first=1 witness=1 redund=1 localX=8 targetX=8 promoteC=64` (**FULL**, E4).
Ported as `L_NAMOA_DR_MVH_FAST` (staged, `scratchpad/proposal/`): same mechanisms, `heuristic_tree_min = ∞`.

**Roi's static heuristic tree is not used in the winning configuration** (`htrees`/`heuristic_trees = 0` in every FAST
row, `ch_tree = 0` in FULL). Reason: after the local-first reordering the remaining CHOOSEH calls are few and mostly
answered by h[start]; the tree's build cost (7–9 %) and traversal overhead are not repaid (E3c lf rows, E4
`plus_roi1024_kdT` 0.92–1.17). It **is** the right choice when the solver keeps Maya's order (CHOOSEH before the local
check) and the MVH is APEX-like: then `chooseh=3 fastpath=1 hX=0` with a K-d T gives 1.2–1.9x over linear.
`FAST@FAST_HTREE=1024` (Roi tree only for |H(s)| ≥ 1024) is a within-noise alternative (Table A).

Measured speedups of FAST over Maya are in the comparison report (1.9–2.7x at M=3, 5–11x at M=4; prototype FULL 26x at
M=5 and 46x at M=6).

---

## 4. Proposed `src/` change set (not applied — needs approval)

`scratchpad/proposal/proposal_clean.patch` (`git apply --check` passes):
1. `src/include/fast_mvh/kdtree/hybrid_frontier.h` — `HybridFrontier<D>` (§2.1, 2.2, 2.5, 2.6).
2. `src/include/fast_mvh/heuristic_index.h` — `HeuristicIndex<D>` (flat Tr(H), redundancy pointers + probe, optional Roi tree with fast path).
3. `src/include/fast_mvh/solvers/l_namoa_dr_mvh_fast.h`, `src/src/solvers/l_namoa_dr_mvh_fast.cpp` — V3 hybrid solver (§2.7).
4. `src/src/main.cpp` — `--algorithm L_NAMOA_DR_MVH_FAST`, `--h-tree-min`, `--promote-min`, `--promote-scan`.
   (Not compiled locally: no Boost.ProgramOptions here; the solver itself is compiled and verified via the harness.)
5. `CMakeLists.txt` — add the new .cpp.
6. Bug fixes: V1 bounds check; V2 `choose_h_bottom_up` → fast path + exact top-down (no synthetic heuristic);
   `choose_h_dual` single query buffer + `min_idx` child order.
The FAST solver was verified bit-identical on all 9 instances it ran on (Table A of the comparison report).

---

## 5. Open / unresolved leads

* **Final head-to-head incomplete**: killed on g10M5-A50 after MAYA, SHAHAF (3 reps) and ORIG (1 rep). Not run in the
  harness: V5/FAST on g10M5-A50, g10M6-A25, the three APEX instances, legacy K50, g12M5-A25 (Maya 366 s).
  Prototype FULL numbers exist for M5/M6/APEX (E4), but the ported FAST binary was only checked on apex-g10M4 there.
* **M = 6 scaling**: one admissible M6 instance measured (Maya 72.6 s, FULL 1.58 s); grid8_M6-A25 (≈ 71k solutions)
  never got a Maya reference (estimated ≫ 15 min).
* M = 3 frontier trees: even the adaptive rule is only parity; a flat SIMD-friendly layout (SoA, 32-bit costs when
  the maximum g + h fits) is untested.
* OPEN/allocation overhead (≈ 40 % of FULL's time: `shared_ptr<Node>`, three `std::vector` per node) was not touched.
  Heap tie-breaking depends only on the comparison sequence, so a compact node type would keep trajectories identical
  within one standard library — note that bit-identity is only defined per standard library (MSVC vs libc++ heaps may
  break ties differently).
* The `std::vector<HybridFrontier>` per state reserves nothing up front; memory footprint was not measured.
* A stray untracked file named `-` (200 KB, created 11:53 by a mis-routed output path) sits in the repository root; it
  was **not** deleted (deletion needs your confirmation) and is not committed.
* `harness.exe` was deleted once and rebuilt at the very start of the session (a scratch build artifact) — noted here
  for transparency given the no-unconfirmed-deletion rule.
