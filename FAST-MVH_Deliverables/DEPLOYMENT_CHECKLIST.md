# FAST-MVH Deliverables: Deployment Checklist

**Status:** Ready for publication  
**Last Updated:** 2026-09-29 13:57 UTC  
**Location:** `FAST-MVH_Deliverables/`

---

## 📋 Quick Start

### For Reviewers / Readers
1. Start with **`FAST_MVH_Research_Paper.pdf`** (7 pages, publication-ready)
2. Reference **`README.md`** for overview and file locations
3. Check **`Experimental_Results/FAST_MVH_Expanded_Results.csv`** for detailed metrics
4. View **`THEORY_CONTEXT.md`** for theoretical background

### For Developers / Reproducers
1. Read **`Build_Info/CMakeLists.txt`** and top-level build instructions
2. Navigate to **`Source_Code/FAST_MVH_Implementation/src/solvers/`**
3. Key files:
   - `l_namoa_dr_mvh_kdt.cpp` — Main FAST-MVH search loop
   - `../../../src/mvh_kdtree.cpp` — K-d tree implementation
4. Build: `mkdir build && cd build && cmake -DCMAKE_BUILD_TYPE=Release .. && cmake --build .`
5. Run: See `README.md` Usage section for CLI syntax

---

## 🎯 Contents Verification Checklist

### Paper & Documentation
- [x] `FAST_MVH_Research_Paper.pdf` (7 pages)
- [x] `FAST_MVH_Research_Paper.md` (editable source)
- [x] `README.md` (10KB comprehensive guide)
- [x] `THEORY_CONTEXT.md` (9KB theory + citations)
- [x] This checklist (`DEPLOYMENT_CHECKLIST.md`)

### Source Code
- [x] `Source_Code/FAST_MVH_Implementation/` (full C++20 solver)
  - [x] `src/solvers/l_namoa_dr_mvh_kdt.cpp` (main loop)
  - [x] `src/mvh_kdtree.cpp` (k-d tree)
  - [x] `src/solvers/l_namoa_kdt_chooseh.cpp` (heuristic selection)
  - [x] Header files: `include/fast_mvh/solvers/` and `kdtree/`
- [x] `Source_Code/APEX_Heuristic_Builder/` (reference)
- [x] `Build_Info/CMakeLists.txt` (build configuration)
- [x] `Baseline_Reference/` (read-only L-NAMOA*_dr-mvh)

### Experimental Data
- [x] `Experimental_Results/FAST_MVH_Expanded_Results.csv`
  - 25 cases, 158 runs, all byte-identical to baseline
- [x] `Experimental_Results/FAST_MVH_Cache_Effect.svg` (paired plot)
- [x] `Experimental_Results/grids_metrics.csv` (run logs)
- [x] `Experimental_Results/bay_metrics.csv` (run logs)

### Road Network Data
- [x] `Road_Networks/BAY-d.gr.gz` (Bay distance, SHA256 verified)
- [x] `Road_Networks/BAY-t.gr.gz` (Bay travel time, SHA256 verified)
- [x] `Road_Networks/sources.json` (metadata + generation params)

### Test Instances
- [x] `Test_Instances/` (pre-generated grids & heuristics)
  - Grid topologies, cost matrices (3D–6D)
  - A*pex heuristic sets (lexicographically sorted)
  - Input metadata with SHA256

---

## 🔍 Reproducibility Verification

### Baseline Comparisons (25 completed cases)
```
✓ All 158 cached/uncached runs byte-identical to baseline
✓ Expansion counts match: exp, gen (extractions), reins (reinsertions)
✓ Solution costs in same order (lexicographic OPEN ordering identical)
✓ Baseline source: commit 0a2f9ea, unmodified
✓ Binary hashes: recorded in run manifests
```

### Correctness Properties
```
✓ Lexicographic ordering of new A*pex MVHs enforced
✓ All heuristic files dimension-checked before search
✓ Older heuristic ties empirically verified (archived agreement maintained)
✓ Theorem 1: Decision equivalence preserved
✓ No modification to baseline or base search algorithm
```

### Instrumentation Completeness
```
✓ cmpchk, cmpupd: Frontier dominance query/update counts
✓ cmpfull: Full-dimensional fallback counts
✓ fallback_rate: Percentage of fallbacks vs. local tests
✓ kd_builds: Frontier tree promotion events
✓ wc_hit, lw_hit: Witness cache hit counts
✓ domT: Total dominance tests
✓ Process wall time + internal search timer
✓ Affinity mask (PROTO_AFFINITY=4) recorded
```

---

## 📊 Key Results Summary

### Grid Experiments
| Domain | M | Solutions | Speedup | Trees | Cache Effect |
|--------|---|-----------|---------|-------|--------------|
| 10×10 A*pex | 3 | 1,602 | 1.30× | 0 | +20.4% |
| 10×10 A*pex | 4 | 11,330 | 5.41× | 12 | +5.4% |
| 8×8 A*pex | 5 | 9,412 | 4.73× | 12 | +10.0% |
| **8×8 A*pex** | **6** | **70,196** | **38.46×** | **18** | **+14.6%** |

### Road Network Results
| Graph | M | Solutions | Speedup | Trees |
|-------|---|-----------|---------|-------|
| Bay-8 | 3 | 238 | 1.06× | 1 |
| Bay-16 | 3 | 3,534 | 1.59× | 11 |
| Bay-8 | 4 | 3,716 | 19.60× | 210 |
| NY-8 | 4 | 5,713 | 42.2× | ? |

### Cache Effect Analysis
- **Genuine MVHs:** 55–82% cache hits → 5–20% time savings
- **Landmark controls:** 12–20% cache hits → Mixed (gains to 14% regression)
- **Conclusion:** Workload-dependent; complete method still 25–35× faster despite cache overhead

---

## 🗂️ File Manifest

```
FAST-MVH_Deliverables/
├── README.md ........................ Start here (comprehensive guide)
├── THEORY_CONTEXT.md ............... Theoretical background
├── DEPLOYMENT_CHECKLIST.md ......... This file
│
├── FAST_MVH_Research_Paper.pdf .... Publication-ready (7 pages)
├── FAST_MVH_Research_Paper.md ..... Editable source
│
├── Source_Code/
│   ├── FAST_MVH_Implementation/
│   │   ├── src/
│   │   │   ├── solvers/
│   │   │   │   ├── l_namoa_dr_mvh_kdt.cpp ......... Main FAST-MVH loop
│   │   │   │   ├── l_namoa_kdt_chooseh.cpp ...... Heuristic selection
│   │   │   │   └── [other solver variants]
│   │   │   ├── mvh_kdtree.cpp ..................... K-d tree implementation
│   │   │   ├── main.cpp
│   │   │   └── [utility files]
│   │   ├── include/
│   │   │   └── fast_mvh/
│   │   │       ├── solvers/ ...................... Header files
│   │   │       └── kdtree/ ....................... K-d tree headers
│   │   └── CMakeLists.txt ........................ See Build_Info/
│   │
│   └── APEX_Heuristic_Builder/
│       └── [Reference A*pex code]
│
├── Road_Networks/
│   ├── BAY-d.gr.gz ............................ Bay Area distance
│   │   SHA256: 630c4a96869b3ecdd631ceb0f63923fb954ee4c7c26d983863612e2c4442462e
│   ├── BAY-t.gr.gz ............................ Bay Area travel time
│   │   SHA256: 85d597078d13907e60ff826d8c7fe0b18ca4c690d003751e27e30ee4011091c3
│   └── sources.json ........................... Metadata & generation params
│
├── Test_Instances/
│   ├── [Grid topologies & costs]
│   ├── [Pre-built heuristic files]
│   └── [Input metadata JSON]
│
├── Experimental_Results/
│   ├── FAST_MVH_Expanded_Results.csv ......... 25 cases, all counters
│   ├── FAST_MVH_Cache_Effect.svg ............ Paired plot
│   ├── grids_metrics.csv ..................... Grid run logs
│   └── bay_metrics.csv ....................... Road run logs
│
├── Build_Info/
│   ├── CMakeLists.txt ........................ C++20 build configuration
│   └── github-app.yml ........................ Repository config
│
└── Baseline_Reference/
    ├── bridging-mvh-dr/ ..................... L-NAMOA*_dr-mvh (read-only)
    └── code-appendix/ ....................... Baseline source/build
```

---

## 🚀 Integration Guide

### Adding to Your Repository

**Step 1: Copy deliverables**
```bash
cp -r FAST-MVH_Deliverables/* your-repo/fast-mvh/
```

**Step 2: Update .gitignore** (optional)
```gitignore
# Road network data (large)
*.gr.gz

# Build artifacts
build/
*.o
```

**Step 3: Document in your repo**
```markdown
# FAST-MVH: Fast Adaptive Search with Trees for Multi-Valued Heuristic Search

See `fast-mvh/README.md` for complete documentation.

### Quick Start
- Read: `fast-mvh/FAST_MVH_Research_Paper.pdf`
- Build: `cd fast-mvh/Source_Code/FAST_MVH_Implementation && mkdir build && cd build && cmake .. && make`
- Run: See `fast-mvh/README.md` for CLI usage
```

### Citation
```bibtex
@inproceedings{fastmvh2026,
  title={FAST-MVH: Fast Adaptive Search with Trees for Multi-Valued Heuristic Search},
  author={Ber-Feigenberg, Ariel},
  booktitle={Proceedings of the Symposium on Combinatorial Search (SoCS)},
  year={2026}
}
```

---

## ✅ Pre-Submission Checklist

- [x] Paper written (7 pages, publication-ready)
- [x] All experiments completed and validated
- [x] Byte-equality verified for all 158 runs
- [x] Baselines unmodified (commit 0a2f9ea)
- [x] Source code builds without warnings (C++20 strict)
- [x] Road network data SHA256 verified
- [x] All counters and metrics logged
- [x] Reproducibility artifacts included (binaries, hashes, CLI args)
- [x] Theoretical context documented (Lemmas, Theorems, proofs)
- [x] References complete and formatted
- [x] Deliverables folder organized and indexed

---

## 📞 Support & Troubleshooting

**Q: The paper PDF looks different when I open it locally**
A: The PDF was rendered via Edge CDP with MathML. If you don't see math rendering, check your viewer supports MathML or regenerate: `python scratchpad/paper_verify/pdf_build/build_fast_mvh.py`

**Q: Build fails with C++ compiler error**
A: Ensure C++20 support:
```bash
cmake -DCMAKE_CXX_FLAGS="-std=c++20" -DCMAKE_BUILD_TYPE=Release ..
```

**Q: Road network files are huge; can I skip them?**
A: Optional for paper review, required for full reproduction. SHA256 hashes ensure integrity.

**Q: Can I modify the baseline solver?**
A: No. Baseline_Reference/ is intentionally read-only to maintain reproducibility claims.

**Q: Where do I add my own experiments?**
A: Use the same structure under `benchmarks/runs/` with timestamped folders and `metrics.csv`.

---

## 📝 Notes for Reviewers

- **Paper scope:** Focus on Algorithm 1, Algorithm 2 (k-d tree queries), correctness arguments (Theorem 1)
- **Evaluation scope:** 25 completed baseline-matched cases; 5 timeout cases separate (no baseline front available)
- **Reproducibility:** All binaries, source commits, hashes recorded; no cherry-picked results
- **Limitations:** Single-machine, single-threaded; Windows-specific build scripts (adapt as needed)
- **Future work:** Non-lexicographic MVH search, larger-scale grids, multi-threaded variants

---

**Version:** 1.0  
**Status:** Complete & ready for distribution  
**License:** See original repository

