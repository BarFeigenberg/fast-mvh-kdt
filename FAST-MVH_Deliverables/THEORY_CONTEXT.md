# FAST-MVH: Theoretical Context & Design Rationale

## Problem Setting

**Multi-Objective Shortest Path (MO-SPP):**
Given a directed graph G = (S, E, c) where costs are M-dimensional vectors c(e) ∈ ℝ≥0^M, find all non-dominated (Pareto-optimal) start-to-goal cost vectors.

**Two core bottlenecks:**
1. Frontier explosion: As solutions accumulate, state frontiers F(s) grow large; testing dominance becomes expensive
2. Heuristic evaluation: Under Multi-Valued Heuristics (MVHs), each path may test multiple heuristic vectors before finding one not refuted by current solutions

---

## Multi-Valued Heuristics (MVHs) vs. Single-Valued Heuristics (SVHs)

### Single-Valued (e.g., ALT, landmark-based)
```
h(s) = max{ℓ ∈ landmarks} dist(s, ℓ)
```
- Component-wise maximum is **admissible** because every landmark bounds every suffix
- Simple but often weak: trades tightness for generality
- No trade-off information captured

### Multi-Valued (e.g., A*pex, Backward Pareto Sets)
```
H(s) = ⟨h¹, h², ..., h^k(s)⟩  (lexicographically ordered)
```
- Each h^i describes a different trade-off among objectives
- h¹ ≤_lex h² ≤_lex ... ≤_lex h^k(s)
- Component-wise max is **NOT admissible** — destroys Pareto optimality
- Selection order is a **correctness precondition** (not a heuristic)

**Example:** Two suffixes with costs (1,10) and (10,1)
- Their max (10,10) bounds neither → unsound aggregation
- But {(1,10), (10,1)} as a set is admissible ✓

---

## Dimensionality Reduction (DR)

**Key insight:** Most dominance comparisons happen in first coordinate; use it as shortcut.

Define truncation operator:
```
Tr(x) = (x₂, x₃, ..., x_M)  ∈ ℝ^(M-1)
```

**Truncated dominance test:**
- Query: Does frontier F(s) have a vector p with Tr(p) ≤ Tr(g)?
- Fast O(1) if first coordinate m(s) ≤ g₁ (monotonicity check)
- Otherwise: need full M-dimensional fallback

**Criticality for MVH:** 
Lexicographic ordering of H(s) is essential — any spatial tree must not reorder members by other coordinates, or the selected heuristic violates correctness.

---

## K-d Trees for Frontier Indexing

### Traditional Approach (no trees, linear scan)
```
Exists(F, q):
  for each p ∈ F:
    if Tr(p) ≤ q:  // componentwise ≤ in M-1 dims
      return true
  return false
```
Cost: O(|F|) per query

### K-d Tree Acceleration
```
Structure: (M-1)-dimensional k-d tree over Tr(F)
Query Exists(F, q):
  return kd_tree.search_dominated(q)  // uses bounding boxes
```

**Bounding box optimization:**
- Interior node stores lower/upper corners of descendant truncations
- Subtree can only contain dominator if lower_corner ≤ q
- Prune subtrees with upper_corner > g₁ (full-dimensional awareness)

**Frontier updates:**
- Delete marked (dominated) members during rebuild
- Mark-and-sweep amortizes insertion costs
- Rebuild when nodes exceed 1.25b + 64 (b = live count at last build)

---

## FAST-MVH Contributions

### 1. Operation Reordering

**Reference (L-NAMOA*_dr-mvh):**
```
For each successor:
  1. Test local dominance first
  2. Select heuristic (if dominated, skip)
```

**FAST-MVH:**
```
For each successor:
  1. Select heuristic first  ← reordered
  2. Test local dominance only if heuristic succeeds
```

**Benefit:** If ChooseH fails (no heuristic survives goal frontier), skip expensive local dominance check entirely.

### 2. Witness Caching

**Observation:** Goal test refutes candidate i repeatedly.
**Cached witness:** Store truncation W_s(i) of goal vector that refuted h^i.

**Next occurrence:**
```
if W_s(i) ≤ Tr(g) + Tr(h^i):
  return true  (immediately, no tree search)
```

Saves complete frontier query when witness still valid.

### 3. K-d Tree Adaptive Promotion

**Small frontiers:** Contiguous array (cache-friendly linear scan)
**Large frontiers:** Promote to k-d tree when:
- Frontier size ≥ 8 vectors
- Average comparisons per query ≥ 64
- Rebuild cost amortized over next queries

**Why not always?** Pointer indirection + tree overhead ≠ contiguous SIMD scan for small sets.

---

## Why No K-d Tree on Heuristics?

**Tempting idea:** Index H(s) with k-d tree, prune heuristics against goal frontier.

**Problem:** Heuristic box test requires complete frontier query.
```
Box corners: L = Tr(min{h^i}), U = Tr(max{h^i})
- Can reject if Exists(T, Tr(g) + L)
- Can accept if ¬Exists(T, Tr(g) + U)
- But neither may decide a subtree
```

**Result:** Most heuristic subtrees remain undecided; tree overhead > linear scan.

**Solution:** Local-first pruning instead
- Test successors locally first → prune dead branches immediately
- Remaining candidates: heuristic selection
- Witness reuse makes many selections cheap
- Complete configuration evaluated: static heuristic tree disabled

---

## Correctness Arguments

### Lemma 1: Local Exactness
If Exists(F(s), Tr(g)) with a witness p where p₁ ≤ g₁, then ∃a ∈ C(s): a ≤ g (full-dimensional).

**Proof:** Frontier covers C(s) by induction; if truncated witness has first coordinate ≤ g₁, then its full representative is dominated by g in all coordinates.

### Lemma 2: Witness Persistence
Once cached witness W ≤ Tr(g) + Tr(h), no subsequent path cost + heuristic pair (g', h') with W-based truncation ≤ Tr(g') + Tr(h') will remove W from the frontier.

### Theorem 1: Decision Equivalence
Given identical heuristic arrays, successor order, arithmetic, and OPEN tie-breaking:
- FAST-MVH and L-NAMOA*_dr-mvh make the same OPEN insertions/extractions
- Return the same solutions in the same order
- Both preserve Pareto optimality (under ordered admissible MVH)

---

## Experimental Validation

### Test Domains
1. **Synthetic grids (3D–6D):**
   - Controlled frontier density and heuristic cardinality
   - A*pex MVHs (ε ∈ {0.01, 0.05, 0.1})
   - Landmark controls (admissible but universal-bound families)

2. **Real road networks (3D–6D):**
   - DIMACS Bay Area (2k–16k vertices)
   - DIMACS New York (5k–8k vertices)
   - Native distance/travel time + synthetic uniform objectives

### Key Findings
- **Genuine MVHs:** 4–38× speedup (cache hits 55–82%)
- **Landmark controls:** 1.7–35× speedup (cache hits 12–20%, regressions common)
- **3D dominance:** No trees often needed; gains come from operation order + witness reuse
- **Larger graphs:** Adaptive tree promotion kicks in; 210 trees at Bay-8 M4 (19.6× speedup)

---

## Open Questions & Future Work

1. **Non-lexicographic MVH search:**
   - Current theory requires lexicographic order of H(s)
   - MIN-order search might enable different heuristic orderings
   - Requires new correctness arguments

2. **Spatial coherence in heuristics:**
   - Bay area has geographic correlation; do spatially coherent MVHs improve tree performance?
   - Synthetic uniform objectives hide latent structure

3. **Other geometric structures:**
   - R-trees, KD-B-trees, quadtrees for larger frontiers?
   - Comparison with alternative spatial indexing

4. **Multi-threaded search:**
   - Current measurements single-threaded; parallelization potential unexplored

---

## Implementation Details

### Build Environment
```
OS: Windows 10 21H2
CPU: Intel Core i5-1135G7 (Tiger Lake, 4 cores, 2.4–4.2 GHz)
RAM: 8 GB
Compiler: clang/libc++ via zig toolchain
Flags: -std=c++20 -O2 -DNDEBUG -march=native
```

### Instrumentation
Per-run counters collected:
- `cmpchk`: Dominance comparisons in truncated frontier queries
- `cmpupd`: Dominance comparisons in frontier updates
- `cmpfull`: Full-dimensional fallback comparisons
- `fallbacks`: Total full-dimensional checks (loc_bad + loc_good)
- `kd_builds`: Frontier tree promotions
- `wc_hit`, `lw_hit`: Witness cache hits (goal-cached, local-cached)

### Heuristic Construction
A*pex builder produces lexicographically sorted MVH sets:
```
apex_builder MAP GOAL M EPS FAST2 OUTPUT.mvh
```
- Approximation threshold ε controls solution quality
- Output file: one heuristic vector per line (M space-separated values)
- All dimensions verified before search

---

## References to Consult

### Core MVH Work
- Skyler et al. (2024): "Theoretical Study on Multi-Objective Heuristic Search" (IJCAI)
- Geisser et al. (2022): "Admissible Heuristics for Multi-Objective Planning" (ICAPS)

### Dimensionality Reduction
- Wohlf et al. (2026): "Bridging Multi-Valued Heuristics and Dimensionality Reduction" (SoCS)
  - Formalizes lexicographic ordering requirement
  - Defines truncation operator and witness validity

### Geometric Indexing
- Shperberg et al. (2019–2021): "A Geometric Index for Multi-Objective Dominance Checking"
  - K-d tree queries on orthant regions
  - Complexity bounds for M ≥ 4

### A* and Classical Search
- Korf (1985): "Depth-first iterative-deepening" — foundational
- Felner et al. (2005+): Multi-objective search variants (KGOAL, BOA*)
- Goldberg & Harrelson (2005): "Computing the Shortest Path: A* Search Meets Graph Theory" (SODA)

---

## Paper Citation

```bibtex
@inproceedings{fastmvh2026,
  title={FAST-MVH: Fast Adaptive Search with Trees for Multi-Valued Heuristic Search},
  author={Ber-Feigenberg, Ariel},
  booktitle={Proceedings of [Venue]},
  year={2026}
}
```

---

**Version:** 2026-09-29  
**Status:** Complete for publication review
