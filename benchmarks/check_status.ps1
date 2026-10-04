Write-Host "=== Python/Pythonw Processes ==="
Get-Process pythonw,python -ErrorAction SilentlyContinue | Format-Table Id,Name,CPU,StartTime -AutoSize

Write-Host "=== All overnight log files ==="
Get-ChildItem "benchmarks\runs" -Filter "overnight_2026*.log" | Sort-Object Name | Format-Table Name,LastWriteTime,Length

Write-Host "=== Latest log content ==="
$logs = Get-ChildItem "benchmarks\runs" -Filter "overnight_2026*.log" | Sort-Object Name -Descending
if ($logs) {
    Write-Host $logs[0].FullName
    Get-Content $logs[0].FullName
}

Write-Host "=== Status heartbeat ==="
Get-Content "benchmarks\runs\overnight_status.txt" -ErrorAction SilentlyContinue
