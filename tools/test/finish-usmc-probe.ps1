# Finish item 2: locate and dump usmc_desert_assault_body_mp_lod0300 (the only missing
# surface of the H1 USMC desert body). Tries the HMW zones one by one with loadzone+dumpasset.
$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$game = "C:\Games\cod-hwm\Call of Duty Horizon Modern Warfare\Call of Duty Modern Warfare Remastered"
$x64zt = Join-Path $game "x64zt.exe"
$log = Join-Path $repo "review\finish\usmc-surface-probe.log"
$zones = @(
  "hmw_ar1", "hmw_smg", "hmw_shotgun", "hmw_nightshade", "hmw_post_gfx", "hmw_killstreak",
  "hmw_dlc_killstreaks", "hmw_pre_gfx", "hmw_patch_code_post_gfx_mp", "hmw_patch_ui_mp"
)
$links = @()
foreach ($z in $zones) {
    $link = Join-Path $game "zone\$z.ff"
    $src = Join-Path $game "hmw-mod\zone\$z.ff"
    if (-not (Test-Path $link)) {
        New-Item -ItemType HardLink -Path $link -Target $src | Out-Null
        $links += $link
    }
}
$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add("probe for usmc_desert_assault_body_mp_lod0300 in HMW zones")
$lines.Add("date: " + (Get-Date -Format "yyyy-MM-dd HH:mm:ss"))
$dumpAssets = Join-Path $game "dump\assets"
$psi = [System.Diagnostics.ProcessStartInfo]::new()
$psi.FileName = $x64zt
$psi.Arguments = "-unbuffered-io"
$psi.WorkingDirectory = $game
$psi.RedirectStandardInput = $true
$psi.RedirectStandardOutput = $true
$psi.RedirectStandardError = $true
$psi.UseShellExecute = $false
$psi.CreateNoWindow = $true
$p = [System.Diagnostics.Process]::Start($psi)
$outTask = $p.StandardOutput.ReadToEndAsync()
$errTask = $p.StandardError.ReadToEndAsync()
try {
    Start-Sleep -Seconds 80
    if ($p.HasExited) { throw "x64zt exited early" }
    foreach ($z in $zones) {
        $cmd = "loadzone $z"
        $p.StandardInput.WriteLine($cmd); $p.StandardInput.Flush()
        $lines.Add("sent: $cmd")
        Start-Sleep -Seconds 40
        $p.StandardInput.WriteLine("dumpasset xmodelsurfs usmc_desert_assault_body_mp_lod0300")
        $p.StandardInput.Flush()
        $lines.Add("sent: dumpasset xmodelsurfs usmc_desert_assault_body_mp_lod0300 ($z)")
        Start-Sleep -Seconds 15
        $p.StandardInput.WriteLine("unloadzones"); $p.StandardInput.Flush()
        Start-Sleep -Seconds 5
        $hit = Get-ChildItem $dumpAssets -Recurse -File -Filter "*usmc_desert_assault_body_mp*" -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($hit) {
            $lines.Add("FOUND: " + $hit.FullName + " " + $hit.Length)
            break
        } else {
            $lines.Add("not found in $z")
        }
    }
    try { $p.StandardInput.WriteLine("quit"); $p.StandardInput.Flush() } catch {}
    if (-not $p.WaitForExit(10000)) { $p.Kill() }
} finally {
    try { $stdout = $outTask.GetAwaiter().GetResult() } catch { $stdout = "" }
    try { $stderr = $errTask.GetAwaiter().GetResult() } catch { $stderr = "" }
    $lines.Add(""); $lines.Add("--- stdout ---"); $lines.Add($stdout)
    if ($stderr.Trim()) { $lines.Add("--- stderr ---"); $lines.Add($stderr) }
    $lines | Set-Content $log -Encoding UTF8
    foreach ($link in $links) { Remove-Item $link -Force -ErrorAction SilentlyContinue }
    Write-Output ("log: " + $log)
}
