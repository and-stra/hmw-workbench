# Finish item 2: dump the 5 usmc_desert_assault_body_mp LOD surfaces from hmw_ar1 into the
# existing workbench_hmw_ar1 folder (new workbench dump, never the original game dumps).
$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$game = "C:\Games\cod-hwm\Call of Duty Horizon Modern Warfare\Call of Duty Modern Warfare Remastered"
$x64zt = Join-Path $game "x64zt.exe"
$log = Join-Path $repo "review\finish\usmc-surface-dump.log"
$zone = "hmw_ar1"
$link = Join-Path $game "zone\$zone.ff"
$src = Join-Path $game "hmw-mod\zone\$zone.ff"
if (-not (Test-Path $link)) { New-Item -ItemType HardLink -Path $link -Target $src | Out-Null }
$dumpAssets = Join-Path $game "dump\assets"
$dest = Join-Path $game "dump\workbench_hmw_ar1\xsurface"
New-Item -ItemType Directory -Force $dest | Out-Null
$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add("dump usmc_desert_assault_body_mp surfaces from $zone")
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
    $p.StandardInput.WriteLine("loadzone $zone"); $p.StandardInput.Flush()
    Start-Sleep -Seconds 45
    foreach ($lod in 0,1,2,3,4) {
        $name = "usmc_desert_assault_body_mp_lod${lod}300"
        $p.StandardInput.WriteLine("dumpasset xmodelsurfs $name"); $p.StandardInput.Flush()
        $lines.Add("sent: dumpasset xmodelsurfs $name")
        Start-Sleep -Seconds 10
        $hit = Get-ChildItem "$dumpAssets\xsurface" -File -Filter "$name.xsb" -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($hit) {
            Copy-Item $hit.FullName (Join-Path $dest $hit.Name) -Force
            $lines.Add("copied: " + $hit.Name + " " + $hit.Length)
        } else {
            $lines.Add("MISSING: $name")
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
    Remove-Item $link -Force -ErrorAction SilentlyContinue
    Write-Output ("log: " + $log)
}
