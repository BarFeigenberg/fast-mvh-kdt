$cases = @(
    @{ dir = "scratchpad/grids/grid_8x8_m8_rho-0.2"; goal = 64; M = 8; eps = 0.05 },
    @{ dir = "scratchpad/grids/grid_8x8_m8_rho-0.4"; goal = 64; M = 8; eps = 0.05 },
    @{ dir = "scratchpad/grids/grid_9x9_m7_rho0.0"; goal = 81; M = 7; eps = 0.05 },
    @{ dir = "scratchpad/grids/grid_9x9_m7_rho-0.4"; goal = 81; M = 7; eps = 0.05 },
    @{ dir = "scratchpad/maps/bay_8d_800"; goal = 800; M = 7; eps = 0.1 },
    @{ dir = "scratchpad/maps/bay_8d_800"; goal = 800; M = 8; eps = 0.1 }
)

foreach ($c in $cases) {
    if ($c.dir -match "bay") {
        $out = "$($c.dir)/apex_$($c.M)d_$($c.eps).mvh"
    } else {
        $out = "$($c.dir)/apex_$($c.eps).mvh"
    }

    if (-not (Test-Path $out)) {
        Write-Host "Generating A*pex for $($c.dir) (Goal=$($c.goal), M=$($c.M))..."
        $objs = 0..($c.M - 1)
        .\build\Release\apex_builder.exe --map $c.dir --goal $c.goal --eps $c.eps --out $out --objectives $objs
    }
}
Write-Host "All heuristics generated!"
