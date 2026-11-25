# Crossbow optics (HMW reflex + ACOG)

Task: `docs\CROSSBOW-OPTICS-TASK.md` (Claude, 25 Nov 2026). Goal: the crossbow takes HMW's
MW2-style `reflex` and `acog`, reticles centred at ADS end, built-in ring hidden when an optic is
mounted, and the `mod` zone rebuilt. No suppressor.

## 1. Workbench fixes (commit `crossbow optics: workbench fixes`)

- **Attachment lookup** (`src\assets\attachment.*`, `src\app\commands.cpp`):
  `FindAttachmentFile` now scans all roots and prefers a definition with `viewModels`/`worldModels`
  over a stub (the `workbench_hmw_ar1\attachments\reflex.json`/`acog.json` stubs no longer
  shadow the real `workbench_hmw_attachments` definitions). `FindAttachmentDefinitions` and
  `AttachmentHasModels` are shared with `-validate`, which now prints one `DUPLICATE attachment`
  block per duplicate name with `models`/`stub` per path. Current dumps: 104 attachment names,
  89 names with several definitions; the real definition wins.
- **Anim overrides** (`src\app\commands.cpp`):
  - `StateForAnimTreeType` uses the H1 `weapAnimFiles_t` values: **ADS_UP = 129, ADS_DOWN = 130**
    (`cod-mwx\src\client\game\database.hpp:4651`) instead of the AW 128/129 shift.
  - `OverrideAppliesToAttachment` resolves `animOverrides[].attachment1/2` through the weapon
    json's `attachments` array **1-based** (0 = none); the old code also tried a 0-based index,
    which made acog/reflex match each other. The AW optic index map
    (`OpticAttachmentIndexForTag`) remains only as the fallback for weapons without an
    `attachments` list.
  - The HMW acog attachment's reticle attach bone is `tag_reticle_acog`; the reticle code now
    falls back to it (then any `tag_reticle*`) after `tag_reticle_attach`.
- **Bone rename** (`src\assets\xmodel_document.*`, `Workbench::RenameTag`, MCP `rename_tag`):
  finds the bone, rejects an existing name (case-insensitive), rewrites the bone name and its
  string item as an inline string, keeps the stored `scr_string_t` placeholder (x64-zt pairs
  each element with the string and regenerates the script string at build time), marks the
  document dirty, updates the scene. `HasTagChanges` now also detects a name change; reverting a
  renamed original bone restores its name.
- **`fit_tag`** MCP helper (`Workbench::FitTagToReticle`): attaches the attachment, disables
  auto-align, plays the ADS_UP override and moves the tag in its parent space until the reticle
  aim is on the camera axis; returns the local/world position, delta, screen position and
  iterations. Used for the tuning below.
- Test scripts: `tools\test\crossbow-optics-tune.ps1` (copy tuning), `-apply.ps1` (real models),
  `-rename.ps1` (rename round-trip on a copy), `rebuild-mod-zone.ps1` (zone build/install).

## 2. Crossbow models (allowed outside-repo writes)

Backups (never overwritten):
```
GAME\zonetool\mod\xmodel\vm_crossbow_base.xmb.bak    4911 B (original, 23 Nov 18:40) unchanged
GAME\zonetool\mod\xmodel\npc_crossbow_base.xmb.bak   3037 B (original, created by save_model)
```
Before-files kept in the repo: `review\crossbow-optics\vm_crossbow_base_before.xmb` (5208 B),
`npc_crossbow_base_before.xmb` (3037 B), `vm_crossbow_base_before.tags.json`.

`vm_crossbow_base.xmb` (39 bones, 20 surfaces):
- renamed `tag_sight_on` -> `tag_sight_front_on` (no crossbow xanim and no field of the weapon
  json uses `tag_sight_on`; checked again, only the model/tags files contain it).
- `tag_reflex` final local **(3.242754, 0.000000, 6.226662)**, parent `j_gun`; source
  `tag_red_dot` local (3.242754, 0, 5.735756), delta **(0, 0, +0.490906)**.
- `tag_acog` final local **(3.377740, 0.000000, 6.027324)**, parent `j_gun`; source
  `tag_acog_2` (3.377740, 0, 6.027328), delta **(0, 0, -0.000004)**.
- `tag_silencer` unchanged/unused (no suppressor).

`npc_crossbow_base.xmb` (17 -> 19 bones, 4 surfaces), the world model:
- renamed `tag_sight_on` -> `tag_sight_front_on`;
- added `tag_reflex` parent `tag_weapon` at **(-7.771000, 0.000000, 2.791906)** = its
  `tag_red_dot` (-7.771492, 0, 2.301450) + 0.490906 Z;
- added `tag_acog` parent `tag_weapon` at **(-7.197000, 0.000000, 2.330996)** = its
  `tag_acog_2` (-7.197178, 0, 2.331469) - 0.000004 Z.
- `vm_crossbow_base.tags.json`/`npc_crossbow_base.tags.json` written by the save.

### Measurements (weapon json path, `-no-ads-align -crosshair`, 1920x1016)

| optic | ADS anim | eye/camera | reticle aim screen |
|---|---|---|---|
| reflex (`attach_h2_reflex_vm`) | `vm_crossbow_reddot_ads_up` (override index 2) | FP, cg_fov 50 | **(960.0, 508.0)** |
| acog (`attach_h2_acog_vm`) | `vm_crossbow_acog_ads_up` (override index 1) | FP, cg_fov 50 | **(960.0, 508.0)** |

Screen centre is (960, 508): **0 px off** for both. The built-in ring surfaces are rigid to the
renamed `tag_sight_front_on`; the HMW attachments hide that tag, so the ring is gone in ADS (see
`reflex_ads_end.png`/`acog_ads_end.png`). No-optic ADS still uses the AW `vm_crossbow_ads_up`
and its built-in sight is 125 px low (known, not part of this task; screenshot included).

World fit: `npc_crossbow_base` now has `tag_reflex`/`tag_acog`, so the HMW `reflex`/`acog` world
models mount (screenshots `world_reflex.png`, `world_acog.png`; fit reports in
`fit_crossbow.txt` show `found`).

## 3. Zone rebuild

Read `cod-mwx\docs\mods.md`, `aw-weapon-porting.md` and `tools\aw-weapon-port\README.md` (read-only)
and rebuilt with the H1 x64-zt (`GAME\x64zt.exe`, `-unbuffered-io`, working dir `GAME`,
`buildzone mod`).

- Backups: `zone\mod.ff.bak-20261125-070627` (19,881,966 B old) and
  `mods\sticksandstones\mod.ff.bak-20261125-070627`.
- Result: **`zone\mod.ff` 19,877,701 B**, SHA256 `E6332954...F2000`, installed as
  `mods\sticksandstones\mod.ff` (same hash).
- Build log: `review\crossbow-optics\build.log`; **0 FATAL, 0 ERROR**, warnings only for the 11
  known missing AW crossbow images (scope/glass/target/emblem), unchanged from before.
- `zone_source\mod.csv` was **not** changed: the build succeeded without `attachment,,reflex` /
  `attachment,,acog` reference lines.
- 0 warnings in the workbench build (`build.log`), no outside writes except the allowed models,
  their `.bak`, their `.tags.json`, the two `.ff` outputs and their backups.

## 4. Verification

- `-roundtrip`: **523/523 identical, 0 different, 0 errors** after the saves
  (`review\crossbow-optics\roundtrip.txt`).
- `-validate`: xmodels 523/0, xsurfaces 298/47, materials 189/30 (unchanged known missing);
  `DUPLICATE attachment` reporting works (`review\crossbow-optics\validate.txt`).
- `rename_tag` round-trip on a copy: `tag_silencer` -> `tag_silencer_rt`, save, reload ->
  new tag present, old absent; real model hash unchanged (`rename-roundtrip.txt`).
- `fit_tag`/`fit_tag` on the copy and real model: 1 iteration each, screen (960, 508)
  (`tune-copy.txt`, `apply-real.txt`).

## 5. In-game test

`docs\porting\aw.md` now has an **In-game test (crossbow optics)** section. Summary: HMW's
`h2_buildweaponname` appends `_<attachment>` to the base `_mp` name, so the resolved names are
`iw5_exocrossbow_mp`, `iw5_exocrossbow_mp_reflex`, `iw5_exocrossbow_mp_acog`.

Before the map loads (uses the Sticks and Stones `sas_weapon` dvar):
```text
set sas_weapon iw5_exocrossbow_mp
set sas_weapon iw5_exocrossbow_mp_reflex
set sas_weapon iw5_exocrossbow_mp_acog
```
Live console: `give iw5_exocrossbow_mp`, `give iw5_exocrossbow_mp_reflex`,
`give iw5_exocrossbow_mp_acog`.

GSC equivalent:
```gsc
weapon = maps\mp\gametypes\_class::buildweaponname("iw5_exocrossbow", "reflex", "none", 0, 0);
maps\mp\_utility::_giveweapon(weapon);
self switchtoweapon(weapon);
```
Use `"acog"` for the ACOG, `"none"` for no optic. If a suffixed `give` reports an unknown weapon,
the mod zone has no variant asset for that combination (the base name still works); use the
class/attachment path or report it rather than adding a fake weapon.

Check in game: optic mounts on the top rail, reticle centred at ADS end, built-in ring gone,
hip/reload still fine; no suppressor.

## 6. Review files

```
review\crossbow-optics\no_optic_hip.png / no_optic_ads_end.png
review\crossbow-optics\reflex_hip.png / reflex_ads_end.png
review\crossbow-optics\acog_hip.png / acog_ads_end.png
review\crossbow-optics\world_reflex.png / world_acog.png
review\crossbow-optics\fit_crossbow.txt
review\crossbow-optics\build.log
review\crossbow-optics\roundtrip.txt / validate.txt
review\crossbow-optics\tune-copy.txt / apply-real.txt / rename-roundtrip.txt
review\crossbow-optics\vm_crossbow_base_before.xmb / npc_crossbow_base_before.xmb
review\crossbow-optics\vm_crossbow_base_before.tags.json
```

## 7. Open issues

- The three other crossbow skin models (`vm_crossbow_base_advanced/operator*/pro/standard`,
  `npc_crossbow_base_*`) still contain the old `tag_sight_on`; they were not touched (the task
  allows only the two base models and the weapon uses `vm_crossbow_base`/`npc_crossbow_base`).
  A skin switched in game may show the built-in ring with an HMW optic until those are converted.
- No-optic ADS is still 125 px low (AW `vm_crossbow_ads_up` + AW sight); not changed.
- The HMW attachment assets (`reflex`, `acog`, their models/reticles) are expected from the HMW
  base zones; the mod zone was built without `attachment,,` references and succeeded. If a
  runtime test on a setup that does not load `hmw_attachments` fails, add the two reference lines
  and rebuild (documented in the task).
- The zone build still warns about the 11 missing AW crossbow images (scope/glass/target), as
  before; the optic glass itself is from the HMW attachment and renders.
