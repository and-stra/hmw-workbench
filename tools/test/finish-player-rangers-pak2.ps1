# Finish item 2 follow-up (corrected): zonetool changes into GAME\dump, so the source zone is
# the hardlink dump\workbench_hmw_rangers.ff. Link the matching .pak there too, so the
# is_self streamed images (fileIndex 96) come from workbench_hmw_rangers.pak.
$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$game = "C:\Games\cod-hwm\Call of Duty Horizon Modern Warfare\Call of Duty Modern Warfare Remastered"
$x64zt = Join-Path $game "x64zt.exe"
$log = Join-Path $repo "review\finish\player-dumps-rangers-pak2.log"
$zone = "hmw_rangers"
foreach ($ext in ".ff", ".pak") {
    $link = Join-Path $game "dump\workbench_$zone$ext"
    $src = Join-Path $game "hmw-mod\zone\$zone$ext"
    if (-not (Test-Path $link)) { New-Item -ItemType HardLink -Path $link -Target $src | Out-Null }
}
$lines = [System.Collections.Generic.List[string]]::new()
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
try {
    Start-Sleep -Seconds 80
    if ($p.HasExited) { throw "x64zt exited early" }
    $p.StandardInput.WriteLine("dumpzone h1 ..\dump\workbench_$zone material,image"); $p.StandardInput.Flush()
    $lines.Add("sent: dumpzone h1 ..\dump\workbench_$zone material,image")
    $dir = Join-Path $game "dump\workbench_$zone"
    $deadline = (Get-Date).AddMinutes(10); $last = -1; $stable = 0
    while ((Get-Date) -lt $deadline) {
        Start-Sleep -Seconds 5
        if (-not (Test-Path $dir)) { continue }
        $count = (Get-ChildItem $dir -Recurse -File -ErrorAction SilentlyContinue | Measure-Object).Count
        if ($count -eq $last -and $count -gt 0) { $stable++; if ($stable -ge 4) { break } } else { $stable = 0; $last = $count }
    }
    $lines.Add("files after dump: $last")
    try { $p.StandardInput.WriteLine("quit"); $p.StandardInput.Flush() } catch {}
    if (-not $p.WaitForExit(10000)) { $p.Kill() }
} finally {
    try { $stdout = $outTask.GetAwaiter().GetResult() } catch { $stdout = "" }
    try { $stderr = $errTask.GetAwaiter().GetResult() } catch { $stderr = "" }
    $lines.Add(""); $lines.Add("--- stdout ---"); $lines.Add($stdout)
    if ($stderr.Trim()) { $lines.Add("--- stderr ---"); $lines.Add($stderr) }
    $lines | Set-Content $log -Encoding UTF8
    foreach ($ext in ".ff", ".pak") { Remove-Item (Join-Path $game "dump\workbench_$zone$ext") -Force -ErrorAction SilentlyContinue }
    Write-Output ("log: " + $log)
}
