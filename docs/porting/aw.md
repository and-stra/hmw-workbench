# Advanced Warfare (S1) → HMW porting: state and findings

Central overview for porting AW content to HMW (H1). Detailed logs stay where they were written:
- `cod-mwx\docs\todo-aw-porting.md` (paused TODO list, 24 Nov 2026), `cod-mwx\docs\aw-weapon-porting.md`,
  `cod-mwx\docs\aw-map-porting.md`, `cod-mwx\tools\aw-weapon-port\README.md`
- `hmw-workbench\docs\milestones\M4.md` ("Crossbow" section), `hmw-workbench\docs\viewmodel.md`,
  review images in `hmw-workbench\review\M4\`

Last update: 25 Nov 2026.

## Pipeline

AW zone (`S1ff`) → x64-zt (S1 build in the AW folder) dump → x64-zt S1→H1 converter → H1 dump
(`GAME\zonetool\<zone>`) → zone CSV (`GAME\zone_source\<zone>.csv`) → x64-zt H1 build (`x64zt.exe` in GAME) → `.ff`.
The crossbow lives in `GAME\zonetool\mod` / `zone_source\mod.csv` (Sticks and Stones mod, zone name `mod`).
`hmw-workbench` reads the converted H1 dump, so every port can be checked before the zone is built.

Disk: plan 60+ GB working space (full AW `common_mp` dump 9.3 GB, `patch_common_mp` 7.2 GB, one map ~2.7 GB + 0.9 GB
build output). Continue on the new machine.

## W0 inventory (25 Nov 2026, plan in `AW-WEAPONS-PLAN.md`, report in `aw-weapons-port.md`)

- **MP content is installed**: `s1_mp64_ship.exe`, `common_mp.ff` and the `mp_*` zones exist (the earlier
  "singleplayer only" note in `cod-mwx\docs\aw-weapons-plan.md` is obsolete).
- **Weapon defs**: modern guns + all DLC guns in `patch_common_mp`, `iw5_maaws_mp` and `iw5_combatknife_mp`
  only in `common_mp`. Dump JSONs with the **H2 target** (`dumpzone h2 patch_common_mp weapon`,
  `dumpzone h2 common_mp weapon` → `AW\dump\<zone>\weapons\`); the H1 target writes 0 weapon files.
- **Models**: the `mp_vm_<internal>_tr` / `mp_wm_<internal>_tr` zones (both `_tr` and `_2_tr` variants exist),
  e.g. `mp_vm_asaw_tr`, `mp_vm_pbw_tr`, `mp_vm_mp11_tr`, `mp_vm_leveraction_tr`, `mp_vm_dlcgun3/4_tr`,
  `mp_vm_m1garand_tr`, `mp_vm_mp40_tr`, `mp_vm_sten_tr`. The knife has no tr zone (models in `common_mp`).
  Models and anims land in the dump root's shared `xmodel`/`xsurface`/`xanim` folders.
- **English names**: `WEAPON_*` strings are in `eng_patch_common` (asset type `localize`). The internal
  `szDisplayName` is a dev key and differs from the retail name: `WEAPON_ASAW`=Ameli, `WEAPON_LSAT`=Pytaek,
  `WEAPON_GM6`=Lynx, `WEAPON_UTS19`=Tac-19, `WEAPON_PBW`=MP443 Grach, `WEAPON_COMBAT_KNIFE`=Combat Knife,
  `WEAPON_MAAWS`=MAAWS. **DLC guns have no English string at all** (`szDisplayName` = `MDLC*_DLCGUN*`
  placeholder) — name them from the model.
- **Class numbers (`weapClass`)**: 0 rifle, 1 sniper, 2 LMG, 3 SMG, 4 shotgun, 5 pistol, 6 grenade, 7 special
  (crossbow, MAAWS, MDL), 9 melee-ish (exo knife, throwing knife), 13 energy (EM1). The combat knife is class 5
  but is no pistol; in HMW it becomes a secondary `weapon_melee`.
- **Combat knife quirk**: `iw5_combatknife_mp` uses `vm_mp_tactical_knife` / `npc_mp_tactical_knife` (not
  `vm_mp_combat_knife`), 67 anims, no attachments. HMW already uses `wpn_h1_melee_combat_knife_vm` as every
  gun's melee-attack knife, so a ported knife looks like a second knife.
- **Attachments** are AW-specific per weapon (18–21 per AR/SMG, 10–15 for shotgun/pistol, 0 for launcher/knife)
  and are replaced by HMW's set (guide §3); the AW lists are only useful to know which mounts exist.
- **Dumps added**: `AW\dump\common_mp\weapons` (413), `AW\dump\eng_patch_common\localizedstrings` (2,127),
  `AW\dump\weapon_models` (HBRa3 + Lever Action). `AW\dump\eng_common_mp` etc. only have ESPORTS strings.
- **Identity checks**: HBRa3 renders as a **CZ 805 BREN** (rectangular handguard slots, full-length top rail,
  CZ folding stock) — `review\aw-weapons\W0\hbra3_*.png`; the Lever Action is a **Browning BLR** (IMFDB,
  corrected by the user — the asset/model names say "winchester" and the untextured W0 render was read as a
  Winchester). The Ameli is the **CETME Ameli**, not an "MG82".
- **Ghosts fallback**: CZ 805 (HBRa3), CBJ-MS (MP11), LSAT (Pytaek) and UTS-15 (Tac-19) also exist in
  Call of Duty: Ghosts (IW6) — a fallback source if an AW asset turns out broken. Ghosts is **not installed
  on this machine** (only AW and HMW are); x64-zt has an IW6→H1 converter, so it would need the IW6 files.
- **Duplicates confirmed** from the dumps: `dlcgun6`=StG44, `dlcgun13`=M1911, `dlcgun43`=AK-47,
  `dlcgun8`=M16, `dlcgun7`=Dragunov/SVO, `sac3`=Vector, `m182spr`=Mk14 EBR. No new duplicates among the 18
  planned weapons (HMW's "Lynx CQ300" AR only collides in name with the GM6 Lynx).
- **Images are still the gap**: AW weapon textures are in the `imagefile*.pak`s; the tr-zone dumps have no
  images, so workbench renders are black/untextured and HUD/kill icons need a targeted image dump later.

## Crossbow (`iw5_exocrossbow_mp`)

In game: `mod.ff` builds and loads, the crossbow fires, bolts stick/explode and kill, kill feed works.
Open in game: **no arms, gun looks oversized/too close**, 11 missing images (scope/glass, red in ADS).

### Workbench findings (M4, 24 Nov 2026)

- **The converted content is consistent.** On `viewhands_tf141` the AW anims drive the MWR arms correctly (bone
  names match: `tag_torso`, `tag_weapon`, `j_gun`, `j_wrist_*`, fingers). Idle, fire, reload, sprint, raise work; the
  hands hold the grip.
- **Not closer than an HMW weapon:** at the game's defaults (`cg_fov` 65, `cg_fovScale` 1, checked against in-game
  M4A1 screenshots) the crossbow's sight tag is 12.17 units from the eye vs 12.23 for the M4's `tag_reflex`;
  `tag_view`/`tag_camera`/`tag_ads` are identical. Differences: the AW hip pose puts the gun root 1.7 units lower,
  and the model is 22 units wide (bow limbs) vs 2.9 for the M4.
- **The bulky ring is the built-in sight:** surfaces 0, 1, 3, 18, 19 of `vm_crossbow_base` (`mtl_crossbow_scope_base`,
  `ms/mtl_crossbow_scope_base`, `mtl_crossbow_target`, `mtl_crossbow_glass_base`, `mtl_iron_sights_color`, 1844 verts)
  are rigid to `tag_sight_on`. Their materials are missing from the dump (same as the 11 missing images).
- **ADS without optic is off:** at the end of `vm_crossbow_ads_up` the sight is at screen (960, 686), centred
  horizontally but 178 px below the crosshair (1920x1016). The HMW M4 lands exactly on the crosshair.
- **Attachment overrides are broken:** the weapon's `attachments` list is empty, but its 21 `animOverrides` use AW
  indices 1..7. `attachment1` indexes `WeaponDef::attachments`, so none of them can apply in game. Mapping from the
  anim names: 1 ACOG, 2 EOTech, 3 red dot, 4 thermal, 5 VRD, 6 target enhancer, 7 scope. AW `animTreeType` 128/129 =
  ADS up/down (H1 uses 129/130).
- **HMW attachments don't fit (tag names):** an attachment's view model mounts on the weapon tag named like its root
  bone. The crossbow has AW tags only. Fit report (`review\M4\fit_crossbow.txt`) for HMW attachments:

  | needed tag | HMW attachments | on crossbow | nearest crossbow tag |
  |---|---|---|---|
  | `tag_acog` | 30 ACOG variants | missing | `tag_acog_2` |
  | `tag_reflex` | 22 red dot variants | missing | `tag_red_dot` |
  | `tag_silencer` / `tag_silencer1` | 11 suppressors | missing | `tag_flash` |
  | `tag_m203` | 5 grenade launchers | missing | – |
  | `tag_steyr_scope`, `tag_heartbeat` | 1 each | missing | – |
  | `tag_thermal_scope` | 4 thermal | **found** | – |

### Still unexplained / next checks

- **"No arms" in game is not reproduced** in the workbench, with `viewhands_tf141` or any of the 7 faction hands
  that exist in the dumps (see the check below). The content (anims, bone names, rig) is fine, so the cause is most
  likely at runtime in game. Next checks, in game (MWX MCP `gsc_eval`, `screenshot`):
  1. which hands model the player really has while holding the crossbow (`self getviewmodel()`), and whether it is
     one of the 5 hands models not in our dumps (`viewhands_african_militia`, `_pmc`, `_henchmen`, `_delta`, `_sas`);
  2. whether the hands are there with another weapon in the same match and disappear only on switching to the
     crossbow (points at the weapon def, e.g. `handModel = viewhands_tf141` referenced with `xmodel,,` but not loaded,
     or a hide flag), or are missing from spawn.
- In-game reference screenshots of the crossbow (hip, ADS) to compare with the workbench.

### Faction hands check (M4b, 24 Nov 2026)

Rendered the crossbow idle, the reload contact sheet (8 cells) and the H2 M4 idle as a control with every
faction hands model from `docs\viewmodel.md` that exists in the current roots
(`review\M4b\hands_<model>_*.png`, 1280x678; sheets 8x480x270). For each run, `-posedump` reports how many
animation channels of `vm_crossbow_idle` / `vm_crossbow_reload` find a bone of the same name in the combined
hands+gun pose.

7 of the 12 table entries exist; `viewhands_african_militia`, `viewhands_pmc`, `viewhands_henchmen`,
`viewhands_delta` and `viewhands_sas` are in no asset root.

| hands model | loads | idle matched | reload matched | unmatched channels | arms show | holds the grip |
|---|---|---|---|---|---|---|
| `viewhands_tf141` | yes | 70/75 | 70/75 | `flipdown`, `flipup`, `arm`, `j_wristtwist_back_le`, `j_wristtwist_back_ri` | yes | yes |
| `viewhands_h1_arab_desert_mp_camo` | yes | 72/75 | 72/75 | `flipdown`, `flipup`, `arm` | yes (white / untextured) | yes |
| `globalViewhands_mw2_airborne` | yes | 70/75 | 70/75 | `flipdown`, `flipup`, `arm`, 2x `j_wristtwist_back_*` | yes | yes |
| `viewhands_rangers` | yes | 70/75 | 70/75 | `flipdown`, `flipup`, `arm`, 2x `j_wristtwist_back_*` | yes | yes |
| `viewhands_udt` | yes | 70/75 | 70/75 | `flipdown`, `flipup`, `arm`, 2x `j_wristtwist_back_*` | yes | yes |
| `globalViewhands_mw2_militia` | yes | 70/75 | 70/75 | `flipdown`, `flipup`, `arm`, 2x `j_wristtwist_back_*` | yes | yes |
| `viewhands_h2_ghillie` | yes | 70/75 | 70/75 | `flipdown`, `flipup`, `arm`, 2x `j_wristtwist_back_*` | yes | yes |

- `flipdown`, `flipup` and `arm` are crossbow parts, not hands; no hand rig has them (and neither does
  `vm_crossbow_base`). The remaining two, `j_wristtwist_back_le/ri`, exist only on the arab rig.
- **"No arms" is not reproduced with any hands model in the roots.** All seven show the left forearm/sleeve and
  the hand on the foregrip, and the right hand at the trigger is hidden behind the receiver exactly as on the
  H2 M4 control. The bones named by the AW anims exist on all seven rigs, so the game failure is not a
  bone-name mismatch for these models.
- The arab hands render white/untextured with the crossbow **and** with the M4 control, so that is a missing
  workbench texture/material, not an AW-anim problem.
- Next: check the five hands models that are not in the roots (the game may pick one of them for the failing
  faction), and the in-game `setviewmodel`/material path for a failure that leaves the hands model unloaded.

### Port fixes (state 25 Nov 2026; details in "Crossbow optics" below)

1. **Done 25 Nov 2026 (weapon json):** `zonetool\mod\weapons\iw5_exocrossbow_mp.json` (original kept as `.json.bak`)
   now has `attachments = ["acog", "reflex"]` (HMW's MW2-style optics, the same ones the M4A1 uses) and 5
   `animOverrides`: acog ADS up/down + empty idle (index 1), red dot ADS up/down (index 2), `animTreeType` converted
   S1 → H1 (128/129 → 129/130). The 16 overrides of AW-only optics (EOTech, thermal, VRD, target enhancer, scope) are
   dropped. Generated by the converter with `--attachment-map opticsreddot=reflex --attachment-map opticsacog2=acog`;
   the converter change is a patch for `cod-mwx`: `hmw-workbench\docs\porting\patches\s1_weapon_to_h1-attachment-map.patch`
   (not applied yet – `cod-mwx` is the MWX agents' repo).
2. **Done 25 Nov 2026 (workbench M5):** `vm_crossbow_base.xmb` in `GAME\zonetool\mod\xmodel` has three new tags
   (original kept as `.xmb.bak`, edits in `vm_crossbow_base.tags.json`), all children of `j_gun`:
   `tag_reflex`, `tag_acog`, `tag_silencer` (= `tag_flash`). First tuned for the MWR optics, then re-tuned the same
   day for HMW's MW2-style `reflex`/`acog` (final offsets in "Crossbow optics" below). World model done too.
3. **Done 25 Nov 2026:** hide the built-in sight when an optic is mounted. HMW's
   `reflex` and `acog` attachments hide `tag_sight_front_on`, so renaming the crossbow bone `tag_sight_on` →
   `tag_sight_front_on` hides the ring exactly when an HMW optic is mounted (no anim or weapon field uses
   `tag_sight_on`). Renamed in `vm_crossbow_base` and `npc_crossbow_base`.

### W1: crossbow zone and Create-a-Class (25 Nov 2026)

- **Zone `hmw_aw_weapons`**: source `GAME\zonetool\hmw_aw_weapons` (copy of the fixed `zonetool\mod` crossbow:
  weapon json with `attachments = ["acog","reflex"]`, fitted view/world tags, 64 xanims, sounds, fx), new
  `GAME\zone_source\hmw_aw_weapons.csv`, `buildzone hmw_aw_weapons` (H1 x64-zt, 8.5 s):
  `hmw_aw_weapons.ff` **19,877,776 B** + `hmw_aw_weapons.pak` **9,236,400 B**, 0 FATAL/ERROR, 11 image warnings.
  The `.pak` carries the streamed images and **must be installed next to the `.ff`** (the old Sticks and Stones
  install had `mod.ff` but no `mod.pak`, which is why its streamed crossbow textures never loaded).
- **Asset ownership**: the crossbow, its models, anims, sounds and fx now exist only in `hmw_aw_weapons.ff`.
  `GAME\zone_source\mod.csv` was reduced to `weapon,,iw5_exocrossbow_mp` (a `,,` reference) and `mod.ff` rebuilt to
  **177 B**. A client that loads `mod` must load `hmw_aw_weapons` first (MWX does); otherwise zone `mod` fails with
  `Could not load default asset 'void' for asset type 'weapon'`.
- **HUD/kill icon**: `hud_icon_exocrossbow` (material) + image `special_crossbow` are in the zone and have pixel
  data; the CAC image column and kill feed use that material.
- **Missing images (11, still open)**: `crossbow_scope_base_col`, `crossbow_scope_base_nml`,
  `~crossbow_scope_base_spc-rgb&~d8544476`, `~crossbow_scope_base_occ-l`, `crossbow_glass_col`,
  `crossbow_glass_nml`, `~crossbow_glass_spc-rgb&cross~a7515f30`, `crossbow_target_ems`,
  `wep_shared_gray_swatch_col`, `weapon_red_cross_background`, `emblem_temp_col`. Their materials
  (`mtl_crossbow_scope_base`, `ms/mtl_crossbow_scope_base`, `mtl_crossbow_target`, `mtl_crossbow_glass_base`,
  `mtl_iron_sights_color`) are in `AW\dump\common_mp\materials` but not in the zone (the images are missing
  anyway). `dumpasset image` writes S1 images; a targeted AW image extraction/conversion is still the gap.
- **CAC**: statstable row class `weapon_projectile` (secondary -> Launchers), name key
  `WEAPON_EXPLOSIVE_EXO_CROSSBOW` -> "Crossbow" (localized override), image `hud_icon_exocrossbow`, attachments
  `reflex`/`acog` in statstable columns 11/12, GUID `-1` (no HMW inventory item). `customweapons.csv` gets
  `ref, weapon_projectile, Secondary`.
- **Engine/script gaps found**: HMW's `buildweaponname` has no `return` for `iw5_*` refs (the class path gave an
  undefined weapon name; fixed by the `aw_weapons.gsc` replacement, see guide section 7); and the CAC attachment
  menu filters attachkits by class token, so a `prj` weapon gets no optics unless the kit row allows `prj` or an
  MWX `ui_scripts` wrapper adds them (done in `data\mwx\ui_scripts\aw_weapons`). AW camo suffixes (`_camoNN`) do
  not exist for the crossbow, so a selected camo is dropped (uncamoed weapon) instead of an unknown weapon.
- **Workbench**: crossbow + `reflex` and + `acog` = **0.0 px** at the end of ADS with auto-align off
  (`review\aw-weapons\W1\crossbow_{reflex,acog}_ads.*`).

**HMW attachment models:** attachments are defined per weapon class, the models are mostly shared. HMW's MW2-style
weapons (`h2_*`, e.g. the M4A1) use one generic set: `reflex` (`attach_h2_reflex_vm` on `tag_reflex`), `acog`
(`attach_h2_acog_vm` on `tag_acog`), `holo`, `thermal`, `silencerar`/`smg`/… All MWR weapons share
`attach_h1_scope_acog_vm` (30 `acogmwr<weapon>` definitions) and `attach_h1_optic_reflex_vm` (22 `reflexmwr<weapon>`);
the per-weapon definitions only differ in anim suffixes and stats. A few models are weapon-specific (AUG/F2000 scopes,
M203/GP25 launchers, M1014 foregrip).
4. Open: fix ADS alignment without optic (built-in sight 125–178 px low in the workbench; check in game first) – check what AW does for the no-optic case first.
5. Open: dump the 11 missing images (AW `common_mp`) in H1 format.

## Crossbow optics: workbench + zone state (25 Nov 2026)

- **Weapon json (Claude, 25 Nov):** `attachments = ["acog", "reflex"]`; five `animOverrides`
  (acog index 1: `vm_crossbow_acog_ads_up/down` tree 129/130 + `vm_crossbow_empty_idle` tree 2;
  reflex index 2: `vm_crossbow_reddot_ads_up/down` tree 129/130). `attachment1` is 1-based.
- **Workbench fixes (25 Nov):** attachment lookup now prefers a definition with models (the
  `workbench_hmw_ar1` stubs no longer shadow `workbench_hmw_attachments`); overrides resolve the
  weapon's attachment list 1-based and H1 `weapAnimFiles_t` 129/130; `rename_tag` (UI command +
  MCP) writes the name string; `-validate` reports duplicate attachment definitions.
- **Models saved (allowed writes):**
  - `vm_crossbow_base.xmb`: `tag_sight_on` → `tag_sight_front_on`; `tag_reflex` final local
    (3.242754, 0.000000, 6.226662) = `tag_red_dot` + 0.490906 Z; `tag_acog` final
    (3.377740, 0.000000, 6.027324) = `tag_acog_2` - 0.000004 Z. M5 tags still present,
    `tag_silencer` unchanged/unused.
  - `npc_crossbow_base.xmb`: same rename; added `tag_reflex` = its `tag_red_dot` + 0.490906 Z
    and `tag_acog` = its `tag_acog_2` - 0.000004 Z (both parent `tag_weapon`), 17 → 19 bones.
  - `.bak` files are the originals (`vm` 4911 B original kept; `npc` got a new `.bak`).
- **ADS measurements** (`-no-ads-align -crosshair`, 1920x1016, weapon json path):
  reflex `vm_crossbow_reddot_ads_up` reticle aim **(960.0, 508.0)**; acog
  `vm_crossbow_acog_ads_up` reticle aim **(960.0, 508.0)**. The built-in ring is hidden by the
  HMW attachments' `hideTags = tag_sight_front_on` (the surfaces are rigid to the renamed bone).
- **Zone:** built with `buildzone mod` (x64-zt, 0 FATAL/ERROR; known missing crossbow images only),
  `zone\mod.ff` 19,877,701 B installed as `mods\sticksandstones\mod.ff`; backups
  `.ff.bak-20261125-070627`. `zone_source\mod.csv` was not changed (the build did **not** need
  `attachment,,reflex/acog` references).

## In-game test (crossbow optics)

The HMW script `custom_weapons.gsc` builds a weapon name with
`h2_buildweaponname(base, attachment1, ...)`: the base name already carries `_mp`, then the
attachment is appended (`weaponName += "_" + attachment`). So:

| loadout | resolved weapon name |
|---|---|
| no optic | `iw5_exocrossbow_mp` |
| reflex | `iw5_exocrossbow_mp_reflex` |
| acog | `iw5_exocrossbow_mp_acog` |

The Sticks and Stones gametype gives `getdvar("sas_weapon")`, so set it before the map loads:

```text
set sas_weapon iw5_exocrossbow_mp
set sas_weapon iw5_exocrossbow_mp_reflex
set sas_weapon iw5_exocrossbow_mp_acog
```

Or give it live from the console after spawning (same names):

```text
give iw5_exocrossbow_mp
give iw5_exocrossbow_mp_reflex
give iw5_exocrossbow_mp_acog
```

GSC equivalent (exactly what the game's class code does):

```gsc
weapon = maps\mp\gametypes\_class::buildweaponname("iw5_exocrossbow", "reflex", "none", 0, 0);
maps\mp\_utility::_giveweapon(weapon);          // "iw5_exocrossbow_mp_reflex"
self switchtoweapon(weapon);
```

Use `"acog"` for the ACOG (`iw5_exocrossbow_mp_acog`) and `"none"` for no optic. Then:

1. switch to the crossbow, ADS and confirm the reticle is centred and the built-in ring is gone;
2. hip fire and reload; the optic body must stay on the top rail (the moved tags);
3. no suppressor is fitted (`tag_silencer` remains on the model, unused);
4. if a suffixed `give` says "unknown weapon", the mod zone has no variant asset for that
   combination; use the no-optic name and the class/attachment path, or build the variant
   (report it rather than adding a fake asset).

## In-game test (camos)

The camo is a weapon-name suffix, not a weapon field. HMW's class code resolves the class
camo name through `mp/camoTable.csv`, converts it to a number and calls
`maps\mp\gametypes\_class::buildweaponnamecamo`, which appends `_camoNN` to the weapon name:

- `refs\dumps\hmw_files\scripts\mp_patches\custom_weapons.gsc:756` (primary) and `:764`
  (secondary): `tablelookup( "mp/camoTable.csv", 1, final_loadout.primarycamo, 0 )`.
- `refs\dumps\hmw_files\scripts\mp_patches\custom_weapons.gsc:758` and `:766`:
  `buildweaponname(..., loadoutPrimaryCamo, ...)`.
- The `iw5_`/`h1_`/`h2_` variants call `buildweaponnamecamo` at
  `refs\dumps\hmw_files\scripts\mp_patches\custom_weapons.gsc:1030,1038` (H2) and
  `:1136,1149` (iw5/h1).
- `refs\iw5-scripts\maps\mp\gametypes\_class.gsc:1491-1503`: if the camo index is `> 0`,
  append `_camo0<index>` for `< 10`, otherwise `_camo<index>`.

The three test camos are in `refs\h1_dump\h1_dump\mp\camotable.csv`:

| camo | camoTable row | index in column 0 | weapon suffix |
|---|---|---|---|
| `camo_h1_red_wash` | `42,camo043,...` (line 43) | 42 | `_camo42` |
| `camo_h1_blue_jade` | `45,camo046,...` (line 46) | 45 | `_camo45` |
| `camo_h1_christmas_wrap` | `84,camo085,...` (line 85) | 84 | `_camo84` |

Controls and commands (H2 M4 control, then the crossbow):

```text
give h2_m4_mp_camo42
give h2_m4_mp_camo45
give h2_m4_mp_camo84

give iw5_exocrossbow_mp_camo42
give iw5_exocrossbow_mp_camo45
give iw5_exocrossbow_mp_camo84
```

The Sticks and Stones gametype gives `getdvar("sas_weapon")` (the same path as the optic test
above), so the same names can be set before the map loads:

```text
set sas_weapon iw5_exocrossbow_mp_camo42
set sas_weapon iw5_exocrossbow_mp_camo45
set sas_weapon iw5_exocrossbow_mp_camo84
```

GSC equivalent (this is the class path, including the `buildweaponnamecamo` step):

```gsc
// 42 = camo_h1_red_wash, 45 = camo_h1_blue_jade, 84 = camo_h1_christmas_wrap
weapon = maps\mp\gametypes\_class::buildweaponname("h2_m4", "none", "none", 42, []);
maps\mp\_utility::_giveweapon(weapon);          // h2_m4_mp_camo42
self switchtoweapon(weapon);

weapon = maps\mp\gametypes\_class::buildweaponname("iw5_exocrossbow", "none", "none", 42, []);
maps\mp\_utility::_giveweapon(weapon);          // iw5_exocrossbow_mp_camo42
self switchtoweapon(weapon);
```

Before testing, clear the optic so the camo is the only variable: `set sas_weapon` to the base
`iw5_exocrossbow_mp` / `h2_m4_mp`, or use `"none"` attachments in the GSC call. Then switch to the
weapon, ADS and check the receiver/stock; for the crossbow also check the rails and limbs (the
`crossbow_mask_02` mask leaves them untouched).

### Will the crossbow take them?

The crossbow material is camo-ready. `GAME\zonetool\mod\materials\m\mtl_crossbow_base.json`
has the same slots as the H2 M4 `_camo` material: `dc1 = camo_transparent_col` (camo design slot),
`c = crossbow_col` (base colour), `m = crossbow_mask_02` (mask), `dna1 = camo_nml` (camo normal),
`3426233914 = ~camo_spcmaps_01_spc-rgb&camo~b29a8732` (camo specular) and the
`~envbrdflut_ggx_16-rg` environment LUT. So nothing in the material prevents a camo.

What can stop it is the name/variant path, not the material:

1. **No crossbow camo variants in the mod zone.** `buildweaponnamecamo` produces
   `iw5_exocrossbow_mp_camo42/45/84`, but `GAME\zonetool\mod\weapons\` only contains the base
   crossbow (and the H1 loot crossbows), and `GAME\zone` has no `mp_vm_exocrossbow_*_tr.ff`
   camo transient zones. If the engine's `isvalidweapon` rejects the suffixed name, `give` says
   "unknown weapon" and the class path falls back to the base weapon with no camo.
2. **`isvalidcamo` whitelist.** HMW replaces `_class::isvalidcamo` in
   `custom_weapons.gsc:835-889`; the switch only lists `camo016..camo062` plus a few names.
   `camo043` and `camo046` pass; `camo085` (Christmas) does **not**, so on the class/loadout path
   it is replaced and prints `Replacing invalid camo: camo085`. Live `give` with the suffix
   bypasses that class check, but still needs the asset from point 1.
3. **Unlock/class lookup.** `iscamounlocked` reads `mp/weaponRankTable.csv` by weapon class; a
   missing crossbow class entry can reject the camo in ranked/public-lobby checks even if the
   asset exists.
4. The engine builtins `overrideviewmodelmaterial` / `setviewmodelmaterialscriptparam`
   (`refs\hmw_decomp\strings.tsv:7253,7258,7259`) are the material-override path the game uses
   for view models; no shipped HMW script calls them directly, so they are not a console command.
   If the suffix path fails, a GSC experiment can call the builtin through MWX `gsc_eval` with the
   `camo_h1_*` material, but this is not a supported/tested path yet.

So the expected result is: the H2 M4 control shows all three camos; the crossbow shows red_wash
(`camo42`) and blue_jade (`camo45`) if the engine can resolve its camo variants, and Christmas
(`camo84`) is additionally blocked by the class whitelist. If no suffix variant resolves, the
crossbow remains un-camoed; that is the missing `_camo` asset, not `mtl_crossbow_base`.

## AW maps

28 of 30 community ports load and play with bots (see `todo-aw-porting.md`). Open: install the rest (disk), generic
helicopter paths, missing classic geometry (ladders/clips), own map pipeline fixes, `mp_seoul2`, `mp_solar`,
`mp_torqued` UAV issue. The workbench maps track (map inspector: mapents, collision, path nodes, heli points) is
meant for these checks.

## Other source games

x64-zt converts S1 (AW), IW6 (Ghosts), IW7 (Infinite Warfare), T7 (Black Ops III) and H2 (MW2CR) to H1. IW5 (MW3) has
no converter: ZoneTool (Joelrau/zonetool, used by Plutonium) dumps IW5, the IW5→H1 step is missing. MW2 maps look
complete in HMW; missing MW3 maps: Carbon, Exchange, Interchange, Outpost, Village and most DLC maps.
