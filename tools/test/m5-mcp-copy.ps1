# M5 self-check: MCP write tools (add/move/save) on a COPY of a model in build\edit_test.
#
#   powershell -ExecutionPolicy Bypass -File tools\test\m5-mcp-copy.ps1
#
# Sets up build\edit_test\ (gitignored): a copy of the release exe, a workbench.json whose
# first asset root is build\edit_test\m5root, and m5root with copies of the crossbow xmodel and
# its xsurface. Starts that copy with -mcp, calls load_model / add_tag / move_tag / save_model,
# reloads the saved copy and verifies that the added tag persisted, that the .bak is the
# untouched copy and that <model>.tags.json exists. Writes review/M5/mcp-smoke.txt.
# The real model in GAME\zonetool\mod is hashed before and after and must not change.
# Exit code 0 = all checks passed.

$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$releaseExe = Join-Path $repo "build\bin\Release\hmw-workbench.exe"
$testDir = Join-Path $repo "build\edit_test"
$testExe = Join-Path $testDir "hmw-workbench.exe"
$root = Join-Path $testDir "m5root"
$review = Join-Path $repo "review\M5"
$logPath = Join-Path $review "mcp-smoke.txt"
$game = "C:\Games\cod-hwm\Call of Duty Horizon Modern Warfare\Call of Duty Modern Warfare Remastered"
$gameModel = Join-Path $game "zonetool\mod\xmodel\vm_crossbow_base.xmb"
$gameSurface = Join-Path $game "zonetool\mod\xsurface\010296ab_3f739e8a_100.xsb"
$copyModel = Join-Path $root "xmodel\vm_crossbow_base.xmb"
$copySurface = Join-Path $root "xsurface\010296ab_3f739e8a_100.xsb"
$copyBackup = "$copyModel.bak"
$copyTags = Join-Path $root "xmodel\vm_crossbow_base.tags.json"

New-Item -ItemType Directory -Force $review | Out-Null
New-Item -ItemType Directory -Force (Join-Path $root "xmodel") | Out-Null
New-Item -ItemType Directory -Force (Join-Path $root "xsurface") | Out-Null

$script:log = [System.Collections.Generic.List[string]]::new()
$script:checks = [System.Collections.Generic.List[string]]::new()
function Write-Log([string]$text) { $script:log.Add($text) }
function Check([bool]$ok, [string]$pass, [string]$fail) {
    if ($ok) { $script:checks.Add("PASS $pass") } else { $script:checks.Add("FAIL $fail") }
}
function Sha256([string]$path) { (Get-FileHash -Algorithm SHA256 -Path $path).Hash }

# ---- setup: fresh copy every run, real game model only read ----
$gameHashBefore = Sha256 $gameModel
Copy-Item $releaseExe $testExe -Force
Copy-Item $gameModel $copyModel -Force
Copy-Item $gameSurface $copySurface -Force
Remove-Item $copyBackup, $copyTags -ErrorAction SilentlyContinue
$pristineHash = Sha256 $copyModel
$pristineSize = (Get-Item $copyModel).Length

$sourceJson = Join-Path $repo "build\bin\Release\workbench.json"
if (-not (Test-Path $sourceJson)) {
    # Any CLI invocation writes the default workbench.json next to the exe.
    & $releaseExe -info vm_crossbow_base | Out-Null
}
$settings = Get-Content $sourceJson -Raw | ConvertFrom-Json
$settings.assetRoots = @("m5root") + @($settings.assetRoots)
$settings | ConvertTo-Json -Depth 10 | Set-Content (Join-Path $testDir "workbench.json") -Encoding UTF8

Write-Log "hmw-workbench M5 MCP copy-save test"
Write-Log "date: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
Write-Log "copy exe: $testExe"
Write-Log "copy root: $root (first asset root in build\edit_test\workbench.json)"
Write-Log ("pristine copy: {0} bytes, sha256 {1}" -f $pristineSize, $pristineHash)
Write-Log ("real model untouched check: sha256 before {0}" -f $gameHashBefore)

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
function Send-Raw([string]$json) {
    $server.StandardInput.WriteLine($json)
    $server.StandardInput.Flush()
}
function Read-Response([int]$timeoutMs = 120000) {
    $task = $server.StandardOutput.ReadLineAsync()
    if (-not $task.Wait($timeoutMs)) { throw "timeout waiting for the MCP server" }
    return $task.Result
}
function Call-Tool([string]$name, $arguments) {
    $script:nextId++
    $id = $script:nextId
    $request = @{ jsonrpc = "2.0"; id = $id; method = "tools/call"; params = @{ name = $name; arguments = $arguments } } |
        ConvertTo-Json -Compress -Depth 20
    Send-Raw $request
    $line = Read-Response
    Write-Log ""
    Write-Log ("> tools/call $name " + ($arguments | ConvertTo-Json -Compress -Depth 10))
    $reply = $line | ConvertFrom-Json
    if ($reply.PSObject.Properties.Name -contains "error") {
        Write-Log ("< RPC error " + ($reply.error | ConvertTo-Json -Compress))
        return $null
    }
    $result = $reply.result
    $text = ($result.content | Where-Object { $_.type -eq "text" } | Select-Object -First 1).text
    Write-Log ("< isError=" + $result.isError)
    if ($text.Length -gt 1500) { $text = $text.Substring(0, 1500) + " ...[truncated]" }
    Write-Log ("< " + $text)
    if ($result.isError) { return $null }
    try { return $text | ConvertFrom-Json } catch { return $null }
}

$failed = 1
try {
    $script:nextId++
    Send-Raw (@{ jsonrpc = "2.0"; id = $script:nextId; method = "initialize"; params = @{ protocolVersion = "2025-06-18" } } | ConvertTo-Json -Compress)
    $init = (Read-Response) | ConvertFrom-Json
    Write-Log ""
    Write-Log "> initialize"
    Write-Log ("< " + $init.result.serverInfo.name + " protocol " + $init.result.protocolVersion)
    Check ($init.result.serverInfo.name -eq "hmw-workbench") "initialize" "initialize"
    Send-Raw '{"jsonrpc":"2.0","method":"notifications/initialized"}'

    $loaded = Call-Tool "load_model" @{ model = "vm_crossbow_base" }
    Check ($loaded -and $loaded.model -eq "vm_crossbow_base" -and $loaded.bones -eq 39) "load_model from the copy (39 bones)" "load_model (copy)"

    $before = Call-Tool "get_tags" @{ name = "tag_red_dot" }
    $redDot = @($before.tags | Where-Object { $_.name -eq "tag_red_dot" })[0]
    Check ($null -ne $redDot) "get_tags: tag_red_dot present on the copy" "get_tags: tag_red_dot missing"

    $added = Call-Tool "add_tag" @{ tag = "tag_m5_copy"; parent = "j_gun"; copy_from = "tag_red_dot" }
    Check ($added -and $added.added -eq $true -and $added.dirty -eq $true) "add_tag tag_m5_copy (copy of tag_red_dot)" "add_tag tag_m5_copy"

    $wanted = @([float]$redDot.local_pos[0], [float]$redDot.local_pos[1], [float]($redDot.local_pos[2] + 0.05))
    $moved = Call-Tool "move_tag" @{ tag = "tag_m5_copy"; pos = $wanted }
    $delta = 99.0
    if ($moved) {
        $delta = [Math]::Sqrt(
            [Math]::Pow($moved.local_pos[0] - $wanted[0], 2) +
            [Math]::Pow($moved.local_pos[1] - $wanted[1], 2) +
            [Math]::Pow($moved.local_pos[2] - $wanted[2], 2))
    }
    Check ($moved -and $moved.dirty -eq $true -and $delta -lt 1e-4) ("move_tag tag_m5_copy to ({0:N5}, {1:N5}, {2:N5})" -f $wanted[0], $wanted[1], $wanted[2]) "move_tag tag_m5_copy"

    $saved = Call-Tool "save_model" @{}
    Check ($saved -and $saved.saved -eq $copyModel) "save_model wrote the copy" "save_model"
    Check ($saved -and $saved.backup -eq $copyBackup) "save_model reports the .bak path" "save_model .bak path"

    $backupExists = Test-Path $copyBackup
    $backupHash = if ($backupExists) { Sha256 $copyBackup } else { "" }
    Check ($backupExists -and $backupHash -eq $pristineHash) (".bak exists and equals the pristine copy ({0} bytes)" -f $pristineSize) ".bak missing or not the pristine copy"
    $savedSize = (Get-Item $copyModel).Length
    Check ($savedSize -ne $pristineSize -and (Sha256 $copyModel) -ne $pristineHash) ("saved copy differs from the .bak ({0} -> {1} bytes)" -f $pristineSize, $savedSize) "saved copy did not change"
    Check (Test-Path $copyTags) "<model>.tags.json written" ".tags.json missing"
    if (Test-Path $copyTags) {
        $tagsJson = Get-Content $copyTags -Raw | ConvertFrom-Json
        $entry = @($tagsJson.tags | Where-Object { $_.name -eq "tag_m5_copy" })[0]
        Check ($null -ne $entry) "tags.json contains tag_m5_copy" "tags.json has no tag_m5_copy"
    }

    # reload from disk: the added tag must be part of the saved baseline
    $reloaded = Call-Tool "load_model" @{ model = "vm_crossbow_base" }
    Check ($reloaded -and $reloaded.bones -eq 40) ("reloaded the saved copy (40 bones = 39 + tag_m5_copy)") ("reload: {0} bones" -f $reloaded.bones)
    $after = Call-Tool "get_tags" @{ name = "tag_m5_copy" }
    $persisted = @($after.tags | Where-Object { $_.name -eq "tag_m5_copy" })[0]
    $persistDelta = 99.0
    if ($persisted) {
        $persistDelta = [Math]::Sqrt(
            [Math]::Pow($persisted.local_pos[0] - $wanted[0], 2) +
            [Math]::Pow($persisted.local_pos[1] - $wanted[1], 2) +
            [Math]::Pow($persisted.local_pos[2] - $wanted[2], 2))
    }
    Check ($null -ne $persisted -and $persisted.added -eq $false -and $persistDelta -lt 1e-4) "added + moved tag persisted after reload (not dirty, local pos matches)" "tag_m5_copy did not persist"

    $gameHashAfter = Sha256 $gameModel
    Check ($gameHashAfter -eq $gameHashBefore) ("real model untouched (sha256 {0})" -f $gameHashAfter) "REAL MODEL CHANGED"

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
    $err = ""
    try { $err = $server.StandardError.ReadToEnd() } catch {}
    if ($err.Trim().Length -gt 0) {
        $script:log.Add("")
        $script:log.Add("stderr:")
        $script:log.Add($err)
    }
    $script:log | Set-Content -Path $logPath -Encoding UTF8
    Write-Output ("wrote " + $logPath)
    if ($failed -gt 0) { exit 1 }
}
