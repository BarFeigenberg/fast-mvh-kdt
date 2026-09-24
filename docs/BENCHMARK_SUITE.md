# Multi-Objective Search Benchmark Suite

This document outlines the benchmark instances generated to stress-test the geometric state-frontier indexing (Shahaf) and heuristic-set indexing (Roi) in multi-objective heuristic search algorithms.

## Benchmark Families

### 1. High Frontier Density (`benchmarks/instances/high_frontier_density/`)
**Objective**: Stress-test Shahaf's state-frontier K-d tree indexing by generating massive local Pareto frontiers ($|G_{\mathrm{cl}}(s)| \gg 1$).
- **Generative Process**: We use synthetic multi-objective 4-connected grids with a negative cost correlation coefficient $\rho < 0$. By forcing trade-offs between objectives (e.g., an edge that is cheap in objective 1 is expensive in objective 2), the number of non-dominated paths between any two nodes grows exponentially.
- **Instance Properties**:
  - Graph Topology: Grid (e.g., $50 \times 50$, $|V| = 2500$, $|E| \approx 10000$)
  - Dimension $M \ge 3$
  - Correlation: $\rho = -0.8$
- **Rationale**: On graphs where costs are highly correlated or single-dimensional, a state typically accumulates 1-3 non-dominated cost vectors. In such cases, linear scanning dominates and the K-d tree overhead is detrimental. By setting $\rho = -0.8$, we artificially explode the frontier sizes, isolating the asymptotic query complexity of Shahaf's geometric K-d tree against baseline list scans.

### 2. High Heuristic Density (`benchmarks/instances/high_heuristic_density/`)
**Objective**: Stress-test Roi's heuristic-set indexing (`KD-CHOOSEH`) by providing a rich, diverse set of admissible heuristic vectors for each state ($|H(s)| \gg 1$).
- **Generative Process**: We compute differential heuristics using $K$ random landmarks. For a graph, we calculate the shortest path distance from each landmark $l$ to every state $s$ on each objective independently. The admissible heuristic vector for $s$ relative to $l$ is $h^{(l)}(s) = \max(dist(s, l) - dist(goal, l), 0)$. We generate $K$ such vectors per state.
- **Instance Properties**:
  - Graph Topology: Grid (e.g., $50 \times 50$, $|V| = 2500$, $|E| \approx 10000$)
  - Dimension $M \ge 3$
  - Correlation: $\rho = 0.0$ (independent costs to provide variance in heuristic estimates)
  - Landmarks: $K \in [5, 100]$ (e.g., $K=50$ heuristics per state)
- **Rationale**: Roi's `KD-CHOOSEH` relies on aggregate branch pruning by tracking the min/max of heuristic values in K-d tree bounding boxes. For small $|H(s)| \approx 1$, a linear scan of heuristics is instantaneous. When $|H(s)| = 50$, computing the dominance check across all heuristics becomes a bottleneck. This benchmark isolates the benefit of KD-tree bounding box pruning over linear search through the heuristic set.

### 3. DIMACS Subgraphs (`benchmarks/instances/dimacs_subgraphs/`)
**Objective**: Validate real-world performance on realistic topological road networks.
- **Generative Process**: Intended to be subgraphs extracted from DIMACS challenge road networks (e.g., NY, BAY, COL) with synthetically assigned correlated costs.
- **Instance Properties**:
  - Real road topology (scale-free, varying degree distribution)
  - Dimension $M \in \{3, 4, 5\}$
- **Rationale**: Grids are structurally uniform, which can bias the density of Pareto fronts. Realistic topologies exhibit bottlenecks and hierarchies which affect the distribution of both $|G_{\mathrm{cl}}(s)|$ and heuristic estimates, providing a final integrated testbed for both Shahaf's and Roi's indexing mechanisms.
