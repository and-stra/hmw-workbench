# M7 scripted window-app test.
#
#   powershell -ExecutionPolicy Bypass -File tools\test\m7-window.ps1
#
# Copies the release exe and a fresh crossbow model into the gitignored build\edit_test root,
# runs the copy with -window-test (real Win32 window + D3D11 swapchain + ImGui frames):
# load crossbow + M4, play their idle anims, mount attachments, add/move/save a tag on the COPY
# model, reload it and close after 30 frames. Writes review/M7/window-test.txt and checks that
# the real game model was not touched. Exit code 0 = all checks passed.

$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$game = "C:\Games\cod-hwm\Call of Duty Horizon Modern Warfare\Call of Duty Modern Warfare Remastered"
$gameModel = Join-Path $game "zonetool\mod\xmodel\vm_crossbow_base.xmb"
$testDir = Join-Path $repo "build\edit_test"
$testExe = Join-Path $testDir "hmw-workbench.exe"
$copyModel = Join-Path $testDir "m5root\xmodel\vm_crossbow_base.xmb"
$copySurface = Join-Path $testDir "m5root\xsurface\010296ab_3f739e8a_100.xsb"
$copyBackup = "$copyModel.bak"
$copyTags = Join-Path $testDir "m5root\xmodel\vm_crossbow_base.tags.json"
$review = Join-Path $repo "review\M7"
$logPath = Join-Path $review "window-test.txt"

New-Item -ItemType Directory -Force $review | Out-Null
New-Item -ItemType Directory -Force (Split-Path $copyModel) | Out-Null
New-Item -ItemType Directory -Force (Split-Path $copySurface) | Out-Null

$gameHashBefore = (Get-FileHash -Algorithm SHA256 $gameModel).Hash
Copy-Item (Join-Path $repo "build\bin\Release\hmw-workbench.exe") $testExe -Force
Copy-Item $gameModel $copyModel -Force
Copy-Item (Join-Path $game "zonetool\mod\xsurface\010296ab_3f739e8a_100.xsb") $copySurface -Force
Remove-Item $copyBackup, $copyTags -ErrorAction SilentlyContinue
$pristineHash = (Get-FileHash -Algorithm SHA256 $copyModel).Hash

$process = Start-Process -FilePath $testExe -ArgumentList @("-window-test", $logPath) `
    -WorkingDirectory $testDir -PassThru -Wait
$gameHashAfter = (Get-FileHash -Algorithm SHA256 $gameModel).Hash

$checks = [System.Collections.Generic.List[string]]::new()
$log = Get-Content $logPath -Raw
if ($process.ExitCode -eq 0) { $checks.Add("PASS window app exited 0") } else { $checks.Add("FAIL exit code $($process.ExitCode)") }
if ($log -match "result: PASS \(0 failed\)") { $checks.Add("PASS scripted window test (load crossbow/M4, anims, attach, edit copy, save, reload, close)") }
else { $checks.Add("FAIL scripted window test") }
if (Test-Path $copyBackup) {
    $backupHash = (Get-FileHash -Algorithm SHA256 $copyBackup).Hash
    if ($backupHash -eq $pristineHash) { $checks.Add("PASS copy .bak is the pristine model") }
    else { $checks.Add("FAIL copy .bak differs from the pristine model") }
} else { $checks.Add("FAIL copy .bak missing") }
if (Test-Path $copyTags) { $checks.Add("PASS copy .tags.json written") } else { $checks.Add("FAIL copy .tags.json missing") }
if ($gameHashAfter -eq $gameHashBefore) { $checks.Add("PASS real game model untouched (sha256 $gameHashAfter)") }
else { $checks.Add("FAIL REAL GAME MODEL CHANGED") }

$summary = @("", "checks:") + ($checks | ForEach-Object { "  $_" })
$failed = @($checks | Where-Object { $_ -like "FAIL*" }).Count
$summary += ""
$summary += "result: $($checks.Count - $failed) passed, $failed failed"
$summary | Add-Content -Path $logPath -Encoding UTF8
Write-Output ("wrote " + $logPath)
if ($failed -gt 0) { exit 1 }
