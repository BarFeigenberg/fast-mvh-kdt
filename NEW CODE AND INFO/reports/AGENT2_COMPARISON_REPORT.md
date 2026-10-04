# Agent 2 — Comparison Report: Maya vs existing `src/` solvers vs proposed hybrid

Base commit `8870a825d0b5c10b16d8928eed259ce6a9f2cfb5`; baseline submodule `0a2f9ea158a138ac9d40d0d84a15e64d5b66a7e1`.
Toolchain, timing protocol and instance definitions: see `docs/AGENT2_PROGRESS_REPORT.md` §0.
All numbers below are regenerated from archived CSVs by `python scratchpad/perf/make_tables.py`
(no new runs were made for this report).

## Solvers compared

| label | code | role in the 2×2 matrix |
|---|---|---|
| MAYA | `baselines/.../l_namoa_dr_mvh.cpp` (`L_NAMOA_DR_MVH`), unmodified | Variant 1, oracle |
| INSTR | `src` `L_NAMOA_KDT_CHOOSEH` with `use_kdt_chooseh=false` | Maya logic re-implemented with flat T |
| SHAHAF | `src` `L_NAMOA_DR_MVH_KDT` | Variant 2 (Maya + dynamic frontier K-d tree) |
| ORIG | `src` `L_NAMOA_KDT_CHOOSEH` (Roi top-down `choose_h`) | Variant 3 (Maya + Roi) |
| V1 / V2 / V3 | `src` `L_NAMOA_KDT_CHOOSEH` variants (h0 pre-test / bottom-up / tree only at reinsertion) | Roi sub-variants |
| V5 | `src` `L_NAMOA_KDT_CHOOSEH` `Variant::V5` (`choose_h_dual` + frontier K-d trees) | Variant 4 (dual tree) |
| FAST | proposed `L_NAMOA_DR_MVH_FAST` (staged in `scratchpad/proposal/`, compiled into the harness) | V3 hybrid, Roi tree off |
| FAST@FAST_HTREE=1024 | same, Roi tree for \|H(s)\| ≥ 1024 | hybrid with Roi tree |
| proto FULL | `proto.exe flatH=1 local_first=1 witness=1 redund=1 localX=8 targetX=8 promoteC=64` | prototype of FAST |
| proto none | `proto.exe` defaults (Maya's algorithm on contiguous storage, no trees) | layout-only control |

Harness command pattern (`scratchpad/runs/final_eval_partial/args.txt` has every exact command line):
`scratchpad/perf/harness2.exe <map_dir> 1 <goal> <M> <mvh> <ALGO> <sol_out>` with `PROTO_AFFINITY=4`
(driver: `python scratchpad/perf/final_eval.py scratchpad/perf/harness2.exe <run_dir> MAYA,SHAHAF,ORIG,V5,FAST,FAST@FAST_HTREE=1024 3 <inst:mvh:M:goal>...`).
Prototype: `python scratchpad/perf/sweep.py scratchpad/runs/e4.csv scratchpad/perf/cfg_e4.txt <specs>` with `REPS=3 PROTO_AFFINITY=4`
(specs in `scratchpad/perf/run_e4.ps1`).

### Counter semantics (important when reading `cmp`)
* Maya's baseline has **no** comparison counters (it is immutable), so `cmp = 0` for MAYA; its cost is only visible as wall time.
* SHAHAF: `cmp = cmpchk + cmpupd` (frontier K-d queries + updates).
* ORIG / V1–V3 / INSTR: `cmp = cmp_chooseh` (CHOOSEH comparisons only; local/global scans are uncounted).
* V5: `cmp_chooseh`, into which the `src` code also adds its K-d check and update comparisons.
* FAST: `cmp = cmpchk + cmpupd + cmp_chooseh + cmp_full` (everything). Split per component is available for the prototype (Table C).
* `full` = number of full-D fallback scans (Maya's `num_full_dominance_check`).
Hence `cmp` is **not** comparable across solvers; wall time, expansions and generations are.

## Headline

* **Correctness:** every row that completed is bit-identical to Maya (solution-file MD5 + `num_expansion` +
  `num_generation`) **except `src` V2**, which is not identical (43,420 vs 41,129 expansions on legacy g10M3-K50), and
  **`src` V1, which crashes** (out-of-bounds read) on legacy g10M4-K50.
* **FAST vs Maya** (same binary/compiler, admissible MVH): 1.88–2.69x at M = 3, **5.06–10.95x at M = 4**.
  Prototype FULL vs Maya: 10.4–10.6x at M = 4, **26.1x at M = 5**, **46.1x at M = 6**, 2.5x at M = 3, 3.4–6.3x on APEX MVHs.
* **FAST vs V5**: 2.21–3.23x at M = 3, 3.04–5.43x at M = 4.

### Where the new work does NOT win (stated plainly)
1. **K-d frontier trees at M = 3 are a regression.** Prototype with trees at every state: 0.85–0.89x of the flat
   prototype on g15M3-A200 (E1/E2); fixed X = 16: 0.89x. FAST avoids it only because the cost-model rule never promotes
   at M = 3 (0 frontier trees in all M = 3 rows) — i.e. at M = 3 the "win" is from layout, witness and ordering, not trees.
2. **The existing `src` K-d solvers are slower than Maya at M = 3**: SHAHAF 0.49–0.69x, V5 0.58–0.87x, ORIG 0.79–1.11x of
   Maya's speed (Table A, 3 instances). On the legacy K50 set V5/SHAHAF are also slower at M = 3 (0.60x, 0.57x).
3. **Roi's tree (ORIG) is slower than Maya on 4 of the 5 admissible M = 4 instances** (0.75–0.97x speed; g12M4: 11.0 s vs 13.7 s; the exception is g10M4-A50 at 1.13x)
   and at M = 5 is at parity (76.4 s vs 77.9 s), despite far fewer "heuristic" comparisons — its min-box tests are full
   scans of the flat T.
4. **Roi's tree inside the hybrid does not pay**: `FAST@FAST_HTREE=1024` is within noise of FAST on M = 4 and *slower*
   on g15M3-A100 (0.580 vs 0.427 s); prototype `plus_roi1024_kdT` 0.92–1.17 of FULL (Table D). With Maya's
   generation order it would help on APEX MVHs (1.2–1.9x, E3c) but that ordering is 1.2–2.4x slower overall.
5. The partial final run is missing V5/FAST rows for M = 5, M = 6, APEX and legacy instances (run was stopped);
   for those only prototype numbers exist (Table C), and the ported FAST binary was checked bit-identical on
   apex-g10M4, g10M3-A50 and g10M4-A50 (`scratchpad/runs/fastcheck`).

## Speedup summary (from Table A; speed relative to MAYA, higher is better)

| instance | SHAHAF | ORIG | V5 | FAST | FAST@HTREE=1024 |
|---|---|---|---|---|---|
| g10M3-A50 | 0.54 | 0.79 | 0.58 | **1.88** | 1.80 |
| g15M3-A100 | 0.69 | 1.06 | 0.87 | **2.69** | 1.98 |
| g15M3-A200 | 0.49 | 1.11 | 0.86 | 1.90 | **2.49** |
| g10M4-A10 | 1.52 | 0.75 | 1.43 | 5.06 | **5.23** |
| g10M4-A25 | 1.74 | 0.89 | 2.00 | 8.50 | **8.66** |
| g10M4-A50 | 1.96 | 1.13 | 2.33 | 7.08 | **10.64** |
| g10M4-A100 | 1.41 | 0.97 | 2.34 | 9.44 | **11.15** |
| g12M4-A50 | 1.80 | 0.81 | 2.02 | **10.95** | 9.90 |
| g10M5-A50 | 3.27 | 1.02 | not run | not run | not run |

(FAST vs FAST@HTREE=1024 differences are ≤ ~1.5x in both directions and correlate with run order on a throttling laptop;
the two configs build 0 heuristic trees on every landmark instance — `heuristic_trees = 0` in Table A2 — so they execute
the same code there and the spread is pure measurement noise. This also bounds the noise level of Table A.)

# Raw tables

## Table A - harness binary (same compiler/flags), min wall of reps (MAYA: 1 rep, others: 3)

| instance | MAYA | SHAHAF | ORIG | V5 | FAST | FAST@FAST_HTREE=1024 | FAST vs Maya | FAST vs V5 | exp / gen | identical (all rows) |
|---|---|---|---|---|---|---|---|---|---|---|
| grid10_M3_r-0.6_s42__A50 | 0.166 (n=1) | 0.305 (n=3) | 0.211 (n=3) | 0.284 (n=3) | 0.088 (n=3) | 0.092 (n=3) | 1.88x | 3.23x | 35862 / 67241 | REF/True |
| grid15_M3_r-0.3_s42__A100 | 1.150 (n=1) | 1.676 (n=3) | 1.081 (n=3) | 1.325 (n=3) | 0.427 (n=3) | 0.580 (n=3) | 2.69x | 3.10x | 139010 / 261618 | REF/True |
| grid15_M3_r-0.6_s42__A200 | 1.387 (n=1) | 2.852 (n=3) | 1.246 (n=3) | 1.614 (n=3) | 0.731 (n=3) | 0.558 (n=3) | 1.90x | 2.21x | 170052 / 338227 | REF/True |
| grid10_M4_r-0.6_s42__A10 | 5.035 (n=1) | 3.302 (n=3) | 6.748 (n=3) | 3.523 (n=3) | 0.995 (n=3) | 0.962 (n=3) | 5.06x | 3.54x | 185394 / 291139 | REF/True |
| grid10_M4_r-0.6_s42__A25 | 7.257 (n=1) | 4.179 (n=3) | 8.126 (n=3) | 3.634 (n=3) | 0.854 (n=3) | 0.838 (n=3) | 8.50x | 4.25x | 186703 / 295334 | REF/True |
| grid10_M4_r-0.6_s42__A50 | 9.438 (n=1) | 4.806 (n=3) | 8.366 (n=3) | 4.055 (n=3) | 1.333 (n=3) | 0.887 (n=3) | 7.08x | 3.04x | 187070 / 296772 | REF/True |
| grid10_M4_r-0.6_s42__A100 | 9.613 (n=1) | 6.819 (n=3) | 9.944 (n=3) | 4.105 (n=3) | 1.019 (n=3) | 0.862 (n=3) | 9.44x | 4.03x | 187230 / 297169 | REF/True |
| grid12_M4_r0.0_s42__A50 | 11.044 (n=1) | 6.141 (n=3) | 13.695 (n=3) | 5.473 (n=3) | 1.008 (n=3) | 1.116 (n=3) | 10.95x | 5.43x | 214670 / 334599 | REF/True |
| grid10_M5_r-0.3_s42__A50 | 77.922 (n=1) | 23.815 (n=3) | 76.351 (n=1) | not run | not run | not run | - | - | 448472 / 717484 | REF/True |

### Table A2 - counters per solver (first rep): `full` = full-D fallback checks, `cmp` = solver's own aggregate comparison counter

| instance | algo | sols | exp | gen | reins | full | cmp | frontier_trees | heuristic_trees | redundant_h | identical |
|---|---|---|---|---|---|---|---|---|---|---|---|
| grid10_M3_r-0.6_s42__A50 | MAYA | 1602 | 35862 | 67241 | 4559 | 19217 | 0 |  |  |  | REF |
| grid10_M3_r-0.6_s42__A50 | SHAHAF | 1602 | 35862 | 67241 | 4559 | 21442 | 3575134 |  |  |  | True |
| grid10_M3_r-0.6_s42__A50 | ORIG | 1602 | 35862 | 67241 | 4559 | 19217 | 31299117 |  |  |  | True |
| grid10_M3_r-0.6_s42__A50 | V5 | 1602 | 35862 | 67241 | 4559 | 21442 | 1911545 |  |  |  | True |
| grid10_M3_r-0.6_s42__A50 | FAST | 1602 | 35862 | 67241 | 4559 | 6999 | 10437530 | 0 | 0 | 4020 | True |
| grid10_M3_r-0.6_s42__A50 | FAST@FAST_HTREE=1024 | 1602 | 35862 | 67241 | 4559 | 6999 | 10437530 | 0 | 0 | 4020 | True |
| grid15_M3_r-0.3_s42__A100 | MAYA | 2178 | 139010 | 261618 | 10873 | 27262 | 0 |  |  |  | REF |
| grid15_M3_r-0.3_s42__A100 | SHAHAF | 2178 | 139010 | 261618 | 10873 | 29252 | 19607855 |  |  |  | True |
| grid15_M3_r-0.3_s42__A100 | ORIG | 2178 | 139010 | 261618 | 10873 | 27262 | 104427912 |  |  |  | True |
| grid15_M3_r-0.3_s42__A100 | V5 | 2178 | 139010 | 261618 | 10873 | 29252 | 5933156 |  |  |  | True |
| grid15_M3_r-0.3_s42__A100 | FAST | 2178 | 139010 | 261618 | 10873 | 3101 | 33136557 | 0 | 0 | 19666 | True |
| grid15_M3_r-0.3_s42__A100 | FAST@FAST_HTREE=1024 | 2178 | 139010 | 261618 | 10873 | 3101 | 33136557 | 0 | 0 | 19666 | True |
| grid15_M3_r-0.6_s42__A200 | MAYA | 2776 | 170052 | 338227 | 9182 | 19498 | 0 |  |  |  | REF |
| grid15_M3_r-0.6_s42__A200 | SHAHAF | 2776 | 170052 | 338227 | 9182 | 21258 | 42361073 |  |  |  | True |
| grid15_M3_r-0.6_s42__A200 | ORIG | 2776 | 170052 | 338227 | 9182 | 19498 | 116445961 |  |  |  | True |
| grid15_M3_r-0.6_s42__A200 | V5 | 2776 | 170052 | 338227 | 9182 | 21258 | 6420084 |  |  |  | True |
| grid15_M3_r-0.6_s42__A200 | FAST | 2776 | 170052 | 338227 | 9182 | 2825 | 42368505 | 0 | 0 | 40193 | True |
| grid15_M3_r-0.6_s42__A200 | FAST@FAST_HTREE=1024 | 2776 | 170052 | 338227 | 9182 | 2825 | 42368505 | 0 | 0 | 40193 | True |
| grid10_M4_r-0.6_s42__A10 | MAYA | 12667 | 185394 | 291139 | 6336 | 46972 | 0 |  |  |  | REF |
| grid10_M4_r-0.6_s42__A10 | SHAHAF | 12667 | 185394 | 291139 | 6336 | 51635 | 30696922 |  |  |  | True |
| grid10_M4_r-0.6_s42__A10 | ORIG | 12667 | 185394 | 291139 | 6336 | 46972 | 956264143 |  |  |  | True |
| grid10_M4_r-0.6_s42__A10 | V5 | 12667 | 185394 | 291139 | 6336 | 51635 | 22965481 |  |  |  | True |
| grid10_M4_r-0.6_s42__A10 | FAST | 12667 | 185394 | 291139 | 6336 | 1804 | 49313594 | 31 | 0 | 545 | True |
| grid10_M4_r-0.6_s42__A10 | FAST@FAST_HTREE=1024 | 12667 | 185394 | 291139 | 6336 | 1804 | 49313594 | 31 | 0 | 545 | True |
| grid10_M4_r-0.6_s42__A25 | MAYA | 12667 | 186703 | 295334 | 8377 | 55069 | 0 |  |  |  | REF |
| grid10_M4_r-0.6_s42__A25 | SHAHAF | 12667 | 186703 | 295334 | 8377 | 59875 | 56066823 |  |  |  | True |
| grid10_M4_r-0.6_s42__A25 | ORIG | 12667 | 186703 | 295334 | 8377 | 55069 | 1281572840 |  |  |  | True |
| grid10_M4_r-0.6_s42__A25 | V5 | 12667 | 186703 | 295334 | 8377 | 59875 | 30389537 |  |  |  | True |
| grid10_M4_r-0.6_s42__A25 | FAST | 12667 | 186703 | 295334 | 8377 | 2118 | 50816680 | 31 | 0 | 1621 | True |
| grid10_M4_r-0.6_s42__A25 | FAST@FAST_HTREE=1024 | 12667 | 186703 | 295334 | 8377 | 2118 | 50816680 | 31 | 0 | 1621 | True |
| grid10_M4_r-0.6_s42__A50 | MAYA | 12667 | 187070 | 296772 | 8754 | 56631 | 0 |  |  |  | REF |
| grid10_M4_r-0.6_s42__A50 | SHAHAF | 12667 | 187070 | 296772 | 8754 | 62005 | 95424502 |  |  |  | True |
| grid10_M4_r-0.6_s42__A50 | ORIG | 12667 | 187070 | 296772 | 8754 | 56631 | 1518582320 |  |  |  | True |
| grid10_M4_r-0.6_s42__A50 | V5 | 12667 | 187070 | 296772 | 8754 | 62005 | 35832431 |  |  |  | True |
| grid10_M4_r-0.6_s42__A50 | FAST | 12667 | 187070 | 296772 | 8754 | 2210 | 51616184 | 31 | 0 | 3652 | True |
| grid10_M4_r-0.6_s42__A50 | FAST@FAST_HTREE=1024 | 12667 | 187070 | 296772 | 8754 | 2210 | 51616184 | 31 | 0 | 3652 | True |
| grid10_M4_r-0.6_s42__A100 | MAYA | 12667 | 187230 | 297169 | 8963 | 57974 | 0 |  |  |  | REF |
| grid10_M4_r-0.6_s42__A100 | SHAHAF | 12667 | 187230 | 297169 | 8963 | 63626 | 172016267 |  |  |  | True |
| grid10_M4_r-0.6_s42__A100 | ORIG | 12667 | 187230 | 297169 | 8963 | 57974 | 1770875269 |  |  |  | True |
| grid10_M4_r-0.6_s42__A100 | V5 | 12667 | 187230 | 297169 | 8963 | 63626 | 41145948 |  |  |  | True |
| grid10_M4_r-0.6_s42__A100 | FAST | 12667 | 187230 | 297169 | 8963 | 2316 | 52110459 | 31 | 0 | 7808 | True |
| grid10_M4_r-0.6_s42__A100 | FAST@FAST_HTREE=1024 | 12667 | 187230 | 297169 | 8963 | 2316 | 52110459 | 31 | 0 | 7808 | True |
| grid12_M4_r0.0_s42__A50 | MAYA | 13709 | 214670 | 334599 | 9543 | 53497 | 0 |  |  |  | REF |
| grid12_M4_r0.0_s42__A50 | SHAHAF | 13709 | 214670 | 334599 | 9543 | 55792 | 114472003 |  |  |  | True |
| grid12_M4_r0.0_s42__A50 | ORIG | 13709 | 214670 | 334599 | 9543 | 53497 | 2793931400 |  |  |  | True |
| grid12_M4_r0.0_s42__A50 | V5 | 13709 | 214670 | 334599 | 9543 | 55792 | 52836048 |  |  |  | True |
| grid12_M4_r0.0_s42__A50 | FAST | 13709 | 214670 | 334599 | 9543 | 1155 | 53757701 | 49 | 0 | 5548 | True |
| grid12_M4_r0.0_s42__A50 | FAST@FAST_HTREE=1024 | 13709 | 214670 | 334599 | 9543 | 1155 | 53757701 | 49 | 0 | 5548 | True |
| grid10_M5_r-0.3_s42__A50 | MAYA | 48950 | 448472 | 717484 | 19982 | 97580 | 0 |  |  |  | REF |
| grid10_M5_r-0.3_s42__A50 | SHAHAF | 48950 | 448472 | 717484 | 19982 | 100229 | 442079705 |  |  |  | True |
| grid10_M5_r-0.3_s42__A50 | ORIG | 48950 | 448472 | 717484 | 19982 | 97580 | 15701124547 |  |  |  | True |

## Table B - existing src variants on the legacy (inadmissible) K50 landmark MVH, single rep

| instance | algo | sols | exp | gen | reins | full | cmp | wall (s) | identical vs MAYA |
|---|---|---|---|---|---|---|---|---|---|
| grid10_M3_r-0.6_s42_K50 | MAYA | 1559 | 41129 | 81390 | 6258 | 51198 | 0 | 0.2671 | REF |
| grid10_M3_r-0.6_s42_K50 | INSTR | 1559 | 41129 | 81390 | 6258 | 51198 | 23965763 | 0.2791 | True |
| grid10_M3_r-0.6_s42_K50 | ORIG | 1559 | 41129 | 81390 | 6258 | 51198 | 50395925 | 0.4052 | True |
| grid10_M3_r-0.6_s42_K50 | V1 | 1559 | 41129 | 81390 | 6258 | 51198 | 38538834 | 0.3273 | True |
| grid10_M3_r-0.6_s42_K50 | V2 | 1559 | 43420 | 83760 | 7267 | 63384 | 10032509 | 0.2827 | False |
| grid10_M3_r-0.6_s42_K50 | V3 | 1559 | 41129 | 81390 | 6258 | 51198 | 23238805 | 0.2668 | True |
| grid10_M3_r-0.6_s42_K50 | V5 | 1559 | 41129 | 81390 | 6258 | 54009 | 2665359 | 0.4441 | True |
| grid10_M3_r-0.6_s42_K50 | SHAHAF | 1559 | 41129 | 81390 | 6258 | 54009 | 4675256 | 0.4664 | True |
| grid10_M4_r-0.6_s42_K50 | MAYA | 11444 | 185331 | 311955 | 27210 | 265468 | 0 | 13.2226 | REF |
| grid10_M4_r-0.6_s42_K50 | INSTR | 11444 | 185331 | 311955 | 27210 | 265468 | 1318031702 | 11.9264 | True |
| grid10_M4_r-0.6_s42_K50 | ORIG | 11444 | 185331 | 311955 | 27210 | 265468 | 2487348165 | 17.6393 | True |
| grid10_M4_r-0.6_s42_K50 | V3 | 11444 | 185331 | 311955 | 27210 | 265468 | 1320365524 | 13.4025 | True |
| grid10_M4_r-0.6_s42_K50 | V5 | 11444 | 185331 | 311955 | 27210 | 272396 | 64672424 | 11.6097 | True |
| grid10_M4_r-0.6_s42_K50 | SHAHAF | 11444 | 185331 | 311955 | 27210 | 272396 | 110131853 | 11.8508 | True |

## Table C - prototype (proto.exe) FULL configuration vs Maya reference (Maya from refA/ref, 1 rep; proto min of 3)

| instance | Maya wall | proto `none` | proto FULL | FULL vs Maya | cmpchk | cmpupd | cmp_chooseh | cmp_full | exp / gen | identical |
|---|---|---|---|---|---|---|---|---|---|---|
| grid10_M4_r-0.6_s42/A50.mvh | 9.434 | 2.749 | 0.906 | 10.41x | 34995013 | 8449901 | 4467793 | 3703477 | 187070 / 296772 | True |
| grid12_M4_r0.0_s42/A50.mvh | 11.225 | 4.194 | 1.063 | 10.56x | 38969205 | 9753731 | 2686083 | 2348682 | 214670 / 334599 | True |
| grid10_M5_r-0.3_s42/A50.mvh | 87.370 | 25.714 | 3.348 | 26.10x | 54882016 | 12824500 | 7060423 | 10348094 | 448472 / 717484 | True |
| grid_10x10_d4_tradeoff_cyc1/g10d4c1_apex.mvh | 2.168 | 1.278 | 0.642 | 3.37x | 11453123 | 2575774 | 56080772 | 14959755 | 47305 / 83990 | True |
| grid15_M3_r-0.6_s42/A200.mvh | 1.541 | 1.054 | 0.613 | 2.51x | 27566060 | 4275869 | 8759560 | 1767016 | 170052 / 338227 | True |
| grid10_M6_r0.3_s42/A25.mvh | 72.594 | 23.000 | 1.576 | 46.07x | 35302383 | 8868446 | 5190504 | 16966196 | 197696 / 251680 | True |
| grid8_M6_r0.0_s42/apex5.mvh | 1.794 | 1.152 | 0.286 | 6.28x | 7174083 | 1570340 | 18428827 | 3276295 | 32318 / 43645 | True |

## Table D - E4 ablation: wall(cfg) / wall(FULL) (>1 means removing the mechanism costs time), min of 3

| cfg | grid10_M4_r-0.6_s42/A50.mvh | grid12_M4_r0.0_s42/A50.mvh | grid10_M5_r-0.3_s42/A50.mvh | grid_10x10_d4_tradeoff_cyc1/g10d4c1_apex.mvh | grid15_M3_r-0.6_s42/A200.mvh | grid10_M6_r0.3_s42/A25.mvh | grid8_M6_r0.0_s42/apex5.mvh |
|---|---|---|---|---|---|---|---|
| none | 3.03 | 3.95 | 7.68 | 1.99 | 1.72 | 14.60 | 4.03 |
| FULL | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 |
| no_local_first | 1.20 | 1.23 | 1.19 | 1.87 | 1.21 | 1.49 | 2.40 |
| no_witness | 1.29 | 1.25 | 1.36 | 1.11 | 1.03 | 1.10 | 1.18 |
| no_redund | 0.99 | 1.01 | 1.09 | 0.96 | 1.06 | 0.94 | 1.04 |
| no_kd | 1.59 | 1.84 | 2.90 | 1.29 | 1.02 | 6.31 | 2.63 |
| no_flatH_redund | 1.26 | 1.02 | 1.19 | 0.98 | 1.03 | 1.01 | 1.03 |
| fixedX16 | 1.07 | 0.94 | 1.12 | 0.99 | 1.12 | 0.96 | 0.97 |
| plus_roi1024_kdT | 1.16 | 0.97 | 0.94 | 0.92 | 1.03 | 0.96 | 1.17 |
| plus_gcl_prune | 1.98 | 1.78 | 3.28 | 1.13 | 1.29 | 1.85 | 1.24 |
| plus_lastwit | 1.03 | 1.31 | 1.03 | 0.92 | 1.03 | 0.98 | 1.05 |
