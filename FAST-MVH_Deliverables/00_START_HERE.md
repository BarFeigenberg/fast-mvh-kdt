# FAST-MVH Deliverables Summary

> **Update 2026-10-04 (current FAST).** The paper now describes the current FAST
> algorithm (residue set R(s) + Bentley–Saxe k-d forest fallback, inline OPEN).
> - Paper: `FAST_MVH_Research_Paper.{md,pdf}` (6 pages) + `FAST_MVH_Speedup.svg` (Figure 2).
> - Code used: `Source_Code/FAST_MVH_Current/` (repo `src/` at commit f0dfb1e;
>   FAST = `L_NAMOA_DR_MVH_FAST3`, FAST_C ablation = `L_NAMOA_DR_MVH_FAST`).
> - Results: `Experimental_Results/FAST_MVH_Results.csv` (27 instances, same machine).
> - Tools: `Experimental_Results/fast_campaign_tools/` (runner, summarizer, reference
>   driver `maya_run.cpp`, plans; plan paths are absolute to the original machine).
> - Sections below describe the earlier FAST2-era package and are kept for history.

**Date:** 2026-09-29  
**Status:** Complete with ongoing experiments  
**Total Package Size:** ~450 MB (code + road networks + results)

---

## 📦 What's Inside

This folder contains everything needed to understand, reproduce, and extend the **FAST-MVH** framework:

### 1. **Publication (7 pages)**
- `FAST_MVH_Research_Paper.pdf` — Formatted, ready-to-submit PDF
- `FAST_MVH_Research_Paper.md` — Editable Markdown source
  - Easy to update tables, algorithm descriptions, results
  - Regenerate PDF with `python scratchpad/paper_verify/pdf_build/build_fast_mvh.py`

### 2. **Complete C++20 Implementation**
- `Source_Code/FAST_MVH_Implementation/` — Full working codebase
  - Main solver: `src/solvers/l_namoa_dr_mvh_kdt.cpp`
  - K-d tree acceleration: `src/mvh_kdtree.cpp`
  - Heuristic selection: `src/solvers/l_namoa_kdt_chooseh.cpp`
  - Build: `mkdir build && cd build && cmake -DCMAKE_BUILD_TYPE=Release .. && make`

### 3. **Road Networks (Official DIMACS Data)**
- `Road_Networks/BAY-d.gr.gz` — San Francisco Bay Area distance graph
- `Road_Networks/BAY-t.gr.gz` — Bay Area travel time graph
- Both with official SHA256 checksums & generation metadata

### 4. **Experimental Results**
- `Experimental_Results/FAST_MVH_Expanded_Results.csv`
  - 25 baseline-matched cases, 158 cached/uncached runs
  - All solutions byte-identical to baselines
  - Operation counters: `cmpchk`, `cmpupd`, `cmpfull`, fallbacks, cache hits, tree counts
  
- `Experimental_Results/FAST_MVH_Cache_Effect.svg` — Paired timing plot
  
- Run logs: `grids_metrics.csv`, `bay_metrics.csv` (detailed per-repetition data)

### 5. **Theoretical Context**
- `THEORY_CONTEXT.md` — In-depth explanations
  - Multi-Valued Heuristics vs. Single-Valued
  - Dimensionality Reduction & k-d tree geometry
  - Correctness arguments (Lemmas, Theorems)
  - Design rationale for each optimization
  
- `README.md` — Quick-start guide & usage instructions

### 6. **Build Configuration**
- `Build_Info/CMakeLists.txt` — C++20 build setup
- `Build_Info/github-app.yml` — Repository metadata

### 7. **Baseline Reference** (Read-only)
- `Baseline_Reference/bridging-mvh-dr/` — Unmodified L-NAMOA*_dr-mvh solver
- Commit `0a2f9ea`, used to generate all baseline results
- Ensures reproducibility & enables direct comparison

### 8. **Test Instances**
- Pre-generated grids (10×10, 8×8) with A*pex heuristics
- Bay/NY subgraph definitions
- All with SHA256 verification metadata

---

## 🎯 Key Numbers

### Speedups Achieved
- **Best case:** 38.5× on 6D grid (70,196 solutions in 2.67s)
- **Average on genuine MVHs:** 5–20×
- **Modest on 3D:** 1–2× (frontier trees often not needed)
- **Large road networks:** 19.6× on Bay-8 4D (210 promoted frontiers)

### Experimental Coverage
- **25 completed baseline-matched cases** across grids, road networks, 3D–6D
- **158 total variant runs** (cached + cache-off, 3–5 repetitions each)
- **100% byte-equality** with baseline solution files
- **Exact matching** on expansion, extraction, reinsertion counts

### Operation Counts (Representative)
On 6D grid with 179k expansions:
- `cmpchk`: 32.32M (frontier queries via k-d tree)
- `cmpupd`: 7.37M (frontier updates)
- Fallbacks: 14,804 full-dimensional checks
- Cache hits: 54.8% (witness reuse effective)
- Trees: 18 frontier promotions

---

## 🔧 How to Use

### For Reviewers
1. **Read the paper:** `FAST_MVH_Research_Paper.pdf`
2. **Check results:** `Experimental_Results/FAST_MVH_Expanded_Results.csv`
3. **Understand theory:** `THEORY_CONTEXT.md`
4. **Verify:** All baselines at commit `0a2f9ea`, binary hashes in run manifests

### For Developers
1. **Navigate:** `Source_Code/FAST_MVH_Implementation/src/solvers/`
2. **Build:** Follow CMakeLists.txt
3. **Run:** Use `proto_w.exe` (compiled binary) with CLI args in README.md
4. **Extend:** Modify and rebuild with new algorithm variants

### For Reproducers
1. **Download road data:** Use SHA256 checksums to verify integrity
2. **Build codebase:** Requires C++20 compiler (clang/gcc/MSVC)
3. **Generate heuristics:** Use A*pex builder on your graphs
4. **Run benchmarks:** See `README.md` for exact commands & affinity settings

---

## 📊 Reproducibility Evidence

### Verification Results
✓ **Byte-equality:** All 158 runs produce identical solution files to baseline  
✓ **Correctness:** Expansion/extraction/reinsertion counts match exactly  
✓ **Baseline integrity:** Unmodified source, binary hashes recorded  
✓ **Data integrity:** Road networks verified via SHA256  
✓ **Instrumentation:** Complete counter sets logged (cmpchk, cmpupd, fallbacks, cache hits, etc.)

### What's NOT Claimed
✗ Speedups at sub-10ms (executable precision: 0.1ms)  
✗ Cache effect on sub-20ms cases (omitted from plot)  
✗ Baseline agreement on timeout cases (no oracle front)  
✗ Generality beyond tested domains (single machine, C++20 Windows build)

---

## 🚀 Next Steps for Distribution

### For GitHub
```bash
# Option 1: Add as submodule
git submodule add https://your-url/FAST-MVH-paper.git docs/fast-mvh

# Option 2: Import directly
cp -r FAST-MVH_Deliverables/* your-repo/fast-mvh/
git add fast-mvh/
git commit -m "Add FAST-MVH paper and code"
```

### For Zenodo/OSF (Archival)
1. Zip the folder: `zip -r FAST-MVH_Deliverables.zip FAST-MVH_Deliverables/`
2. Upload to Zenodo with DOI
3. Link in paper as: "Code and data available at https://zenodo.org/..."

### For Your Lab's Wiki
1. Link `README.md` as main reference
2. Point to `THEORY_CONTEXT.md` for students
3. Direct reproducers to `DEPLOYMENT_CHECKLIST.md`

---

## 📋 File Checklist

Quick verification before sharing:

```bash
# Core paper
[ ] FAST_MVH_Research_Paper.pdf (must be 7 pages)
[ ] FAST_MVH_Research_Paper.md (editable, ~20KB)

# Documentation
[ ] README.md (comprehensive guide)
[ ] THEORY_CONTEXT.md (theory + citations)
[ ] DEPLOYMENT_CHECKLIST.md (reviewer guide)

# Source Code
[ ] Source_Code/FAST_MVH_Implementation/src/solvers/l_namoa_dr_mvh_kdt.cpp
[ ] Source_Code/FAST_MVH_Implementation/src/mvh_kdtree.cpp
[ ] Build_Info/CMakeLists.txt

# Data
[ ] Road_Networks/BAY-d.gr.gz (SHA256 verified)
[ ] Road_Networks/BAY-t.gr.gz (SHA256 verified)
[ ] Road_Networks/sources.json

# Experiments
[ ] Experimental_Results/FAST_MVH_Expanded_Results.csv (25 cases)
[ ] Experimental_Results/FAST_MVH_Cache_Effect.svg (plot)

# Baseline
[ ] Baseline_Reference/bridging-mvh-dr/ (unmodified)
```

---

## 💡 Customization Tips

### Editing the Paper
```bash
# 1. Open in Markdown editor
vim FAST_MVH_Research_Paper.md

# 2. Update tables, algorithms, or text

# 3. Rebuild PDF (requires Pandoc + Edge)
python scratchpad/paper_verify/pdf_build/build_fast_mvh.py
```

### Adding New Experiments
```bash
# Follow same structure under benchmarks/runs/
benchmarks/runs/
└── YYYY-MM-DD_HHMMSS_your_experiment/
    ├── metrics.csv          (results table)
    ├── *.sol                (solution files)
    └── manifest.json        (hashes & metadata)
```

### Adapting the Build
```bash
# For Linux/macOS:
# 1. Update CMakeLists.txt (remove Windows-specific paths)
# 2. Change compiler: cmake -DCMAKE_CXX_COMPILER=clang++ ..
# 3. Rebuild
```

---

## 📞 Questions?

See `README.md` § Troubleshooting for common issues and solutions.

---

**Package Version:** 1.0  
**Last Updated:** 2026-09-29 13:58 UTC  
**Maintainer:** Ariel Ber-Feigenberg  
**Status:** Ready for publication & distribution

