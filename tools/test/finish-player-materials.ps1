# Finish item 2: dump materials/images for the faction bodies/heads into the new
# GAME\dump\workbench_hmw_rangers and ..._dlc_factions folders (streamed images on).
$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$game = "C:\Games\cod-hwm\Call of Duty Horizon Modern Warfare\Call of Duty Modern Warfare Remastered"
$x64zt = Join-Path $game "x64zt.exe"
$log = Join-Path $repo "review\finish\player-dumps-materials.log"
$lines = [System.Collections.Generic.List[string]]::new()
$jobs = @("hmw_rangers", "hmw_dlc_factions")
foreach ($z in $jobs) {
    $link = Join-Path $game "dump\workbench_$z.ff"
    $src = Join-Path $game "hmw-mod\zone\$z.ff"
    if (-not (Test-Path $link)) { New-Item -ItemType HardLink -Path $link -Target $src | Out-Null }
}
$psi = [System.Diagnostics.ProcessStartInfo]::new()
$psi.FileName = $x64zt
$psi.Arguments = "-unbuffered-io -dump_streamed_image"
$psi.WorkingDirectory = $game
$psi.RedirectStandardInput = $true
$psi.RedirectStandardOutput = $true
$psi.RedirectStandardError = $true
$psi.UseShellExecute = $false
$psi.CreateNoWindow = $true
$p = [System.Diagnostics.Process]::Start($psi)
$outTask = $p.StandardOutput.ReadToEndAsync()
$errTask = $p.StandardError.ReadToEndAsync()
function Wait-Dump([string]$dir) {
    $deadline = (Get-Date).AddMinutes(10); $last = -1; $stable = 0
    while ((Get-Date) -lt $deadline) {
        Start-Sleep -Seconds 5
        if (-not (Test-Path $dir)) { continue }
        $count = (Get-ChildItem $dir -Recurse -File -ErrorAction SilentlyContinue | Measure-Object).Count
        if ($count -eq $last -and $count -gt 0) { $stable++; if ($stable -ge 4) { return $count } } else { $stable = 0; $last = $count }
    }
    return $last
}
try {
    Start-Sleep -Seconds 80
    if ($p.HasExited) { throw "x64zt exited early" }
    foreach ($z in $jobs) {
        $cmd = "dumpzone h1 ..\dump\workbench_$z material,image"
        $p.StandardInput.WriteLine($cmd); $p.StandardInput.Flush()
        $lines.Add("sent: $cmd")
        $count = Wait-Dump (Join-Path $game "dump\workbench_$z")
        $lines.Add("done: $z ($count files)")
    }
    try { $p.StandardInput.WriteLine("quit"); $p.StandardInput.Flush() } catch {}
    if (-not $p.WaitForExit(10000)) { $p.Kill() }
} finally {
    try { $stdout = $outTask.GetAwaiter().GetResult() } catch { $stdout = "" }
    try { $stderr = $errTask.GetAwaiter().GetResult() } catch { $stderr = "" }
    $lines.Add(""); $lines.Add("--- stdout ---"); $lines.Add($stdout)
    if ($stderr.Trim()) { $lines.Add("--- stderr ---"); $lines.Add($stderr) }
    $lines | Set-Content $log -Encoding UTF8
    foreach ($z in $jobs) { $link = Join-Path $game "dump\workbench_$z.ff"; if (Test-Path $link) { Remove-Item $link -Force } }
    Write-Output ("log: " + $log)
}
