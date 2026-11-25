# Finish item 5: hot reload for material jsons, image/pixel streams and new root files.
# Uses a copy of the crossbow model/material/images in build\hot_reload_test so no real game
# file is touched.
#   powershell -ExecutionPolicy Bypass -File tools\test\finish-hot-reload.ps1
$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$game = "C:\Games\cod-hwm\Call of Duty Horizon Modern Warfare\Call of Duty Modern Warfare Remastered"
$testDir = Join-Path $repo "build\hot_reload_test"
$root = Join-Path $testDir "root"
$exe = Join-Path $testDir "hmw-workbench.exe"
$review = Join-Path $repo "review\finish"
$logPath = Join-Path $review "hot-reload.txt"
New-Item -ItemType Directory -Force $review | Out-Null
Remove-Item $testDir -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force (Join-Path $root "xmodel") | Out-Null
New-Item -ItemType Directory -Force (Join-Path $root "xsurface") | Out-Null
New-Item -ItemType Directory -Force (Join-Path $root "materials\m") | Out-Null
New-Item -ItemType Directory -Force (Join-Path $root "streamed_images") | Out-Null
New-Item -ItemType Directory -Force (Join-Path $root "weapons") | Out-Null
Copy-Item (Join-Path $repo "build\bin\Release\hmw-workbench.exe") $exe -Force
Copy-Item (Join-Path $game "zonetool\mod\xmodel\vm_crossbow_base.xmb") (Join-Path $root "xmodel\vm_crossbow_base.xmb") -Force
Copy-Item (Join-Path $game "zonetool\mod\xsurface\010296ab_3f739e8a_100.xsb") (Join-Path $root "xsurface\010296ab_3f739e8a_100.xsb") -Force
Copy-Item (Join-Path $game "zonetool\mod\materials\m\mtl_crossbow_base.json") (Join-Path $root "materials\m\mtl_crossbow_base.json") -Force
foreach ($suffix in "crossbow_col.h1Image","crossbow_col_stream0.pixels","crossbow_col_stream1.pixels","crossbow_col_stream2.pixels","crossbow_col_stream3.pixels") {
    Copy-Item (Join-Path $game "zonetool\mod\streamed_images\$suffix") (Join-Path $root "streamed_images\$suffix") -Force
}
$settings = Get-Content (Join-Path $repo "build\bin\Release\workbench.json") -Raw | ConvertFrom-Json
$settings.assetRoots = @("root") + @($settings.assetRoots)
$settings | ConvertTo-Json -Depth 20 | Set-Content (Join-Path $testDir "workbench.json") -Encoding UTF8

$materialPath = Join-Path $root "materials\m\mtl_crossbow_base.json"
$pixelPath = Join-Path $root "streamed_images\crossbow_col_stream3.pixels"
$newWeaponPath = Join-Path $root "weapons\hot_reload_new_weapon.json"
$beforeShot = Join-Path $review "hot_reload_material_before.png"
$afterShot = Join-Path $review "hot_reload_material_after.png"
$imageShot = Join-Path $review "hot_reload_image_after.png"

$psi = [System.Diagnostics.ProcessStartInfo]::new()
$psi.FileName = $exe
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
function Poll-Once() {
    Start-Sleep -Milliseconds 1100
    $null = Call-Json "hot_reload" @{ action = "poll" }
    Start-Sleep -Milliseconds 700
    return Call-Json "hot_reload" @{ action = "poll" }
}

$failed = 1
try {
    Write-Log "finish item 5: hot reload (copy of the crossbow material/images in build\hot_reload_test)"
    Write-Log ("date: " + (Get-Date -Format "yyyy-MM-dd HH:mm:ss"))
    $script:nextId++
    Send-Raw (@{ jsonrpc = "2.0"; id = $script:nextId; method = "initialize"; params = @{ protocolVersion = "2025-06-18" } } | ConvertTo-Json -Compress)
    $init = (Read-Response) | ConvertFrom-Json
    Write-Log ("> initialize -> " + $init.result.serverInfo.name)
    Send-Raw '{"jsonrpc":"2.0","method":"notifications/initialized"}'

    $loaded = Call-Json "load_model" @{ model = "vm_crossbow_base"; camo = "camo_h1_christmas_wrap" }
    $status0 = Call-Json "hot_reload" @{ action = "status" }
    $watched = @($status0.watched)
    $hasMaterial = $watched | Where-Object { $_ -like "*mtl_crossbow_base.json" }
    $hasImage = $watched | Where-Object { $_ -like "*crossbow_col.h1Image" }
    $hasPixels = $watched | Where-Object { $_ -like "*crossbow_col_stream3.pixels" }
    if ($loaded -and $hasMaterial -and $hasImage -and $hasPixels) {
        $script:checks.Add("PASS watcher lists the material json, image and pixel stream ($($watched.Count) files)")
    } else {
        $script:checks.Add("FAIL watcher missing material/image/pixels")
    }

    $null = Call-Json "screenshot" @{ path = $beforeShot; view = "orbit"; yaw = 35; pitch = 18; dist = 26; width = 1200; height = 900 }
    $hashBefore = (Get-FileHash $beforeShot -Algorithm SHA256).Hash

    # 1. material json: change decalParams1 from 2,2 to 4,4.
    $material = Get-Content $materialPath -Raw | ConvertFrom-Json
    foreach ($constant in $material.constantTable) {
        if ($constant.name -eq "decalParams1") {
            $constant.literal = @(4.0, 4.0, 0.0, 0.0)
        }
    }
    $material | ConvertTo-Json -Depth 30 | Set-Content $materialPath -Encoding UTF8
    $pollMaterial = Poll-Once
    $null = Call-Json "screenshot" @{ path = $afterShot; view = "orbit"; yaw = 35; pitch = 18; dist = 26; width = 1200; height = 900 }
    $hashAfter = (Get-FileHash $afterShot -Algorithm SHA256).Hash
    if ($pollMaterial.reloads -ge 1 -and $hashBefore -ne $hashAfter) {
        $script:checks.Add("PASS material json change reloaded (reloads $($pollMaterial.reloads), screenshot changed)")
    } else {
        $script:checks.Add("FAIL material json change (reloads $($pollMaterial.reloads), same=$($hashBefore -eq $hashAfter))")
    }

    # 2. streamed image pixels: zero every stream (the loader may choose any mip stream).
    foreach ($stream in 0..3) {
        $path = Join-Path $root "streamed_images\crossbow_col_stream$stream.pixels"
        if (Test-Path $path) {
            $bytes = [System.IO.File]::ReadAllBytes($path)
            [System.IO.File]::WriteAllBytes($path, (New-Object byte[] $bytes.Length))
        }
    }
    $pollImage = Poll-Once
    $null = Call-Json "screenshot" @{ path = $imageShot; view = "orbit"; yaw = 35; pitch = 18; dist = 26; width = 1200; height = 900 }
    $hashImage = (Get-FileHash $imageShot -Algorithm SHA256).Hash
    if ($pollImage.reloads -ge 2 -and $hashAfter -ne $hashImage) {
        $script:checks.Add("PASS image pixel change reloaded (reloads $($pollImage.reloads), screenshot changed)")
    } else {
        $script:checks.Add("FAIL image pixel change (reloads $($pollImage.reloads), same=$($hashAfter -eq $hashImage))")
    }

    # 3. new file in a root: add a weapon json and check that the asset index count grows.
    $countBefore = (Call-Json "list_assets" @{ limit = 1000 }).count
    Copy-Item (Join-Path $root "weapons\hot_reload_new_weapon.json") $newWeaponPath -Force -ErrorAction SilentlyContinue
    if (-not (Test-Path $newWeaponPath)) {
        Copy-Item (Join-Path $game "zonetool\mod\weapons\iw5_exocrossbow_mp.json") $newWeaponPath -Force
    }
    $pollRoot = Poll-Once
    $countAfter = (Call-Json "list_assets" @{ limit = 1000 }).count
    if ($pollRoot.reloads -ge 3 -and $countAfter -gt $countBefore) {
        $script:checks.Add("PASS new root file picked up (assets $countBefore -> $countAfter, reloads $($pollRoot.reloads))")
    } else {
        $script:checks.Add("FAIL new root file (assets $countBefore -> $countAfter, reloads $($pollRoot.reloads))")
    }

    Write-Log ""
    Write-Log ("material hashes: $hashBefore -> $hashAfter")
    Write-Log ("image hashes:    $hashAfter -> $hashImage")
    Write-Log ("status: " + ($pollRoot | ConvertTo-Json -Compress))
} finally {
    try { $server.StandardInput.Close() } catch {}
    if (-not $server.WaitForExit(15000)) { $server.Kill() }
    $script:log.Add("")
    $script:log.Add("checks:")
    foreach ($check in $script:checks) { $script:log.Add("  $check") }
    $failed = @($script:checks | Where-Object { $_ -like "FAIL*" }).Count
    $script:log.Add("")
    $script:log.Add("result: $($script:checks.Count - $failed) passed, $failed failed")
    $script:log | Set-Content -Path $logPath -Encoding UTF8
    Write-Output ("wrote " + $logPath)
    if ($failed -gt 0) { exit 1 }
}

