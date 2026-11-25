# Crossbow optics: rename_tag round-trip on a COPY (build\edit_test\m5root).
#   powershell -ExecutionPolicy Bypass -File tools\test\crossbow-optics-rename.ps1
# Renames tag_silencer -> tag_silencer_rt, saves, reloads and verifies; real file hash is checked.

$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$game = "C:\Games\cod-hwm\Call of Duty Horizon Modern Warfare\Call of Duty Modern Warfare Remastered"
$testDir = Join-Path $repo "build\edit_test"
$testExe = Join-Path $testDir "hmw-workbench.exe"
$root = Join-Path $testDir "m5root"
$copyModel = Join-Path $root "xmodel\vm_crossbow_base.xmb"
$copySurface = Join-Path $root "xsurface\010296ab_3f739e8a_100.xsb"
$realModel = Join-Path $game "zonetool\mod\xmodel\vm_crossbow_base.xmb"
$review = Join-Path $repo "review\crossbow-optics"
New-Item -ItemType Directory -Force $review | Out-Null
Copy-Item (Join-Path $repo "build\bin\Release\hmw-workbench.exe") $testExe -Force
Copy-Item $realModel $copyModel -Force
Copy-Item (Join-Path $game "zonetool\mod\xsurface\010296ab_3f739e8a_100.xsb") $copySurface -Force
Remove-Item "$copyModel.bak", (Join-Path $root "xmodel\vm_crossbow_base.tags.json") -ErrorAction SilentlyContinue
$realHash = (Get-FileHash -Algorithm SHA256 $realModel).Hash

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
    if ($logText.Length -gt 1800) { $logText = $logText.Substring(0, 1800) + " ...[truncated]" }
    $script:log.Add("< " + $logText)
    if ($result.isError) { return $null }
    try { return $text | ConvertFrom-Json } catch { return $null }
}

try {
    $script:log.Add("rename_tag round-trip on a copy")
    $script:nextId++
    Send-Raw (@{ jsonrpc = "2.0"; id = $script:nextId; method = "initialize"; params = @{ protocolVersion = "2025-06-18" } } | ConvertTo-Json -Compress)
    $init = (Read-Response) | ConvertFrom-Json
    $script:log.Add("initialize -> " + $init.result.serverInfo.name)
    Send-Raw '{"jsonrpc":"2.0","method":"notifications/initialized"}'
    $null = Call-Json "load_model" @{ model = "vm_crossbow_base" }
    $renamed = Call-Json "rename_tag" @{ old = "tag_silencer"; new = "tag_silencer_rt" }
    $saved = Call-Json "save_model" @{}
    $null = Call-Json "load_model" @{ model = "vm_crossbow_base" }
    $tags = Call-Json "get_tags" @{}
    $hasNew = @($tags.tags | Where-Object { $_.name -eq "tag_silencer_rt" }).Count
    $hasOld = @($tags.tags | Where-Object { $_.name -eq "tag_silencer" }).Count
    $script:log.Add("")
    $script:log.Add("rename: " + ($renamed | ConvertTo-Json -Compress))
    $script:log.Add("save:   " + ($saved | ConvertTo-Json -Compress))
    $script:log.Add("reloaded: tag_silencer_rt=$hasNew tag_silencer=$hasOld")
}
finally {
    try { $server.StandardInput.Close() } catch {}
    if (-not $server.WaitForExit(15000)) { $server.Kill() }
    $realHashAfter = (Get-FileHash -Algorithm SHA256 $realModel).Hash
    $script:log.Add("real model unchanged: $($realHashAfter -eq $realHash)")
    $script:log | Set-Content (Join-Path $review "rename-roundtrip.txt") -Encoding UTF8
    Write-Output ("wrote " + (Join-Path $review "rename-roundtrip.txt"))
}
