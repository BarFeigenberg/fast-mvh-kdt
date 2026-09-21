# Reproduce Appendix E (objective-ordering sensitivity): Tables tab:ordersynth
# and tab:ordertime, and the "k-d worst beats every baseline's best" check.
# Self-contained from the shipped grid streams (data/streams/grid/). Dominance
# counts are exact and machine-independent; the tab:ordertime speedup column is
# single-core wall-clock and hardware-dependent.
#
# Usage:  pwsh scripts/reproduce_appendix.ps1  [-Build build] [-Python python]
param([string]$Build = "build", [string]$Python = "python")

$ErrorActionPreference = "Stop"
Set-Location (Join-Path $PSScriptRoot "..")     # repo root

Write-Host "[1/4] building drivers at -O2 ..."
cmake -S . -B $Build -DCMAKE_CXX_FLAGS="-O2" | Out-Null
cmake --build $Build --target replay_coordorder replay_stats -j | Out-Null

function Bin($name) {
  $p = Join-Path $Build $name
  if (Test-Path "$p.exe") { return "$p.exe" } else { return $p }
}
$CO = Bin "replay_coordorder"; $ST = Bin "replay_stats"

New-Item -ItemType Directory -Force results | Out-Null
$grid = @("data/streams/grid/m3.stream", "data/streams/grid/m4.stream",
          "data/streams/grid/m5.stream", "data/streams/grid/m6.stream")

Write-Host "[2/4] sweeping all d! coordinate orderings on m3..m6 (exact counts) ..."
& $Python scripts/coord_order_study.py --bin $CO --out results/order_synth.csv --streams $grid

Write-Host "`n[3/4] ===== summary + LaTeX (Table tab:ordersynth) ====="
& $Python scripts/coord_order_summarize.py --synth results/order_synth.csv
& $Python scripts/coord_order_latex.py --synth results/order_synth.csv

Write-Host "`n[4/4] ===== timing table (Table tab:ordertime) ====="
& $Python scripts/coord_order_timetable.py --bin $ST

Write-Host "`ndone.  Raw per-ordering counts: results/order_synth.csv"
