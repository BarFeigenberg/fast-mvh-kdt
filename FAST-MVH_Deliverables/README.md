# FAST-MVH: Fast Adaptive Search with Trees for Multi-Valued Heuristic Search

**Publication-Ready Research Paper & Complete Implementation**

---

## Contents

This folder contains a complete, self-contained archive of the FAST-MVH research framework, including the peer-review-ready paper, full source code, experimental data, and road networks used in evaluation.

### 📄 **Paper**
- **`FAST_MVH_Research_Paper.pdf`** — 7-page publication-ready PDF
- **`FAST_MVH_Research_Paper.md`** — Editable Markdown source for the paper

**Key findings:**
- 38.5× speedup on 6D grids (70,196 solutions in 2.67s vs. 102.85s baseline)
- 19.6× speedup on large multi-objective road networks
- Complete baseline-matched evaluation: 25 cases, 158 variant runs, byte-identical solutions
- Cache effects analyzed: gains up to 20%, regressions up to 14% (workload-dependent)
- Coverage: 3D–6D multi-objective search on grids and real road networks

---

## 📂 **Folder Structure**

### 1. **Source_Code/**
Complete C++20 implementation with two key modules:

#### `FAST_MVH_Implementation/`
- **Core solver:** `src/solvers/l_namoa_dr_mvh_kdt.cpp`
  - Main search loop implementing Algorithm 1 (FAST-MVH)
  - K-d tree frontier indexing with adaptive promotion (§5.1)
  - Witness caching and exact fallback dominance checks (§3.1)
  - Heuristic selection with cached witnesses (§3.2)
  
- **K-d tree index:** `src/mvh_kdtree.cpp`
  - Adaptive k-d tree over truncated (M-1)-dimensional frontiers
  - Dominance queries and frontier updates
  - Promotion/rebuild thresholds: 8 vectors min, 64 avg comparisons per query
  
- **Heuristic builder:** `src/solvers/l_namoa_kdt_chooseh.cpp`
  - A*pex multi-valued heuristic construction
  - Lexicographic ordering enforcement (critical for correctness under DR)

#### `APEX_Heuristic_Builder/`
- Genuine multi-valued heuristics (trade-off Pareto sets)
- Built with: `apex_builder.exe MAP GOAL M EPS FAST2 OUTPUT.mvh`

**Compiler flags (verified on Intel i5-1135G7, clang/libc++):**
```bash
-std=c++20 -O2 -DNDEBUG -march=native
```

---

### 2. **Experimental_Results/**
Complete evaluation data and metrics:

- **`FAST_MVH_Expanded_Results.csv`**
  - 25 completed baseline-matched cases
  - Minimum and median times (3+ repetitions per variant)
  - Cache effects, operation counters, tree promotion counts
  - All 158 variant runs byte-identical to baselines
  
- **`FAST_MVH_Cache_Effect.svg`**
  - Paired cache-effect plot (cached vs. cache-off) by dimension
  - Shows both improvements and regressions
  - Sub-20ms cases omitted from plot but retained in summary
  
- **`grids_metrics.csv`** — Grid-specific run logs
- **`bay_metrics.csv`** — Bay Area road network run logs

**Test domains:**
- **Grids:** 10×10 and 8×8 lattices, 3D–6D genuine MVHs + landmark controls
- **Roads (DIMACS):** New York (5k–8k) and San Francisco Bay Area (2k–16k)
- **Heuristics:** A*pex genuine MVHs (ε ∈ {0, 0.01, 0.05, 0.1}) and admissible landmarks

---

### 3. **Road_Networks/**
Official DIMACS graph data used in experiments:

- **`BAY-d.gr.gz`** — Bay Area distance (arc length in meters)
  - SHA256: `630c4a96869b3ecdd631ceb0f63923fb954ee4c7c26d983863612e2c4442462e`
  
- **`BAY-t.gr.gz`** — Bay Area travel time
  - SHA256: `85d597078d13907e60ff826d8c7fe0b18ca4c690d003751e27e30ee4011091c3`
  
- **`sources.json`** — Metadata and generation parameters
  - Bay centre (goal): 160636
  - New York centre: 140000
  - Additional objectives: U[1,100], seed 42 (synthetic, for dimension scaling)

**Graph formats:**
- DIMACS `.gr` (directed, parallel arcs preserved)
- BFS subgraphs: start = last discovered vertex, goal = BFS centre
- Symmetric search: loader constructs reverse edges

---

### 4. **Test_Instances/**
Pre-generated search instances and heuristic files:

- Grid inputs with topology and cost matrices (3D–6D)
- Bay/NY subgraph definitions and BFS orderings
- A*pex heuristic sets (lexicographically sorted)
- Input metadata with SHA256 verification

---

### 5. **Build_Info/**
Build configuration and project setup:

- **`CMakeLists.txt`** — CMake configuration for C++20, clang, optimization flags
- **`github-app.yml`** — GitHub Copilot app repository configuration

---

### 6. **Baseline_Reference/**
*Read-only reference oracle* (Wolff et al.'s L-NAMOA*_dr-mvh):

- Unmodified baseline solver at commit `0a2f9ea`
- No changes made to baseline source; all measurements directly comparable
- Baseline results recorded with binary/source hashes for reproducibility

---

## 🔧 **Usage & Integration**

### Building FAST-MVH
```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -- -j $(nproc)
```

### Running a Search
```bash
proto_w.exe MAP START GOAL M HEURISTIC OUTPUT.sol \
  flatH=1 local_first=1 witness=1 redund=1 \
  localX=8 targetX=8 promoteC=64 exact_max=1 \
  tcache=2
```

**Parameters:**
- `tcache=2`: Enable witness caching (FAST-MVH)
- `tcache=0`: Disable caching (ablation control)
- `flatH=1, witness=1`: Full configuration as in paper
- `promoteC=64`: K-d tree promotion threshold

### Building a Heuristic
```bash
apex_builder.exe MAP GOAL M EPS FAST2 OUTPUT.mvh
```

---

## 📊 **Key Metrics**

All measurements conducted on: **Intel Core i5-1135G7 @ 2.4 GHz, 8 GB RAM, Windows 10**

### Grid Results (Table 1)
| Problem | M | Solutions | Expansions | Baseline | FAST-MVH | Speedup |
|---------|---|-----------|------------|----------|----------|---------|
| 10×10 A*pex | 3 | 1,602 | 9,499 | 0.065s | 0.050s | 1.30× |
| 10×10 A*pex | 4 | 11,330 | 53,651 | 2.554s | 0.472s | 5.41× |
| 8×8 A*pex | 5 | 9,412 | 33,005 | 1.462s | 0.309s | 4.73× |
| **8×8 A*pex** | **6** | **70,196** | **179,297** | **102.849s** | **2.674s** | **38.46×** |

### Road Network Results (Table 2)
| Graph | M | Solutions | Baseline | FAST-MVH | Speedup | Trees |
|-------|---|-----------|----------|----------|---------|-------|
| Bay-8 | 3 | 238 | 0.109s | 0.103s | 1.06× | 1 |
| Bay-16 | 3 | 3,534 | 3.326s | 2.097s | 1.59× | 11 |
| Bay-8 | 4 | 3,716 | 24.949s | 1.273s | **19.60×** | 210 |

### Correctness Verification
- ✓ All 158 variant runs byte-identical to baseline solution files
- ✓ Expansion, extraction, and reinsertion counts match exactly
- ✓ Pareto sets verified across all 25 matched instances
- ✓ New MVHs lexicographically sorted per theory
- ✓ Older heuristic ties empirically verified (historical agreement maintained)

---

## 🧠 **Theoretical Contributions**

### Algorithms
1. **Algorithm 1 (FAST-MVH):** Main search loop with reordered generation tests
2. **Algorithm 2 (Dominance checks):** K-d tree queries with exact fallback
3. **Algorithm 3 (Heuristic selection):** Cached witness reuse for fast filtering

### Lemmas & Theorems
- **Lemma 1:** Local exactness of truncated queries + fallback
- **Lemma 2:** Witness persistence across frontier updates
- **Theorem 1:** Decision equivalence with L-NAMOA*_dr-mvh (search returns identical solutions)

### Operation Reordering
- **Generation:** Heuristic selection **before** local dominance test
  - Avoids frontier queries for candidates that fail selection
  - Saves both selection and fallback costs on trade-off instances
  
- **Cache effect:** Workload-dependent (5–20% gains on genuine MVHs, up to 14% regression on landmarks)

---

## 📚 **References**

1. **Wohlf, M.; Felner, A.; Salzman, O.** (2026). "Bridging Multi-Valued Heuristics and Dimensionality Reduction in Multi-Objective Search." *Symposium on Combinatorial Search* (SoCS). — Defines L-NAMOA*_dr-mvh baseline

2. **Anonymous.** "A Geometric Index for Multi-Objective Dominance Checking." Unpublished. — K-d tree geometry and orthant queries

3. **Geisser et al.** (2022). "Admissible Heuristics for Multi-Objective Planning." *ICAPS*, 100–109.

4. **Skyler, S.; Shperberg, S.S.; et al.** (2024). "Theoretical Study on Multi-Objective Heuristic Search." *IJCAI*, 7021–7028.

5. **Demetrescu, C.; Goldberg, A.V.; Johnson, D.S.** (2009). *The Shortest Path Problem: Ninth DIMACS Implementation Challenge.* AMS DIMACS Series 74.

6. **Zhang et al.** (2022). "A*pex: Efficient Approximate Multi-Objective Search on Graphs." *ICAPS*, 394–403.

---

## 🛠️ **Reproducibility**

All runs include:
- ✓ Exact git commit hashes (baseline & FAST-MVH source)
- ✓ Binary SHA256 hashes (proof of identical executables)
- ✓ Heuristic/input SHA256 hashes (data integrity)
- ✓ Complete CLI arguments and affinity masks
- ✓ Raw solution files and per-repetition logs
- ✓ Wall-clock process time + internal search timer

**Verification script:** See `Experimental_Results/` CSV metadata columns

---

## ❓ **Questions & Troubleshooting**

### Baseline Timeouts
- Five large cases (Bay-8 M6, Bay-16 M4/M5/M6) exceeded 180s baseline cap
- FAST-MVH and cache-off variants for these are included in extended results
- No baseline solution file available for speedup claims; cached/uncached agreement only

### 3D Gains Are Modest
- Truncation at M=3 leaves M-1=2 dimensions; not all instances benefit from k-d indexing
- Landmark controls show cache regression (~14% on 15×15 grid)
- Bay-8 M3 and Bay-16 M3 still build 1 and 11 trees respectively (representation driven by scan cost, not dimension alone)

### How to Edit the Paper
1. Open `FAST_MVH_Research_Paper.md`
2. Edit Markdown/LaTeX as needed
3. Rebuild PDF: `python scratchpad/paper_verify/pdf_build/build_fast_mvh.py`
4. (Requires Pandoc, Edge CDP, pymupdf)

---

## 📋 **Artifact Manifest**

**Total files:** 174  
**Total size:** ~2.5 GB (dominated by graph data & run logs)

**Critical files for replication:**
- Source code: `Source_Code/FAST_MVH_Implementation/`
- Paper: `FAST_MVH_Research_Paper.pdf` + `.md`
- Results: `Experimental_Results/FAST_MVH_Expanded_Results.csv`
- Road data: `Road_Networks/BAY-{d,t}.gr.gz` (SHA256 verified)

---

**Last Updated:** 2026-09-29  
**Status:** Ready for publication  
**License:** See original repository

