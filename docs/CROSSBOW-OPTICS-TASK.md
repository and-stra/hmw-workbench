# Task: HMW red dot + ACOG on the AW crossbow (written by Claude, 25 Nov 2026)

Goal: the crossbow takes HMW's `reflex` and `acog` (the MW2-style optics the M4A1 uses), the reticle is centred in
ADS, the built-in ring sight disappears when an optic is mounted, and the `mod` zone is rebuilt. Background and all
findings: `docs\porting\aw.md`. Keep the crossbow without a suppressor attachment (`tag_silencer` stays, unused).

Already done by Claude (don't redo):
- `GAME\zonetool\mod\weapons\iw5_exocrossbow_mp.json`: `attachments = ["acog", "reflex"]`, 5 animOverrides
  (index 1 = acog: `vm_crossbow_acog_ads_up/down` tree 129/130 + `vm_crossbow_empty_idle` tree 2; index 2 = reflex:
  `vm_crossbow_reddot_ads_up/down` tree 129/130). `attachment1` is 1-based (0 = none). Original: `.json.bak`.
- Converter patch for cod-mwx (applied by the MWX side, not by you): `docs\porting\patches\`.

`GAME` = `C:\Games\cod-hwm\Call of Duty Horizon Modern Warfare\Call of Duty Modern Warfare Remastered`.

## 1. Workbench fixes (repo, commit "crossbow optics: workbench fixes")
- Attachment lookup: an attachment json without models (stubs like `workbench_hmw_ar1\attachments\reflex.json`,
  `viewModels: null`) must not shadow the real definition in another root (`workbench_hmw_attachments`). Prefer the
  definition that has models; report the duplicates in `-validate`.
- When the weapon json has an `attachments` list, resolve `animOverrides[].attachment1/2` through it (1-based, 0 = none)
  and H1 `animTreeType` numbers (ADS_UP 129, ADS_DOWN 130, see x64-zt h1 `weapAnimFiles_t`). Keep the M4 family map
  only as a fallback for weapons without a list.
- Bone rename in the command layer + MCP `rename_tag` (old, new): update the name and its hash/script-string
  exactly as x64-zt reads/writes them; round-trip test with a renamed copy. Saving uses the M5 `.bak` rules (an
  existing `.bak` is never overwritten).

## 2. Crossbow models (allowed writes: these two .xmb files in GAME\zonetool\mod\xmodel, + tags.json; .bak rules)
- `vm_crossbow_base`: rename `tag_sight_on` → `tag_sight_front_on` (HMW's `reflex`/`acog` have
  `hideTags = tag_sight_front_on`, so the built-in ring hides exactly when an HMW optic is mounted). Check first that
  no xanim of the crossbow and no field of the weapon json uses `tag_sight_on` (Claude checked: none).
- Re-tune `tag_reflex` and `tag_acog` for the HMW MW2-style models `attach_h2_reflex_vm` and `attach_h2_acog_vm`
  (M5 tuned them for the MWR models `attach_h1_*`). Test through the weapon json path: load the weapon with
  attachment `reflex` / `acog`, state ADS_UP (so the override from the json is used), end of ADS, `-crosshair`:
  reticle within 3 px of the centre.
- `npc_crossbow_base` (world model): add `tag_reflex` / `tag_acog` with the same offsets relative to its own
  `tag_red_dot` / `tag_acog_2` (if those exist; otherwise report), rename `tag_sight_on` the same way if it has one.
  Check with the M8 world view.
- Screenshots in `review\crossbow-optics\`: no optic hip + ADS end, reflex hip + ADS end (`-crosshair`), acog hip +
  ADS end, world model with reflex and acog (orbit), `fit_crossbow.txt`.

## 3. Rebuild the mod zone
- Read how the mod zone is built and installed: `cod-mwx\docs\mods.md`, `cod-mwx\docs\aw-weapon-porting.md`,
  `cod-mwx\tools\aw-weapon-port\README.md` (read-only). `GAME\zone_source\mod.csv` builds the zone `mod`.
- Back up every output file you will replace (`.ff` → `.ff.bak-<date>`, never overwrite an existing backup).
- If the build needs the HMW attachments as references, add `attachment,,reflex` and `attachment,,acog` to
  `GAME\zone_source\mod.csv` (backup first) – only if the build fails without them; write down which it was.
- Build with the H1 x64-zt as documented, install the result where the docs say, log to
  `review\crossbow-optics\build.log`. Zero errors.

## 4. In-game test preparation (don't run the game; the user tests)
- Find in the HMW GSC dumps how a weapon name with attachments is built (e.g. `buildweaponname`, the `_mp_<att>`
  naming) and write the exact console/GSC commands the user needs to get the crossbow with no optic, with reflex and
  with acog, into `docs\porting\aw.md` ("In-game test" section).

## Rules
- Writes outside the repo only: `GAME\zonetool\mod\xmodel\vm_crossbow_base.xmb` and `npc_crossbow_base.xmb`
  (+ tags.json), `GAME\zone_source\mod.csv` (only if needed), and the zone build output + its install location – each
  with a backup first. Never touch cod-mwx (read-only), other dumps, HMW zones, or `.pak` files.
- Don't kill processes you didn't start; if the workbench exe is locked by the Claude app's MCP server, rename it
  (hmw-workbench-prev-<n>.exe) and build again.
- When done: `docs\milestones\crossbow-optics.md` (what changed, backups, measurements, build result, in-game test
  commands), commit, then STOP and report.
