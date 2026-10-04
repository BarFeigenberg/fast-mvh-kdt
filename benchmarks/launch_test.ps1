$pythonw = "C:\Users\barf9\AppData\Local\Programs\Python\Python313\pythonw.exe"
$script  = "c:\Users\barf9\Desktop\FAST MVH-KDT\benchmarks\overnight_campaign.py"
$workdir = "c:\Users\barf9\Desktop\FAST MVH-KDT"
$testout = "c:\Users\barf9\Desktop\FAST MVH-KDT\benchmarks\runs\pythonw_test.txt"

# Test first
Remove-Item $testout -ErrorAction SilentlyContinue
$testscript = "c:\Users\barf9\Desktop\FAST MVH-KDT\benchmarks\test_pythonw.py"

Write-Host "=== Testing pythonw with quoted path ==="
# Key fix: wrap script path in escaped quotes so Python sees one argument
$proc = Start-Process -FilePath $pythonw -ArgumentList "`"$testscript`"" -WorkingDirectory $workdir -PassThru
Start-Sleep -Seconds 5
Write-Host "Exit code: $($proc.ExitCode)"
Write-Host "File exists: $(Test-Path $testout)"
if (Test-Path $testout) {
    Write-Host "=== SUCCESS - pythonw works ==="
    Get-Content $testout
    Write-Host ""
    Write-Host "=== Launching overnight campaign ==="
    Start-Process -FilePath $pythonw -ArgumentList "`"$script`"" -WorkingDirectory $workdir
    Write-Host "Campaign launched. PID tracking via Get-Process pythonw."
} else {
    Write-Host "=== FAILED - checking exit code ==="
}
