param([string]$Out='scratchpad\perf\proto.exe', [string]$Opt='-O2', [string[]]$Defs=@())
$B='baselines\bridging-mvh-dr'
$srcs=@("$B\src\parser.cpp","$B\src\parsers\multi_valued_heuristic_parser.cpp","$B\src\data_structures\adjacency_matrix.cpp","$B\src\data_structures\node.cpp","scratchpad\perf\proto_main.cpp")
python -m ziglang c++ -std=c++20 $Opt -DNDEBUG @Defs -w -march=native -Iscratchpad\shim -Iscratchpad\perf -Isrc\include -I"$B\include" @srcs -o $Out "-Wl,--stack,536870912" 2>&1 | Select-String -Pattern 'error' -Context 0,4 | Select -First 40
if (Test-Path $Out) { "built $Out " + (Get-Item $Out).LastWriteTime }
