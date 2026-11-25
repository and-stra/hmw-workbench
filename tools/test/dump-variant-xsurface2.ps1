# Dump xsurfaces from HMW hmw_patch_common_mp into a new GAME\dump\workbench_hmw_reflex_variant
# (new folder, xsurface only) to recover the missing H1 reflex-variant surface.
$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$game = "C:\Games\cod-hwm\Call of Duty Horizon Modern Warfare\Call of Duty Modern Warfare Remastered"
$x64zt = Join-Path $game "x64zt.exe"
$link = Join-Path $game "dump\workbench_hmw_reflex_variant.ff"
$src = Join-Path $game "hmw-mod\zone\hmw_patch_common_mp.ff"
$outDir = Join-Path $game "dump\workbench_hmw_reflex_variant"
$log = Join-Path $repo "review\reddots\dump-variant-xsurface.log"
New-Item -ItemType Directory -Force (Split-Path $link) | Out-Null
if (-not (Test-Path $link)) { New-Item -ItemType HardLink -Path $link -Target $src | Out-Null }
$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add("dump xsurface-only HMW patch_common_mp -> workbench_hmw_reflex_variant")
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
    $p.StandardInput.WriteLine("dumpzone h1 ..\dump\workbench_hmw_reflex_variant xmodelsurfs")
    $p.StandardInput.Flush()
    $lines.Add("sent: dumpzone h1 ..\dump\workbench_hmw_reflex_variant xmodelsurfs")
    $deadline = (Get-Date).AddMinutes(6)
    $lastCount = -1
    $stable = 0
    while ((Get-Date) -lt $deadline) {
        Start-Sleep -Seconds 5
        if ($p.HasExited) { break }
        if (Test-Path $outDir) {
            $count = (Get-ChildItem $outDir -Recurse -File -ErrorAction SilentlyContinue | Measure-Object).Count
            if ($count -eq $lastCount -and $count -gt 0) { $stable++; if ($stable -ge 4) { break } } else { $stable = 0; $lastCount = $count }
        }
    }
    try { $p.StandardInput.WriteLine("quit"); $p.StandardInput.Flush() } catch {}
    if (-not $p.WaitForExit(10000)) { $p.Kill() }
} finally {
    try { $stdout = $outTask.GetAwaiter().GetResult() } catch { $stdout = "" }
    try { $stderr = $errTask.GetAwaiter().GetResult() } catch { $stderr = "" }
    $lines.Add("--- stdout ---"); $lines.Add($stdout)
    if ($stderr.Trim()) { $lines.Add("--- stderr ---"); $lines.Add($stderr) }
    $found = Get-ChildItem $outDir -Recurse -File -Filter "*reflex_variant*" -ErrorAction SilentlyContinue
    $lines.Add("--- variant files ---")
    foreach ($f in $found) { $lines.Add($f.FullName + " " + $f.Length) }
    $lines | Set-Content $log -Encoding UTF8
    Write-Output ("log: " + $log)
}
