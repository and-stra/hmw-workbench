# Rebuild the H1 "mod" zone with the updated crossbow models and install it.
#   powershell -ExecutionPolicy Bypass -File tools\test\rebuild-mod-zone.ps1
# Backs up the old .ff files first (never overwrites an existing backup), logs to
# review\crossbow-optics\build.log.

$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$game = "C:\Games\cod-hwm\Call of Duty Horizon Modern Warfare\Call of Duty Modern Warfare Remastered"
$x64zt = Join-Path $game "x64zt.exe"
$zoneOut = Join-Path $game "zone\mod.ff"
$install = Join-Path $game "mods\sticksandstones\mod.ff"
$review = Join-Path $repo "review\crossbow-optics"
$logPath = Join-Path $review "build.log"
New-Item -ItemType Directory -Force $review | Out-Null

$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add("crossbow optics: rebuild mod zone")
$lines.Add("date: " + (Get-Date -Format "yyyy-MM-dd HH:mm:ss"))
$lines.Add("tool: $x64zt")
$lines.Add("command: buildzone mod")
$lines.Add("")

function Backup-File([string]$path) {
    if (-not (Test-Path $path)) { return "" }
    $backup = "$path.bak-$stamp"
    if (Test-Path $backup) { throw "backup already exists: $backup" }
    Copy-Item $path $backup -Force
    return $backup
}
$zoneBackup = Backup-File $zoneOut
$installBackup = Backup-File $install
$lines.Add("backup zone:    $zoneBackup")
$lines.Add("backup install: $installBackup")
$oldHash = if (Test-Path $zoneOut) { (Get-FileHash -Algorithm SHA256 $zoneOut).Hash } else { "" }
$oldTime = if (Test-Path $zoneOut) { (Get-Item $zoneOut).LastWriteTime } else { [datetime]::MinValue }

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
$lines.Add("pid: " + $p.Id)
$outTask = $p.StandardOutput.ReadToEndAsync()
$errTask = $p.StandardError.ReadToEndAsync()

try {
    $ready = 80
    $lines.Add("waiting $ready s for initialization")
    $deadline = (Get-Date).AddSeconds($ready)
    while ((Get-Date) -lt $deadline) { Start-Sleep -Seconds 2; if ($p.HasExited) { break } }
    if ($p.HasExited) { throw "x64zt exited during initialization (code $($p.ExitCode))" }
    $p.StandardInput.WriteLine("buildzone mod")
    $p.StandardInput.Flush()
    $lines.Add("sent: buildzone mod")
    $built = $false
    $deadline = (Get-Date).AddMinutes(8)
    while ((Get-Date) -lt $deadline) {
        Start-Sleep -Seconds 2
        if ($p.HasExited) { break }
        if ((Test-Path $zoneOut) -and ((Get-Item $zoneOut).LastWriteTime -gt $oldTime)) {
            Start-Sleep -Seconds 4
            $built = $true
            break
        }
    }
    $lines.Add("new zone file: $built")
    try { $p.StandardInput.WriteLine("quit"); $p.StandardInput.Flush() } catch {}
    if (-not $p.WaitForExit(10000)) { $p.Kill(); $lines.Add("killed after quit timeout") }
}
finally {
    try { $p.StandardInput.Close() } catch {}
    $stdout = ""
    $stderr = ""
    try { $stdout = $outTask.GetAwaiter().GetResult() } catch {}
    try { $stderr = $errTask.GetAwaiter().GetResult() } catch {}
    $lines.Add("")
    $lines.Add("--- stdout ---")
    $lines.Add($stdout)
    if ($stderr.Trim().Length -gt 0) { $lines.Add("--- stderr ---"); $lines.Add($stderr) }
    $lines.Add("")
    $lines.Add("--- files ---")
    if (Test-Path $zoneOut) {
        $newHash = (Get-FileHash -Algorithm SHA256 $zoneOut).Hash
        $lines.Add("zone\mod.ff: $($newHash) size $((Get-Item $zoneOut).Length) (old $oldHash)")
    } else {
        $lines.Add("zone\mod.ff missing")
    }
    $fatal = ([regex]::Matches($stdout + $stderr, "(?im)^.*\b(FATAL|ERROR)\b.*$")).Count
    $lines.Add("FATAL/ERROR lines: $fatal")
    $lines | Set-Content $logPath -Encoding UTF8

    if ($built -and (Test-Path $zoneOut)) {
        Copy-Item $zoneOut $install -Force
        $lines.Add("installed: $install")
        $lines | Set-Content $logPath -Encoding UTF8
        Write-Output ("built and installed " + $install)
    } else {
        Write-Output ("build failed, see " + $logPath)
        exit 1
    }
}
