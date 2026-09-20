---
name: benchmark-runner
description: Skill for executing benchmark experiment sweeps, managing timeouts, and logging deterministic artifacts.
---

# Benchmark Runner Skill

## Purpose
Orchestrates empirical evaluation sweeps across multi-objective benchmark instances (synthetic grid graphs and DIMACS road networks), tracks micro-metrics (`cmpchk`, `cmpupd`, expansions, fallbacks) alongside macro wall-clock timing, and isolates all outputs into deterministic timestamped runs.

## Benchmark Driver: `run_experiments.py`

The primary runner script is `benchmarks/run_experiments.py`.

### Execution Examples

#### 1. Quick Verification Sweep on Synthetic Graph
```powershell
py benchmarks/run_experiments.py `
  --baseline-bin build/baselines/bridging-mvh-dr/Release/MultivaluedHeuristicSearch.exe `
  --kdt-bin build/Release/fast_mvh.exe `
  --maps baselines/bridging-mvh-dr/resources/synthetic_graph `
  --queries "1 20" "1 15" "2 18" `
  --objectives 0 1 2 `
  --timeout 30 `
  --tag synthetic_3obj
```

#### 2. Scaling Dimension Sweep ($M = 3, 4, 5$)
```powershell
py benchmarks/run_experiments.py `
  --baseline-bin build/baselines/bridging-mvh-dr/Release/MultivaluedHeuristicSearch.exe `
  --kdt-bin build/Release/fast_mvh.exe `
  --maps benchmarks/instances/grid_30x30 `
  --queries "1 900" `
  --objectives 0 1 2 3 4 `
  --timeout 120 `
  --tag scaling_dim5
```

## Deterministic Run Artifact Isolation

Every execution creates an isolated directory under `benchmarks/runs/<YYYY-MM-DD_HHMMSS>_<tag>/`:

```
benchmarks/runs/2026-09-20_183000_synthetic_3obj/
├── run_info.json           # Git commit hash, exact CLI arguments, environment metadata
├── baseline_*_stdout.log   # Raw stdout streams
├── baseline_*_stderr.log   # Error streams and warnings
├── kdt_*_sols.txt          # Solution cost vectors
├── metrics_summary.csv     # Machine-readable tabular metrics
└── metrics_summary.md      # GitHub-flavored formatted comparison table
```

### Metrics Tracked in `metrics_summary.md`

| Method / Run | Instance | Objectives | Sols | Time (s) | Expansions | Generations | `cmpchk` | `cmpupd` | Good FB | Bad FB | Status |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| `baseline_...` | synthetic | 3 | 4 | 0.012 | 152 | 410 | 1240 | N/A | 0 | 0 | OK |
| `kdt_...`      | synthetic | 3 | 4 | 0.003 | 152 | 410 | 185  | 42  | 0 | 0 | OK |

## Benchmark Hygiene Rules
1. Never run ad-hoc benchmarks without routing output through `benchmarks/runs/`.
2. Ensure binaries are compiled in Release mode (`-O2` / `/O2`) before taking wall-clock measurements.
3. Verify that `num_expansion` and `num_solutions` match between baseline and accelerated solvers to guarantee search trajectory fidelity.
