# Finish task progress log

Task: `docs\FINISH-TASK.md` (25 Nov 2026). One commit per item; this file is updated with each
item's result and is the resume point after a context reset.

## Status

| item | state | commit | notes |
|---|---|---|---|
| 1 ADS auto-align off by default | **done** | `343cc96` | M4 reflex after: 22.1 px off (reported) |
| 2 player model data | **done** | `64b227e` | all 14 factions textured with head; head attach bug fixed |
| 3 camo fidelity | **done** | `909a89e` | full colour/normal/spec/env/decal technique + native H2 variants |
| 4 camos in game: test commands | **done** | `5a0689d` | `docs\porting\aw.md` "In-game test (camos)" |
| 5 comfort gaps | **done** | item 5 commit (hash in final log) | live split viewport + material/image/new-root hot reload |

Extra (not in the task list): red-dot comparison on the crossbow, commit `41348f9`
(`review: red dot comparison on the crossbow`), files in `review\reddots\`; 4 of 5 red dots done,
`variant` is BLOCKED (missing `vm_h1_optic_reflex_variant_ads_mp_lodvm100.xsb`).

## Item 1

- `adsAutoAlign_` default false (Claude's source change); CLI `-ads-align` turns it on for debug,
  `-no-ads-align` is accepted as a no-op. UI checkbox starts unchecked and is labelled
  "debug: snap ADS to reticle (hides misalignment)". README and workflow-porting updated; the
  workflow no longer tells reviewers to pass `-no-ads-align`.
- Re-measured with auto-align **OFF** (1920x1016, reticle aim from `-posedump`; before = auto-align
  ON, after = new default OFF):
  - H2 M4 + `reflexmwrm4`: before **(960.0, 508.0)** -> after **(957.9, 486.7)** = 2.1 px left,
    21.3 px high, **22.1 px total - over the 3 px limit**.
  - H2 M4 + `acogmwrm4`: before **(960.0, 508.0)** -> after **(958.2, 526.0)** = 1.8 px left,
    18.0 px low, 18.1 px total - over 3 px.
  - Crossbow + HMW `reflex` (`vm_crossbow_reddot_ads_up`): before/after **(960.0, 508.0)**, 0 px.
  - Crossbow + HMW `acog` (`vm_crossbow_acog_ads_up`): before/after **(960.0, 508.0)**, 0 px.
  - Crossbow via the old M5 `reflexmwr` name: the current weapon json's `attachments` list is
    `["acog","reflex"]`, so `reflexmwr` no longer selects an override (base `vm_crossbow_ads_up`)
    and the H1 reticle is at (960.0, 376.4), 131.6 px high. This is a name mapping consequence,
    not an auto-align result: use `reflex`/`acog` for the current port. `acogmwr` similarly:
    (960.0, 527.3), 19.3 px low, because only the base ADS anim applies.
- Review images: `review\finish\m4_reflex_{on,off}.png`, `m4_acog_{on,off}.png`,
  `crossbow_{reflex,acog}_off.png`, `crossbow_{reflexmwr,acogmwr}_off.png`; pose dumps alongside.
- Build 0 warnings.

## Item 2

Status: **done**. All dumps are new `GAME\dump\workbench_*` folders; no existing dump, `.ff`, `.pak`
or zone was modified.

Dumps and sizes (full commands in `docs\assets.md`):

| folder | contents | size |
|---|---|---|
| `workbench_hmw_rangers` | HMW body_h2_* / head_h2_* bodies, heads, materials, techsets, streamed images | 1,072.2 MB / 2,230 files |
| `workbench_hmw_dlc_factions` | H1 port `mp_body_*` + `head_*` bodies/heads and materials/images | 854.7 MB / 3,340 files |
| `workbench_hmw_patch_common_mp_xsurface` | xsurface-only dump of `hmw_patch_common_mp` | 33.8 MB / 185 files |
| `workbench_hmw_ar1\xsurface` | the 5 `usmc_desert_assault_body_mp_lod{0..4}300.xsb` files | +7.4 MB / 5 files |

Roots added to `settings.cpp` after `workbench_hmw_post_gfx`: `workbench_hmw_rangers`,
`workbench_hmw_dlc_factions`, `workbench_hmw_patch_common_mp_xsurface` (docs\assets.md updated).
Free disk during the dumps: 30.8 GB (limit 20 GB).

Streamed images: `hmw_rangers` is special. Its colour/normal maps are in `hmw_rangers.pak`, and
zonetool only reads that pak when both the `.ff` and `.pak` are hardlinked into the dump source
folder. Without the pak it writes 141-byte `*.h1Image` metadata stubs and `body_h2_airborne_assault`
renders as the missing-texture checker. With both linked, the `.pixels` streams are written and the
body is textured.

Head attachment (the previously untested code) had two real bugs:

1. All H1/H2 head models are authored on the body skeleton with root `j_spine4`, not `j_head`.
   The old code attached the head root at body `j_head` (and applied the spine pose), so the head
   was displaced far above/in front of the body. The head palette already maps the head bones onto
   the body pose, so the head world matrix must stay identity when the head root exists in the body
   pose. The neck/head-tag fallback is kept for heads that are not on the body skeleton.
2. `BuildBonePalette` used an identity skin for bones without a pose channel, which left their
   vertices at the bind origin. The H2 scarf/face surfaces stretched from the head down to the floor.
   Unmapped bones now inherit the nearest animated ancestor's skin.

Verification: `tools\test\finish-factions.ps1` renders all 14 faction entries, writes
`review\finish\factions\report.txt` and the contact sheet; every line reports
`head attach: bone j_spine4 | root j_spine4`. Screenshots: `review\finish\factions\<id>.png` and
`factions_sheet.png`. The H2 M4 / crossbow ADS renders are byte-identical before/after the palette
fix (SHA-256 checked on `reflex_ads.png`), so the viewmodel path did not regress.

Still missing / not covered:

- The table only lists the assault variant per faction. The SMG/LMG/shotgun bodies and heads are in
  the same dump folders but are not separate table entries.
- `usmc_desert` is a single H1 body with the head included; `hazmat`/`infect` are full-body HMW
  models, also with the head included. They have no separate head model to attach.
- No anim is loaded for the faction screenshots; they are bind-pose renders against the body
  skeleton. The 14 table entries are all textured and all heads are present.

## Item 3

Status: **done**. The camo preview now uses the full material technique instead of only the
diffuse swap:

- Camo colour: design `dc1` (typeHash `659983135`).
- Camo normal: design `dn1` (`1507003663`), fallback `dna1`/`camo_nml` (`1518176625`) and the
  universal `camo_nml` image; blended over the base normal.
- Camo specular: slot `3426233914` (`~camo_spcmaps_01...`), fallback design `ds1` (`887934131`);
  replaces the base specular mask.
- Environment: env BRDF LUT `3447584578` (`~envbrdflut_ggx_16-rg`), read as `R16G16_FLOAT`
  (the dump stores `R16G16_TYPELESS`), sampled at `(NdotV, 0.5)`, scaled by `envMapParms.x`
  (0.04), with a neutral sky gradient because the dumps have no cubemap.
- `decalParams1` scale: parsed from the material (`2,2` for the H2 M4 receiver and the crossbow);
  the design/normal/spec are sampled at `uv * decalParams1 * userScale`. The UI/CLI scale is now
  a multiplier (default 1) on the material value; the mask stays at the base UV.
- H2 native variants are exposed as `native:<material stem>`. `native:mtl_h2_m4a1_receiver_fire`
  and `native:mtl_h2_m4a1_receiver_doomsday` replace only the receiver materials
  (`mtl_h2_m4a1_receiver[_camo]`) and use their static design `dc1` plus emissive map
  (typeHash `878789447`); the fire/doomsday receiver skin does not leak onto the stock/handguard.
  Their camo specular slot has no pixel data in the dumps; the white fallback is used.

No-camo pixel identity: the pre-change exe and the new exe render byte-identical images with no
camo selected. SHA-256 (in `review\finish\camos\camo_check.txt`):

- crossbow model orbit: `6C48E205...E56D` before and after.
- H2 M4 first person: `F864BD9A...A1D5` before and after.

Review files (1600x900, before = pre-change diffuse-only exe, after = full technique):

```
review\finish\camos\m4_<christmas_wrap|blue_jade|red_wash>_{before,after}.png
review\finish\camos\crossbow_<christmas_wrap|blue_jade|red_wash>_{before,after}.png
review\finish\camos\m4_native_fire.png
review\finish\camos\m4_native_doomsday.png
review\finish\camos\camos_m4_before_after_sheet.png
review\finish\camos\camos_crossbow_before_after_sheet.png
review\finish\camos\camos_native_sheet.png
review\finish\camos\camo_check.txt
```

`docs\camos.md` section 4 is the living description of the technique; `docs\mcp.md` documents
`camo:{name,texture,native,strength,warnings}` and the `native:` names. Build 0 warnings.

## Item 4

Status: **done**. The commands are in `docs\porting\aw.md`, section "In-game test (camos)".
The camo is a weapon-name suffix: HMW resolves the class camo name through `mp/camoTable.csv`,
turns it into a number and `buildweaponnamecamo` appends `_camoNN` (`_camo0N` below 10).
Sources: `custom_weapons.gsc:756,758,764,766,1030,1038,1136,1149` and
`refs\iw5-scripts\maps\mp\gametypes\_class.gsc:1491-1503`.

Indices used (camoTable column 0, `camotable.csv:43,46,85`):

| camo | row | index | suffix |
|---|---|---:|---|
| red wash | `camo043` | 42 | `_camo42` |
| blue jade | `camo046` | 45 | `_camo45` |
| christmas wrap | `camo085` | 84 | `_camo84` |

Commands documented:

```text
give h2_m4_mp_camo42 / _camo45 / _camo84
give iw5_exocrossbow_mp_camo42 / _camo45 / _camo84
set sas_weapon iw5_exocrossbow_mp_camo42 / _camo45 / _camo84
```

plus the GSC `buildweaponname("h2_m4"|"iw5_exocrossbow", "none", "none", <index>, [])` +
`_giveweapon` equivalent.

Crossbow: its material is camo-ready (`mtl_crossbow_base` has `dc1`, `c`, `m`,
`dna1 = camo_nml`, the `~camo_spcmaps_01...` slot and the env LUT; `crossbow_mask_02` leaves the
rails/limbs untouched). The likely blockers are the name/variant path, not the material: the mod
zone has no `iw5_exocrossbow_mp_camo*` assets and `GAME\zone` has no `mp_vm_exocrossbow_*_tr.ff`
camo transient zones; and HMW's `isvalidcamo` replacement (`custom_weapons.gsc:835-889`) allows
`camo016..camo062` but not `camo085`, so Christmas is rejected on the class path (red/blue pass).
Live `give` bypasses the class check but still needs the suffix asset. `iscamounlocked` can also
reject if the crossbow has no weapon-class row in the rank table. The engine's
`overrideviewmodelmaterial`/`setviewmodelmaterialscriptparam` builtins are not called by any
shipped script and are not console commands; using them is an untested GSC experiment.

## Item 5

Status: **done**.

### Live split viewport

- `Application` now owns a second `commands::Workbench` (same settings/assets/renderer/roots).
- `DrawViewportPanel` renders A and B into the two halves of the viewport, copies each scene
  target into a persistent SRV (`Renderer::CaptureSceneTo`/`CompareTexture`) and displays them
  with a 2 px divider. Input still drives A; B's camera is copied from A every frame.
- `SyncCompareState` copies yaw/pitch/distance/fov/target (and first-person eye/target) and
  normalizes the animation time: `timeB = timeA / durationA * durationB`, so different idle
  lengths stay in sync. The selected camo is mirrored too.
- UI: Inspector -> *Live compare* (checkbox, B asset field, Load/Clear). CLI:
  `-live-compare <b>` opens the window with the split already loaded.
- Review: `review\finish\live_compare_viewport.png` (A `wpn_h2_m4a1_vm`, B `vm_crossbow_base`,
  loaded from `-model wpn_h2_m4a1_vm -live-compare vm_crossbow_base`).

### Hot reload

- `RefreshWatchedFiles` now also watches every loaded material json and every image/pixel stream
  the materials reference (`<name>.h1Image` plus `_stream0..3.pixels`), for the gun, hands,
  attachments, standalone model, character body/head/weapon and the selected camo. Material or
  image changes reload/re-upload the current asset.
- `RefreshRootDirWatch` records the mtimes of each root and its `materials`, `materials/m`,
  `images`, `streamed_images`, `weapons`, `xmodel`, `xsurface`, `xanim` folders. `PollHotReload`
  scans them once per second; a new file sets the debounced pending state, then
  `assets_.Scan(roots_)` refreshes the asset index, the attachment list is rebuilt and the
  current asset is reloaded. The window also notices the changed index count and rebuilds the
  asset filter.
- Test: `tools\test\finish-hot-reload.ps1` on a copy of the crossbow in `build\hot_reload_test`
  (never the real files). 4/4 passed, log `review\finish\hot-reload.txt`:
  - watcher lists the material json + image + pixel stream;
  - `decalParams1` 2,2 -> 4,4 reloaded and changed the in-memory screenshot hash;
  - zeroing the `crossbow_col` pixel streams reloaded and changed the screenshot hash;
  - adding a new weapon json increased the asset index 3795 -> 3796 after the debounce.
- Review screenshots: `review\finish\hot_reload_material_{before,after}.png`,
  `review\finish\hot_reload_image_after.png` (the last one is deliberately a zeroed texture; it
  is evidence of the image reload, not a normal render).

Build 0 warnings.
