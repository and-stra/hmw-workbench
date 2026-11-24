# M5 MCP write-tool test: move/add/revert tags without saving.
#
#   powershell -ExecutionPolicy Bypass -File tools\test\m5-edit.ps1
#
# Writes review/M5/mcp-edit.txt. Exit code 0 = all checks passed.

$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$exe = Join-Path $repo "build\bin\Release\hmw-workbench.exe"
$review = Join-Path $repo "review\M5"
New-Item -ItemType Directory -Force $review | Out-Null
$logPath = Join-Path $review "mcp-edit.txt"

$psi = [System.Diagnostics.ProcessStartInfo]::new()
$psi.FileName = $exe
$psi.Arguments = "-mcp"
$psi.WorkingDirectory = $repo
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

function Send-Raw([string]$json) {
    $server.StandardInput.WriteLine($json)
    $server.StandardInput.Flush()
}

function Read-Response([int]$timeoutMs = 90000) {
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
        $script:checks.Add("FAIL ${name}: RPC error")
        return $null
    }
    $result = $reply.result
    if ($result.isError) {
        $text = ($result.content | Where-Object { $_.type -eq "text" } | Select-Object -First 1).text
        Write-Log ("< isError: " + $text)
        $script:checks.Add("FAIL ${name}: isError")
        return $null
    }
    $text = ($result.content | Where-Object { $_.type -eq "text" } | Select-Object -First 1).text
    if ($text.Length -gt 1200) { $text = $text.Substring(0, 1200) + " ...[truncated]" }
    Write-Log ("< " + $text)
    $script:checks.Add("PASS $name")
    try { return $text | ConvertFrom-Json } catch { return $null }
}

try {
    Write-Log "hmw-workbench M5 MCP write-tool test"
    Write-Log ("exe: " + $exe)
    $script:nextId++
    Send-Raw (@{ jsonrpc = "2.0"; id = $script:nextId; method = "initialize"; params = @{ protocolVersion = "2025-06-18" } } | ConvertTo-Json -Compress)
    $init = (Read-Response) | ConvertFrom-Json
    Write-Log "> initialize"
    Write-Log ("< " + $init.result.serverInfo.name + " " + $init.result.protocolVersion)
    Send-Raw '{"jsonrpc":"2.0","method":"notifications/initialized"}'

    $load = Call-Tool "load_weapon" @{ weapon = "iw5_exocrossbow_mp"; hands = "viewhands_tf141" }
    if ($load -and $load.gun_model -eq "vm_crossbow_base") { $script:checks.Add("PASS loaded crossbow") }
    else { $script:checks.Add("FAIL load crossbow") }

    $before = Call-Tool "get_tags" @{ name = "tag_reflex" }
    $reflex = @($before.tags | Where-Object { $_.name -eq "tag_reflex" })[0]
    if ($reflex) { $script:checks.Add("PASS tag_reflex present") }
    else { $script:checks.Add("FAIL tag_reflex missing") }

    $moved = Call-Tool "move_tag" @{ tag = "tag_reflex"; pos = @(3.2528, 0.0000638, 6.091953) }
    if ($moved -and $moved.dirty -eq $true) { $script:checks.Add("PASS move_tag (dirty)") }
    else { $script:checks.Add("FAIL move_tag") }

    $revert = Call-Tool "revert_tags" @{ tag = "tag_reflex" }
    if ($revert -and $revert.dirty -eq $false) { $script:checks.Add("PASS revert_tags single (clean)") }
    else { $script:checks.Add("FAIL revert_tags single") }

    $added = Call-Tool "add_tag" @{ tag = "tag_workbench_test"; parent = "j_gun"; copy_from = "tag_red_dot" }
    if ($added -and $added.added -eq $true) { $script:checks.Add("PASS add_tag") }
    else { $script:checks.Add("FAIL add_tag") }

    $after = Call-Tool "get_tags" @{ name = "tag_workbench_test" }
    $testTag = $after.tags | Where-Object { $_.name -eq "tag_workbench_test" } | Select-Object -First 1
    if ($testTag -and $testTag.added -eq $true) { $script:checks.Add("PASS get_tags sees the added tag") }
    else { $script:checks.Add("FAIL get_tags added tag") }

    $all = Call-Tool "revert_tags" @{}
    if ($all -and $all.dirty -eq $false) { $script:checks.Add("PASS revert_tags all (clean)") }
    else { $script:checks.Add("FAIL revert_tags all") }

    $gone = Call-Tool "get_tags" @{ name = "tag_workbench_test" }
    if ($gone.count -eq 0) { $script:checks.Add("PASS added tag removed by revert") }
    else { $script:checks.Add("FAIL added tag still present") }

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
