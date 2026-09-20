---
name: kdt-oracle-validator
description: Skill for executing the oracle test harness and verifying bit-identical Pareto fronts between baselines/ and src/.
---

# K-d Tree Oracle Validator Skill

## Purpose
Executes the automated oracle verification harness to guarantee that the accelerated K-d tree search algorithms under `src/` produce Pareto-optimal solution sets that are **100% bit-identical** to the Maya Wohlf reference baseline (`baselines/bridging-mvh-dr`).

## Core Invariant
For any problem instance $P$:
$$\Pi^*_{\mathrm{fast\_mvh}}(P) \equiv \Pi^*_{\mathrm{baseline}}(P)$$
Every Pareto cost vector in the candidate set must exist in the oracle set, and no extraneous or dominated vectors may be produced.

## Automated Verification Driver: `verify_oracle.py`

The primary validation tool is `benchmarks/verify_oracle.py`.

### 1. Direct Vector File Comparison
When baseline and candidate solution files are already generated:
```powershell
py benchmarks/verify_oracle.py --oracle-file benchmarks/golden_results/synthetic_s1_g20_d3.txt --test-file scratchpad/candidate_s1_g20_d3.txt
```
- **Exit Code 0**: Solution sets match bit-for-bit.
- **Exit Code 1**: Divergence detected. Isolates and prints missing/spurious cost vectors.
- **Exit Code 2**: File missing or parsing error.

### 2. End-to-End Solver Execution Comparison
Directly invokes both binaries on an instance and checks their solutions in a single command:
```powershell
py benchmarks/verify_oracle.py `
  --oracle-bin build/baselines/bridging-mvh-dr/Release/MultivaluedHeuristicSearch.exe `
  --test-bin build/Release/fast_mvh.exe `
  --map baselines/bridging-mvh-dr/resources/synthetic_graph `
  --start 1 --goal 20 `
  --objectives 0 1 2 `
  --scratch-dir scratchpad
```

## Failure Diagnosis Protocol
When a mismatch occurs:
1. **Count Discrepancy**:
   - If $|\Pi^*_{\mathrm{test}}| < |\Pi^*_{\mathrm{oracle}}|$: Pruning rule is overly aggressive. A valid path was incorrectly pruned. Inspect `KD-Prune` aggregate acceptance condition (`N.max` check) or fallback check in `LOCALDOMCHECK`.
   - If $|\Pi^*_{\mathrm{test}}| > |\Pi^*_{\mathrm{oracle}}|$: Pruning rule is unsound or missed dominated vectors. Inspect aggregate discard condition (`N.min` check) or frontier updating.
2. **First Diverging Vector**:
   The script outputs sorted missing and spurious vectors. Locate the first diverging vector and trace the node generation history.
3. **Lexicographical Tie-Breaking**:
   Verify that `KD-ChooseH` returned the lowest lexicographical index $h_i \in H(s)$ among surviving heuristics. Any deviation alters the tie-breaking sequence in `OPEN`.
