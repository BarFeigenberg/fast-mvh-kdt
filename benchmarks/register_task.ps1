$action = New-ScheduledTaskAction `
    -Execute 'python' `
    -Argument '-u benchmarks\overnight_campaign.py' `
    -WorkingDirectory 'c:\Users\barf9\Desktop\FAST MVH-KDT'

$trigger = New-ScheduledTaskTrigger -Once -At (Get-Date).AddSeconds(5)

$settings = New-ScheduledTaskSettingsSet `
    -ExecutionTimeLimit (New-TimeSpan -Hours 72) `
    -DontStopIfGoingOnBatteries `
    -RunOnlyIfNetworkAvailable:$false `
    -MultipleInstances IgnoreNew

Register-ScheduledTask `
    -TaskName 'FAST_Overnight_Campaign' `
    -Action $action `
    -Trigger $trigger `
    -Settings $settings `
    -RunLevel Highest `
    -Force

Write-Host "Task registered. Starting now..."
Start-ScheduledTask -TaskName 'FAST_Overnight_Campaign'
Start-Sleep -Seconds 3

$info = Get-ScheduledTaskInfo -TaskName 'FAST_Overnight_Campaign'
Write-Host "State: $($info.LastTaskResult) | Last Run: $($info.LastRunTime)"
Write-Host "Done."
