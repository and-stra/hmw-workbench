# Dump one missing xsurface from a HMW zone into a new GAME\dump\workbench_* folder.
#   powershell -ExecutionPolicy Bypass -File tools\test\dump-reflex-variant.ps1
$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$game = "C:\Games\cod-hwm\Call of Duty Horizon Modern Warfare\Call of Duty Modern Warfare Remastered"
$x64zt = Join-Path $game "x64zt.exe"
$zoneName = "hmw_patch_common_mp"
$zoneLink = Join-Path $game "zone\$zoneName.ff"
$zoneReal = Join-Path $game "hmw-mod\zone\$zoneName.ff"
$log = Join-Path $repo "review\reddots\dump-reflex-variant.log"
New-Item -ItemType Directory -Force (Split-Path $log) | Out-Null
$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add("dump xsurface vm_h1_optic_reflex_variant_ads_mp_lodvm100")
if (-not (Test-Path $zoneLink)) {
    New-Item -ItemType HardLink -Path $zoneLink -Target $zoneReal | Out-Null
    $lines.Add("hardlinked $zoneLink -> $zoneReal")
} else {
    $lines.Add("zone link already exists: $zoneLink")
}
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
    $p.StandardInput.WriteLine("loadzone $zoneName"); $p.StandardInput.Flush()
    $lines.Add("sent loadzone $zoneName")
    Start-Sleep -Seconds 8
    $p.StandardInput.WriteLine("dumpasset xsurface vm_h1_optic_reflex_variant_ads_mp_lodvm100"); $p.StandardInput.Flush()
    $lines.Add("sent dumpasset xsurface ...")
    Start-Sleep -Seconds 10
    try { $p.StandardInput.WriteLine("quit"); $p.StandardInput.Flush() } catch {}
    if (-not $p.WaitForExit(10000)) { $p.Kill() }
} finally {
    try { $stdout = $outTask.GetAwaiter().GetResult() } catch { $stdout = "" }
    try { $stderr = $errTask.GetAwaiter().GetResult() } catch { $stderr = "" }
    $lines.Add("--- stdout ---"); $lines.Add($stdout)
    if ($stderr.Trim()) { $lines.Add("--- stderr ---"); $lines.Add($stderr) }
    $lines | Set-Content $log -Encoding UTF8
    Write-Output ("log: " + $log)
}
