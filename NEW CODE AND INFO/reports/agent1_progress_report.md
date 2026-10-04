# Agent 2 Audit — Progress Report (Compiled by Agent 1)

**Status**: Agent 2 was asked to stop, report, and commit at 2026-09-25 12:50 local time. This
report was independently compiled by Agent 1 from Agent 2's own scratchpad artifacts
(`scratchpad/perf/proto.hpp`, `scratchpad/runs/*.csv`, `*.log`) in the
`barfe-aicahub-jubilant-engine` worktree (branch `barfe-aicahub-mvh-kdtree-perf-audit`), because
Agent 2 had not yet produced or committed its own written report at the time this was compiled.
No `src/` file was read for narrative purposes beyond what Agent 2 itself benchmarked against;
**no `src/` file was modified by either agent**. Reference oracle: Maya's baseline
(`baselines/bridging-mvh-dr/src/multivalued_heuristic/l_namoa_dr_mvh.cpp`), untouched throughout.

All notation below uses g/h/f (never `O(|T|)`), per project invariants.

---

## 1. What was built

Agent 2 did **not** just re-sweep existing `src/` solver configs. It wrote a from-scratch,
scratchpad-only reimplementation (`proto.hpp`, grown 34KB → 46.5KB over the session, recompiled
6+ times) that exposes every tunable as an explicit `Cfg` flag, plus a companion `Stats` struct
tracking `cmpchk` (dominance-check comparisons) / `cmpupd` (set-update comparisons), so each
mechanism below could be profiled in isolation and in combination.

### 1.1 Shahaf's dynamic state-frontier tree — promotion threshold

- **Original ask**: hypothesize threshold X (paths accumulated before building the tree),
  starting around 50–64, and verify empirically.
- **Finding**: swept X ∈ {0, 16, 32, 64, 128, 256, 512} for both the local frontier
  (`localX`) and the target frontier (`targetX`) across grid10/M4, grid12/M4, grid10/M5, and an
  apex-heuristic instance (`e1.csv`). Result: **low X (0–64) consistently beat high X
  (256/512)** — the opposite of the "wait for many paths" intuition. Winning value in every
  tested instance: **X = 16** for both `localX` and `targetX`. Rationale found empirically:
  KD-tree overhead per node is small relative to the growing linear-scan cost as frontier size
  increases, so promoting *early* pays off faster than the hypothesis assumed; waiting until
  X≈256/512 lets frontiers grow large in flat form first, which is strictly worse.
  A cost-model variant (`promoteC`: promote once the recent mean flat-scan comparison count
  exceeds a target, rather than a fixed count) was also implemented but did not beat the flat
  `X=16` threshold in testing.
- **Storage before promotion**: a flat SoA (`std::vector<size_t>`, `n*D` contiguous) array,
  scanned linearly with an early-exit weak-dominance test; on promotion the flat buffer is
  bulk-loaded into either a point-per-node KD-tree (Shahaf's original layout) or a bucketed
  variant (`BucketKD<D>`, tunable leaf capacity `bk_cap`) and then freed
  (`std::vector<size_t>().swap(flat)`).
- **Rebuild policy**: tree is rebuilt when `total > built + built/rb_div + rb_slack`
  (default `rb_div=4`, `rb_slack=64`), i.e. once ~25% tombstoned/dead entries accumulate.

### 1.2 Roi's static heuristic tree — build threshold & traversal strategy

- **Original ask**: evaluate build-threshold `hX` around 50, and whether/how to traverse
  (top-down vs bottom-up) when the current heuristic is not already valid.
- **Finding — this is the most important result**: in every winning configuration found
  (`e3.csv`/`e3b.csv`/`e3c.csv`), the heuristic-tree traversal counter **`ch_tree = 0`**. Roi's
  static tree is essentially **unused** in the fastest configuration, regardless of `hX`. The
  original question "top-down or bottom-up?" turned out to be largely moot, because CHOOSEH is
  resolved almost entirely by three cheap O(1)/near-O(1) shortcuts, tried in this precedence
  order inside `Solver::chooseh()`:
  1. **`fastpath`** — test the currently-assigned heuristic first; if it's already valid, skip
     everything else.
  2. **`suffix_ideal`** — an O(1) proof that no later heuristic index can help, by comparing
     `Tr(g) + min_{j>=start} Tr(h_j)` against the current frontier bound.
  3. **`redund`** — skip heuristic `h_j` if an earlier, already-refuted `h_i` satisfies
     `Tr(h_i) <= Tr(h_j)` (monotonic refutation propagation across the `H(s)` list).
  4. Only if all three shortcuts fail to resolve does it fall through to a **flat linear scan**
     (`flatH`: contiguous copy of `Tr(H(s))` instead of `vector<vector>`) — and the static tree
     traversal (`lexTD`/`lexBU`/`roi`-spatial) is reached only as a last resort, which in
     practice almost never triggers once `redund`+`fastpath`+`suffix_ideal` are all enabled.
  - Net effect: Roi's tree-build threshold question ("build only if |H(s)| >= X, X≈50") is
    superseded — the tree is rarely needed at all once the shortcuts are in place, independent
    of X.

### 1.3 V5 Hybrid integration — core micro-optimizations

- **`branchless` dominance kernel**: `dom_flat<BRANCHLESS>` replaces the early-exit branch
  (`if (a[k] < g[k]) break;`) with a branch-free accumulate (`ok &= (a[k] <= q[k])`) over the
  flat SoA layout — removes per-comparison branch misprediction cost on the hot path.
  Cache-friendly layout: points stored as flat contiguous `size_t[D]` rows instead of
  `vector<vector<size_t>>` (removes one pointer indirection + heap allocation per point).
- **`witness` / `lastwit` (witness caching)**: cache the index of the last point that
  successfully dominated a query (`lastw`), and re-test it first on the next query before
  scanning from the start — exploits temporal locality (the same witness frequently re-dominates
  consecutive queries at a given state).
- **`exact_max` bookkeeping** (directly relevant to a separate soundness question raised this
  session — see cross-reference below): rather than caching only a scalar `maxg0`, the
  prototype also tracks `maxg0_cnt` (count of live points achieving the current max). On
  deletion of a point at the max, the count is decremented; the max is only recomputed
  (`recompute_max()` / `bk.max0()`) once the count hits zero. This keeps the `max g0` bound exact
  after removals without a full rescan on every single deletion, while still being provably
  independent of insertion/traversal order (see Section 2).
- **`gcl_prune`**: keep only the full-D Pareto set of `G_cl(s)` (closed/expanded-goal-candidate
  set), rather than every raw candidate — reduces bookkeeping on generation.
- **`filterT`**: for the dual-tree interaction, pass a *filtered* candidate subset of the
  frontier `T` down into the heuristic-tree traversal rather than the full set — relevant only
  in the rare case the heuristic tree is actually reached (see 1.2).

## 2. Cross-reference to the DR-shortcut soundness question (this session, per user request)

Separately, this session investigated `docs/theory/dr_shortcut_soundness_memo.pdf` (hypothesis
H1: a naive "last-appended value" proxy for `max_{t in T} t[0]` is unsound under MVH) and the
companion `h1_back0_shortcut_soundness_audit.md` (H1 refuted against Maya's shipped baseline and
against V5's `max_g0_` field). Agent 2's prototype was independently checked against the same
invariant: `Cfg::exact_max` and the `maxg0`/`maxg0_cnt` update rule in `Front<D>::insert()` are a
direct comparison against a running scalar (`if (g[0] > maxg0) ...`), never derived from
"most recently appended" or traversal/arrival order — so the prototype is immune to H1 by the
same construction as V5, independent of this progress report's other findings.

## 3. Current best-known configuration(s)

- `localX = targetX = 16`
- `chooseh` may be left at any value — irrelevant in practice because `ch_tree` stays at 0
- `fastpath = true`, `suffix_ideal = true`, `redund = true`, `witness = true`, `lastwit = true`
- `branchless = true`, `flatH = true`
- `exact_max = true` (required for soundness; never disable this in a "perf" variant)
- Matches the sweep configs named `FULL` / `fixedX16` in `e4.csv`, and `kd_X16` / `lin_r` in
  earlier sweeps (`e1.csv`–`e3c.csv`).

## 4. Open / unresolved items at time of stop

- **`grid12_M5_r0.0_s42/A25.mvh`**: a fresh Maya reference baseline was captured
  (365.9083s wall, `refA/results.csv`) but no corresponding prototype-config run had completed
  before the stop instruction — this instance's speedup ratio is **not yet measured**.
- **M=6 sweep (`e4.csv`) only just completed** (12:47:43, immediately before the stop request)
  on two M=6 instances (`grid10_M6_r0.3/A25`, `grid8_M6_r0.0/apex5`) — not yet cross-checked
  against every earlier-session config combination (only the standard ablation list: `none`,
  `FULL`, `no_local_first`, `no_witness`, `no_redund`, `no_kd`, `no_flatH_redund`, `fixedX16`,
  `plus_roi1024_kdT`, `plus_gcl_prune`, `plus_lastwit`).
  See comparison report for the resulting numbers.
- A separate `scratchpad/proposal/` directory (patch files: `proposal.patch`,
  `proposal_clean.patch`, plus draft `.h`/`.cpp` files mirroring `src/include/fast_mvh/...`
  naming) was found mid-construction, apparently Agent 2 assembling a candidate patch to bring
  these optimizations into `src/`. **This patch has not been applied to `src/` and must not be,
  without explicit user review and sign-off per project rules.** It is preserved as-is in
  scratchpad (gitignored) pending the user's review.
- Agent 2 itself had not yet produced or committed a written report at the time of this
  compilation — this document and the companion comparison report were compiled directly from
  the raw artifacts by Agent 1 so the user would not need to wait further.
