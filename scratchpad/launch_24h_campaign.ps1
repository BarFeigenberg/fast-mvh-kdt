# Wait for active generators to finish
Wait-Process -Name "apex_builder" -ErrorAction SilentlyContinue

# Generate A*pex heuristics for new heavy grids
$heavy_cases = @(
    @{ dir = "scratchpad/grids/grid_n8_m7_rho-0.2"; goal = 64; M = 7; eps = 0.05 },
    @{ dir = "scratchpad/grids/grid_n8_m8_rho0.0";  goal = 64; M = 8; eps = 0.05 },
    @{ dir = "scratchpad/grids/grid_n9_m6_rho-0.2"; goal = 81; M = 6; eps = 0.05 },
    @{ dir = "scratchpad/grids/grid_n10_m6_rho0.0"; goal = 100; M = 6; eps = 0.05 }
)

foreach ($c in $heavy_cases) {
    $out = "$($c.dir)/apex_$($c.eps).mvh"
    if (-not (Test-Path $out)) {
        Write-Host "Generating A*pex for $($c.dir) (Goal=$($c.goal), M=$($c.M))..."
        $objs = 0..($c.M - 1)
        .\build\Release\apex_builder.exe --map $c.dir --goal $c.goal --eps $c.eps --out $out --objectives $objs
    }
}

# Run the 24h benchmark campaign
Write-Host "Starting 24h benchmark campaign..."
python scratchpad/run_24h_campaign.py
