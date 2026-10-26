# Wait for heuristic generation to finish
Write-Host "Waiting for apex_builder to finish generating heuristics..."
Wait-Process -Name "apex_builder" -ErrorAction SilentlyContinue

# Give it a few seconds just to ensure file handles are closed
Start-Sleep -Seconds 3

# Launch the 18 hour campaign
Write-Host "Starting the 18-hour continuous extreme benchmark campaign!"
python scratchpad/run_18h_campaign.py
