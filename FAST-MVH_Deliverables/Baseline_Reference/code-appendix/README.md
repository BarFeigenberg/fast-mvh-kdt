# A Geometric Index for Multi-Objective Dominance Checking — Code Appendix

Anonymous supplementary code for the submission *"A Geometric Index for
Multi-Objective Dominance Checking."*

> **Double-blind note.** This artifact contains no author-, affiliation-, or
> institution-identifying information. When packaging for submission, exclude the
> version-control directory (`.git/`), which carries commit metadata (see
> [Packaging](#packaging-for-submission)).

---

## 1. Overview

Multi-objective A\* spends most of its time on *dominance checking*: every
generated path is tested against a per-vertex frontier of non-dominated cost
vectors. This artifact implements five frontier backends behind one interface
(`include/bench/ifrontier.hpp`) and provides the drivers and scripts that
reproduce every table and figure in the paper.

| name         | source                              | index                                            |
|--------------|-------------------------------------|--------------------------------------------------|
| `linear`     | `bench/linear_frontier`             | unsorted list scan (the NAMOA\*/EMOA\* baseline) |
| `sortlinear` | `bench/sorted_linear_frontier`      | lexicographically-sorted scan + early termination |
| `avlfast`    | `bench/avlfast_frontier`            | EMOA\* lexicographic AVL tree                     |
| `ndinc`      | `bench/nd_inc_frontier`             | incremental ND-tree (bounding-box archive)       |
| `kdinc`      | `bench/kd_inc_frontier`             | **our** incremental, lazy-deletion *k*-d tree    |

Two measurement modes are used throughout:

* **Dominance-comparison counts** are tallied by a single instrumented predicate
  (`rzq::basic::EpsDom`, `include/vec_type.hpp`) that every backend calls, so the
  counts are **exact, deterministic, and reproduce bit-for-bit on any machine or
  compiler.**
* **Wall-clock** is measured single-threaded. Absolute times depend on hardware;
  speedup *ratios* are stable across machines.

---

## 2. Requirements

* A C++ compiler supporting C++11 (e.g. `g++` ≥ 9; the paper used `g++` 13.2).
* CMake ≥ 3.16 and a build tool (`make` or `ninja`).
* Python ≥ 3.8 for the analysis/plotting scripts (`numpy`, `matplotlib` for the
  figures only).

## 3. Build

```bash
cmake -S . -B build -DCMAKE_CXX_FLAGS="-O2"
cmake --build build -j
```

Each `test/*.cpp` becomes an executable in `build/` (`build/<name>` on Linux/macOS,
`build/<name>.exe` on Windows). **All wall-clock figures in the paper use `-O2`, as
above** (the default unoptimized build is much slower). Individual drivers can be
built with `cmake --build build --target <name>`.

## 4. Repository layout

```
include/            core MO-SPP search + frontier backends (headers)
  bench/            the five backends and the replay/benchmark harness
  search_*.hpp,     EMOA* search core (derives from the public EMOA* base; see Provenance)
  avltree.hpp, vec_type.hpp
source/             implementations (mirrors include/)
test/               experiment drivers + unit tests (entry points below)
scripts/            orchestration + analysis (Python / shell / PowerShell)
data/               sample graphs; data/streams/grid/ holds the synthetic op-streams
results/            output CSVs written by the scripts (created on demand)
docs/               UPSTREAM_EMOA_README.md — the base framework's original README
```

### Op-streams and the replay protocol

A **stream** is the recorded `Check`/`Update` operation sequence at the busiest
vertex of a real EMOA\* search. Text format: a `DIM <d>` header line, then one op
per line (`C`/`U` followed by the `d` projected cost components). Replaying a
stream through each backend isolates dominance-check cost independent of search
overhead. The grid streams used by Experiment 1 and Appendix E are shipped in
`data/streams/grid/`.

---

## 5. Reproducing the results

> Comparison counts reproduce exactly. Wall-clock reproduces up to a
> hardware-dependent constant; see the note in §1.

### Experiment 1 — replay query phase and the separation (Table `tab:split`)

Deterministic counts + single-core times for M = 3..6:

```bash
cmake --build build --target replay_stats
python3 scripts/regen_table1.py --bin build/replay_stats   # prints the LaTeX table rows
```

`data/streams/grid/m{3..6}.stream` are shipped so the **counts** are bit-exact.
To regenerate a grid stream from scratch (30×30 four-neighbour grid, seeded):

```bash
cmake --build build --target gen_and_capture
build/gen_and_capture 30 30 4 0 899 data/streams/grid/m4.stream
```

The exact separation construction of the theorem (the total-order AVL performs
*n* comparisons per `Check`; the *k*-d tree performs 0):

```bash
cmake --build build --target lowerbound_construction
build/lowerbound_construction
```

### Experiment 2 — average-case query cost (Figure `cost_model`)

```bash
cmake --build build --target avg_sweep_driver
build/avg_sweep_driver > results/avg_sweep.csv
python3 scripts/validate_cost_model.py results/avg_sweep.csv   # fits n^a, writes figs/cost_model.png
```

### Experiment 3 — end-to-end search (Table `tab:e2e`)

Runs a full EMOA\* search with the AVL frontier vs. the *k*-d frontier on the same
grid, asserts the returned Pareto sets are identical, and reports search time and
dominance-check counts:

```bash
cmake --build build --target run_emoa_bench
build/run_emoa_bench 30 30 6 0 899        # M=6 grid
```

The robot-arm instance (`panda-RRG_8`, M=5) uses the benchmark cost files under
`data/panda/` (see [Data](#6-data)).

### Experiment 4 — real road networks via replay (Table `tab:roadreplay`, Figure `road_scatter`)

Requires the road data of §6. Capture a busiest-vertex stream, then replay it
through all backends:

```bash
cmake --build build --target road_capture replay_stats
# road_capture <net> <M> <vo> <vd> <out.stream> <mode:geo|synth> <rho> <seed> <time_limit_s>
build/road_capture NY 5 <vo> <vd> results/NY_M5_r0.6_geo.stream geo 0.6 1 300
build/replay_stats results/NY_M5_r0.6_geo.stream 5
```

For the full matrix, loop `road_capture` + `replay_stats` over the maps/M/ρ of
Table `tab:roadreplay`, appending each `replay_stats` CSV block — prefixed with a
`### MARK map=.. M=.. rho=.. stream=..` line — to `build/road_replay_raw.log`, then:

```bash
python3 scripts/build_road_replay_csv.py     # build/road_replay_raw.log -> results/road_replay.csv
python3 scripts/plot_road.py results/road_replay.csv   # -> figs/road_scatter.png
```

### Appendix E — objective-ordering sensitivity (Tables `tab:ordersynth`, `tab:orderroad`, `tab:ordertime`)

Fully self-contained from the shipped grid streams. One command:

```bash
bash scripts/reproduce_appendix.sh          # Linux/macOS
# or on Windows:  pwsh scripts/reproduce_appendix.ps1
```

This builds `replay_coordorder`, replays every one of the *d!* coordinate
orderings on `m3..m6`, and prints the three appendix tables plus the verification
that the *k*-d tree's **worst** ordering beats every baseline's **best**. The road
rows (`tab:orderroad`, road rows of `tab:ordertime`) additionally need captured
road streams from Experiment 4.

Manual invocation:

```bash
cmake --build build --target replay_coordorder replay_stats
python3 scripts/coord_order_study.py --bin build/replay_coordorder \
    --out results/order_synth.csv \
    --streams data/streams/grid/m3.stream data/streams/grid/m4.stream \
              data/streams/grid/m5.stream data/streams/grid/m6.stream
python3 scripts/coord_order_summarize.py --synth results/order_synth.csv   # human-readable summary
python3 scripts/coord_order_latex.py    --synth results/order_synth.csv --road results/order_road.csv
python3 scripts/coord_order_timetable.py --bin build/replay_stats          # tab:ordertime
```

`scripts/coord_order_study.py` is the core sweep: it permutes the *d* projected
coordinate axes of every vector in a stream, replays each ordering, and records
the exact per-backend dominance-check counts. `--max-perms N` samples *N* seeded
orderings when *d!* is large.

---

## 6. Data

* **Synthetic grids** — generated on the fly (seeded, deterministic). The captured
  streams are shipped in `data/streams/grid/`.
* **Road networks** — PACE conversions of the 9th DIMACS road graphs (New York,
  San Francisco Bay, Colorado), from the public PACE road-graph distribution
  referenced in the paper. Place the graph files under `data/road/`.
  `test/road_costs.hpp` documents the expected format and the seeded
  landmark-field (geometric) and i.i.d. (synthetic) cost models.
* **Robot arm** — the 7-DOF Panda road-map instance (`panda-RRG_8`) from the
  standardized multi-objective search benchmark suite cited in the paper. Place
  its cost files under `data/panda/`.

---

## 7. Correctness / tests

Every backend is validated against a brute-force linear oracle. Build and run any
`test/test_*.cpp` target (e.g. `test_kd_inc_frontier`, `test_nd_inc_frontier`,
`test_diff_harness`); the diff harness replays random streams through all backends
in lockstep and asserts identical `Check` results and live sets. In the end-to-end
runs (`run_emoa_bench`) the *k*-d frontier returns Pareto fronts **bit-identical**
to the AVL.

---

## 8. Provenance

The MO-SPP search core (`include/search_*.hpp`, `source/search_*.cpp`,
`include/avltree.hpp`, `include/vec_type.hpp`) derives from the public reference
implementation of the **EMOA\*** framework cited in the paper; for double-blind
review these file headers carry a generic provenance note, and the framework is
credited through the paper's references. All frontier-index code
(`include/bench/`, `source/bench/`), the experiment drivers (`test/`), and the
scripts (`scripts/`) are contributed by this work.

---

## 9. Packaging for submission

Produce the anonymized artifact zip by including only the sources and data and
excluding local/build/version-control state:

```bash
zip -r code-appendix.zip . \
    -x '.git/*' 'build*/*' '**/__pycache__/*' 'results/*' 'figs/*' '**/*.o' '**/*.obj'
```

Include: `README.md`, `CMakeLists.txt`, `include/`, `source/`, `test/`,
`scripts/`, `data/` (sample graphs + `data/streams/grid/`), `docs/`.
Exclude: `.git/` (commit identity), any `build*/` directories, `__pycache__/`, and
editor/IDE configuration.
