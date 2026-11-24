# Porting workflow: checking a new weapon in hmw-workbench

This is the step-by-step check for a weapon that has just been ported to HMW/MWR. The AW crossbow
(`iw5_exocrossbow_mp` + `vm_crossbow_base`) is the worked example; it has the same problems a
fresh port usually has (AW tag names where HMW attachments expect `tag_reflex` etc.). Commands
are run from `C:\Games\cod-hwm\hmw-workbench`; `hmw-workbench.exe` is
`build\bin\Release\hmw-workbench.exe`.

If a step fails, fix it in the port (xmodel/materials/anim json) and repeat the step. The
workbench is read-only until step 6; only step 6 writes, and only with a `.bak`.

## 0. Build and scan

```bat
tools\build\build.bat
hmw-workbench.exe -validate
```

`-validate` parses every `.xmb` in all asset roots plus its surface set and material json. It
exits 1 when known assets are missing; the current dumps have **523 xmodels ok / 0 failed, 298
surface sets ok / 47 missing, 189 materials ok / 30 missing**. Those 77 missing assets are known
(mostly AW crossbow materials and HMW streamed images); check that your new weapon did not add a
new `FAIL`. Also check `workbench.json` contains the dump root where the weapon lives (README
"Asset roots").

## 1. Load the weapon and inspect it

```bat
hmw-workbench.exe -info <weapon>_mp
hmw-workbench.exe -info <gun_model>
hmw-workbench.exe -screenshot review\check\<weapon>_orbit.png -weapon <weapon>_mp -yaw 30 -pitch 15
hmw-workbench.exe -screenshot review\check\<weapon>_fp.png -weapon <weapon>_mp -view firstperson
```

Check in `-info`:

- `gunModel`, `worldModel`, `handModel` are what you expect; the gun model exists in a root and
  its LOD0 surface set is printed (`lod0 surfaces:` / `xsurface:`);
- the bone/tag list contains the tags HMW attachments need (`tag_weapon`, `tag_clip`,
  `tag_flash`, `tag_reflex`, `tag_acog`, ...), not just the source game's names;
- every surface has a material and the material json is found. Missing materials render as the
  checker and are listed as warnings when the model loads.

The crossbow example: `-info iw5_exocrossbow_mp` shows `gunModel vm_crossbow_base`, `handModel
viewhands_tf141`; `-info vm_crossbow_base` lists the AW tags (`tag_red_dot`, `tag_acog_2`,
`tag_scope`, `tag_flash`, ...) and no `tag_reflex` before step 5.

## 2. Check the animation states

```bat
hmw-workbench.exe -info <idle_anim>
hmw-workbench.exe -screenshot review\check\<weapon>_reload_sheet.png -weapon <weapon>_mp ^
                  -view firstperson -anim <reload_anim> -sheet 8 -width 480 -height 270
```

The weapon json's `szXAnims` states must point at anims that exist and target the gun/hands
bone names. Look at idle, fire, reload, raise/pullout, sprint loop and ADS up. `-info` prints the
frame count, fps, notetracks and the channel coverage (which anim bones match the rig); a state
whose anim is missing is reported when it is played. `-sheet` renders several frames of one anim
in a grid, which makes a broken pose obvious.

Crossbow: `vm_crossbow_idle`, `vm_crossbow_fire`, `vm_crossbow_reload` (86 frames, 7 notetracks)
and `vm_crossbow_ads_up` all play; `animOverrides` map the AW optic ADS anims to HMW's states
(`docs/milestones/M3.md`).

## 3. Check every faction's hands

```bat
hmw-workbench.exe -screenshot review\check\hands_<hands>_<weapon>_idle.png -weapon <weapon>_mp ^
                  -hands <hands> -view firstperson -anim <idle_anim> -time 0 -width 1280 -height 678
```

Repeat for the HMW viewhands models in the dumps: `viewhands_tf141`, `viewhands_udt`,
`viewhands_rangers`, `viewhands_h2_ghillie`, `viewhands_h1_arab_desert_mp_camo`,
`globalViewhands_mw2_airborne`, `globalViewhands_mw2_militia` (see `review\M4b\`). The gun joins
the hands on the weapon's `tag_weapon`/`j_gun` rule; a wrong hand model or missing viewhands
shows as no/misplaced arms. If a hand model is not in any dump yet, dump it with zonetool into a
new folder under `GAME\dump\` and add that folder to `workbench.json` (docs/assets.md).

## 4. Fit report: which tags do the HMW attachments need?

```bat
hmw-workbench.exe -fit-report review\check\fit.txt -weapon <weapon>_mp
```

The report lists every attachment asset with its view model, the **required tag** (the root bone
of the attachment view model), `found`/`missing`, and the nearest weapon tags for a missing one.
The crossbow's report before M5:

```
reflexmwr   view model attach_h1_optic_reflex_vm   required tag_reflex   MISSING
            nearest: tag_red_dot, tag_eotech, ...
acogmwr     ...                                    required tag_acog     MISSING
```

Filters: add `-attachments "reflexmwr,acogmwr"`? The CLI takes no filter; the MCP
`attachment_fit_report` has an `attachments` array (or use the UI attachment list, which shows
the same catalog). A missing tag is not fatal: the attachment still mounts in red at the weapon
origin so a screenshot shows where the gap is.

## 5. Add or move the tags

In the UI (Inspector → **Tags (edit + save)**): select the tag, type local position/quaternion or
use the ImGuizmo move/rotate handles; the ghost shows the original. **Add tag** takes a name from
the fit-needed dropdown, a parent (normally `j_gun`), and a start transform copied from an
existing tag; new bones are appended so parents stay before children and every per-bone array is
updated.

Headless or scripted (same command layer; MCP tools are `add_tag`, `move_tag`, `revert_tags`,
`save_model`):

```bat
hmw-workbench.exe -fit-crossbow -weapon iw5_exocrossbow_mp -hands viewhands_tf141
```

`-fit-crossbow` is the M5 review helper: it adds `tag_reflex` (copy of `tag_red_dot`),
`tag_acog` (copy of `tag_acog_2`) and `tag_silencer` (exactly `tag_flash`), then moves each optic
tag in its parent space until the mounted reticle is on the camera axis at the end of ADS, and
saves. For a normal port do the same steps manually or with a small MCP script; keep the numbers
in `<model>.tags.json`.

### ADS check

```bat
hmw-workbench.exe -screenshot review\check\<weapon>_reflex_ads.png -weapon <weapon>_mp ^
                  -view firstperson -ads -anim <idle_anim> -time 0 -attach reflexmwr ^
                  -no-ads-align -crosshair -width 1920 -height 1016
hmw-workbench.exe -posedump review\check\pose.txt -weapon <weapon>_mp -view firstperson -ads ^
                  -anim <idle_anim> -time 0 -attach reflexmwr -no-ads-align -width 1920 -height 1016
```

Use `-no-ads-align` when you are testing the tag itself (the workbench otherwise auto-aligns the
reticle to hide small tag errors). The crosshair marks the image centre; `-posedump` prints the
reticle/tag screen position so you can write down the offset. The M5 crossbow result: reflex aim
**(960.0, 508.0)** at 1920x1016 with `vm_crossbow_reddot_ads_up`, ACOG the same with
`vm_crossbow_acog_ads_up`, i.e. 0 px off centre. The built-in `tag_sight_on` ring still occludes
the reflex lens in game; that is a `hideTags` weapon-json change, not a tag fit issue.

If an optic is off:

1. `-fit-report` to confirm which tag is used, and `-info <optic vm>` to see its root bone
   (that bone is the required weapon tag);
2. move the weapon tag (local to its parent) until the reticle is centred; prefer moving the
   weapon tag over editing the optic model;
3. add different tags instead of renaming one when two optic types need different positions
   (`tag_reflex` vs `tag_acog`).

## 6. Save (in place, with backup)

UI **Save model**, or MCP `save_model`. The first save:

- copies the original to `<model>.xmb.bak` **only if no `.bak` exists** (never overwrites an
  existing backup);
- writes the model in place;
- writes `<model>.tags.json` next to it (name, parent, local pos/rot, world pos, `added`);
- re-reads the saved file and runs the same checks as `-validate` on it.

For a new port, check the re-read/validation line in the UI or the MCP result. To test on a copy
instead of the real file, see `tools\test\m5-mcp-copy.ps1` (it builds `build\edit_test\` with a
local root and only touches the copy).

## 7. Build the zone and test in game

The workbench does not build zones or modify `.ff`/`.pak`. After the save:

1. rebuild the port zone with zonetool (the same command that produced `GAME\zonetool\mod`; see
   docs/porting/aw.md for the crossbow);
2. start the game and check: the weapon renders with its materials, the hands are correct, the
   attachment mounts on the new/moved tag, ADS puts the reticle where the workbench predicted,
   and the idle/fire/reload animations still play. For camos, compare
   `camo_h1_christmas_wrap`, `camo_h1_blue_jade` and `camo_h1_red_wash` (docs/camos.md);
3. if the reticle is right in the workbench but not in game, the difference is usually the
   game's viewmodel offset/FOV or an attachment-specific ADS anim, not the tag; check the
   matching in-game screenshot against `-screenshot` with the same cg_fov.

## Crossbow checklist (the full worked example)

```bat
tools\build\build.bat
hmw-workbench.exe -info iw5_exocrossbow_mp
hmw-workbench.exe -info vm_crossbow_base
hmw-workbench.exe -info vm_crossbow_reload
hmw-workbench.exe -screenshot review\M3\crossbow_reload_sheet.png -weapon iw5_exocrossbow_mp ^
                  -view firstperson -anim vm_crossbow_reload -sheet 12
hmw-workbench.exe -screenshot review\M2\crossbow_fp.png -weapon iw5_exocrossbow_mp -view firstperson
hmw-workbench.exe -fit-report review\M5\fit_crossbow_after.txt -weapon iw5_exocrossbow_mp
hmw-workbench.exe -screenshot review\M5\crossbow_reflex_ads_end.png -weapon iw5_exocrossbow_mp ^
                  -view firstperson -ads -anim vm_crossbow_idle -time 0 -attach reflexmwr ^
                  -no-ads-align -crosshair -hidesurface 0,1,3,18,19 -width 1920 -height 1016
hmw-workbench.exe -screenshot review\M5\crossbow_acog_ads_end.png -weapon iw5_exocrossbow_mp ^
                  -view firstperson -ads -anim vm_crossbow_idle -time 0 -attach acogmwr ^
                  -no-ads-align -crosshair -width 1920 -height 1016
```

Expected: `tag_reflex`, `tag_acog`, `tag_silencer` found; both reticles at (960, 508) at
1920x1016; `vm_crossbow_base.xmb.bak` is the 4911-byte original; `tags.json` lists the three
added tags. Then build the zone and check in game; the ring occlusion is the known `hideTags`
item.
