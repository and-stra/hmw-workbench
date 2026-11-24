# M10 hot-reload test on a COPY in build\edit_test (no real game file is touched).
#
#   powershell -ExecutionPolicy Bypass -File tools\test\m10-hot-reload.ps1
#
# Starts the copy exe -mcp with build\edit_test\m5root as the first root, loads the copy model,
# replaces the copy's .xmb with the pristine model (different bone count), waits for the debounce
# and checks that the workbench reloaded it. Writes review/M10/hot-reload.txt.

$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$game = "C:\Games\cod-hwm\Call of Duty Horizon Modern Warfare\Call of Duty Modern Warfare Remastered"
$gameModel = Join-Path $game "zonetool\mod\xmodel\vm_crossbow_base.xmb"
$testDir = Join-Path $repo "build\edit_test"
$testExe = Join-Path $testDir "hmw-workbench.exe"
$root = Join-Path $testDir "m5root"
$copyModel = Join-Path $root "xmodel\vm_crossbow_base.xmb"
$copySurface = Join-Path $root "xsurface\010296ab_3f739e8a_100.xsb"
$review = Join-Path $repo "review\M10"
$logPath = Join-Path $review "hot-reload.txt"

New-Item -ItemType Directory -Force $review | Out-Null
New-Item -ItemType Directory -Force (Split-Path $copyModel) | Out-Null
New-Item -ItemType Directory -Force (Split-Path $copySurface) | Out-Null

$gameHashBefore = (Get-FileHash -Algorithm SHA256 $gameModel).Hash
Copy-Item (Join-Path $repo "build\bin\Release\hmw-workbench.exe") $testExe -Force
# Start with a model that has an extra tag (the M5 copy test output), then replace it.
$edited = Join-Path $root "xmodel\vm_crossbow_base_edited.xmb"
Copy-Item $gameModel $edited -Force
Copy-Item $gameModel $copyModel -Force
Copy-Item (Join-Path $game "zonetool\mod\xsurface\010296ab_3f739e8a_100.xsb") $copySurface -Force
Remove-Item "$copyModel.bak", (Join-Path $root "xmodel\vm_crossbow_base.tags.json") -ErrorAction SilentlyContinue
$settingsPath = Join-Path $testDir "workbench.json"
if (-not (Test-Path $settingsPath)) { throw "missing $settingsPath (run tools\test\m5-mcp-copy.ps1 first)" }

$psi = [System.Diagnostics.ProcessStartInfo]::new()
$psi.FileName = $testExe
$psi.Arguments = "-mcp"
$psi.WorkingDirectory = $testDir
$psi.RedirectStandardInput = $true
$psi.RedirectStandardOutput = $true
$psi.RedirectStandardError = $true
$psi.UseShellExecute = $false
$psi.CreateNoWindow = $true
$server = [System.Diagnostics.Process]::Start($psi)

$script:nextId = 0
$script:log = [System.Collections.Generic.List[string]]::new()
$script:checks = [System.Collections.Generic.List[string]]::new()
function Write-Log([string]$text) { $script:log.Add($text) }
function Send-Raw([string]$json) { $server.StandardInput.WriteLine($json); $server.StandardInput.Flush() }
function Read-Response([int]$timeoutMs = 120000) {
    $task = $server.StandardOutput.ReadLineAsync()
    if (-not $task.Wait($timeoutMs)) { throw "timeout waiting for the MCP server" }
    return $task.Result
}
function Call-Json([string]$name, $arguments) {
    $script:nextId++
    $request = @{ jsonrpc = "2.0"; id = $script:nextId; method = "tools/call"; params = @{ name = $name; arguments = $arguments } } |
        ConvertTo-Json -Compress -Depth 20
    Send-Raw $request
    $line = Read-Response
    Write-Log ""
    Write-Log ("> tools/call $name " + ($arguments | ConvertTo-Json -Compress -Depth 10))
    $reply = $line | ConvertFrom-Json
    $result = $reply.result
    $text = ($result.content | Where-Object { $_.type -eq "text" } | Select-Object -First 1).text
    $logText = $text
    if ($logText.Length -gt 1400) { $logText = $logText.Substring(0, 1400) + " ...[truncated]" }
    Write-Log ("< " + $logText)
    if ($result.isError) { return $null }
    try { return $text | ConvertFrom-Json } catch { return $null }
}

$failed = 1
try {
    Write-Log "hmw-workbench M10 hot-reload test (copy in build\edit_test)"
    Write-Log ("date: " + (Get-Date -Format "yyyy-MM-dd HH:mm:ss"))
    $script:nextId++
    Send-Raw (@{ jsonrpc = "2.0"; id = $script:nextId; method = "initialize"; params = @{ protocolVersion = "2025-06-18" } } | ConvertTo-Json -Compress)
    $init = (Read-Response) | ConvertFrom-Json
    Write-Log ("> initialize -> " + $init.result.serverInfo.name)
    Send-Raw '{"jsonrpc":"2.0","method":"notifications/initialized"}'

    $loaded = Call-Json "load_model" @{ model = "vm_crossbow_base" }
    $before = Call-Json "get_bones" @{ limit = 200 }
    $status0 = Call-Json "hot_reload" @{ action = "status" }
    Write-Log ("watched: " + ($status0.watched -join "; "))
    if ($loaded -and $status0 -and $status0.reloads -eq 0 -and $status0.watched.Count -ge 2) {
        $script:checks.Add("PASS load copy + watcher registered $($status0.watched.Count) files")
    } else {
        $script:checks.Add("FAIL load copy / watcher")
    }

    # A valid edit: the M5 copy save produced a 40-bone model; build one by adding a tag with
    # the same command layer, then replace the file with the pristine 39-bone model.
    $add = Call-Json "add_tag" @{ tag = "tag_hot_reload_test"; parent = "j_gun"; copy_from = "tag_red_dot" }
    $save = Call-Json "save_model" @{}
    $editedBones = (Call-Json "load_model" @{ model = "vm_crossbow_base" }).bones
    if ($save -and $editedBones -eq 40) {
        $script:checks.Add("PASS copy model edited to 40 bones (baseline)")
    } else {
        $script:checks.Add("FAIL copy model edit")
    }

    # Replace the copy with the pristine model (39 bones) and wait for the watcher.
    Copy-Item $gameModel $copyModel -Force
    Start-Sleep -Milliseconds 1200
    $poll1 = Call-Json "hot_reload" @{ action = "poll" }
    Start-Sleep -Milliseconds 700
    $poll2 = Call-Json "hot_reload" @{ action = "poll" }
    $after = Call-Json "get_bones" @{ limit = 200 }
    if ($poll2 -and $poll2.reloads -ge 1) {
        $script:checks.Add("PASS hot reload fired after the copy changed (reloads $($poll2.reloads))")
    } else {
        $script:checks.Add("FAIL hot reload did not fire")
    }
    if ($after -and $after.count -eq 39) {
        $script:checks.Add("PASS reloaded copy has 39 bones (was 40)")
    } else {
        $script:checks.Add("FAIL reloaded bone count $($after.count)")
    }
    Write-Log ""
    Write-Log ("status: " + ($poll2 | ConvertTo-Json -Compress))

    $gameHashAfter = (Get-FileHash -Algorithm SHA256 $gameModel).Hash
    if ($gameHashAfter -eq $gameHashBefore) {
        $script:checks.Add("PASS real game model untouched (sha256 $gameHashAfter)")
    } else {
        $script:checks.Add("FAIL REAL GAME MODEL CHANGED")
    }

    $script:log.Add("")
    $script:log.Add("checks:")
    foreach ($check in $script:checks) { $script:log.Add("  $check") }
    $failed = @($script:checks | Where-Object { $_ -like "FAIL*" }).Count
    $script:log.Add("")
    $script:log.Add("result: $($script:checks.Count - $failed) passed, $failed failed")
}
finally {
    try { $server.StandardInput.Close() } catch {}
    if (-not $server.WaitForExit(15000)) { $server.Kill() }
    $script:log | Set-Content -Path $logPath -Encoding UTF8
    Remove-Item $edited -ErrorAction SilentlyContinue
    Write-Output ("wrote " + $logPath)
    if ($failed -gt 0) { exit 1 }
}
