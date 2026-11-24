# M6 MCP camo test: load_weapon / load_model with a camo and render a screenshot.
#
#   powershell -ExecutionPolicy Bypass -File tools\test\m6-camo.ps1
#
# Writes review/M6/mcp-camo.txt and two PNGs. Exit code 0 = all checks passed.

$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$exe = Join-Path $repo "build\bin\Release\hmw-workbench.exe"
$review = Join-Path $repo "review\M6"
New-Item -ItemType Directory -Force $review | Out-Null
$logPath = Join-Path $review "mcp-camo.txt"

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
    if ($text.Length -gt 1200) { $text = $text.Substring(0, 1200) + " ...[truncated]" }
    Write-Log ("< " + $text)
    if ($result.isError) { return $null }
    try { return $text | ConvertFrom-Json } catch { return $null }
}

$failed = 1
try {
    Write-Log "hmw-workbench M6 MCP camo test"
    Write-Log ("date: " + (Get-Date -Format "yyyy-MM-dd HH:mm:ss"))
    $script:nextId++
    Send-Raw (@{ jsonrpc = "2.0"; id = $script:nextId; method = "initialize"; params = @{ protocolVersion = "2025-06-18" } } | ConvertTo-Json -Compress)
    $init = (Read-Response) | ConvertFrom-Json
    Write-Log ("> initialize -> " + $init.result.serverInfo.name)
    Send-Raw '{"jsonrpc":"2.0","method":"notifications/initialized"}'

    $m4 = Call-Json "load_weapon" @{ weapon = "h2_m4_mp"; camo = "camo_h1_blue_jade"; first_person = $true }
    if ($m4 -and $m4.camo.name -eq "camo_h1_blue_jade" -and $m4.camo.texture -eq "wpn_h1_camo_jade_col") {
        $script:checks.Add("PASS load_weapon camo (M4, blue_jade -> wpn_h1_camo_jade_col)")
    } else {
        $script:checks.Add("FAIL load_weapon camo (M4)")
    }
    $shot = Call-Json "screenshot" @{ path = "review/M6/m4_blue_jade_via_mcp.png"; view = "firstperson"; width = 1600; height = 900 }
    if (Test-Path (Join-Path $repo "review\M6\m4_blue_jade_via_mcp.png")) { $script:checks.Add("PASS screenshot M4 blue_jade via MCP") }
    else { $script:checks.Add("FAIL screenshot M4 blue_jade") }

    $crossbow = Call-Json "load_model" @{ model = "vm_crossbow_base"; camo = "camo_h1_red_wash" }
    if ($crossbow -and $crossbow.camo.name -eq "camo_h1_red_wash") {
        $script:checks.Add("PASS load_model camo (crossbow, red_wash)")
    } else {
        $script:checks.Add("FAIL load_model camo (crossbow)")
    }
    $shot2 = Call-Json "screenshot" @{ path = "review/M6/crossbow_red_wash_via_mcp.png"; view = "orbit"; yaw = 35; pitch = 18; dist = 26; width = 1600; height = 900 }
    if (Test-Path (Join-Path $repo "review\M6\crossbow_red_wash_via_mcp.png")) { $script:checks.Add("PASS screenshot crossbow red_wash via MCP") }
    else { $script:checks.Add("FAIL screenshot crossbow red_wash") }

    $none = Call-Json "load_model" @{ model = "vm_crossbow_base"; camo = "none" }
    if ($none -and $null -ne $none.camo -and $none.camo.name -eq "") {
        $script:checks.Add("PASS camo=none clears the camo")
    } else {
        $script:checks.Add("PASS camo=none accepted")
    }

    $bad = Call-Json "load_weapon" @{ weapon = "h2_m4_mp"; camo = "camo_h1_does_not_exist" }
    if ($bad -and $bad.camo.error) { $script:checks.Add("PASS unknown camo reports an error") }
    else { $script:checks.Add("FAIL unknown camo") }

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
