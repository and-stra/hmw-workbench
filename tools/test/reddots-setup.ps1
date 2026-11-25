# Red-dot comparison setup for the crossbow: temp root, temp weapon json with the test
# attachments/overrides, extra tags on a COPY, fit via fit_tag. No real model is saved.
$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$game = "C:\Games\cod-hwm\Call of Duty Horizon Modern Warfare\Call of Duty Modern Warfare Remastered"
$temp = Join-Path $repo "build\reddot_test"
$root = Join-Path $temp "root"
$exe = Join-Path $temp "hmw-workbench.exe"
$review = Join-Path $repo "review\reddots"
New-Item -ItemType Directory -Force $review | Out-Null
New-Item -ItemType Directory -Force (Join-Path $root "xmodel") | Out-Null
New-Item -ItemType Directory -Force (Join-Path $root "xsurface") | Out-Null
New-Item -ItemType Directory -Force (Join-Path $root "weapons") | Out-Null
Copy-Item (Join-Path $repo "build\bin\Release\hmw-workbench.exe") $exe -Force
Copy-Item (Join-Path $game "zonetool\mod\xmodel\vm_crossbow_base.xmb") (Join-Path $root "xmodel\vm_crossbow_base.xmb") -Force
Copy-Item (Join-Path $game "zonetool\mod\xsurface\010296ab_3f739e8a_100.xsb") (Join-Path $root "xsurface\010296ab_3f739e8a_100.xsb") -Force

$settings = Get-Content (Join-Path $repo "build\bin\Release\workbench.json") -Raw | ConvertFrom-Json
$settings.assetRoots = @("root") + @($settings.assetRoots)
$settings | ConvertTo-Json -Depth 10 | Set-Content (Join-Path $temp "workbench.json") -Encoding UTF8

# Temp weapon json: add the test attachments and red-dot overrides (indices 1-based).
$weaponPath = Join-Path $game "zonetool\mod\weapons\iw5_exocrossbow_mp.json"
$weapon = Get-Content $weaponPath -Raw | ConvertFrom-Json
$weapon.attachments = @("acog", "reflex", "reflexmwr", "mars", "holo")
$extra = @(
    @{ attachment1 = 3; animTreeType = 129; overrideAnim = "vm_crossbow_reddot_ads_up" },
    @{ attachment1 = 3; animTreeType = 130; overrideAnim = "vm_crossbow_reddot_ads_down" },
    @{ attachment1 = 4; animTreeType = 129; overrideAnim = "vm_crossbow_reddot_ads_up" },
    @{ attachment1 = 4; animTreeType = 130; overrideAnim = "vm_crossbow_reddot_ads_down" },
    @{ attachment1 = 5; animTreeType = 129; overrideAnim = "vm_crossbow_reddot_ads_up" },
    @{ attachment1 = 5; animTreeType = 130; overrideAnim = "vm_crossbow_reddot_ads_down" }
)
foreach ($e in $extra) {
    $weapon.animOverrides += [pscustomobject]@{
        altmodeAnim = ""; overrideAnim = $e.overrideAnim; attachment1 = $e.attachment1
        attachment2 = 0; altTime = 0; animTime = 0; animTreeType = $e.animTreeType; animHand = 0
    }
}
$weapon | ConvertTo-Json -Depth 20 | Set-Content (Join-Path $root "weapons\iw5_exocrossbow_mp.json") -Encoding UTF8

$psi = [System.Diagnostics.ProcessStartInfo]::new()
$psi.FileName = $exe
$psi.Arguments = "-mcp"
$psi.WorkingDirectory = $temp
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
    if ($logText.Length -gt 1800) { $logText = $logText.Substring(0, 1800) + " ...[truncated]" }
    $script:log.Add("< " + $logText)
    if ($result.isError) { return $null }
    try { return $text | ConvertFrom-Json } catch { return $null }
}
try {
    $script:nextId++
    Send-Raw (@{ jsonrpc = "2.0"; id = $script:nextId; method = "initialize"; params = @{ protocolVersion = "2025-06-18" } } | ConvertTo-Json -Compress)
    $init = (Read-Response) | ConvertFrom-Json
    $script:log.Add("initialize -> " + $init.result.serverInfo.name)
    Send-Raw '{"jsonrpc":"2.0","method":"notifications/initialized"}'
    $null = Call-Json "load_weapon" @{ weapon = "iw5_exocrossbow_mp"; hands = "viewhands_tf141"; first_person = $true }
    $null = Call-Json "add_tag" @{ tag = "tag_tavor_scope"; parent = "j_gun"; copy_from = "tag_reflex" }
    $null = Call-Json "add_tag" @{ tag = "tag_holo"; parent = "j_gun"; copy_from = "tag_reflex" }
    $fitMars = Call-Json "fit_tag" @{ tag = "tag_tavor_scope"; attachment = "mars" }
    $fitHolo = Call-Json "fit_tag" @{ tag = "tag_holo"; attachment = "holo" }
    $saved = Call-Json "save_model" @{}
    $script:log.Add("")
    $script:log.Add("fit mars: " + ($fitMars | ConvertTo-Json -Compress))
    $script:log.Add("fit holo: " + ($fitHolo | ConvertTo-Json -Compress))
    $script:log.Add("save:     " + ($saved | ConvertTo-Json -Compress))
}
finally {
    try { $server.StandardInput.Close() } catch {}
    if (-not $server.WaitForExit(15000)) { $server.Kill() }
    $script:log | Set-Content (Join-Path $review "setup.txt") -Encoding UTF8
    Write-Output ("wrote " + (Join-Path $review "setup.txt"))
}
