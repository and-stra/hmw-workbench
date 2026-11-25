# Porting guide: weapons from other CoD games into HMW / MWX

The general, reusable knowledge for porting weapons (and later other assets) into HMW (H1 engine). Game-specific
findings live next to it (`aw.md` for Advanced Warfare); progress/status of a port lives in its plan/report
(`hmw-workbench\docs\porting\aw-weapons-port.md`). **Every agent that learns something reusable adds it here** (see "How to
document" at the end).

Started by Claude, 25 Nov 2026, from the crossbow port.

## 1. Pipeline

1. Dump the source zone with x64-zt for the H1 target (`dumpzone h1 <zone>`), into a NEW folder.
   Weapons can't be dumped for H1 from S1 – dump them with the H2 target and convert
   (`cod-mwx\tools\aw-weapon-port\s1_weapon_to_h1.py`).
2. Check the converted dump with **hmw-workbench** before building anything (`hmw-workbench\docs\workflow-porting.md`).
3. Fix models/weapon json (tags, attachments, hideTags) – always with backups.
4. Build the zone with the H1 x64-zt (`buildzone <zone>` from `GAME\zone_source\<zone>.csv`).
5. Test in game (MWX MCP).

Converters to H1 exist in x64-zt for S1 (AW), IW6 (Ghosts), IW7, T7, H2. None for IW5 (MW3): ZoneTool
(Joelrau/zonetool) dumps IW5, the IW5→H1 step is missing.

## 2. Viewmodel rules (verified in hmw-workbench)

- Game space is Z-up, X forward, Y left, inches. `cg_fov` (default 65) is the horizontal FOV of a 4:3 view, widened
  Hor+; ADS uses the weapon's `adsZoomFov`.
- The gun's root bone (`j_gun` or `tag_weapon*`) is parented to the hands' `tag_weapon`; xanims are applied by bone
  name, parent-relative, to the combined hands+gun skeleton. `tag_ads` is the ADS pivot.
- HMW replaces the hands per faction at runtime (`custom_teams.gsc` → `setviewmodel`); the weapon's `handModel` is a
  placeholder (`viewmodel_base_viewhands` for HMW weapons). A ported weapon must work with every faction's hands.
- A mod zone must reference hands that are loaded before it (`xmodel,,viewhands_tf141`), never
  `viewmodel_base_viewhands` (crash: not loaded yet when mod.ff loads).

## 3. Attachments

- An attachment's view/world model mounts on the weapon tag named like the attachment model's **root bone**
  (`attach_h2_reflex_vm` → `tag_reflex`). Ported weapons have the source game's tag names → add the HMW tags
  (workbench `add_tag` / `fit_tag`, on the view model AND the world model). There are no position offset fields in
  H1 weapons or attachments.
- HMW attachment models are mostly shared; definitions exist per weapon class/weapon only for anim suffixes and stats.
  MW2-style set (used by `h2_*` weapons): `reflex`, `holo`, `acog`, `thermal`, `silencer<class>`, `foregrip`, `xmag`,
  `fmj`, `heartbeat`, `gl`/`glpre`. MWR set: `reflexmwr`, `acogmwr` (+ per-weapon variants with the same models).
- Weapon json `attachments` lists the allowed attachments; `animOverrides[].attachment1/2` are **1-based** indices
  into that list (0 = none). `animTreeType` uses the engine's `weapAnimFiles_t` – **numbers differ between games**
  (S1 ADS_UP 128 → H1 129; table in the converter, generated from x64-zt's `s1/structs.hpp` vs `h1/structs.hpp`).
- Attachment `fields` replace anim name tokens (`_ads` → `_reflex_ads`). A missing result anim does not error: the
  base ADS anim plays and the optic is off-centre (see "How the game aligns ADS" below).
- Hiding a built-in sight when an optic is mounted: HMW's `reflex`/`acog`/`holo` have `hideTags = tag_sight_front_on`.
  Rename the weapon's sight bone to `tag_sight_front_on` (workbench `rename_tag`) if no anim/weapon field uses the old
  name – then it hides exactly when an HMW optic is on.
- Weapon names with attachments: `h2_buildweaponname` appends `_<attachment>` to the base name
  (`iw5_exocrossbow_mp_reflex`).
- ADS check: end of ADS_UP with the attachment's override anim, reticle within 3 px of screen centre, workbench ADS
  auto-align OFF (it hides misalignment).

- **How the game aligns ADS with an optic (answered 25 Nov 2026).** There is no engine-side alignment. The
  attachment carries weapon-field overrides (`fields`: `szXAnims` `ADS_UP`/`ADS_DOWN`, `code 1`, value
  `"_ads _reflex_ads"` / `"_ads _acog_ads"`, see `GAME\dump\workbench_hmw_attachments\attachments\reflex.json`).
  That replaces the token in the weapon's anim name, and the game plays `<weapon>_reflex_ads_up`. Those anims are
  real assets in **`hmw_attachments.ff`** and key **only the `tag_ads` bone** — `vm_h2_m4a1_reflex_ads_up` is 13
  frames with 1 bone `tag_ads` (rot 11/trans 12 keys), `..._acog_ads_up` the same, base `..._ads_up` 12 frames.
  `tag_ads` is a hands bone and the parent of `tag_torso → tag_weapon → j_gun`, so the anim moves the whole gun:
  that is the alignment. A missing optic anim (or an attachment without the field) falls back to the base ADS anim
  and leaves the optic wherever its tag is.
  Measurements with auto-align OFF (1920x1016, `-posedump`): HMW M4 + HMW `reflex` = **2.8 px**, + `acog` = 1.6 px
  (within the 3 px bar; logs `review\aw-weapons\W0\m4_{reflex,acog}_fp.txt`). The earlier 22.1/18.1 px came from
  mounting **MWR** optics (`reflexmwrm4`, `acogmwrm4`) on the H2 M4's MW2 rail tags — same `tag_ads` anim, different
  reticle height in the MWR model, i.e. a real mismatch, not an auto-align failure. Crossbow `reflex`/`acog`/
  `mars`/`holo` are 0 px; `reflexmwr` is 131.6 px high (housing/anim mismatch).
  **Rule:** a port must ship an ADS_UP/DOWN anim named `<weapon>_<optic>_ads_up/down` that keys `tag_ads`, or list
  explicit `animOverrides` per attachment (crossbow). Auto-align OFF is the mode that matches the game.

## 4. Camos

- HMW applies camos by script (`overrideviewmodelmaterial`, camo tables), not via a weapon field. A weapon takes
  camos if its material has the camo layout: `camo_transparent_col` in `dc1`, base colour in `c`, a camo mask in
  `m` (+ `camo_nml`, camo spec, env LUT). Details: `hmw-workbench\docs\camos.md`.

## 5. Player models

- H1/H2 heads share the body skeleton: they attach via the body's bones (`j_spine4`…), not a `j_head` tag; head bones
  without an anim channel inherit the nearest animated ancestor (hmw-workbench M9/FINISH).

## 6. Tools and their traps

- hmw-workbench: `README.md`, `docs\workflow-porting.md`, `docs\mcp.md`. Saves `.xmb` with `.bak` (never overwrites
  an existing `.bak`), writes `<model>.tags.json` so tag edits can be re-applied after a re-port.
- Dump stubs: some zones contain attachment/weapon jsons without models (references only). Don't let them shadow the
  real definition from the zone that owns the asset.
- x64-zt versions differ (game-folder build hung on a transient zone where the refs build worked) – note the build
  used in every report.
- **Localized strings (AW and probably other S1-family games):** the asset type is `localize`, not
  `localizedstrings` (`dumpzone h1 eng_common_mp localizedstrings` fails with `Asset type "localizedstrings" does
  not exist`). The zone name is the file name without `eng_` prefix changes: localized zones live in
  `zone\english\` and are found by their `eng_*` name. Weapon strings are **not** in `eng_common_mp` (only 27
  ESPORTS strings) nor in `eng_patch_common_mp`; they are in **`eng_patch_common`**
  (`dumpzone h1 eng_patch_common localize` → 2,127 strings incl. every `WEAPON_*`).
- **AW model dumps:** `dumpzone h1 mp_vm_<weapon>_tr` / `mp_wm_<weapon>_tr` writes the `.xmb`/`.xsb` into the dump
  root's shared `xmodel\`/`xsurface\` folders (or `weapon_models\` for zone names containing "weapon"), **not** into
  the per-zone folder — the per-zone folder can stay empty while the models are there. AW surface sets are named by
  hash (`5f51a9cd_bc54a12a_100.xsb`) and are found through the .xmb, never by the model name.
- **AW models in hmw-workbench:** add the AW dump folders to `workbench.json` (W0 did: `AW\dump\common_mp`,
  `AW\dump\patch_common_mp`, `AW\dump\weapon_models`). AW materials render untextured/black because the AW image
  paks are not dumped; silhouettes are still usable for real-gun identification.


## 7. Create-a-Class (MWX/HMW)

How a weapon becomes selectable in HMW's MW2-style CAC (all verified in the HMW zones/scripts, 25 Nov 2026):

- **`mp/customweapons.csv`** is the item list. `refs\dumps\hmw_files\ui_scripts\mp\create_a_class\__init__.lua`
  (lines ~172-205) reads `[0]=ref`, `[1]=class`, `[2]=type` ("Primary"/"Secondary") and appends `{ref, 0}` to
  `Cac.Weapons[type][class]`. The class tables are pre-created at the top of the file (`weapon_assault`, …,
  `weapon_melee`, primary and secondary).
- **`mp/statstable.csv`** (67 columns in the current HMW table) is the details + server list: `_weapons::init`
  (`maps\mp\gametypes\_weapons.gsc:69-95`) builds `level.weaponlist` from rows whose column 2 starts `weapon_`,
  column 51 is empty, the last column is not `Never` and column 4 (ref) is non-empty. CAC reads
  Name/Image/WeaponDesc/WeaponId/Ref/ExternalRef/GUID from it.
- **GSC whitelists** in `scripts\mp_patches\custom_weapons.gsc`: `isvalidprimary` (hardcoded `h2_*` switch),
  `isvalidsecondary` (guns + the five melee weapons), `isvalidmeleeweapon` (only `none` + deathstreak refs — the
  melee *items* are secondaries), `isvalidattachment`. HMW installs them with `replacefunc` (lines 4-11); MWX does
  the same from `data\mwx\scripts\mp\`.
- **Category tabs** are hardcoded in `create_a_class\loadout_editor.lua` (lines 1-35): primary = Assault, SMG,
  Sniper, LMG; secondary = Machine Pistols, Shotguns, Handguns, Launchers, Melee. A weapon shows up in a tab by
  its statstable/customweapons class only; a **new tab** requires shadowing that Lua file (the tab tables are
  file-local, so an additive ui_scripts module cannot add one).
- **MWX already has the plumbing:** `src\client\component\stock_weapons.cpp` appends rows from
  `stock_weapons_data.hpp` (`rows[][68]`, gated by `weapon_loaded(ref)` = the `<ref>_mp` asset exists) to both
  tables at runtime; `localized_strings::override(key, value)` supplies `WEAPON_*` names; the zone list is
  `patches.cpp::ui_init_stub()` (HMW zones, then `mwx_...`, then the mod zone). `GAME\mwx` is a registered FS path
  (`filesystem.cpp:68`), so `GAME\mwx\zone\<name>.ff` is a valid MWX-owned zone location, and `build-mwx.bat`
  mirrors `data\mwx` → `GAME\mwx` with `/MIR` (a zone must be committed in `data\mwx\zone\` or the next normal
  build deletes it).
- **Trap (fixed in W1):** the `mp/customweapons.csv` row needs three columns (ref, class, **type** =
  `Primary`/`Secondary`). `stock_weapons.cpp` used to write only ref + class, and the CAC loop `pcall` aborts at a
  row with an empty type — appended weapons never reached the menu. It now derives the type from the statstable
  class (`customweapons_type()`); check the console line `stock weapons: ... added N ...` when a row does not show.
- **Statstable attachment columns:** the engine's attachment-array for a weapon is `tablelookup(statsTable, 4,
  ref, 11+n)` for n = 0..29 (`_utility::getweaponattachmentarrayfromstats`), so **columns 11..40 are the attachments**
  (first empty ends the array). `isvalidattachment` checks that array; the CAC *menu* is filtered by the attachkit
  table instead (see below). Put the attachment refs in 11, 12, … .
- **HMW's `buildweaponname` does not return for `iw5_*` refs.** `custom_weapons.gsc::buildweaponname` builds
  `<ref>_mp_<attachment>` in its `if (!issubstr(var_0, "h1_") && !issubstr(var_0, "h2_"))` branch and then falls
  off the end of the function (no `return` anywhere on that path), so any `iw5_*` class weapon gets an undefined
  name (`applyloadout` then calls `_giveweapon(undefined)`). A port must replace the function: run `replacefunc` in
  a script's `init()` (the loader runs every `main()` at `scr_load_structs` and every `init()` at `scr_load_level`,
  `scripting.cpp`, so init wins) and build `iw5_` names directly; delegate other refs with
  `scripts\mp_patches\custom_weapons::buildweaponname(...)` (cross-script patch paths work, e.g. `dz.gsc` calls
  `scripts\mp_patches\common::is_not_h1_map()`).
- **Attachment menu for a secondary:** `Cac.GetValidAttachKits` (utils.lua) filters the attachkit table by
  `Cac.KitWeaponTypes[Cac.H2MGetWeaponTypeFromWeaponWithoutCategory(ref)]` (`ast/smg/lmg/sht/snp/pst/prj/mel`) and
  then asks the engine (`DoesWeaponSupportAttachment`) per kit. A new class (`prj` for launchers) only gets the
  kits whose `weapon classes` column contains that token; HMW's launchers have no attachments at all. For a
  secondary that must take optics, either extend the attachkit row for the class or wrap
  `Cac.GetValidAttachKits` in an MWX `ui_scripts` module (loaded after every HMW zone `__init__.lua`, so the `Cac`
  functions exist). The secondary attachment/camo menus are hidden for HMW's own melee/launcher refs only
  (`MeleeOrLauncherInUseCheck` hardcodes at4/h2_rpg/javelin/h2_m79/stinger and the five melee refs).


## 8. Zone loading and a second test client

- **HMW/MWX zones are an explicit list, not a glob.** `patches.cpp::ui_init_stub()` has a `std::vector<std::string>`
  of the `hmw_*` content zones (each loaded only if `fastfiles::exists()`), then the loaded mod's zone last. Adding
  a ported-content zone means one entry in that list, after the HMW zones (so it can use HMW hands/attachments/
  anims) and before `mod` (so a mod can reference it). `fastfiles.cpp` also loads the fixed engine zones via
  `try_load_zone("hmw_pre_gfx"/"hmw_files"/...)`; no code scans for `hmw_*` by prefix, so there is no second load
  path.
- **A built zone has two files.** `buildzone <name>` writes `GAME\zone\<name>.ff` **and** `<name>.pak` when the
  zone has streamed images. Install both next to each other (the engine looks for the pak of the loaded zone); a
  missing pak shows as missing/red/checkered streamed textures, not as a load error (the old Sticks and Stones
  install had this: `mod.ff` 19.8 MB, `mod.pak` left in `GAME\zone`).
- **A `,,` reference in a zone** (`weapon,,x`) resolves the asset from another *loaded* zone at runtime. If that
  zone is not loaded, the load fails with `Could not load default asset 'void' for asset type 'weapon'`. Good for
  one owner per asset (e.g. crossbow only in `hmw_aw_weapons`), but every client that loads the referencing zone
  must load the owner zone too.
- **Second client next to the shared one** (no interference with another agent's `GAME\mwx`/`mwx.exe`): a test
  branch can add `-datadir <dir>` / `-logdir <dir>` (see `filesystem::get_data_dir()` in the `aw-weapons` branch)
  so the client loads `GAME\<dir>\ui_scripts`, `GAME\<dir>\scripts\mp`, `GAME\<dir>\zone` and writes its logs to
  `GAME\<dir>-logs`; combine with `-debugport <port>` (exists in `debug_server.cpp`) so two debug/MCP servers never
  collide. Build the exe manually (`premake5` + `msbuild`, skip the script's install step) and copy it to
  `GAME\<name>.exe`. The install step in `build-mwx.bat` overwrites `GAME\mwx.exe` and mirrors `data\mwx` with
  `/MIR`, so it must not be used while another client owns those paths.

## How to document (rule for every agent)

After every milestone or whenever something cost more than ~15 minutes to figure out, add it:
- reusable, engine/tool knowledge → this file (right section, short, with the source: file/function/commit);
- source-game specific (AW names, tag names, quirks of one weapon) → the game file (`aw.md`, later `ghosts.md`, …);
- errors: the exact message, the cause and the fix → "Errors and fixes" below;
- status/progress → the port's report, not here.
Keep entries short and current: correct or delete outdated statements instead of appending contradictions.

## Errors and fixes

| error / symptom | cause | fix |
|---|---|---|
| `Could not load default asset 'void' for asset type 'xmodel'. Tried to load asset 'viewmodel_base_viewhands'` | mod zone references HMW's placeholder hands, not loaded yet | use `viewhands_tf141` by reference (`xmodel,,viewhands_tf141`) |
| `Disc read error 'mod'` | mod zone built with another internal name | build as zone `mod` (`zone_source\mod.csv`) |
| `vertexshader "X.hlsl_s1" not found` | shader's zone not in `zonetool_paths` | dump the code zones (`code_pre_gfx_mp`, `code_post_gfx_mp`) with `dumpzone h1` |
| HMW optic floats/missing on a ported weapon | weapon lacks the HMW tag | add the tag (section 3) |
| built-in sight covers the optic in ADS | sight not hidden | `tag_sight_front_on` rename (section 3) |
| AW ADS anims for optics never play | empty `attachments` list / S1 anim tree numbers | converter `--attachment-map` + animTreeType conversion |
| `Asset type "localizedstrings" does not exist` (x64-zt) | the AW/S1 asset type is `localize` | `dumpzone h1 <zone> localize`; weapon strings are in `eng_patch_common` (not `eng_common_mp`) |
| hmw-workbench: `cannot load model <aw model>: xmodel has no LOD0 surface set` | only the `common_mp` xmodel was dumped; AW surfaces live in the weapon tr zones | `dumpzone h1 mp_vm_<weapon>_tr` + `mp_wm_<weapon>_tr`; the files land in the dump root's `xmodel`/`xsurface`, add that root to `workbench.json` |
| ported weapon missing from Create-a-Class (or replaced by the default gun) | no `mp/customweapons.csv`/`mp/statstable.csv` row, or not in `isvalidprimary`/`isvalidsecondary` | add the `stock_weapons_data.hpp` row (only added when `<ref>_mp` exists) + GSC whitelist via `replacefunc` (guide §7); a row with an empty customweapons **type** column aborts the CAC loop |
| `iw5_*` class weapon spawns nothing / `_giveweapon(undefined)` script error | HMW's `buildweaponname` has no `return` for non-`h1_/h2_` refs | replace `_class::buildweaponname` in an `init()` and build `<ref>_mp[_<attachment>]` for `iw5_*`, delegate the rest to `scripts\mp_patches\custom_weapons::buildweaponname` (guide §7) |
| streamed textures of a ported zone are red/checkered although the build had no error | `<zone>.pak` was not installed next to `<zone>.ff` | copy both files (guide §8) |
| `Could not load default asset 'void' for asset type 'weapon'` when loading a mod | the mod zone uses `weapon,,<name>` but no loaded zone provides the asset | load the owner zone (`hmw_aw_weapons`) before the mod, or keep a copy in the mod |
