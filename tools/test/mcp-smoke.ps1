# hmw-workbench MCP smoke test.
#
# Starts `hmw-workbench.exe -mcp`, calls every tool once, writes the calls/results to
# review/M4b/mcp-smoke.txt and the "done when" screenshot of the crossbow + reflexmwr.
#
#   powershell -ExecutionPolicy Bypass -File tools\test\mcp-smoke.ps1
#
# Exit code 0 = all calls answered and the checks passed.

$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$exe = Join-Path $repo "build\bin\Release\hmw-workbench.exe"
$review = Join-Path $repo "review\M4b"
New-Item -ItemType Directory -Force $review | Out-Null
$logPath = Join-Path $review "mcp-smoke.txt"

if (-not (Test-Path $exe)) {
    throw "missing $exe (build first: tools\build\build.bat)"
}

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

function Write-Log([string]$text) {
    $script:log.Add($text)
}

function Send-Raw([string]$json) {
    $server.StandardInput.WriteLine($json)
    $server.StandardInput.Flush()
}

function Read-Response([int]$timeoutMs = 90000) {
    $task = $server.StandardOutput.ReadLineAsync()
    if (-not $task.Wait($timeoutMs)) {
        throw "timeout waiting for the MCP server"
    }
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
    if ([string]::IsNullOrWhiteSpace($line)) {
        Write-Log "< empty response"
        $script:checks.Add("FAIL ${name}: empty response")
        return $null
    }
    try {
        $reply = $line | ConvertFrom-Json
    } catch {
        Write-Log ("< invalid JSON: " + $line.Substring(0, [Math]::Min(200, $line.Length)))
        $script:checks.Add("FAIL ${name}: invalid JSON")
        return $null
    }
    if ($reply.PSObject.Properties.Name -contains "error") {
        Write-Log ("< RPC error " + ($reply.error | ConvertTo-Json -Compress))
        $script:checks.Add("FAIL ${name}: RPC error")
        return $null
    }
    $result = $reply.result
    if ($result.PSObject.Properties.Name -contains "isError" -and $result.isError) {
        $text = ($result.content | Where-Object { $_.type -eq "text" } | Select-Object -First 1).text
        Write-Log ("< isError: " + $text)
        $script:checks.Add("FAIL ${name}: isError")
        return $null
    }
    if ($result.PSObject.Properties.Name -contains "content") {
        $textPart = $result.content | Where-Object { $_.type -eq "text" } | Select-Object -First 1
        $imagePart = $result.content | Where-Object { $_.type -eq "image" } | Select-Object -First 1
        if ($imagePart) {
            Write-Log ("< image " + $imagePart.mimeType + ", " + $imagePart.data.Length + " base64 chars; " + $textPart.text)
        } else {
            $text = $textPart.text
            if ($text.Length -gt 800) { $text = $text.Substring(0, 800) + " ...[truncated]" }
            Write-Log ("< " + $text)
        }
        $script:checks.Add("PASS $name")
        return $result
    }
    Write-Log ("< " + (($result | ConvertTo-Json -Compress -Depth 20)))
    $script:checks.Add("PASS $name")
    return $result
}

function Get-Text($result) {
    if ($null -eq $result) { return "" }
    $textPart = $result.content | Where-Object { $_.type -eq "text" } | Select-Object -First 1
    if ($textPart) { return $textPart.text }
    return ""
}

function Get-Json($result) {
    $text = Get-Text $result
    if ([string]::IsNullOrWhiteSpace($text)) { return $null }
    try { return $text | ConvertFrom-Json } catch { return $null }
}

try {
    Write-Log "hmw-workbench MCP smoke test"
    Write-Log ("exe: " + $exe)
    Write-Log ("date: " + (Get-Date -Format "yyyy-MM-dd HH:mm:ss"))

    $script:nextId++
    Send-Raw (@{ jsonrpc = "2.0"; id = $script:nextId; method = "initialize"; params = @{ protocolVersion = "2025-06-18" } } | ConvertTo-Json -Compress)
    $init = Read-Response
    Write-Log ""
    Write-Log "> initialize"
    Write-Log ("< " + $init)
    $initReply = $init | ConvertFrom-Json
    $script:checks.Add("PASS initialize")

    Send-Raw '{"jsonrpc":"2.0","method":"notifications/initialized"}'
    Write-Log ""
    Write-Log "> notifications/initialized"

    $script:nextId++
    Send-Raw (@{ jsonrpc = "2.0"; id = $script:nextId; method = "tools/list" } | ConvertTo-Json -Compress)
    $toolList = (Read-Response) | ConvertFrom-Json
    $toolNames = @($toolList.result.tools | ForEach-Object { $_.name })
    Write-Log ""
    Write-Log ("> tools/list")
    Write-Log ("< tools: " + ($toolNames -join ", "))
    if ($toolNames.Count -ge 12) {
        $script:checks.Add("PASS tools/list ($($toolNames.Count) tools)")
    } else {
        $script:checks.Add("FAIL tools/list: only $($toolNames.Count) tools")
    }

    $list = Get-Json (Call-Tool "list_assets" @{ kind = "weapon"; name = "crossbow" })
    if ($list -and $list.count -ge 1) { $script:checks.Add("PASS list_assets: $($list.count) hits") }
    else { $script:checks.Add("FAIL list_assets: no crossbow weapons") }

    $crossbow = Call-Tool "load_weapon" @{ weapon = "iw5_exocrossbow_mp"; hands = "viewhands_tf141"; attachments = @("reflexmwr") }
    $crossbowJson = Get-Json $crossbow
    if ($null -eq $crossbowJson -and $null -ne $crossbow) { $crossbowJson = $crossbow }
    if ($crossbowJson -and $crossbowJson.attachments -and $crossbowJson.attachments[0].found -eq $false) {
        $script:checks.Add("PASS load_weapon: reflexmwr missing its tag_reflex")
    } else {
        $script:checks.Add("FAIL load_weapon: attachment result")
    }

    $fit = Get-Json (Call-Tool "attachment_fit_report" @{ weapon = "iw5_exocrossbow_mp"; hands = "viewhands_tf141"; attachments = @("reflexmwr", "acogmwr", "thermalmwr") })
    $reflex = $fit.attachments | Where-Object { $_.name -eq "reflexmwr" } | Select-Object -First 1
    if ($fit -and $reflex -and $reflex.found -eq $false -and ($reflex.nearest -contains "tag_red_dot")) {
        $script:checks.Add("PASS attachment_fit_report: reflexmwr -> tag_reflex missing, nearest tag_red_dot")
    } else {
        $script:checks.Add("FAIL attachment_fit_report: reflexmwr result")
    }

    $bones = Get-Json (Call-Tool "get_bones" @{ name = "tag_weapon" })
    if ($bones -and $bones.count -ge 1) { $script:checks.Add("PASS get_bones: $($bones.count) matches") }
    else { $script:checks.Add("FAIL get_bones") }

    $tags = Get-Json (Call-Tool "get_tags" @{ name = "tag_" })
    if ($tags -and $tags.count -ge 10) { $script:checks.Add("PASS get_tags: $($tags.count) tags") }
    else { $script:checks.Add("FAIL get_tags") }

    $dump = Get-Text (Call-Tool "posedump" @{ width = 1920; height = 1016 })
    if ($dump -match "anim coverage:" -and $dump -match "gun posed bounds") { $script:checks.Add("PASS posedump") }
    else { $script:checks.Add("FAIL posedump: unexpected text") }

    $idle = Get-Json (Call-Tool "set_anim" @{ name = "IDLE"; time = 0; ads = $false })
    if ($idle -and $idle.anim) { $script:checks.Add("PASS set_anim: $($idle.anim)") }
    else { $script:checks.Add("FAIL set_anim") }

    $hip = Call-Tool "screenshot" @{ path = "review/M4b/crossbow_hip_via_mcp.png"; view = "firstperson"; width = 1920; height = 1016 }
    if (Test-Path (Join-Path $repo "review\M4b\crossbow_hip_via_mcp.png")) { $script:checks.Add("PASS screenshot: crossbow hip") }
    else { $script:checks.Add("FAIL screenshot: crossbow hip missing") }

    $reload = Get-Json (Call-Tool "set_anim" @{ name = "RELOAD"; time = 0.5 })
    if ($reload -and $reload.anim) { $script:checks.Add("PASS set_anim: reload") }
    else { $script:checks.Add("FAIL set_anim: reload") }
    $sheet = Call-Tool "screenshot" @{ path = "review/M4b/crossbow_reload_sheet_via_mcp.png"; sheet = 8; width = 480; height = 270 }
    if (Test-Path (Join-Path $repo "review\M4b\crossbow_reload_sheet_via_mcp.png")) { $script:checks.Add("PASS screenshot: sheet") }
    else { $script:checks.Add("FAIL screenshot: sheet missing") }

    $model = Get-Json (Call-Tool "load_model" @{ model = "vm_crossbow_base" })
    if ($model -and $model.bones -ge 30) { $script:checks.Add("PASS load_model") }
    else { $script:checks.Add("FAIL load_model") }

    $infoModel = Get-Text (Call-Tool "info" @{ asset = "vm_crossbow_base"; kind = "model" })
    $infoAnim = Get-Text (Call-Tool "info" @{ asset = "vm_crossbow_idle"; kind = "anim" })
    $infoWeapon = Get-Text (Call-Tool "info" @{ asset = "iw5_exocrossbow_mp"; kind = "weapon" })
    if ($infoModel -match "model: vm_crossbow_base") { $script:checks.Add("PASS info model") } else { $script:checks.Add("FAIL info model") }
    if ($infoAnim -match "anim: vm_crossbow_idle") { $script:checks.Add("PASS info anim") } else { $script:checks.Add("FAIL info anim") }
    if ($infoWeapon -match "weapon: iw5_exocrossbow_mp") { $script:checks.Add("PASS info weapon") } else { $script:checks.Add("FAIL info weapon") }

    $compare = Get-Json (Call-Tool "compare_tags" @{ a = "iw5_exocrossbow_mp"; b = "h2_m4_mp" })
    if ($compare -and $compare.shared.Count -ge 5) { $script:checks.Add("PASS compare_tags: $($compare.shared.Count) shared") }
    else { $script:checks.Add("FAIL compare_tags") }

    $validateStarted = Get-Date
    $validate = Get-Json (Call-Tool "validate" @{})
    $validateSeconds = ((Get-Date) - $validateStarted).TotalSeconds
    $xmodelLine = @($validate.summary | Where-Object { $_ -match "xmodels:" }) -join " "
    if ($validate -and $xmodelLine -match "failed 0") {
        $script:checks.Add("PASS validate: xmodels 0 parse failures, $($validate.failures) known missing asset warnings ($([Math]::Round($validateSeconds,1)) s)")
    } else {
        $script:checks.Add("FAIL validate: $xmodelLine")
    }

    $crossbow2 = Call-Tool "load_weapon" @{ weapon = "iw5_exocrossbow_mp"; hands = "viewhands_tf141"; attachments = @("reflexmwr") }
    $null = Call-Tool "set_anim" @{ name = "IDLE"; time = 0; ads = $false }
    $missing = Call-Tool "screenshot" @{
        path = "review/M4b/crossbow_reflexmwr_via_mcp.png"; view = "firstperson"; pitch = -18
        width = 1920; height = 1016
        caption = "crossbow + reflexmwr via MCP: weapon has no tag_reflex (nearest tag_red_dot)"
    }
    if (Test-Path (Join-Path $repo "review\M4b\crossbow_reflexmwr_via_mcp.png")) {
        $script:checks.Add("PASS screenshot: crossbow_reflexmwr_via_mcp.png")
    } else {
        $script:checks.Add("FAIL screenshot: crossbow_reflexmwr_via_mcp.png missing")
    }

    $m4 = Call-Tool "load_weapon" @{ weapon = "h2_m4_mp"; attachments = @("reflexmwrm4") }
    $ads = Get-Json (Call-Tool "set_anim" @{ name = "IDLE"; time = 0; ads = $true })
    $m4shot = Call-Tool "screenshot" @{ path = "review/M4b/m4_reflex_ads_via_mcp.png"; view = "firstperson"; crosshair = $true; width = 1920; height = 1016 }
    if (Test-Path (Join-Path $repo "review\M4b\m4_reflex_ads_via_mcp.png")) { $script:checks.Add("PASS screenshot: M4 reflex ADS") }
    else { $script:checks.Add("FAIL screenshot: M4 reflex ADS missing") }

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

