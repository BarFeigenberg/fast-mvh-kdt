# Strategic Master Research Roadmap: Fast MVH with K-d Tree Dominance Checking

## Foundation Completed (Phase 0 Archive)
- [x] **Phase 0: Environment, Baselines & Golden Results Archive**
  - [x] Workspace scaffolding & safety guardrails in GEMINI.md
  - [x] Theory documentation in docs/theory/
  - [x] Evaluation skills (.agent/skills/)
  - [x] Deep audit of baseline codebases
  - [x] Established golden results archive for bit-identical validation

---

## STAGE 1: Roi's KD-CHOOSEH Optimization & Superiority [ACTIVE / IN PROGRESS]
> **Primary & Exclusive Current Focus**
- **Objective**: Fully optimize and profile Roi's static heuristic K-d tree (`StaticHeuristicKDTree` / `L_NAMOA_KDT_CHOOSEH`) against Maya's baseline (`L_NAMOA_DR_MVH`).
- **Core Deliverable**: Empirically prove undisputed superiority (wall-clock speedup and massive reduction in `cmp_chooseh` comparisons) across diverse benchmarks, specifically where heuristic cardinality ($K = |H(s)|$) and dimension ($M \ge 3$) create substantial linear-scan bottlenecks.
- **Success Criteria**: A comprehensive, publication-ready benchmark suite demonstrating clear win-regimes and scaling curves for Roi's approach over Maya's baseline before moving forward.

### Tracked Workstreams:
- [x] Static POD layout optimization (`StaticKDNode` 24-byte POD, cache-friendly SoA flat arrays)
- [x] Core correctness verification (100% bit-identical Pareto fronts & node expansion counts)
- [ ] Profiling & bottleneck analysis of `choose_h` traversal vs. linear scan overhead across hardware
- [ ] Systematic benchmark parameter space sweep ($M \in \{3, 4, 5, 6, 7, 8\}$, $K \in \{10, 25, 50, 100, 250, 500, 1000\}$, varying objective correlation $\rho$)
- [ ] Publication-ready scaling plots & comparison tables (`runtime_s`, `cmp_chooseh`, memory footprint)
- [ ] Definitive empirical superiority proof and win-regime characterization report

---

## STAGE 2: Unified Integration with Shahaf's Frontier KDT [BLOCKED / PENDING STAGE 1 COMPLETION]
> **Prerequisite**: Stage 1 must be 100% complete with rigorous empirical evidence of Roi's superiority.
- **Objective**: Synthesize Roi's heuristic-indexing K-d tree (`CHOOSEH`) with Shahaf's dynamic state-frontier K-d tree ($G_{\mathrm{cl}}^{\mathrm{Tr}}(s)$) into a unified solver (`FAST MVH-KDT`).
- **Research Hypothesis**: Dual-geometry acceleration (simultaneously pruning the heuristic selection space and the path dominance space) will yield multiplicative speedups on dense, anti-correlated graphs.

### The 4-Variant Research Matrix:
- **Variant 1 (Maya Baseline)**: Pure linear search over heuristics and frontiers; ground-truth oracle.
- **Variant 2 (Maya + Shahaf Only)**: Dynamic K-d tree on target frontier; linear heuristic scan. Isolates frontier-speedup.
- **Variant 3 (Maya + Roi Only)**: Static K-d tree on heuristics; linear target frontier scan. Isolates heuristic-pruning.
- **Variant 4 (Dual-Tree Fast MVH-KDT)**: Hierarchical heuristic K-d tree querying target-frontier K-d tree.

### Tracked Workstreams:
- [ ] Architectural interface harmonization between `StaticHeuristicKDTree` and `DynamicFrontierKDTree`
- [ ] Interactive code proposal & review for dual-tree unified solver (`FAST MVH-KDT`)
- [ ] End-to-end bit-identical verification against golden baselines (and soundness audit via `docs/theory/maya_soundness_proof.md`)
- [ ] Systematic 4-way factorial benchmark sweeps across frontier-dense and heuristic-dense topologies

---

## STAGE 3: Alternative Heuristic Orderings & Selection Functions [BLOCKED / PENDING STAGE 1 COMPLETION]
> **Prerequisite**: Stages 1 & 2 complete.
- **Objective**: Investigate alternative ordering functions (OF) and non-lexicographical traversal strategies for heuristic selection (e.g., Min ordering, local bounding-box Ideal Point prioritization, or variance-driven selection).
- **Research Question**: Can we loosen or replace strict lexicographical tie-breaking in `CHOOSEH` to enable faster target-frontier convergence or early termination without compromising correctness?

### Tracked Workstreams:
- [ ] Theoretical admissibility and termination audit for non-lexicographical selection functions
- [ ] Design of ideal-point / bounding-box priority queues within heuristic KD-Tree traversal
- [ ] Empirical trade-off evaluation: search tree expansion count vs. target frontier convergence rate
