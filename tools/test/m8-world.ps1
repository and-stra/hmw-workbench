# M8 MCP world-model test: load_weapon world=true, world attachments and the world fit report.
#
#   powershell -ExecutionPolicy Bypass -File tools\test\m8-world.ps1
#
# Writes review/M8/mcp-world.txt and two PNGs. Exit code 0 = all checks passed.

$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$exe = Join-Path $repo "build\bin\Release\hmw-workbench.exe"
$review = Join-Path $repo "review\M8"
New-Item -ItemType Directory -Force $review | Out-Null
$logPath = Join-Path $review "mcp-world.txt"

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
    if ($text.Length -gt 1500) { $text = $text.Substring(0, 1500) + " ...[truncated]" }
    Write-Log ("< " + $text)
    if ($result.isError) { return $null }
    try { return $text | ConvertFrom-Json } catch { return $null }
}

$failed = 1
try {
    Write-Log "hmw-workbench M8 MCP world-model test"
    Write-Log ("date: " + (Get-Date -Format "yyyy-MM-dd HH:mm:ss"))
    $script:nextId++
    Send-Raw (@{ jsonrpc = "2.0"; id = $script:nextId; method = "initialize"; params = @{ protocolVersion = "2025-06-18" } } | ConvertTo-Json -Compress)
    $init = (Read-Response) | ConvertFrom-Json
    Write-Log ("> initialize -> " + $init.result.serverInfo.name)
    Send-Raw '{"jsonrpc":"2.0","method":"notifications/initialized"}'

    $crossbow = Call-Json "load_weapon" @{ weapon = "iw5_exocrossbow_mp"; world = $true; first_person = $true }
    if ($crossbow -and $crossbow.world_model -eq $true -and $crossbow.loaded_model -eq "npc_crossbow_base" -and
        $crossbow.hands -eq "" -and $crossbow.first_person -eq $false) {
        $script:checks.Add("PASS load_weapon world=true: npc_crossbow_base, no hands, orbit")
    } else {
        $script:checks.Add("FAIL load_weapon world=true (crossbow)")
    }
    $shot = Call-Json "screenshot" @{ path = "review/M8/crossbow_world_via_mcp.png"; view = "orbit"; yaw = 25; pitch = 20; dist = 52; width = 1600; height = 900 }
    if (Test-Path (Join-Path $repo "review\M8\crossbow_world_via_mcp.png")) { $script:checks.Add("PASS screenshot crossbow world via MCP") }
    else { $script:checks.Add("FAIL screenshot crossbow world") }

    $fitCrossbow = Call-Json "attachment_fit_report" @{ weapon = "iw5_exocrossbow_mp"; world = $true; attachments = @("reflexmwr", "acogmwr") }
    $reflex = $fitCrossbow.attachments | Where-Object { $_.name -eq "reflexmwr" } | Select-Object -First 1
    if ($fitCrossbow -and $fitCrossbow.world -eq $true -and $fitCrossbow.gun_model -eq "npc_crossbow_base" -and
        $reflex -and $reflex.found -eq $false -and ($reflex.nearest -contains "tag_red_dot")) {
        $script:checks.Add("PASS world fit report: npc_crossbow_base has no tag_reflex (nearest tag_red_dot)")
    } else {
        $script:checks.Add("FAIL world fit report (crossbow)")
    }

    $m4 = Call-Json "load_weapon" @{ weapon = "h2_m4_mp"; world = $true; attachments = @("reflexmwr") }
    $m4Reflex = $m4.attachments | Where-Object { $_.name -eq "reflexmwr" } | Select-Object -First 1
    if ($m4 -and $m4.world_model -eq $true -and $m4.loaded_model -eq "wpn_h2_m4a1_npc" -and
        $m4Reflex -and $m4Reflex.found -eq $true) {
        $script:checks.Add("PASS load_weapon world=true: wpn_h2_m4a1_npc + reflexmwr found on tag_reflex")
    } else {
        $script:checks.Add("FAIL load_weapon world=true (M4 + reflex)")
    }
    $shot2 = Call-Json "screenshot" @{ path = "review/M8/m4_world_reflex_via_mcp.png"; view = "orbit"; yaw = 35; pitch = 18; dist = 40; width = 1600; height = 900 }
    if (Test-Path (Join-Path $repo "review\M8\m4_world_reflex_via_mcp.png")) { $script:checks.Add("PASS screenshot M4 world + reflex via MCP") }
    else { $script:checks.Add("FAIL screenshot M4 world + reflex") }

    $fitM4 = Call-Json "attachment_fit_report" @{ weapon = "h2_m4_mp"; world = $true; attachments = @("reflexmwr", "acogmwr") }
    $m4FitReflex = $fitM4.attachments | Where-Object { $_.name -eq "reflexmwr" } | Select-Object -First 1
    if ($fitM4 -and $fitM4.world -eq $true -and $fitM4.gun_model -eq "wpn_h2_m4a1_npc" -and
        $m4FitReflex -and $m4FitReflex.found -eq $true) {
        $script:checks.Add("PASS world fit report: wpn_h2_m4a1_npc has tag_reflex")
    } else {
        $script:checks.Add("FAIL world fit report (M4)")
    }

    $vm = Call-Json "load_weapon" @{ weapon = "h2_m4_mp"; world = $false }
    if ($vm -and $vm.world_model -eq $false -and $vm.loaded_model -eq "wpn_h2_m4a1_vm") {
        $script:checks.Add("PASS load_weapon world=false returns the view model")
    } else {
        $script:checks.Add("FAIL load_weapon world=false")
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
    Write-Output ("wrote " + $logPath)
    if ($failed -gt 0) { exit 1 }
}
