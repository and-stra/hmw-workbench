# Crossbow optics: apply the workbench fixes to the real models (with .bak rules).
#   powershell -ExecutionPolicy Bypass -File tools\test\crossbow-optics-apply.ps1

$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$game = "C:\Games\cod-hwm\Call of Duty Horizon Modern Warfare\Call of Duty Modern Warfare Remastered"
$modelDir = Join-Path $game "zonetool\mod\xmodel"
$vm = Join-Path $modelDir "vm_crossbow_base.xmb"
$vmBak = "$vm.bak"
$npc = Join-Path $modelDir "npc_crossbow_base.xmb"
$npcBak = "$npc.bak"
$review = Join-Path $repo "review\crossbow-optics"
New-Item -ItemType Directory -Force $review | Out-Null
$exe = Join-Path $repo "build\bin\Release\hmw-workbench.exe"

$vmHashBefore = (Get-FileHash -Algorithm SHA256 $vm).Hash
$vmBakHash = if (Test-Path $vmBak) { (Get-FileHash -Algorithm SHA256 $vmBak).Hash } else { "" }
$npcHashBefore = (Get-FileHash -Algorithm SHA256 $npc).Hash
Copy-Item $vm (Join-Path $review "vm_crossbow_base_before.xmb") -Force
Copy-Item $npc (Join-Path $review "npc_crossbow_base_before.xmb") -Force
if (Test-Path (Join-Path $modelDir "vm_crossbow_base.tags.json")) {
    Copy-Item (Join-Path $modelDir "vm_crossbow_base.tags.json") (Join-Path $review "vm_crossbow_base_before.tags.json") -Force
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
    $script:log.Add("crossbow optics apply to real models")
    $script:nextId++
    Send-Raw (@{ jsonrpc = "2.0"; id = $script:nextId; method = "initialize"; params = @{ protocolVersion = "2025-06-18" } } | ConvertTo-Json -Compress)
    $init = (Read-Response) | ConvertFrom-Json
    $script:log.Add("initialize -> " + $init.result.serverInfo.name)
    Send-Raw '{"jsonrpc":"2.0","method":"notifications/initialized"}'

    $null = Call-Json "load_weapon" @{ weapon = "iw5_exocrossbow_mp"; hands = "viewhands_tf141"; first_person = $true }
    $renamed = Call-Json "rename_tag" @{ old = "tag_sight_on"; new = "tag_sight_front_on" }
    $reflex = Call-Json "fit_tag" @{ tag = "tag_reflex"; attachment = "reflex" }
    $acog = Call-Json "fit_tag" @{ tag = "tag_acog"; attachment = "acog" }
    $savedVm = Call-Json "save_model" @{}
    $script:log.Add("")
    $script:log.Add("rename vm: " + ($renamed | ConvertTo-Json -Compress))
    $script:log.Add("fit reflex: " + ($reflex | ConvertTo-Json -Compress))
    $script:log.Add("fit acog:   " + ($acog | ConvertTo-Json -Compress))
    $script:log.Add("save vm:    " + ($savedVm | ConvertTo-Json -Compress))

    $null = Call-Json "load_model" @{ model = "npc_crossbow_base" }
    $renamedNpc = Call-Json "rename_tag" @{ old = "tag_sight_on"; new = "tag_sight_front_on" }
    $addReflex = Call-Json "add_tag" @{ tag = "tag_reflex"; parent = "tag_weapon"; copy_from = "tag_red_dot" }
    $moveReflex = Call-Json "move_tag" @{ tag = "tag_reflex"; pos = @(-7.771000, 0.0, 2.791906) }
    $addAcog = Call-Json "add_tag" @{ tag = "tag_acog"; parent = "tag_weapon"; copy_from = "tag_acog_2" }
    $moveAcog = Call-Json "move_tag" @{ tag = "tag_acog"; pos = @(-7.197000, 0.0, 2.330996) }
    $savedNpc = Call-Json "save_model" @{}
    $script:log.Add("")
    $script:log.Add("rename npc: " + ($renamedNpc | ConvertTo-Json -Compress))
    $script:log.Add("add reflex: " + ($addReflex | ConvertTo-Json -Compress))
    $script:log.Add("move reflex:" + ($moveReflex | ConvertTo-Json -Compress))
    $script:log.Add("add acog:   " + ($addAcog | ConvertTo-Json -Compress))
    $script:log.Add("move acog:  " + ($moveAcog | ConvertTo-Json -Compress))
    $script:log.Add("save npc:   " + ($savedNpc | ConvertTo-Json -Compress))

    $vmHashAfter = (Get-FileHash -Algorithm SHA256 $vm).Hash
    $npcHashAfter = (Get-FileHash -Algorithm SHA256 $npc).Hash
    $vmBakAfter = if (Test-Path $vmBak) { (Get-FileHash -Algorithm SHA256 $vmBak).Hash } else { "" }
    $script:log.Add("")
    $script:log.Add("vm before $vmHashBefore")
    $script:log.Add("vm after  $vmHashAfter")
    $script:log.Add("vm .bak  $vmBakAfter (unchanged: $($vmBakAfter -eq $vmBakHash))")
    $script:log.Add("npc before $npcHashBefore")
    $script:log.Add("npc after  $npcHashAfter")
    $script:log.Add("npc .bak exists: $(Test-Path $npcBak)")
}
finally {
    try { $server.StandardInput.Close() } catch {}
    if (-not $server.WaitForExit(15000)) { $server.Kill() }
    $script:log | Set-Content (Join-Path $review "apply-real.txt") -Encoding UTF8
    Write-Output ("wrote " + (Join-Path $review "apply-real.txt"))
}
