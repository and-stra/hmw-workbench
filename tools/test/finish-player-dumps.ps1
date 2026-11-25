# Finish item 2: dump HMW faction bodies/heads into NEW GAME\dump\workbench_* folders.
#   powershell -ExecutionPolicy Bypass -File tools\test\finish-player-dumps.ps1
$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$game = "C:\Games\cod-hwm\Call of Duty Horizon Modern Warfare\Call of Duty Modern Warfare Remastered"
$x64zt = Join-Path $game "x64zt.exe"
$review = Join-Path $repo "review\finish"
$logPath = Join-Path $review "player-dumps.log"
New-Item -ItemType Directory -Force $review | Out-Null
$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add("finish item 2: player model dumps")
$lines.Add("date: " + (Get-Date -Format "yyyy-MM-dd HH:mm:ss"))

$jobs = @(
  @{ zone = "hmw_rangers"; types = "xmodel,xmodelsurfs" },
  @{ zone = "hmw_dlc_factions"; types = "xmodel,xmodelsurfs" },
  @{ zone = "hmw_pre_gfx"; types = "xmodel,xmodelsurfs" },
  @{ zone = "hmw_patch_common_mp"; types = "xmodelsurfs"; name = "hmw_patch_common_mp_xsurface" }
)
foreach ($job in $jobs) {
    $name = if ($job.ContainsKey("name")) { $job.name } else { $job.zone }
    $link = Join-Path $game "dump\workbench_$name.ff"
    $src = Join-Path $game "hmw-mod\zone\$($job.zone).ff"
    if (-not (Test-Path $link)) {
        New-Item -ItemType HardLink -Path $link -Target $src | Out-Null
        $lines.Add("hardlink $link -> $src")
    }
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
function Wait-Dump([string]$dir) {
    $deadline = (Get-Date).AddMinutes(10)
    $last = -1; $stable = 0
    while ((Get-Date) -lt $deadline) {
        Start-Sleep -Seconds 5
        if (-not (Test-Path $dir)) { continue }
        $count = (Get-ChildItem $dir -Recurse -File -ErrorAction SilentlyContinue | Measure-Object).Count
        if ($count -eq $last -and $count -gt 0) {
            $stable++
            if ($stable -ge 4) { return $count }
        } else { $stable = 0; $last = $count }
    }
    return $last
}
try {
    Start-Sleep -Seconds 80
    if ($p.HasExited) { throw "x64zt exited early" }
    foreach ($job in $jobs) {
        $name = if ($job.ContainsKey("name")) { $job.name } else { $job.zone }
        $cmd = "dumpzone h1 ..\dump\workbench_$name $($job.types)"
        $p.StandardInput.WriteLine($cmd); $p.StandardInput.Flush()
        $lines.Add("sent: $cmd")
        $dir = Join-Path $game "dump\workbench_$name"
        $count = Wait-Dump $dir
        $lines.Add("done: $dir ($count files)")
    }
    try { $p.StandardInput.WriteLine("quit"); $p.StandardInput.Flush() } catch {}
    if (-not $p.WaitForExit(10000)) { $p.Kill() }
} finally {
    try { $stdout = $outTask.GetAwaiter().GetResult() } catch { $stdout = "" }
    try { $stderr = $errTask.GetAwaiter().GetResult() } catch { $stderr = "" }
    $lines.Add("")
    $lines.Add("--- stdout ---"); $lines.Add($stdout)
    if ($stderr.Trim()) { $lines.Add("--- stderr ---"); $lines.Add($stderr) }
    $lines.Add("--- folders ---")
    foreach ($job in $jobs) {
        $name = if ($job.ContainsKey("name")) { $job.name } else { $job.zone }
        $dir = Join-Path $game "dump\workbench_$name"
        if (Test-Path $dir) {
            $sum = (Get-ChildItem $dir -Recurse -File | Measure-Object Length -Sum)
            $lines.Add("$dir : $($sum.Count) files, $($sum.Sum) bytes")
        }
    }
    $lines | Set-Content $logPath -Encoding UTF8
    foreach ($job in $jobs) {
        $name = if ($job.ContainsKey("name")) { $job.name } else { $job.zone }
        $link = Join-Path $game "dump\workbench_$name.ff"
        if (Test-Path $link) { Remove-Item $link -Force }
    }
    Write-Output ("log: " + $logPath)
}
