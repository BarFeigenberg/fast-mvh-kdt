#!/usr/bin/env bash
# Reproduce Appendix E (objective-ordering sensitivity): Tables tab:ordersynth
# and tab:ordertime, and the "k-d worst beats every baseline's best" check.
#
# Self-contained from the shipped grid streams (data/streams/grid/). Dominance
# counts are exact and machine-independent; the tab:ordertime speedup column is
# single-core wall-clock and hardware-dependent. Road rows additionally require
# captured road streams (see README, Experiment 4).
#
# Usage:  bash scripts/reproduce_appendix.sh
#   env:  BUILD=<dir>   (default: build)   PYTHON=<python>  (default: python3)
set -euo pipefail
cd "$(dirname "$0")/.."                         # repo root
BUILD="${BUILD:-build}"
PY="${PYTHON:-python3}"

echo "[1/4] building drivers at -O2 ..."
cmake -S . -B "$BUILD" -DCMAKE_CXX_FLAGS="-O2" >/dev/null
cmake --build "$BUILD" --target replay_coordorder replay_stats -j >/dev/null

bin() { local p="$BUILD/$1"; [ -x "$p" ] && echo "$p" || echo "$p.exe"; }
CO=$(bin replay_coordorder); ST=$(bin replay_stats)

mkdir -p results
GRID=(data/streams/grid/m3.stream data/streams/grid/m4.stream \
      data/streams/grid/m5.stream data/streams/grid/m6.stream)

echo "[2/4] sweeping all d! coordinate orderings on m3..m6 (exact counts) ..."
"$PY" scripts/coord_order_study.py --bin "$CO" --out results/order_synth.csv --streams "${GRID[@]}"

echo
echo "[3/4] ===== summary + LaTeX (Table tab:ordersynth) ====="
"$PY" scripts/coord_order_summarize.py --synth results/order_synth.csv
"$PY" scripts/coord_order_latex.py --synth results/order_synth.csv

echo
echo "[4/4] ===== timing table (Table tab:ordertime) ====="
"$PY" scripts/coord_order_timetable.py --bin "$ST"

echo
echo "done.  Raw per-ordering counts: results/order_synth.csv"
