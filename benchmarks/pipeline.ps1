if (-not (Test-Path "benchmarks/instances/overnight")) {
    Write-Host "Starting Data Generation (Phase 1)..."
    python benchmarks/generators/generate_overnight.py
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Generation failed!"
        exit 1
    }
} else {
    Write-Host "Data already generated (Phase 1 skipped)."
}

Write-Host "Starting Sequential Benchmarking (Phase 2)..."

while ($true) {
    python benchmarks/run_overnight.py
    if ($LASTEXITCODE -eq 0) {
        Write-Host "Overnight Pipeline Complete!"
        break
    }
    Write-Host "Process exited with code $LASTEXITCODE. Auto-resuming in 3 seconds..."
    Start-Sleep -Seconds 3
}
