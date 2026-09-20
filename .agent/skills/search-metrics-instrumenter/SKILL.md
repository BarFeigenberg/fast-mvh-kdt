---
name: search-metrics-instrumenter
description: Skill for capturing dominance comparison counters (cmpchk, cmpupd) and outputting formatted comparison tables.
---

# Search Metrics Instrumenter Skill

## Purpose
This skill captures, tracks, and formats search efficiency metrics across baseline and accelerated geometric multi-objective search algorithms. It monitors both macro-level wall-clock performance and micro-level operation counters to measure geometric pruning efficacy.

## Instrumented Metrics

1. **Dominance Comparison Counters**:
   - `cmpchk`: Exact number of individual vector-vector dominance tests performed during dominance check queries (`CHECK` / `LOCALDOMCHECK` / `global_dominance_check`).
   - `cmpupd`: Exact number of vector-vector comparisons performed while inserting points and pruning dominated vectors in the frontier (`UPDATE`).
   - `cmp_chooseh`: Number of vector dominance comparisons performed during heuristic selection in `CHOOSEH`.
2. **Search Progress Counters**:
   - `expansions`: Number of nodes popped and expanded from `OPEN`.
   - `generations`: Number of successor nodes generated.
   - `reinsertions`: Number of nodes re-evaluated and re-inserted into `OPEN` due to global goal frontier dominance.
   - `fallbacks`:
     - `good_fallback`: Monotonicity violation where node was salvaged (survived full Pareto check).
     - `bad_fallback`: Monotonicity violation where node was dominated in full Pareto check.
3. **Timing Metrics**:
   - Total search wall-clock time ($t$ in milliseconds).
   - Time spent in `CHOOSEH` vs. time spent in node expansion and queue operations.

## Output Format & Comparison Table

When presenting evaluation results, format the comparison table following Shahaf's empirical reporting structure:

```markdown
| Objective Count $M$ | Method | Time (ms) | `cmpchk` | `cmpupd` | `cmp/chk` | Expansions | Speedup |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| $M = 3$ | Baseline (Linear) | ... | ... | ... | ... | ... | 1.00x |
| $M = 3$ | K-d Tree (Ours)   | ... | ... | ... | ... | ... | ...   |
| $M = 4$ | Baseline (Linear) | ... | ... | ... | ... | ... | 1.00x |
| $M = 4$ | K-d Tree (Ours)   | ... | ... | ... | ... | ... | ...   |
```

## Verification Rule
Always verify that speedup does not come at the expense of extra expansions: when strictly preserving Maya's heuristic selection semantics, total node expansions must remain constant or decrease.
