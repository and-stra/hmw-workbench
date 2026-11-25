# Crossbow optics: tune the HMW reflex/acog tags on a COPY (build\edit_test\m5root).
#   powershell -ExecutionPolicy Bypass -File tools\test\crossbow-optics-tune.ps1
# Writes review\crossbow-optics\tune-copy.txt and ADS screenshots. No real game file is touched.

$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$game = "C:\Games\cod-hwm\Call of Duty Horizon Modern Warfare\Call of Duty Modern Warfare Remastered"
$testDir = Join-Path $repo "build\edit_test"
$testExe = Join-Path $testDir "hmw-workbench.exe"
$root = Join-Path $testDir "m5root"
$copyModel = Join-Path $root "xmodel\vm_crossbow_base.xmb"
$copySurface = Join-Path $root "xsurface\010296ab_3f739e8a_100.xsb"
$review = Join-Path $repo "review\crossbow-optics"
New-Item -ItemType Directory -Force $review | Out-Null
New-Item -ItemType Directory -Force (Split-Path $copyModel) | Out-Null
New-Item -ItemType Directory -Force (Split-Path $copySurface) | Out-Null

Copy-Item (Join-Path $repo "build\bin\Release\hmw-workbench.exe") $testExe -Force
Copy-Item (Join-Path $game "zonetool\mod\xmodel\vm_crossbow_base.xmb") $copyModel -Force
Copy-Item (Join-Path $game "zonetool\mod\xsurface\010296ab_3f739e8a_100.xsb") $copySurface -Force
Remove-Item "$copyModel.bak", (Join-Path $root "xmodel\vm_crossbow_base.tags.json") -ErrorAction SilentlyContinue
$settings = Get-Content (Join-Path $repo "build\bin\Release\workbench.json") -Raw | ConvertFrom-Json
$settings.assetRoots = @("m5root") + @($settings.assetRoots)
$settings | ConvertTo-Json -Depth 10 | Set-Content (Join-Path $testDir "workbench.json") -Encoding UTF8

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
    $script:log.Add("")
    $script:log.Add("> $name " + ($arguments | ConvertTo-Json -Compress -Depth 10))
    $reply = $line | ConvertFrom-Json
    $result = $reply.result
    $text = ($result.content | Where-Object { $_.type -eq "text" } | Select-Object -First 1).text
    $logText = $text
    if ($logText.Length -gt 2500) { $logText = $logText.Substring(0, 2500) + " ...[truncated]" }
    $script:log.Add("< " + $logText)
    if ($result.isError) { return $null }
    try { return $text | ConvertFrom-Json } catch { return $null }
}

try {
    $script:log.Add("crossbow optics tune on copy")
    $script:nextId++
    Send-Raw (@{ jsonrpc = "2.0"; id = $script:nextId; method = "initialize"; params = @{ protocolVersion = "2025-06-18" } } | ConvertTo-Json -Compress)
    $init = (Read-Response) | ConvertFrom-Json
    $script:log.Add("initialize -> " + $init.result.serverInfo.name)
    Send-Raw '{"jsonrpc":"2.0","method":"notifications/initialized"}'

    $weapon = Call-Json "load_weapon" @{ weapon = "iw5_exocrossbow_mp"; hands = "viewhands_tf141"; first_person = $true }
    $renamed = Call-Json "rename_tag" @{ old = "tag_sight_on"; new = "tag_sight_front_on" }
    $reflex = Call-Json "fit_tag" @{ tag = "tag_reflex"; attachment = "reflex" }
    $acog = Call-Json "fit_tag" @{ tag = "tag_acog"; attachment = "acog" }
    $saved = Call-Json "save_model" @{}
    $script:log.Add("")
    $script:log.Add("fit reflex: " + ($reflex | ConvertTo-Json -Compress))
    $script:log.Add("fit acog:   " + ($acog | ConvertTo-Json -Compress))
    $script:log.Add("save:       " + ($saved | ConvertTo-Json -Compress))

    foreach ($optic in @("reflex", "acog")) {
        $null = Call-Json "load_weapon" @{ weapon = "iw5_exocrossbow_mp"; hands = "viewhands_tf141"; first_person = $true; attachments = @($optic) }
        $null = Call-Json "set_anim" @{ name = "ADS_UP"; ads = $true }
        $dump = Call-Json "posedump" @{}
        $script:log.Add("")
        $script:log.Add("=== $optic ADS_UP ===")
        if ($dump) {
            foreach ($line in ($dump.text -split "`n")) {
                if ($line -match "effective ADS_UP|reticle|tag_$optic|screen centre") { $script:log.Add($line) }
            }
        }
        $shot = Call-Json "screenshot" @{ path = ("review/crossbow-optics/ads_end_$optic.png"); view = "firstperson"; crosshair = $true; width = 1920; height = 1016 }
    }
}
finally {
    try { $server.StandardInput.Close() } catch {}
    if (-not $server.WaitForExit(15000)) { $server.Kill() }
    $script:log | Set-Content (Join-Path $review "tune-copy.txt") -Encoding UTF8
    Write-Output ("wrote " + (Join-Path $review "tune-copy.txt"))
}
