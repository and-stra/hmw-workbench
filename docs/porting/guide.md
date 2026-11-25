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
- Attachment `fields` replace anim name tokens (`_ads` → `_reflex_ads`); HMW weapons tolerate a missing result anim.
- Hiding a built-in sight when an optic is mounted: HMW's `reflex`/`acog`/`holo` have `hideTags = tag_sight_front_on`.
  Rename the weapon's sight bone to `tag_sight_front_on` (workbench `rename_tag`) if no anim/weapon field uses the old
  name – then it hides exactly when an HMW optic is on.
- Weapon names with attachments: `h2_buildweaponname` appends `_<attachment>` to the base name
  (`iw5_exocrossbow_mp_reflex`).
- ADS check: end of ADS_UP with the attachment's override anim, reticle within 3 px of screen centre, workbench ADS
  auto-align OFF (it hides misalignment).

- **Open question – how the game aligns ADS with an optic.** With the workbench's ADS auto-align off, HMW's own M4 +
  `reflex` ends 22 px and + `acog` 18 px off centre, although HMW's M4 has no reflex/acog ADS anims. Either the engine
  aligns the sight itself (then auto-align modelled the game) or the workbench misses an offset. Must be answered
  (H1 code + an in-game screenshot of the M4 with reflex/acog in ADS) before "reticle within 3 px" is used as the
  attachment acceptance test. Crossbow `reflex`/`acog`/`mars`/`holo` are 0 px with auto-align off; `reflexmwr` with
  the red-dot override anim is 131.6 px high (housing/anim mismatch).

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
