# M10 MCP test: compare_screenshot (side + ghost), rename_map and compare_bones.
#
#   powershell -ExecutionPolicy Bypass -File tools\test\m10-comfort.ps1
#
# Writes review/M10/mcp-comfort.txt and two compare PNGs. Exit code 0 = all checks passed.

$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$exe = Join-Path $repo "build\bin\Release\hmw-workbench.exe"
$review = Join-Path $repo "review\M10"
New-Item -ItemType Directory -Force $review | Out-Null
$map = Join-Path $review "rename_map_test.json"
Set-Content $map '{"j_mainroot":"j_gun"}' -Encoding UTF8
$logPath = Join-Path $review "mcp-comfort.txt"

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
    $logText = $text
    if ($logText.Length -gt 1400) { $logText = $logText.Substring(0, 1400) + " ...[truncated]" }
    Write-Log ("< " + $logText)
    if ($result.isError) { return $null }
    try { return $text | ConvertFrom-Json } catch { return $null }
}

$failed = 1
try {
    Write-Log "hmw-workbench M10 MCP comfort test"
    Write-Log ("date: " + (Get-Date -Format "yyyy-MM-dd HH:mm:ss"))
    $script:nextId++
    Send-Raw (@{ jsonrpc = "2.0"; id = $script:nextId; method = "initialize"; params = @{ protocolVersion = "2025-06-18" } } | ConvertTo-Json -Compress)
    $init = (Read-Response) | ConvertFrom-Json
    Write-Log ("> initialize -> " + $init.result.serverInfo.name)
    Send-Raw '{"jsonrpc":"2.0","method":"notifications/initialized"}'

    $sidePath = Join-Path $repo "review\M10\compare_side_via_mcp.png"
    $side = Call-Json "compare_screenshot" @{ a = "iw5_exocrossbow_mp"; b = "h2_m4_mp"; mode = "side"; view = "orbit"; yaw = 35; pitch = 18; dist = 40; width = 800; height = 600; path = "review/M10/compare_side_via_mcp.png" }
    $sideOk = $false
    if (Test-Path $sidePath) {
        $bytes = [System.IO.File]::ReadAllBytes($sidePath)
        $w = [int]$bytes[16] * 16777216 + [int]$bytes[17] * 65536 + [int]$bytes[18] * 256 + [int]$bytes[19]
        $h = [int]$bytes[20] * 16777216 + [int]$bytes[21] * 65536 + [int]$bytes[22] * 256 + [int]$bytes[23]
        $sideOk = ($w -eq 1604 -and $h -eq 600)
    }
    if ($sideOk) { $script:checks.Add("PASS compare_screenshot side (1604x600)") }
    else { $script:checks.Add("FAIL compare_screenshot side") }
    $ghostPath = Join-Path $repo "review\M10\compare_ghost_via_mcp.png"
    $ghost = Call-Json "compare_screenshot" @{ a = "iw5_exocrossbow_mp"; b = "h2_m4_mp"; mode = "ghost"; alpha = 0.45; view = "orbit"; yaw = 35; pitch = 18; dist = 40; width = 1200; height = 900; path = "review/M10/compare_ghost_via_mcp.png" }
    $ghostOk = $false
    if (Test-Path $ghostPath) {
        $bytes = [System.IO.File]::ReadAllBytes($ghostPath)
        $w = [int]$bytes[16] * 16777216 + [int]$bytes[17] * 65536 + [int]$bytes[18] * 256 + [int]$bytes[19]
        $h = [int]$bytes[20] * 16777216 + [int]$bytes[21] * 65536 + [int]$bytes[22] * 256 + [int]$bytes[23]
        $ghostOk = ($w -eq 1200 -and $h -eq 900)
    }
    if ($ghostOk) { $script:checks.Add("PASS compare_screenshot ghost (1200x900)") }
    else { $script:checks.Add("FAIL compare_screenshot ghost") }

    $nocmp = Call-Json "compare_bones" @{ anim = "mp_jug_maniac_idle"; model = "vm_crossbow_base" }
    if ($nocmp -and $nocmp.renamed -eq 0 -and $nocmp.missing -eq 92) {
        $script:checks.Add("PASS compare_bones without map (92 missing, 0 renamed)")
    } else {
        $script:checks.Add("FAIL compare_bones without map")
    }
    $loaded = Call-Json "rename_map" @{ path = $map }
    if ($loaded -and $loaded.entries -eq 1) { $script:checks.Add("PASS rename_map loaded 1 entry") }
    else { $script:checks.Add("FAIL rename_map") }
    $withmap = Call-Json "compare_bones" @{ anim = "mp_jug_maniac_idle"; model = "vm_crossbow_base"; rename_map = $map }
    if ($withmap -and $withmap.renamed -eq 1 -and $withmap.missing -eq 91 -and
        ($withmap.renamed_channels -contains "j_mainroot -> j_gun")) {
        $script:checks.Add("PASS compare_bones with map (1 renamed: j_mainroot -> j_gun)")
    } else {
        $script:checks.Add("FAIL compare_bones with map")
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
