# Finish item 2: render every faction in the table with body + head, collect the head
# attachment result, and build the review contact sheet.
#   powershell -ExecutionPolicy Bypass -File tools\test\finish-factions.ps1
$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$exe = Join-Path $repo "build\bin\Release\hmw-workbench.exe"
$out = Join-Path $repo "review\finish\factions"
New-Item -ItemType Directory -Force $out | Out-Null
$ids = @(
    "tf141", "opfor", "spetsnaz", "rangers", "seals", "militia", "gign", "pmc", "ic", "africa",
    "delta", "usmc_desert", "hazmat", "infect"
)
$report = [System.Collections.Generic.List[string]]::new()
$report.Add("finish item 2: faction body/head renders")
$report.Add("exe: $exe")
$report.Add("date: " + (Get-Date -Format "yyyy-MM-dd HH:mm:ss"))
$report.Add("")
$failed = $false
foreach ($id in $ids) {
    $png = Join-Path $out "$id.png"
    $log = Join-Path $out "$id.log"
    $command = "`"$exe`" -screenshot `"$png`" -character $id -view orbit -yaw 30 -pitch 10 -dist 90 " +
        "-exposure 1.4 -width 1200 -height 900 > `"$log`" 2>&1"
    cmd /c $command
    $text = Get-Content $log
    $report.Add("=== $id ===")
    foreach ($line in $text) {
        if ($line -match "^(character:|head attach:|warning:|missing:)") {
            $report.Add($line)
        }
    }
    if (-not (Test-Path $png) -or (Get-Item $png).Length -lt 10000) {
        $report.Add("FAIL: no screenshot")
        $failed = $true
    }
    if ($text -match "cannot load character") {
        $failed = $true
    }
    $report.Add("")
}
$report | Set-Content (Join-Path $out "report.txt") -Encoding UTF8

Add-Type -AssemblyName System.Drawing
$cellW = 420
$cellH = 315
$cols = 5
$rows = [Math]::Ceiling($ids.Count / $cols)
$top = 40
$bmp = New-Object System.Drawing.Bitmap (($cellW * $cols), ($cellH * $rows + $top))
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.Clear([System.Drawing.Color]::FromArgb(20, 22, 26))
$font = New-Object System.Drawing.Font("Arial", 12, [System.Drawing.FontStyle]::Bold)
$title = New-Object System.Drawing.Font("Arial", 15, [System.Drawing.FontStyle]::Bold)
$g.DrawString("HMW factions on the workbench - textured body + head (orbit yaw 30 pitch 10)", $title,
    [System.Drawing.Brushes]::White, 8, 8)
for ($i = 0; $i -lt $ids.Count; $i++) {
    $x = [int](($i % $cols) * $cellW)
    $y = [int]($top + [Math]::Floor($i / $cols) * $cellH)
    $png = Join-Path $out ($ids[$i] + ".png")
    if (Test-Path $png) {
        $img = [System.Drawing.Image]::FromFile($png)
        $g.DrawImage($img, [System.Drawing.Rectangle]::new($x, $y, $cellW, [int]($cellH - 20)))
        $img.Dispose()
    }
    $g.DrawString($ids[$i], $font, [System.Drawing.Brushes]::White, $x + 6, [int]($y + $cellH - 18))
}
$g.Dispose()
$bmp.Save((Join-Path $out "factions_sheet.png"), [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()

Write-Output (("rendered {0} factions -> {1}") -f $ids.Count, $out)
if ($failed) {
    Write-Output "FAIL: see report.txt"
    exit 1
}
