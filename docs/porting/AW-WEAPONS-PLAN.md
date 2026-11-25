# Plan: AW weapons in MWX (written by Claude, 25 Nov 2026)

**Goal:** one fastfile `mwx_aw_weapons.ff` with every approved Advanced Warfare (S1) weapon, shipped with the MWX
game files, loaded by the MWX client after the HMW zones, and selectable in Create-a-Class like HMW's own weapons.
HMW attachments work on them wherever possible; where not, it is documented per weapon. The combat knife joins
HMW's melee weapons (hatchet, karambit, ice pick, shovel, sickle). First milestone: only the crossbow, natively in
Create-a-Class.

Who: a dedicated opencode session (not the agent working on MWX game modes) implements; Claude reviews after every
milestone (the agent STOPS after each one) and checks models/attachments with the hmw-workbench MCP. Until the
isolation setup below is approved, it works read-only on cod-mwx and writes only to the hmw-workbench repo and new
dump folders.

References: `C:\Games\cod-hwm\hmw-workbench\docs\porting\aw.md` (crossbow findings, attachment rules),
`hmw-workbench\README.md` + `docs\workflow-porting.md` (how to check a weapon), `cod-mwx\docs\aw-weapon-porting.md`,
`cod-mwx\tools\aw-weapon-port\README.md`, `cod-mwx\docs\todo-aw-porting.md`, `cod-mwx\docs\mods.md`,
`cod-mwx\docs\hmw-weapons-perks.md`.

`GAME` = `C:\Games\cod-hwm\Call of Duty Horizon Modern Warfare\Call of Duty Modern Warfare Remastered`,
`AW` = `C:\Games\cod-hwm\Call of Duty Advanced Warfare`.

## Weapon list (approved by the user, 25 Nov 2026)

Rules: base weapons only, no cosmetic variants (`*lootN`, royalty/atlas/gold/operator/pro/standard). **No duplicates:**
skip a gun when HMW already has the same real-world weapon. **Real names:** every ported gun gets its real-world name
as display name (localized string, CAC, kill feed); internal asset names stay `iw5_*` (HMW's buildweaponname
handles `iw5_*`). Confirm each identification from the model in P0 (workbench screenshot) before naming it.

### PORT – modern conventional guns
| display name (real gun) | AW name | internal | class |
|---|---|---|---|
| AK-12 | AK12 | iw5_ak12 | AR |
| Beretta ARX160 | ARX-160 | iw5_arx160 | AR |
| CZ 805 BREN (sources also say Robinson XCR – decide from the model) | HBRa3 | iw5_hbra3 | AR |
| SIG MPX | KF5 | iw5_kf5 | SMG |
| CBJ-MS | MP11 | iw5_mp11 | SMG |
| Ameli (MG82) | Ameli | iw5_asaw | LMG |
| LSAT | Pytaek | iw5_lsat | LMG |
| GM6 Lynx | Lynx | iw5_gm6 | sniper |
| UTS-15 | Tac-19 | iw5_uts19 | shotgun |
| MP-443 Grach | MP443 Grach | iw5_pbw | pistol |
| Carl Gustaf M3 MAAWS | MAAWS | iw5_maaws (common_mp, dump it) | launcher |
| Combat knife | Combat knife | iw5_combatknife (common_mp) | melee (optional, last) |

### PORT – classic guns (AW supply-drop DLC weapons)
| display name (real gun) | AW name | internal | class |
|---|---|---|---|
| M1 Garand | M1 Garand | iw5_dlcgun23 | rifle |
| MP40 | MP40 | iw5_dlcgun18 | SMG |
| Sten | Sten | iw5_dlcgun28 | SMG |
| Winchester lever-action rifle (exact model, e.g. Model 1892/1894, from the model) | Lever Action | iw5_dlcgun33 | rifle |
| Blunderbuss | Blunderbuss | iw5_dlcgun4 | shotgun |
| LeMat 1861 revolver | M1 Irons | iw5_dlcgun3 | pistol |

### NOT PORTED – duplicates of HMW guns
StG 44 (iw5_dlcgun6 = HMW `h1_mp44`), M1911 (iw5_dlcgun13 = `h2_colt45`), AK-47 (iw5_dlcgun43), M16 (iw5_dlcgun8),
SVO (iw5_dlcgun7, Dragunov), SAC3 (iw5_sac3, Kriss Vector = `h2_kriss`), MK14 (iw5_m182spr, Mk 14 EBR / M14 family).
P0 still compares every PORT entry against HMW's complete weapon list (all zones, MWR DLC weapons included, by
real-world gun) and reports further duplicates instead of porting them.

### NOT PORTED – fictional or futuristic look without a clean real counterpart (user may approve later)
Atlas 45 (iw5_titan45), ASM1 (iw5_asm1, futuristic Thompson), MAUL/Bulldog (iw5_maul), Repulser (iw5_dlcgun38,
"fm4"), Bal-27, IMR (iw5_himar), SN6 (UMP-based; HMW has the UMP45), S-12 (iw5_rhino), PDW (iw5_vbr), AMR9
(iw5_hmr9), Atlas 20mm (iw5_thor).

Approved later by the user: (none)

### EXCLUDED – futuristic weapons
MORS (railgun), NA-45 (iw5_m990), RW1 (rail pistol), EM1 + EM1 gold (laser), EPM3 (pulse), CEL-3, AE4 (iw5_dlcgun1),
OHM (iw5_dlcgun2), XMG / exo minigun, MDL (incl. jugg variant), MAHEM, Stinger M7 (HMW has a Stinger), AW riot
shield (separate project), Goliath/mech/turret weapons, exo grenades/launchers, Exo Zombies `*zm`, MW3 leftovers.

## Attachment rules (all weapons)

- An attachment's view/world model mounts on the weapon tag named like the attachment model's root bone
  (`reflex` → `tag_reflex`, `acog` → `tag_acog`, `holo` → `tag_holo`, suppressors → `tag_silencer`/`tag_silencer1`,
  …). AW weapons have AW tag names; add the HMW tags with hmw-workbench (`add_tag`, `fit_tag`, ADS auto-align OFF)
  to the view AND world model, as done for the crossbow.
- Use HMW's MW2-style attachments (the set HMW's `h2_*` weapons use): optics `reflex`, `holo`, `acog`, `thermal`;
  `silencerar`/`silencersmg`/`silencerlmg`/`silencershotgun`/`silencersniper`/`silencerpistol` by class; `foregrip`,
  `xmag`, `fmj`, `heartbeat`, `gl`/`glpre` only if the weapon has a sensible mount.
- Weapon json: `attachments` = the HMW names; ADS anims for optics via `animOverrides` (1-based index into the list,
  H1 `animTreeType`) using the AW optic anims (converter `--attachment-map`, see the patch in
  `hmw-workbench\docs\porting\patches\`); hide the weapon's built-in sight with HMW's `hideTags`
  (`tag_sight_front_on` rename trick, or the tag the weapon uses).
- Every weapon gets a row in the attachment matrix (`docs\aw-weapons-attachments.md`): per HMW attachment
  "works" (with screenshot + ADS reticle offset ≤ 3 px) / "added tag" / "not possible: reason".

## Isolation: another agent works in cod-mwx (game modes) – do not interfere
- Never touch `C:\\Games\\cod-hwm\\cod-mwx`'s working tree: no edits, no checkout/branch switch, no stash, no commit,
  no build (`build-mwx.bat`/`build-branch.bat` check out branches and overwrite `mwx.exe` and `GAME\\mwx\\`).
  Reading files is fine.
- Never start, stop or overwrite the game, `mwx.exe`, the dedicated server or the MWX MCP server while the other
  agent may be using them; never write into `GAME\\mwx\\`, `GAME\\players2\\`, `GAME\\hmw-mod\\` or the zone/zone_source
  files the game modes use.
- W0 is read-only (plus new dump folders and the hmw-workbench docs). Reports and docs go to the hmw-workbench repo.
- From W1 on (client code, zone loading, CAC, in-game tests) the work needs its own cod-mwx worktree/branch, its own
  exe name and a time slot when the game is free. W0 proposes exactly how (e.g. `git worktree add` on a new branch
  `aw-weapons`, build output `mwx-aw.exe`, separate copy of `GAME\\mwx` data, merge later); **the user approves it
  before W1 starts.**

## Milestones (STOP after each one and report; Claude reviews before the next)

### W0 – Inventory and decisions (no zone, no game files changed)
- Disk: free space now, space needed (AW `common_mp` dump ~9.3 GB etc.). If less than 20 GB would remain, report.
- Confirm every weapon in the list: szDisplayName, English name, models (view/world), anim count, zone it lives in;
  confirm the real-world identification from a hmw-workbench screenshot (HBRa3: CZ 805 vs XCR; Lever Action:
  Winchester model). Compare against HMW's complete weapon list (all HMW zones incl. MWR DLC weapons, by real gun)
  and list further duplicates.
- Limits: weapon pool / networked weapon limit (fastfiles.cpp pools, weapon index limit, mwx-plan "Risks"). Count
  the weapon assets the full set adds incl. attachment variants. If it does not fit, propose what to cut.
- Create-a-Class integration, read-only: how HMW lists weapons per category (statstable rows via
  `stock_weapons.cpp`, `custom_weapons.gsc` `isvalidprimary`/`isvalidsecondary`/melee lists, CAC LUI menus in the
  HMW zones and how MWX can extend them from `GAME\mwx\ui_scripts`, weapon/attachment unlock tables, camo tables,
  icons, localized strings). Where does a new category entry come from?
- Zone: where `mwx_aw_weapons.ff` lives (GAME\zone? hmw-mod\zone? an MWX folder), how MWX loads it
  (`patches.cpp` zone list, load order after `hmw_*`), how it ships to friends (packaging step in mwx-plan).
- ADS alignment rule (blocks the attachment acceptance test): with hmw-workbench's ADS auto-align off, HMW's own
  M4 + `reflex`/`acog` end 22/18 px off centre (see guide.md "Open question"). Find in the H1 code/decomp how the game
  aligns ADS with a mounted optic (reticle tag, `tag_ads`, attachment fields, adsOverlay…) and state which workbench
  mode matches the game. The user provides in-game screenshots of the M4A1 with red dot and ACOG in ADS as
  reference (`hmw-workbench\references\m4\`); ask for them if they are missing. Update guide.md with the answer.
- Optics shortlist for the crossbow from `hmw-workbench\review\reddots\reddots.md`: `reflex`, `holo` (needs
  `tag_holo`), `mars` (TAR-21 only, needs `tag_tavor_scope`) are 0 px; `reflexmwr` 131.6 px high. Propose the final
  optic list for the crossbow.
- Crossbow category decision: propose the CAC category for the crossbow (HMW has no "special" class; default
  proposal: secondary "Launchers") and for the combat knife (melee list next to the hatchet).
- Conflicts: `mods\sticksandstones\mod.ff` also defines `iw5_exocrossbow_mp`. Decide how both coexist (the mod
  references the weapon from `mwx_aw_weapons.ff`, or the mod keeps its own copy and MWX doesn't load both).
- Write `hmw-workbench\docs\porting\aw-weapons-port.md`: confirmed table, limits, integration plan with file/function names,
  decisions to confirm. STOP.

### W1 – Crossbow natively in Create-a-Class
- New zone `mwx_aw_weapons` (zone_source CSV in cod-mwx, built with x64-zt as documented) containing the crossbow
  exactly as it is now in `GAME\zonetool\mod` (weapon json with `acog`/`reflex`, models with the new tags, anims,
  sounds, fx). Display name "Crossbow".
- MWX loads the zone after the HMW zones; statstable row; CAC category from W0; whitelist via MWX GSC
  `replacefunc` (never edit HMW's zones); attachments `reflex` and `acog` selectable for it in CAC; camos as
  decided in W0 (or disabled with a note).
- Sticks and Stones keeps working (decision from W0).
- Test with the MWX MCP: CAC shows the crossbow in its category (`ui_snapshot`), equip it with no optic / reflex /
  acog, spawn, fire, reload, ADS (reticle centred, ring hidden), kill feed, swap weapons; a bot match; restart the
  game and the class is still there. Screenshots into `mwx-debug\aw-weapons\W1\`.
- `load_times.log` before/after (zone load cost). Commit. STOP.

### W2 – Pilot: M1 Garand, SIG MPX (KF5), combat knife
- Full pipeline per weapon: dump (new folders only) → convert (`s1_weapon_to_h1.py`, patch applied) → hmw-workbench
  check (anims on MWR hands incl. faction hands, fit report, add/tune HMW attachment tags on view + world model,
  ADS checks, hide built-in sights) → add to `mwx_aw_weapons` → statstable/CAC/names/icons → in-game test.
- Combat knife: in the melee list next to the hatchet; check its anims, melee range/damage against HMW's melee
  weapons.
- Attachment matrix rows for the three weapons. Commit. STOP.

### W3 – All remaining weapons, in batches
- Batches of 4–5 (modern first, then classic), same process as W2, one commit per batch, matrix rows per weapon.
  A weapon that fails gets "BLOCKED: reason" in the report and the batch continues.
- After the last batch: STOP with the full status table.

### W4 – Attachment pass
- Go through the whole matrix: fix what can be fixed (missing tags, ADS offsets, hideTags, reticle visibility,
  attachment-specific anims), document the rest with the reason. Every optic: ADS reticle within 3 px.

### W5 – Polish and release
- Real names everywhere (CAC, kill feed, HUD, pickup hints), icons/kill icons, CAC descriptions, balance table vs
  similar HMW weapons (damage, range, RPM, mobility) with proposed tweaks, unlock levels (LAN: unlockall).
- Regression: all HMW weapons still work, weapon pool not overflowing, a full bot match on several maps, MWX game
  mode tests (`run_all_tests`).
- Packaging: final location of `mwx_aw_weapons.ff`, size, load-time cost, how friends install it (MWX packaging
  step). Update `hmw-workbench\docs\porting\aw-weapons-port.md` and `hmw-workbench\docs\porting\aw.md`. Commit. STOP.

## Documentation (required at every milestone)
Every important finding is written down so future ports (more AW weapons, Ghosts, IW7, T7, H2 …) get easier:
- reusable engine/tool knowledge → `hmw-workbench\docs\porting\guide.md` (follow its "How to document" rules);
- AW-specific findings (names, tags, quirks per weapon) → `hmw-workbench\docs\porting\aw.md`;
- every error that took time: exact message, cause, fix → the "Errors and fixes" table in `guide.md`;
- status/progress → `hmw-workbench\docs\porting\aw-weapons-port.md`.
A milestone is not done until these are updated; the milestone report lists which entries were added. Commit the
hmw-workbench doc changes in the hmw-workbench repo (docs only).

## Rules
- Backups before changing any existing file outside the repo (`.bak-<date>`, never overwrite a backup); new dumps
  only in new folders; never modify HMW's `.ff`/`.pak`; keep >= 20 GB free on C:; never copy outside
  `C:\Games\cod-hwm`. AGENTS.md rules of cod-mwx apply (branches, build script, tests).
- hmw-workbench is used read-only from cod-mwx's side, except saving tag edits to the new weapon dumps (its `.bak`
  rules). Its asset roots are set in `hmw-workbench\build\bin\Release\workbench.json` – add the new dump folders.
- The crossbow mod (`mods\sticksandstones`) must keep working at every milestone.
