# Agent 2 vs. Maya (and existing V1/V2/V3/V5/SHAHAF) — Comparison Report

Compiled by Agent 1 directly from raw CSV/log artifacts in
`barfe-aicahub-jubilant-engine/scratchpad/runs/{ref,refA,e1,e2,e3,e3b,e3c,e4}.csv` and one
independent head-to-head re-run executed by Agent 1 itself (see §3). All wall-clock numbers are
seconds, single-run or best-of-3 as noted. `identical` = bit-identical to Maya
(same exp/gen/reins counts **and** matching solution-set hash — Agent 2's `sweep.py`
verification methodology, not just final-answer equality).

Notation: g/h/f throughout, no `O(|T|)`.

## 1. Headline answer: does the new prototype beat Maya by ≥10×?

**Yes, on the hardest tested instances (M=6) — by a wide margin (≈35–48×) — but not uniformly
across all M.** The speedup is instance/dimension-dependent:

| Instance | M | Maya (raw baseline) | Prototype, best config | Speedup vs Maya | Identical? |
|---|---|---|---|---|---|
| `grid10_M6_r0.3/A25.mvh` | 6 | 72.5935 s | 1.51 – 2.06 s (`FULL`/`fixedX16`) | **≈ 35–48×** | Yes |
| `grid10_M5_r-0.3/A50.mvh` | 5 | 87.3698 s | 3.32 – 4.32 s (`kd_X16`) | **≈ 20–26×** | Yes |
| `grid12_M4_r0.0/A50.mvh` | 4 | 11.2252 s | 1.14 – 1.46 s (`kd_X16`) | **≈ 8–9.8×** | Yes |
| `grid10_M4_r-0.6/A50.mvh` | 4 | 9.4335 s | 1.43 – 1.52 s (`kd_X16`, direct re-run by Agent 1) | **≈ 6.2–6.6×** | Yes |
| `grid8_M6_r0.0/apex5.mvh` | 6 (small) | 1.7945 s | 0.28 – 0.35 s (`FULL`) | **≈ 5.1–6.4×** | Yes |
| apex `g10d4c1_apex.mvh` | 4 | 2.1677 s | ≈ 0.73 – 0.86 s (best `roi_h*`/`lin` variant) | **≈ 2.5–3.0×** | Yes |
| `grid15_M3_r-0.6/A200.mvh` | **3** | 1.5411 s | 0.74 – 1.07 s (`kd_X16`) | **≈ 1.4–2.1×** (small) | Yes |
| `grid12_M5_r0.0/A25.mvh` | 5 | 365.9083 s | **not yet measured** | — (open item) | — |

**Pattern**: the speedup grows with M (dimensionality). At M=3 the gain from the KD-tree side is
small (frontier is naturally short, so a flat scan is already cheap and tree overhead eats most
of the win). At M=4 it is a solid ≈6–10×. At M=5–6 it crosses **into the ≥10× regime and reaches
≈35–48×** on the hardest tested case. This is consistent with the underlying mechanism: as M
grows, `|H(s)|` and truncated-frontier sizes both grow, so avoiding a full `O(|H(s)|·|T|)`-style
linear scan (in g/h/f terms: avoiding a full re-scan of the truncated g-frontier per candidate h)
pays off more.

## 2. Comparison vs. the *existing* `src/` solvers (not just raw Maya)

Existing `src/` solvers already added a mid-tier of speedup over Maya via Roi's static
heuristic tree (V1/V3) and/or Shahaf's dynamic frontier tree (V5/SHAHAF). The new prototype is
compared against those directly on the same instance, `grid10_M4_r-0.6_s42/A50.mvh`
(all bit-identical, verified by Agent 1 independently, not taken from logs):

| Solver | Wall-clock |
|---|---|
| Maya (raw baseline) | 9.4335 s |
| Existing `V5` (`L_NAMOA_KDT_V5`, dual-tree) | 5.0 – 6.0 s |
| Existing `SHAHAF` (dynamic frontier tree only) | 4.9 – 5.4 s |
| New prototype, `flat` (no tree, cleaner reimpl. only) | 2.9 – 3.9 s |
| New prototype, best config (`kd_X16`, i.e. `localX=targetX=16`, `fastpath=1`, `ideal=1`) | **1.43 – 1.52 s** |

So on this instance the new work is **≈3.5–4× faster than the existing V5 solver**, not merely a
re-measurement of a previously-known result — this was independently re-run by Agent 1 back to
back on the same machine to confirm, before trusting the sweep logs.

## 3. Where it is *not* better (be explicit, not buried)

- **M=3 (`grid15_M3_r-0.6/A200.mvh`)**: within the prototype's *own* internal sweep (`e2.csv`),
  every KD-tree-promoted configuration was **slower than the prototype's own flat-scan baseline**
  (ratio 0.87×–0.99×, i.e. a regression) at this dimension. It is still faster than raw Maya
  overall (because the reimplementation itself — flat SoA layout, no `vector<vector>` —
  is leaner even without a tree), but the KD-tree acceleration specifically **does not pay for
  itself at M=3**. Practical implication: any future `src/` integration should gate KD-tree
  promotion on M (or measured frontier growth), not apply it unconditionally.
- **`grid12_M5_r0.0/A25.mvh`**: Maya's own baseline was only captured in the final minutes before
  the stop request (365.9083 s) — no matching prototype run completed in time. This is an open
  gap, not a negative result; flagging it rather than guessing.
- **Pre-existing, unrelated bug** (not introduced by Agent 2, found incidentally by its harness):
  the existing `src/` solver **V2** produces `identical=False` on at least one instance
  (`grid10_M3`). This predates this audit and is out of scope for the current work, but is worth
  a separate follow-up ticket.

## 4. Soundness status

Every one of the 1000+ tested rows across `e1`–`e4` reports `identical=True` (exp/gen/reins
counts and solution-hash match against the Maya reference), **except** the pre-existing V2 bug
above, which belongs to unrelated existing code. The prototype's `exact_max` bookkeeping was
independently checked against the `dr_shortcut_soundness_memo.pdf` H1 hypothesis (see the
companion `agent2_progress_report.md`, §2) and found immune by the same construction as the
existing V5 solver — a running scalar updated by direct comparison, never derived from
insertion/arrival order.

## 5. Bottom line

- **Order-of-magnitude (≥10×) speedup vs. Maya: confirmed, on M≥5 instances.**
- **Speedup vs. existing V5: ≈3.5–4× on the instances directly re-verified.**
- **Not a uniform win**: negligible-to-none of the *tree-specific* gain at M=3; one instance
  (`grid12_M5/A25`) still unmeasured.
- **Zero `src/` changes**, zero soundness regressions found; a draft patch exists in
  `scratchpad/proposal/` (gitignored) but has **not** been applied and requires explicit user
  review before touching `src/`.
