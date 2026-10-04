param([string]$Out='scratchpad\perf\harness.exe', [string[]]$Extra=@(), [string]$Main='scratchpad\perf\harness.cpp', [string]$Opt='-O2')
$B='baselines\bridging-mvh-dr'
$srcs=@("$B\src\parser.cpp","$B\src\parsers\multi_valued_heuristic_parser.cpp","$B\src\data_structures\adjacency_matrix.cpp","$B\src\data_structures\apex_path_pair.cpp","$B\src\data_structures\map_queue.cpp","$B\src\data_structures\node.cpp","$B\src\multivalued_heuristic\apex_mvh.cpp","$B\src\multivalued_heuristic\l_namoa_dr_mvh.cpp","$B\src\solvers\apex.cpp","src\src\mvh_kdtree.cpp","src\src\solvers\l_namoa_kdt_chooseh.cpp","src\src\solvers\l_namoa_dr_mvh_kdt.cpp","scratchpad\proposal\src\src\solvers\l_namoa_dr_mvh_fast.cpp",$Main) + $Extra
python -m ziglang c++ -std=c++20 $Opt -DNDEBUG -w -march=native -Iscratchpad\shim -Iscratchpad\perf -Iscratchpad\proposal\src\include -Isrc\include -I"$B\include" @srcs -o $Out "-Wl,--stack,536870912" 2>&1 | Select-String -Pattern 'error' -Context 0,4 | Select -First 40
if (Test-Path $Out) { "built $Out" }

