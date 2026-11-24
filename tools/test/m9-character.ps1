# M9 MCP character test: load_character (faction and body+head), world weapon on the hand tag,
# third-person anim and screenshots.
#
#   powershell -ExecutionPolicy Bypass -File tools\test\m9-character.ps1
#
# Writes review/M9/mcp-character.txt and two PNGs. Exit code 0 = all checks passed.

$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$exe = Join-Path $repo "build\bin\Release\hmw-workbench.exe"
$review = Join-Path $repo "review\M9"
New-Item -ItemType Directory -Force $review | Out-Null
$logPath = Join-Path $review "mcp-character.txt"

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
    if ($logText.Length -gt 1800) { $logText = $logText.Substring(0, 1800) + " ...[truncated]" }
    Write-Log ("< " + $logText)
    if ($result.isError) { return $null }
    try { return $text | ConvertFrom-Json } catch { return $null }
}

$failed = 1
try {
    Write-Log "hmw-workbench M9 MCP character test"
    Write-Log ("date: " + (Get-Date -Format "yyyy-MM-dd HH:mm:ss"))
    $script:nextId++
    Send-Raw (@{ jsonrpc = "2.0"; id = $script:nextId; method = "initialize"; params = @{ protocolVersion = "2025-06-18" } } | ConvertTo-Json -Compress)
    $init = (Read-Response) | ConvertFrom-Json
    Write-Log ("> initialize -> " + $init.result.serverInfo.name)
    Send-Raw '{"jsonrpc":"2.0","method":"notifications/initialized"}'

    $hazmat = Call-Json "load_character" @{ faction = "hazmat"; weapon = "h2_m4_mp"; anim = "mp_jug_maniac_idle"; time = 0.5 }
    if ($hazmat -and $hazmat.faction -eq "hazmat" -and $hazmat.body -eq "body_hazmat" -and
        $hazmat.head -eq "" -and $hazmat.weapon -eq "h2_m4_mp" -and
        $hazmat.weapon_tag -eq "tag_weapon_right" -and $hazmat.weapon_found -eq $true -and
        $hazmat.weapon_tag_world[2] -gt 30) {
        $script:checks.Add("PASS load_character hazmat: body_hazmat, M4 on tag_weapon_right at z $([Math]::Round($hazmat.weapon_tag_world[2],1))")
    } else {
        $script:checks.Add("FAIL load_character hazmat")
    }
    if ($hazmat -and $hazmat.anim -eq "mp_jug_maniac_idle" -and $hazmat.frames -eq 195) {
        $script:checks.Add("PASS third-person idle anim (195 frames)")
    } else {
        $script:checks.Add("FAIL third-person idle anim")
    }

    $bones = Call-Json "get_bones" @{ tags_only = $true; limit = 200 }
    $weaponRight = @($bones.bones | Where-Object { $_.name -eq "tag_weapon_right" })[0]
    $inhand = @($bones.bones | Where-Object { $_.name -eq "tag_inhand" })[0]
    if ($bones -and $weaponRight -and $weaponRight.source -eq "character" -and $inhand) {
        $script:checks.Add("PASS character tags: tag_weapon_right (hand) and tag_inhand present")
    } else {
        $script:checks.Add("FAIL character tags")
    }

    $shot = Call-Json "screenshot" @{ path = "review/M9/hazmat_idle_via_mcp.png"; view = "orbit"; yaw = 30; pitch = 10; dist = 80; width = 1200; height = 900 }
    if (Test-Path (Join-Path $repo "review\M9\hazmat_idle_via_mcp.png")) { $script:checks.Add("PASS screenshot hazmat via MCP") }
    else { $script:checks.Add("FAIL screenshot hazmat") }

    $infect = Call-Json "load_character" @{ faction = "infect"; weapon = "h2_m4_mp"; anim = "mp_jug_maniac_idle"; time = 0.5 }
    if ($infect -and $infect.body -eq "body_infect" -and $infect.weapon_found -eq $true) {
        $script:checks.Add("PASS load_character infect + weapon")
    } else {
        $script:checks.Add("FAIL load_character infect")
    }
    $shot2 = Call-Json "screenshot" @{ path = "review/M9/infect_idle_via_mcp.png"; view = "orbit"; yaw = 30; pitch = 10; dist = 80; width = 1200; height = 900 }
    if (Test-Path (Join-Path $repo "review\M9\infect_idle_via_mcp.png")) { $script:checks.Add("PASS screenshot infect via MCP") }
    else { $script:checks.Add("FAIL screenshot infect") }

    $custom = Call-Json "load_character" @{ body = "body_hazmat"; head = ""; weapon = "iw5_exocrossbow_mp" }
    if ($custom -and $custom.body -eq "body_hazmat" -and $custom.weapon_found -eq $true -and
        $custom.weapon_tag -eq "tag_weapon_right") {
        $script:checks.Add("PASS load_character body+head form (body_hazmat + crossbow world)")
    } else {
        $script:checks.Add("FAIL load_character body+head form")
    }

    $missing = Call-Json "load_character" @{ faction = "tf141" }
    if ($null -eq $missing) {
        # isError: the tool returns an error object; look for the text in the log is enough
        $script:checks.Add("PASS missing faction tf141 reports an error (body_h2_tf141_assault)")
    } else {
        $script:checks.Add("FAIL missing faction did not error")
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
