# M7 regression run: -validate, -roundtrip and the review commands of M1-M6.
#
#   powershell -ExecutionPolicy Bypass -File tools\test\m7-regression.ps1
#
# Every command runs into review\M7\regression\ (logs and PNGs). The MCP scripts write their
# usual review\M4b/M5/M6 logs. Writes review\M7\regression.txt with an exit/size table.
# Exit code 0 = every command exited 0 and produced its file.

$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$exe = Join-Path $repo "build\bin\Release\hmw-workbench.exe"
$review = Join-Path $repo "review\M7"
$outDir = Join-Path $review "regression"
$logDir = Join-Path $outDir "logs"
New-Item -ItemType Directory -Force $outDir | Out-Null
New-Item -ItemType Directory -Force $logDir | Out-Null
$script:rows = [System.Collections.Generic.List[string]]::new()
$script:failed = 0

function Run-Exe([string]$name, [string]$arguments, [string]$expectFile, [int]$allowExit = 0) {
    $psi = [System.Diagnostics.ProcessStartInfo]::new()
    $psi.FileName = $exe
    $psi.Arguments = $arguments
    $psi.WorkingDirectory = $repo
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow = $true
    $process = [System.Diagnostics.Process]::Start($psi)
    $stdout = $process.StandardOutput.ReadToEnd()
    $stderr = $process.StandardError.ReadToEnd()
    $process.WaitForExit()
    $log = Join-Path $logDir ($name + ".txt")
    @("exit " + $process.ExitCode, "", $stdout, $stderr) | Set-Content $log -Encoding UTF8
    $ok = $process.ExitCode -eq $allowExit
    $details = "exit $($process.ExitCode)"
    if ($expectFile.Length -gt 0) {
        $path = Join-Path $repo $expectFile
        if (-not (Test-Path $path)) { $ok = $false; $details += ", missing " + $expectFile }
        elseif ((Get-Item $path).Length -lt 100) { $ok = $false; $details += ", empty " + $expectFile }
        else { $details += ", " + (Get-Item $path).Length + " B" }
    }
    if (-not $ok) { $script:failed++ }
    $script:rows.Add(("{0} {1,-34} {2}" -f ($(if ($ok) { "PASS" } else { "FAIL" })), $name, $details))
}

function Run-Script([string]$name, [string]$script) {
    $log = Join-Path $logDir ($name + ".txt")
    $process = Start-Process -FilePath "powershell" -ArgumentList @("-ExecutionPolicy", "Bypass", "-File", $script) `
        -WorkingDirectory $repo -Wait -PassThru -RedirectStandardOutput $log -RedirectStandardError ($log + ".err")
    $ok = $process.ExitCode -eq 0
    if (-not $ok) { $script:failed++ }
    $script:rows.Add(("{0} {1,-34} exit {2} (log {3})" -f ($(if ($ok) { "PASS" } else { "FAIL" })), $name, $process.ExitCode, $log))
}

# ---- global checks -------------------------------------------------------
Run-Exe "validate" "-validate" "" 1
$validateLog = Get-Content (Join-Path $logDir "validate.txt") -Raw
if ($validateLog -match "xmodels:\s+ok 523, failed 0" -and $validateLog -match "xsurfaces:\s+ok 298, failed 47" -and
    $validateLog -match "materials:\s+ok 189, failed 30") {
    $script:rows.Add("PASS validate counts                    523/0, 298/47, 189/30 (known missing)")
} else {
    $script:rows.Add("FAIL validate counts differ from M5")
    $script:failed++
}
Run-Exe "roundtrip" "-roundtrip review\M7\regression\roundtrip.txt" "review\M7\regression\roundtrip.txt"
$roundtrip = Get-Content (Join-Path $outDir "roundtrip.txt") -Raw
if ($roundtrip -match "identical:\s+523" -and $roundtrip -match "different:\s+0" -and
    $roundtrip -match "errors:\s+0") {
    $script:rows.Add("PASS roundtrip                          523/523 identical")
} else {
    $script:rows.Add("FAIL roundtrip")
    $script:failed++
}

# ---- M1 ------------------------------------------------------------------
Run-Exe "m1_info_crossbow" "-info vm_crossbow_base" ""
Run-Exe "m1_info_knife" "-info wpn_h1_melee_combat_knife_vm" ""
Run-Exe "m1_crossbow_orbit" "-screenshot review\M7\regression\m1_crossbow_orbit.png -model vm_crossbow_base -yaw 30 -pitch 15" "review\M7\regression\m1_crossbow_orbit.png"
Run-Exe "m1_crossbow_bones" "-screenshot review\M7\regression\m1_crossbow_bones.png -model vm_crossbow_base -yaw 30 -pitch 15 -bones" "review\M7\regression\m1_crossbow_bones.png"
Run-Exe "m1_crossbow_wire" "-screenshot review\M7\regression\m1_crossbow_wire.png -model vm_crossbow_base -yaw 30 -pitch 15 -wire" "review\M7\regression\m1_crossbow_wire.png"
Run-Exe "m1_knife_orbit" "-screenshot review\M7\regression\m1_knife_orbit.png -model wpn_h1_melee_combat_knife_vm -yaw 30 -pitch 15" "review\M7\regression\m1_knife_orbit.png"

# ---- M2 ------------------------------------------------------------------
Run-Exe "m2_crossbow_fp" "-screenshot review\M7\regression\m2_crossbow_fp.png -weapon iw5_exocrossbow_mp -view firstperson" "review\M7\regression\m2_crossbow_fp.png"
Run-Exe "m2_crossbow_orbit" "-screenshot review\M7\regression\m2_crossbow_orbit.png -weapon iw5_exocrossbow_mp -yaw 30 -pitch 15" "review\M7\regression\m2_crossbow_orbit.png"
Run-Exe "m2_m4_fp" "-screenshot review\M7\regression\m2_m4_fp.png -weapon h2_m4_mp -view firstperson" "review\M7\regression\m2_m4_fp.png"
Run-Exe "m2_m4_orbit" "-screenshot review\M7\regression\m2_m4_orbit.png -weapon h2_m4_mp -yaw 30 -pitch 15" "review\M7\regression\m2_m4_orbit.png"
Run-Exe "m2_m4_textured_orbit" "-screenshot review\M7\regression\m2_m4_textured_orbit.png -model wpn_h2_m4a1_vm -yaw 30 -pitch 15" "review\M7\regression\m2_m4_textured_orbit.png"
Run-Exe "m2_m4_h1_checker_orbit" "-screenshot review\M7\regression\m2_m4_h1_checker_orbit.png -weapon h1_m4_mp -yaw 30 -pitch 15" "review\M7\regression\m2_m4_h1_checker_orbit.png"

# ---- M3 ------------------------------------------------------------------
Run-Exe "m3_crossbow_idle" "-screenshot review\M7\regression\m3_crossbow_idle.png -weapon iw5_exocrossbow_mp -view firstperson -anim vm_crossbow_idle -time 0" "review\M7\regression\m3_crossbow_idle.png"
Run-Exe "m3_crossbow_reload_sheet" "-screenshot review\M7\regression\m3_crossbow_reload_sheet.png -weapon iw5_exocrossbow_mp -view firstperson -anim vm_crossbow_reload -sheet 12" "review\M7\regression\m3_crossbow_reload_sheet.png"
Run-Exe "m3_crossbow_fire_sheet" "-screenshot review\M7\regression\m3_crossbow_fire_sheet.png -weapon iw5_exocrossbow_mp -view firstperson -anim vm_crossbow_fire -sheet 6" "review\M7\regression\m3_crossbow_fire_sheet.png"
Run-Exe "m3_crossbow_sprint_sheet" "-screenshot review\M7\regression\m3_crossbow_sprint_loop_sheet.png -weapon iw5_exocrossbow_mp -view firstperson -anim vm_crossbow_sprint_loop -sheet 6" "review\M7\regression\m3_crossbow_sprint_loop_sheet.png"
Run-Exe "m3_crossbow_raise_sheet" "-screenshot review\M7\regression\m3_crossbow_raise_sheet.png -weapon iw5_exocrossbow_mp -view firstperson -anim vm_crossbow_pullout -sheet 6" "review\M7\regression\m3_crossbow_raise_sheet.png"
Run-Exe "m3_crossbow_ads_up_sheet" "-screenshot review\M7\regression\m3_crossbow_ads_up_sheet.png -weapon iw5_exocrossbow_mp -view firstperson -anim vm_crossbow_ads_up -sheet 6" "review\M7\regression\m3_crossbow_ads_up_sheet.png"
Run-Exe "m3_crossbow_reload_orbit_sheet" "-screenshot review\M7\regression\m3_crossbow_reload_orbit_sheet.png -weapon iw5_exocrossbow_mp -view orbit -anim vm_crossbow_reload -sheet 8" "review\M7\regression\m3_crossbow_reload_orbit_sheet.png"
Run-Exe "m3_m4_idle" "-screenshot review\M7\regression\m3_m4_idle.png -weapon h2_m4_mp -view firstperson -anim vm_h2_m4a1_idle -time 0" "review\M7\regression\m3_m4_idle.png"
Run-Exe "m3_m4_reload_sheet" "-screenshot review\M7\regression\m3_m4_reload_sheet.png -weapon h2_m4_mp -view firstperson -anim vm_h2_m4a1_reload -sheet 12" "review\M7\regression\m3_m4_reload_sheet.png"

# ---- M3.1 ----------------------------------------------------------------
Run-Exe "m31_m4_hip" "-screenshot review\M7\regression\m31_workbench_m4_hip.png -weapon h2_m4_mp -view firstperson -anim vm_h2_m4a1_idle -time 0 -width 1920 -height 1016" "review\M7\regression\m31_workbench_m4_hip.png"
Run-Exe "m31_m4_ads" "-screenshot review\M7\regression\m31_workbench_m4_ads.png -weapon h2_m4_mp -view firstperson -ads -width 1920 -height 1016" "review\M7\regression\m31_workbench_m4_ads.png"

# ---- M4 ------------------------------------------------------------------
Run-Exe "m4_crossbow_hip" "-screenshot review\M7\regression\m4_crossbow_hip.png -weapon iw5_exocrossbow_mp -view firstperson -anim vm_crossbow_idle -time 0 -width 1920 -height 1016" "review\M7\regression\m4_crossbow_hip.png"
Run-Exe "m4_crossbow_ads" "-screenshot review\M7\regression\m4_crossbow_ads.png -weapon iw5_exocrossbow_mp -view firstperson -ads -anim vm_crossbow_idle -time 0 -crosshair -width 1920 -height 1016" "review\M7\regression\m4_crossbow_ads.png"
Run-Exe "m4_crossbow_reload_sheet" "-screenshot review\M7\regression\m4_crossbow_reload_sheet.png -weapon iw5_exocrossbow_mp -view firstperson -anim vm_crossbow_reload -sheet 6 -width 640 -height 360" "review\M7\regression\m4_crossbow_reload_sheet.png"
Run-Exe "m4_m4_reflex_hip" "-screenshot review\M7\regression\m4_m4_reflex_hip.png -weapon h2_m4_mp -view firstperson -anim vm_h2_m4a1_idle -time 0 -attach reflexmwrm4 -width 1920 -height 1016" "review\M7\regression\m4_m4_reflex_hip.png"
Run-Exe "m4_m4_reflex_ads" "-screenshot review\M7\regression\m4_m4_reflex_ads_end.png -weapon h2_m4_mp -view firstperson -ads -anim vm_h2_m4a1_idle -time 0 -attach reflexmwrm4 -crosshair -width 1920 -height 1016" "review\M7\regression\m4_m4_reflex_ads_end.png"
Run-Exe "m4_m4_acog_hip" "-screenshot review\M7\regression\m4_m4_acogmwrm4_hip.png -weapon h2_m4_mp -view firstperson -anim vm_h2_m4a1_idle -time 0 -attach acogmwrm4 -width 1920 -height 1016" "review\M7\regression\m4_m4_acogmwrm4_hip.png"
Run-Exe "m4_m4_silencer_hip" "-screenshot review\M7\regression\m4_m4_silencerlmg_hip.png -weapon h2_m4_mp -view firstperson -anim vm_h2_m4a1_idle -time 0 -attach silencerlmg -width 1920 -height 1016" "review\M7\regression\m4_m4_silencerlmg_hip.png"
Run-Exe "m4_crossbow_reflexmwr" "-screenshot review\M7\regression\m4_crossbow_reflexmwr.png -weapon iw5_exocrossbow_mp -view firstperson -pitch -18 -anim vm_crossbow_idle -time 0 -attach reflexmwr -caption `"crossbow + reflexmwr via the fitted tag_reflex`" -width 1600 -height 900" "review\M7\regression\m4_crossbow_reflexmwr.png"
Run-Exe "m4_fit_crossbow" "-fit-report review\M7\regression\m4_fit_crossbow.txt -weapon iw5_exocrossbow_mp" "review\M7\regression\m4_fit_crossbow.txt"
Run-Exe "m4_fit_m4" "-fit-report review\M7\regression\m4_fit_m4.txt -weapon h2_m4_mp" "review\M7\regression\m4_fit_m4.txt"

# ---- M4b: hands matrix + MCP smoke --------------------------------------
$hands = @("viewhands_tf141", "viewhands_udt", "viewhands_rangers", "viewhands_h2_ghillie",
           "viewhands_h1_arab_desert_mp_camo", "globalViewhands_mw2_airborne", "globalViewhands_mw2_militia")
foreach ($h in $hands) {
    Run-Exe ("m4b_hands_" + $h + "_crossbow") "-screenshot review\M7\regression\m4b_${h}_crossbow_idle.png -weapon iw5_exocrossbow_mp -hands $h -view firstperson -anim vm_crossbow_idle -time 0 -width 1280 -height 678" "review\M7\regression\m4b_${h}_crossbow_idle.png"
    Run-Exe ("m4b_hands_" + $h + "_reload") "-screenshot review\M7\regression\m4b_${h}_crossbow_reload_sheet.png -weapon iw5_exocrossbow_mp -hands $h -view firstperson -anim vm_crossbow_reload -sheet 8 -width 480 -height 270" "review\M7\regression\m4b_${h}_crossbow_reload_sheet.png"
    Run-Exe ("m4b_hands_" + $h + "_m4") "-screenshot review\M7\regression\m4b_${h}_m4_idle.png -weapon h2_m4_mp -hands $h -view firstperson -anim vm_h2_m4a1_idle -time 0 -width 1280 -height 678" "review\M7\regression\m4b_${h}_m4_idle.png"
}
Run-Script "m4b_mcp_smoke" (Join-Path $repo "tools\test\mcp-smoke.ps1")

# ---- M5 ------------------------------------------------------------------
Run-Exe "m5_fit_after" "-fit-report review\M7\regression\m5_fit_crossbow_after.txt -weapon iw5_exocrossbow_mp" "review\M7\regression\m5_fit_crossbow_after.txt"
Run-Exe "m5_reflex_ads_end" "-screenshot review\M7\regression\m5_crossbow_reflex_ads_end.png -weapon iw5_exocrossbow_mp -view firstperson -ads -anim vm_crossbow_idle -time 0 -attach reflexmwr -no-ads-align -crosshair -hidesurface 0,1,3,18,19 -width 1920 -height 1016" "review\M7\regression\m5_crossbow_reflex_ads_end.png"
Run-Exe "m5_acog_ads_end" "-screenshot review\M7\regression\m5_crossbow_acog_ads_end.png -weapon iw5_exocrossbow_mp -view firstperson -ads -anim vm_crossbow_idle -time 0 -attach acogmwr -no-ads-align -crosshair -width 1920 -height 1016" "review\M7\regression\m5_crossbow_acog_ads_end.png"
Run-Exe "m5_reflex_hip" "-screenshot review\M7\regression\m5_crossbow_reflex_hip.png -weapon iw5_exocrossbow_mp -view firstperson -anim vm_crossbow_idle -time 0 -attach reflexmwr -width 1920 -height 1016" "review\M7\regression\m5_crossbow_reflex_hip.png"
Run-Exe "m5_silencer_side" "-screenshot review\M7\regression\m5_crossbow_silencer_side.png -weapon iw5_exocrossbow_mp -view orbit -anim vm_crossbow_idle -time 0 -attach silencermwrxmlar -yaw 90 -pitch 0 -dist 32 -width 1920 -height 1016" "review\M7\regression\m5_crossbow_silencer_side.png"
Run-Exe "m5_silencer_top" "-screenshot review\M7\regression\m5_crossbow_silencer_top.png -weapon iw5_exocrossbow_mp -view orbit -anim vm_crossbow_idle -time 0 -attach silencermwrxmlar -yaw 0 -pitch 75 -dist 45 -width 1920 -height 1016" "review\M7\regression\m5_crossbow_silencer_top.png"
Run-Exe "m5_reload_sheet" "-screenshot review\M7\regression\m5_crossbow_reload_sheet.png -weapon iw5_exocrossbow_mp -view firstperson -anim vm_crossbow_reload -sheet 8 -width 480 -height 270" "review\M7\regression\m5_crossbow_reload_sheet.png"
Run-Exe "m5_fire_sheet" "-screenshot review\M7\regression\m5_crossbow_fire_sheet.png -weapon iw5_exocrossbow_mp -view firstperson -anim vm_crossbow_fire -sheet 6 -width 480 -height 270" "review\M7\regression\m5_crossbow_fire_sheet.png"
Run-Script "m5_mcp_edit" (Join-Path $repo "tools\test\m5-edit.ps1")
Run-Script "m5_mcp_copy_save" (Join-Path $repo "tools\test\m5-mcp-copy.ps1")

# ---- M6 ------------------------------------------------------------------
Run-Exe "m6_m4_christmas_stock" "-screenshot review\M7\regression\m6_camo_m4_christmas_wrap_stock.png -model wpn_h2_m4a1_vm -view orbit -yaw 200 -pitch 10 -dist 24 -exposure 1.5 -camo camo_h1_christmas_wrap -width 1600 -height 900" "review\M7\regression\m6_camo_m4_christmas_wrap_stock.png"
Run-Exe "m6_m4_blue_jade_stock" "-screenshot review\M7\regression\m6_camo_m4_blue_jade_stock.png -model wpn_h2_m4a1_vm -view orbit -yaw 200 -pitch 10 -dist 24 -exposure 1.5 -camo camo_h1_blue_jade -width 1600 -height 900" "review\M7\regression\m6_camo_m4_blue_jade_stock.png"
Run-Exe "m6_m4_red_wash_stock" "-screenshot review\M7\regression\m6_camo_m4_red_wash_stock.png -model wpn_h2_m4a1_vm -view orbit -yaw 200 -pitch 10 -dist 24 -exposure 1.5 -camo camo_h1_red_wash -width 1600 -height 900" "review\M7\regression\m6_camo_m4_red_wash_stock.png"
Run-Exe "m6_crossbow_christmas" "-screenshot review\M7\regression\m6_camo_crossbow_christmas_wrap.png -model vm_crossbow_base -view orbit -yaw 35 -pitch 18 -dist 26 -exposure 1.4 -camo camo_h1_christmas_wrap -width 1600 -height 900" "review\M7\regression\m6_camo_crossbow_christmas_wrap.png"
Run-Exe "m6_crossbow_blue_jade" "-screenshot review\M7\regression\m6_camo_crossbow_blue_jade.png -model vm_crossbow_base -view orbit -yaw 35 -pitch 18 -dist 26 -exposure 1.4 -camo camo_h1_blue_jade -width 1600 -height 900" "review\M7\regression\m6_camo_crossbow_blue_jade.png"
Run-Exe "m6_crossbow_red_wash" "-screenshot review\M7\regression\m6_camo_crossbow_red_wash.png -model vm_crossbow_base -view orbit -yaw 35 -pitch 18 -dist 26 -exposure 1.4 -camo camo_h1_red_wash -width 1600 -height 900" "review\M7\regression\m6_camo_crossbow_red_wash.png"
Run-Exe "m6_m4_fp" "-screenshot review\M7\regression\m6_camo_m4_fp_christmas_wrap.png -weapon h2_m4_mp -view firstperson -camo camo_h1_christmas_wrap -exposure 1.2 -width 1600 -height 900" "review\M7\regression\m6_camo_m4_fp_christmas_wrap.png"
Run-Exe "m6_nocamo_m4" "-screenshot review\M7\regression\m6_nocamo_m4_model.png -model wpn_h2_m4a1_vm -view orbit -yaw 35 -pitch 18 -dist 30 -exposure 1.4 -width 1600 -height 900" "review\M7\regression\m6_nocamo_m4_model.png"
Run-Script "m6_mcp_camo" (Join-Path $repo "tools\test\m6-camo.ps1")

# ---- window app test -----------------------------------------------------
Run-Script "window_test" (Join-Path $repo "tools\test\m7-window.ps1")
$windowLog = Get-Content (Join-Path $repo "review\M7\window-test.txt") -Raw
if ($windowLog -match "result: 5 passed, 0 failed") { $script:rows.Add("PASS window app test                  5/5") }
else { $script:rows.Add("FAIL window app test"); $script:failed++ }

$summary = @("M7 regression run",
             ("date: " + (Get-Date -Format "yyyy-MM-dd HH:mm:ss")),
             ("exe: " + $exe),
             "") + $script:rows + @("", "result: $($script:rows.Count - $script:failed) passed, $($script:failed) failed")
$summary | Set-Content (Join-Path $review "regression.txt") -Encoding UTF8
Write-Output ("wrote " + (Join-Path $review "regression.txt"))
if ($script:failed -gt 0) { exit 1 }
