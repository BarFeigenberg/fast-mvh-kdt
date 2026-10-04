@echo off
:: launch_overnight.bat
:: Launches overnight_campaign.py as a fully independent Windows process.
:: This process will continue running even if the terminal, agent, or wifi closes.
:: Logs go to: benchmarks\runs\overnight_<timestamp>.log and overnight_stdout.log

set REPO=c:\Users\barf9\Desktop\FAST MVH-KDT
set SCRIPT=%REPO%\benchmarks\overnight_campaign.py
set STDOUT=%REPO%\benchmarks\runs\overnight_stdout.log
set STDERR=%REPO%\benchmarks\runs\overnight_stderr.log

echo Launching overnight campaign as independent process...
echo Log files:
echo   STDOUT: %STDOUT%
echo   SCRIPT log: benchmarks\runs\overnight_*.log

cd /d "%REPO%"

:: Use Start-Process to detach fully from this shell
powershell -Command "Start-Process -FilePath 'python' -ArgumentList '-u', 'benchmarks\overnight_campaign.py' -WorkingDirectory '%REPO%' -RedirectStandardOutput '%STDOUT%' -RedirectStandardError '%STDERR%' -WindowStyle Hidden"

echo Done. Process is running independently in the background.
echo You can close this window or disconnect — the campaign will continue.
echo Check progress with: type "%STDOUT%"
