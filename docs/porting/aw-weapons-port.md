# AW weapons port — W0: inventory and decisions (25 Nov 2026)

Status: **W0 done, W1 not started.** This file is the living report of the AW weapons port
(plan: `AW-WEAPONS-PLAN.md`). Read-only work: no game file, `.ff`, `.pak`, zone or cod-mwx file was
changed. New files are only in new AW dump folders (see §2) and in this repo.

Sources used (all read-only):
- weapon defs: `AW\dump\patch_common_mp\weapons\*.json` and the new `AW\dump\common_mp\weapons\*.json`
  (H2-shaped dumps, same format the converter reads);
- AW English strings: new dump `AW\dump\eng_patch_common\localizedstrings\*` (`WEAPON_*`);
- AW zone manifests `AW\dump\patch_common_mp\patch_common_mp.csv`, `AW\dump\common_mp\common_mp.csv`;
- HMW side: `refs\dumps\hmw_files\scripts\mp_patches\custom_weapons.gsc`,
  `refs\dumps\hmw_files\ui_scripts\mp\create_a_class\*.lua`, `refs\dumps\hmw_files\maps\mp\gametypes\_weapons.gsc`,
  `cod-mwx\src\client\component\{stock_weapons,fastfiles,patches,filesystem,localized_strings}.cpp`,
  `cod-mwx\docs\hmw-weapons-perks.md`, `cod-mwx\docs\mods.md`;
- HMW client code: `refs\hmw_decomp\decomp_*.c`, `refs\hmw_decomp\strings.tsv`, `refs\hmw_bin\hmw-mod.exe`;
- workbench: new renders in `review\aw-weapons\W0\` (logs are the matching `.txt`).

`AW` = `C:\Games\cod-hwm\Call of Duty Advanced Warfare`,
`GAME` = `C:\Games\cod-hwm\Call of Duty Horizon Modern Warfare\Call of Duty Modern Warfare Remastered`.

## 1. Confirmed weapon set

All 18 weapons exist in the installed AW MP content (the old "SP only" statement in
`cod-mwx\docs\aw-weapons-plan.md` is obsolete: `s1_mp64_ship.exe`, `common_mp.ff` and the MP zones
are present).

| planned real name | AW name | internal (asset) | def zone | class (S1) | view model | world model | anims | AW attachments |
|---|---|---|---|---:|---|---|---:|---:|
| AK-12 | AK12 | `iw5_ak12_mp` | patch_common_mp | 0 rifle | `vm_ak12_loot_base` | `npc_ak12_base_loot` | 88 | 20 |
| Beretta ARX160 | ARX-160 | `iw5_arx160_mp` | patch_common_mp | 0 rifle | `vm_arx160_base_loot` | `npc_arx160_base_loot` | 87 | 20 |
| CZ 805 BREN | HBRa3 | `iw5_hbra3_mp` | patch_common_mp | 0 rifle | `vm_hbra3_base_loot` | `npc_hbra3_base_loot` | 86 | 21 |
| SIG MPX | KF5 | `iw5_kf5_mp` | patch_common_mp | 3 SMG | `vm_kf5_base_loot` | `npc_kf5_base_loot` | 87 | 18 |
| CBJ-MS | MP11 | `iw5_mp11_mp` | patch_common_mp | 3 SMG | `vm_cbj_ms_base_loot` | `npc_cbj_ms_base_loot` | 89 | 18 |
| CETME Ameli (see note) | Ameli | `iw5_asaw_mp` | patch_common_mp | 2 LMG | `vm_ameli_base_loot` | `npc_ameli_base_loot` | 85 | 18 |
| LSAT | Pytaek | `iw5_lsat_mp` | patch_common_mp | 2 LMG | `vm_lsat_base_loot` | `npc_lsat_base_loot` | 87 | 18 |
| GM6 Lynx | Lynx | `iw5_gm6_mp` | patch_common_mp | 1 sniper | `vm_gm6_base_loot` | `npc_gm6_base_loot` | 84 | 13 |
| UTS-15 | Tac-19 | `iw5_uts19_mp` | patch_common_mp | 4 shotgun | `vm_uts19_base_loot` | `npc_uts19_base_loot` | 87 | 10 |
| MP-443 Grach | MP443 Grach | `iw5_pbw_mp` | patch_common_mp | 5 pistol | `vm_mp443_base_loot` | `npc_mp443_base_loot` | 86 | 14 |
| Carl Gustaf M3 MAAWS | MAAWS | `iw5_maaws_mp` | **common_mp** | 7 special | `vm_maaws_base` | `npc_maaws_base` | 77 | 0 |
| Combat knife | Combat Knife | `iw5_combatknife_mp` | **common_mp** | 5 pistol¹ | `vm_mp_tactical_knife` | `npc_mp_tactical_knife` | 67 | 0 |
| M1 Garand | M1 Garand | `iw5_dlcgun23_mp` | patch_common_mp (DLC) | 0 rifle | `vm_m1garand_base` | `npc_m1garand_base` | 85 | 21 |
| MP40 | MP40 | `iw5_dlcgun18_mp` | patch_common_mp (DLC) | 3 SMG | `vm_ger_mp40_dlc_base` | `npc_ger_mp40_dlc_base` | 87 | 18 |
| Sten | Sten | `iw5_dlcgun28_mp` | patch_common_mp (DLC) | 3 SMG | `vm_sten_dlc_base` | `npc_sten_dlc_base` | 90 | 18 |
| Browning BLR | Lever Action | `iw5_dlcgun33_mp` | patch_common_mp (DLC) | 0 rifle | `vm_winchester_loot_base` | `npc_winchester_loot_base` | 85 | 20 |
| Blunderbuss | Blunderbuss | `iw5_dlcgun4_mp` | patch_common_mp (DLC) | 4 shotgun | `vm_blunderbuss_base_loot` | `npc_blunderbuss_base_loot` | 83 | 10 |
| LeMat 1861 | M1 Irons | `iw5_dlcgun3_mp` | patch_common_mp (DLC) | 5 pistol | `vm_m1_irons_base_loot` | `npc_m1_irons_base_loot` | 85 | 15 |

¹ AW calls the knife class "pistol" in the asset (weapClass 5); for HMW CAC it becomes a secondary
melee (`weapon_melee`), not a pistol.

"anims" = non-empty entries of `szXAnims` (the H1 state map). All 18 are full weapon anim sets
(idle/fire/reload/raise/sprint/ADS/melee/inspect), i.e. normal ports, not stubs.

### Names: `szDisplayName` is a dev key, not the English name

The English strings are in AW `eng_patch_common` (new dump). Confirmed:

| key | English |
|---|---|
| `WEAPON_AK12` | AK12 |
| `WEAPON_ARX160` | ARX-160 |
| `WEAPON_HBRA3` | HBRa3 |
| `WEAPON_KF5` | KF5 |
| `WEAPON_MP11` | MP11 |
| `WEAPON_ASAW` | **Ameli** |
| `WEAPON_LSAT` | **Pytaek** |
| `WEAPON_GM6` | **Lynx** |
| `WEAPON_UTS19` | **Tac-19** |
| `WEAPON_PBW` | **MP443 Grach** |
| `WEAPON_MAAWS` | MAAWS |
| `WEAPON_COMBAT_KNIFE` | Combat Knife |

Consequence: the internal dev name and the retail name differ (`asaw`→Ameli, `lsat`→Pytaek,
`gm6`→Lynx, `uts19`→Tac-19, `pbw`→MP443 Grach). Never name a weapon after the internal name.

**DLC guns have no English string.** Their `szDisplayName` is a placeholder
(`MDLC6_DLCGUN23`, `MDLC5_DLCGUN18`, `MDLC6_DLCGUN28`, `MDLC6_DLCGUN33`, `MDLC3_DLCGUN4`,
`MDLC3_DLCGUN3`) and there is no `WEAPON_*`/`MDLC*` string in any dumped English zone. Their
identity comes from the model (see below), which is what we use for naming.

### Real-world identification from the models

Renders (untextured — AW images are the known gap, so identification is from geometry):
`review\aw-weapons\W0\hbra3_side.png`, `hbra3_close.png`, `hbra3_34.png`.

- **HBRa3 = CZ 805 BREN** (A1/A2 family): rectangular vent slots in the handguard, full-length
  monolithic top rail continuing over the handguard, the CZ-type side-folding stock with the open
  strut/loop, curved 30-round magazine. The Robinson XCR alternative is rejected: the XCR has a
  SCAR-style upper with round lightening holes in the handguard and a straighter, solid stock.
  Because the render has no textures, this is a geometry decision — Claude/user should confirm from
  the PNGs before W2.
- **Lever Action = Browning BLR** (correction by the user after W0: IMFDB identifies AW's Lever
  Action as the Browning BLR, not a Winchester). Model/zone names are `vm_winchester_loot_base` /
  `mp_vm_leveraction_tr` (AW's internal name is "winchester" — the asset name is not the real gun).
  The W0 render (`review\aw-weapons\W0\leveraction_side.png`) shows a lever rifle with an exposed
  hammer, round lever loop, tubular-looking forend and straight wrist — the BLR is a box-magazine
  lever rifle, so the "tubular magazine" read of the untextured render was wrong. Display name:
  **"Browning BLR"**. A textured render in W5 can confirm the barrel/receiver details.
- **AW assets that also exist in Ghosts (IW6)**: CZ 805 (HBRa3), CBJ-MS (MP11), LSAT (Pytaek) and
  UTS-15 (Tac-19) — Ghosts is a fallback source if an AW asset turns out broken. Not needed for W1.
- The other 12 are unambiguous from model/zone names (`vm_m1garand`, `vm_ger_mp40_dlc`,
  `vm_sten_dlc`, `vm_blunderbuss`, `vm_m1_irons` = LeMat-type revolver with its second barrel
  (`_damage_`/`_range_` model parts)).

**Correction to the plan:** the Ameli is not an "MG82". The model is `ameli` (CETME Ameli, Spanish
5.56 LMG) and AW's name is Ameli; propose display name **"Ameli"** (or "CETME Ameli"), not MG82.

### Duplicates against HMW's complete weapon list

Compared with `cod-mwx\docs\hmw-weapons-perks.md` §1.2/§1.3 (HMW runtime statstable + MWR content,
i.e. every weapon HMW can offer, MWR DLC loot guns included):

| AW weapon | HMW has | verdict |
|---|---|---|
| AK-12 | AK-47 (`h2_ak47`, `h1_ak47`), AK-74u | different rifle → port |
| ARX160 | – | port |
| HBRa3 (CZ 805) | – | port |
| KF5 (SIG MPX) | MP5/MP5K, MP7, PM-9, UMP45, P90, Uzi, Vector, Skorpion | different SMG → port |
| MP11 (CBJ-MS) | – | port |
| Ameli (CETME Ameli) | MG4, M240, M60E4, RPD, M249, L86, PKM, AUG HBAR, Bered MK8 | different LMG → port |
| Pytaek (LSAT) | M249 SAW (Minimi) | different weapon (LSAT is a separate prototype) → port |
| GM6 Lynx | Barrett, Intervention, WA2000, M21, M40A3, MSR, AS50, D-25S, L118A, USR, Dragunov, R700, S-Tac | no GM6; **name clash only**: MWR loot AR `h1_augast` is called "Lynx CQ300" in HMW. Display name "GM6 Lynx" avoids it → port |
| Tac-19 (UTS-15) | M1014, W1200, SPAS-12, AA-12, Striker, Model 1887, Ranger, KSG, Kamchatka-12 | different shotgun → port |
| MP443 Grach | M9, M1911, USP, Desert Eagle, P226, MP412, Anaconda, BR9, Prokolot, .44 Magnum | different pistol → port |
| MAAWS (Carl Gustaf) | AT4, RPG-7, M79, M320, Stinger, Javelin | different launcher → port |
| Combat knife | melee: shovel, ice pick, hatchet, sickle, karambit (+ 28 MWR melee skins); every gun's melee-attack knife model is `wpn_h1_melee_combat_knife_vm` | no knife *item* → port as secondary melee; note it looks like the already-used melee-attack knife |
| M1 Garand | M14/M21 (M14 family) | different rifle → port |
| MP40 | MP44/StG44 (`h1_mp44`) | different gun → port |
| Sten | – | port |
| Lever Action (Browning BLR) | Model 1887 (`h2_model1887`, lever-action *shotgun*) | different gun → port |
| Blunderbuss | – | port |
| M1 Irons (LeMat) | Anaconda, MP412 (revolvers) | different revolver → port |

The plan's own "NOT PORTED" list is confirmed by the dumps: `iw5_dlcgun6` = `vm_stg44_base`
(StG44), `iw5_dlcgun13` = `vm_m1911_base`, `iw5_dlcgun43` = `vm_ak47_base_skulls`,
`iw5_dlcgun8` = `vm_m16_operator02`, `iw5_dlcgun7` = `vm_dragunov_dlc` (SVO/Dragunov),
`iw5_sac3` = `vm_sac3` (Vector), `iw5_m182spr` = `vm_mk14_ebr`. No *new* duplicates were found.

### Asset-name collisions

Checked every available HMW/MWR zone manifest (`refs\dumps\patch_common_mp`, `hmw_patch_common_mp`,
`GAME\dump\hmw_ar1`, `mod_sticksandstones`): no `iw5_*` weapon asset besides the crossbow. The
`iw5_*` namespace is free for our 18 names. (HMW's `iw5_lsr_mp` lives in a zone we cannot dump
without hardlinking; it is not in our set.)

## 2. Disk and limits

Free disk: **30.81 GB** before W0, **30.82 GB** after (new dumps are tiny).

New dumps created in W0 (all new files/folders, nothing overwritten):

| path | content | size |
|---|---|---:|
| `AW\dump\common_mp\weapons` | 413 H2-shaped weapon JSONs (incl. `iw5_maaws_mp`, `iw5_combatknife_mp`) | 23.9 MB |
| `AW\dump\eng_patch_common\localizedstrings` | 2,127 AW English strings (all `WEAPON_*`) | 0.1 MB |
| `AW\dump\eng_common_mp`, `eng_common_core_mp`, `eng_code_post_gfx_mp`, `eng_code_post_gfx`, `eng_patch_common_mp` | localize-only dumps (only `eng_patch_common` had useful strings) | < 1 MB |
| `AW\dump\weapon_models` (xmodel/xsurface) | HBRa3 view+world models, Lever Action view model (+ crossbow models) | 8.1 MB |

Budget for W1–W3 (estimate, targeted dumps only): 18 weapons × (models ≈ 10 MB + anims ≈ 10 MB +
materials/techsets ≈ 2 MB) ≈ 0.4 GB; images (the open gap) ≈ 0.5–1 GB; sounds ≈ 0.1–0.3 GB; built
`hmw_aw_weapons.ff` ≈ 0.1–0.4 GB; client build ≈ 0.2 GB. **≈ 2 GB total**, so ~28.8 GB stay free.
Do **not** run the full `common_mp` (8.9 GB) or `patch_common_mp` (7.2 GB) dumps; use targeted asset
types and delete the per-weapon tr-zone dumps after the zone is built. Rule from the plan: keep
≥ 20 GB free.

Limits:
- Weapon asset pool: `fastfiles.cpp` `asset_pool_sizes[ASSET_TYPE_WEAPON]` = 200, MWX doubles it to
  **400** (`reallocate_weapon_pool()`, multiplier 2); attachment pool 128 → **256**
  (`reallocate_attachment_pool()`), xanim 5000 → 10000, loaded_sound and localize also doubled.
- The set adds **18 weapon assets** (one per gun) and **0 attachment assets** (HMW's attachments are
  reused), **0 camo assets** (camo disabled for AW guns initially).
  Attachment and camo *variants* are not separate weapon assets: HMW ships one asset per gun
  (`GAME\dump\hmw_ar1\hmw_ar1.csv` has 13 `weapon,` lines for the zone's 9 ARs plus shared
  launchers/mode weapon — no per-attachment assets), and `buildweaponname` builds the suffix name at
  runtime, so the pool does not grow per attachment combination.
- Current occupancy: HMW's own table has **85** weapon rows (`mwx-logs\statstable_dump.csv`, classes
  `weapon_*`), MWX adds up to **43** MWR stock rows (`stock_weapons_data.hpp`) — ~128 rows, plus
  killstreak/script-only weapons. 18 more is ~4% of the 400-slot pool. **It fits**; no cuts needed.
- The "networked weapon index" risk is **not confirmed** in the MWX code (no weapon index limit in
  `src\client`; the pool is the only hard cap). MWX already runs with 43 extra MWR weapons, so +18 is
  not a new class of load. Verify in W1 in a LAN match (second client sees the gun, kill feed, and
  can pick it up) and, if a DEBUG build is available, with `poolUsage 39` / `poolUsages`
  (`fastfiles.cpp:1441-1471`, DEBUG-only).
- Server/client must both have the zone (LAN, no download): friends need `hmw_aw_weapons.ff`.

## 3. Create-a-Class: how a new weapon appears (read-only findings)

Three data layers, no Lua change needed for existing categories:

1. **`mp/customweapons.csv`** — the CAC item lists. `create_a_class\__init__.lua:172-205` reads
   columns `[0]=ref`, `[1]=class`, `[2]=type` ("Primary"/"Secondary") and appends
   `{ref, 0}` to `Cac.Weapons[type][class]`. This is why HMW's `create_a_class\__init__.lua:2-32`
   pre-creates empty tables per class (`weapon_assault` … `weapon_melee`).
2. **`mp/statstable.csv`** — item details and the server-side weapon list. 67 columns in HMW's
   current table; `_weapons::init` (`maps\mp\gametypes\_weapons.gsc:69-95`) builds
   `level.weaponlist` from rows whose column 2 starts `weapon_`, column 51 is empty, the last column
   is not `Never` and column 4 (ref) is non-empty. The CAC Lua reads `StatsTable.Cols.Name`,
   `.Image`, `.WeaponRef`, `.WeaponDesc`, `.WeaponId`, `.Ref`, `.ExternalRef`, `.GUID` for
   name/icon/description/unlock.
3. **GSC whitelists** — `scripts\mp_patches\custom_weapons.gsc`:
   `isvalidprimary` (25 real refs + `h2_inspect`), `isvalidsecondary` (21 guns + 5 melee + `none`),
   `isvalidmeleeweapon` (only deathstreak refs), `isvalidattachment`. An invalid primary/secondary is
   silently replaced by the default class. HMW itself installs them with `replacefunc`
   (`custom_weapons.gsc:4-11`), so MWX can do the same from `data\mwx\scripts\mp\`.

MWX hooks that already exist:

| what | file / function |
|---|---|
| append rows to `mp/statstable.csv` **and** `mp/customweapons.csv` | `src\client\component\stock_weapons.cpp` → `build()`; rows in `stock_weapons_data.hpp` (`rows[][68]`, `row_count`); gated by `weapon_loaded(ref)` = `<ref>_mp` asset exists |
| localized names | `src\client\component\localized_strings.cpp` → `localized_strings::override(key, value)` (hook at `0x585DA0`); used by `factions.cpp`, `gamemodes.cpp` |
| zone list | `src\client\component\patches.cpp` → `ui_init_stub()`, vector loaded after the HMW zones, before the mod zone |
| GSC overrides | `data\mwx\scripts\mp\*.gsc` (mirrored to `GAME\mwx\scripts\mp`, loaded like mods) |
| client data path | `src\client\component\filesystem.cpp:68` `filesystem::register_path(L"mwx")` |

So a new weapon needs **one new row in `stock_weapons_data.hpp`** (67 values: id, class, `WEAPON_*`
key, ref, image, description, attachment refs, stats, unlock columns) + a `localized_strings`
override + the GSC whitelist entry + the ref in `customweapons.csv` (done by the same table patch).
Icon: `image` column (HUD material) and the weapon def's `killIcon`; AW icons exist
(`hud_icon_ak12`, `launcher_maaws`, …) but their images are in the AW image paks (see §6 gap) — map
to HMW icons or port the icon images in W5.

**Gap found in W0 (W1 code task):** `stock_weapons.cpp` writes only columns 0 (ref) and 1 (class)
into `mp/customweapons.csv`, but `create_a_class\__init__.lua` also reads **column 2 = type**
(`"Primary"`/`"Secondary"`). With an empty type, `Cac.Weapons[weapon_type]` is nil and the whole
append loop (inside a `pcall`) aborts at the first such row. So the 43 MWR stock rows most likely do
**not** appear in CAC today (they still reach `level.weaponlist` through statstable). W1 must extend
`stock_weapons.cpp` (or the row data) with the type column: Primary for
assault/smg/sniper/heavy/shotgun, Secondary for pistol/machine_pistol/secondary_shotgun/projectile/
melee (crossbow → Secondary). Verify in game with `ui_snapshot` before W2.


Category tabs (`create_a_class\loadout_editor.lua:1-35`) are hardcoded:
- Primary: **Assault, SMG, Sniper, LMG**
- Secondary: **Machine Pistols, Shotguns, Handguns, Launchers, Melee**

Consequences for our list:
- ARs → Assault, SMGs → SMG, LMGs → LMG, GM6 → Sniper, pistols → Handguns, MAAWS → Launchers
  (secondary), combat knife → Melee (secondary), crossbow → Launchers (secondary, see §6).
- **The UTS-19 and the Blunderbuss have no primary tab** (HMW has no primary shotguns; the
  `weapon_shotgun` table exists in `__init__.lua:6` but there is no tab). Two options:
  (a) put them in the secondary Shotguns tab (`weapon_secondary_shotgun`) — works with zero UI
  work, but they become secondaries; (b) add a primary "Shotguns" tab — needs an MWX override of
  HMW's compiled `loadout_editor.lua` (the tab tables are file-local, so an additive `ui_scripts`
  module cannot add a tab; a file shadow under `GAME\mwx\ui_scripts\mp\create_a_class\` must be
  proven first). **Decision needed** (§8).
- A new *category* (e.g. "Special") would need `loadout_editor.lua` tabs + `__init__.lua` table +
  `Cac.GetWeaponTypeFromWeaponWithoutCategory` (utils.lua:169-178) + a statstable class name; not
  worth it for two weapons.

## 4. Zone `hmw_aw_weapons.ff`: build, install, load, ship

- **Build source:** new `GAME\zone_source\hmw_aw_weapons.csv` (new file only; never touch existing
  CSVs) + `GAME\zonetool\hmw_aw_weapons\` (assets of the same layout as `GAME\zonetool\mod`:
  `weapons\`, `xmodel\`, `xsurface\`, `xanim\`, `materials\`, …; a copy/extension of the crossbow
  dump with the new tags). Build with H1 x64-zt (`GAME\x64zt.exe`, working dir `GAME`):
  `buildzone hmw_aw_weapons` → `GAME\zone\hmw_aw_weapons.ff`.
- **Install (proposal):** `GAME\mwx\zone\hmw_aw_weapons.ff`. `GAME\mwx` is an MWX-owned file-system
  path (`filesystem.cpp:68`, highest priority) and the zone lookup goes through the engine's
  `Sys_CreateFile(SF_ZONE, "<name>.ff")`, so `<path>\zone\<name>.ff` is found. This keeps the zone
  out of `GAME\zone` (game files) and `GAME\hmw-mod\zone` (HMW content, repacked by
  `mwx.exe -repack`) and out of the mod folders.
  Alternative if the path order ever bites: `GAME\hmw-mod\zone\` (same place as `hmw_ar1`), but that
  folder is HMW's and is rewritten by repack/packaging.
- **Load:** add `"hmw_aw_weapons"` to the `zones` vector in `patches.cpp::ui_init_stub()`, **after**
  the `hmw_*` zones and **before** `zones.emplace_back("mod")` (so a mod can reference the crossbow
  and MWX can override HMW assets). `try_load_all_zones()` skips it if the file is missing, so a
  client without the zone still starts.
- **Ship to friends:** `build-mwx.bat` mirrors `data\mwx` → `GAME\mwx` (robocopy `/MIR`), so the
  canonical repo location is **`cod-mwx\data\mwx\zone\hmw_aw_weapons.ff`** (≈ 0.1–0.4 GB). Note
  `/MIR` deletes files in `GAME\mwx` that are not in the repo — the zone must be committed in
  `data\mwx\zone\` or it disappears on the next normal build. Optional `mwx.exe -repack
  hmw_aw_weapons` (LZ4) for load time; `-repack` currently targets `hmw-mod\zone`, so a separate
  call/param is needed if we want it — **not required for W1** (measure `load_times.log` first).

## 5. ADS alignment: the answer

**The engine does not align the sight. The game plays an *optic-specific ADS animation* that keys
only the `tag_ads` bone.**

Mechanism (verified in the dumps and the client code):

1. An attachment defines weapon-field overrides. HMW's `reflex`/`acog`/`reflexmwr`/`acogmwr`
   attachments carry `fields` = `szXAnims` with anim `ADS_UP`/`ADS_DOWN`, `code 1`, value
   `"_ads _reflex_ads"` / `"_ads _acog_ads"` (`GAME\dump\workbench_hmw_attachments\attachments\reflex.json`,
   `acog.json`). The field replaces the token in the weapon's anim name, so a weapon whose ADS anim
   is `vm_h2_m4a1_ads_up` plays `vm_h2_m4a1_reflex_ads_up` when the reflex is mounted.
2. Those anims are real assets shipped in **`hmw_attachments.ff`** (dumped into
   `GAME\dump\workbench_hmw_attachments\xanim\`): `vm_h2_m4a1_reflex_ads_up` = 13 frames, **1 bone
   `tag_ads`** (rot 11 keys, trans 12); `vm_h2_m4a1_acog_ads_up` = 13 frames, **1 bone `tag_ads`**
   (rot 1, trans 10); base `vm_h2_m4a1_ads_up` = 12 frames, `tag_ads` rot 8/trans 10. The zone dump has
   1,136 xanims in total, including 30 `*_reflex_ads_up` and 26 `*_acog_ads_up` for HMW's weapons.
   `tag_ads` is a bone of the hands rig and the parent of `tag_torso → tag_weapon → j_gun`, so moving
   it moves the whole gun: the optic-specific ADS anim is the alignment.
3. Therefore: if the optic-specific anim exists, the optic is centred by the authored anim; if it
   does not exist (or the attachment has no matching `fields`), the base ADS anim plays and the optic
   sits where its tag is — off by the model/tag difference. The engine has no reticle-based
   auto-align step.

Evidence that the workbench models this correctly (auto-align **off**, 1920×1016, `-posedump`):

| case | effective ADS anim | reticle screen | offset |
|---|---|---|---:|
| `h2_m4_mp` + HMW `reflex` | `vm_h2_m4a1_reflex_ads_up` | (958.1, 510.0) | **2.8 px** |
| `h2_m4_mp` + HMW `acog` | `vm_h2_m4a1_acog_ads_up` | (958.5, 507.4) | **1.6 px** |
| `h2_m4_mp` + MWR `reflexmwrm4` (FINISH) | `vm_h2_m4a1_reflex_ads_up` | (957.9, 486.7) | 22.1 px |
| `h2_m4_mp` + MWR `acogmwrm4` (FINISH) | `vm_h2_m4a1_acog_ads_up` | (958.2, 526.0) | 18.1 px |
| `iw5_exocrossbow_mp` + `reflex` | `vm_crossbow_reddot_ads_up` | (960.0, 508.0) | 0.0 px |
| `iw5_exocrossbow_mp` + `acog` | `vm_crossbow_acog_ads_up` | (960.0, 508.0) | 0.0 px |

The 22/18 px from FINISH is **not** an auto-align failure: those runs mounted the *MWR* optic models
(`attach_h1_optic_reflex_vm`, `attach_h1_scope_acog_vm`) on the H2 M4's **MW2** rail tags. Same
`tag_ads` anim, different reticle height in the model → real misalignment (which is exactly what a
wrong optic set does in game). With the attachments HMW actually offers on the M4 (`reflex`,
`acog`), the M4 is within the 3 px bar. New logs: `review\aw-weapons\W0\m4_{reflex,acog}_fp.txt`,
`h1m4_reflexmwr_fp.txt` (the MWR M4 test is inconclusive because the MWR weapon anim set is not in
the dumps).

The crossbow's `vm_crossbow_reddot_ads_up` / `vm_crossbow_acog_ads_up` are also single-bone
`tag_ads` anims (10 frames, rot 9/trans 11), which is why the fitted tags + AW optic anims measure
0 px.

**Rule for the port (write-up lives in `guide.md` §3):** with the workbench's ADS auto-align OFF
(default), an optic is accepted when the reticle is ≤ 3 px from centre at the end of ADS_UP. The
game aligns via the optic-specific `tag_ads` ADS anim; a port must therefore either
(a) ship an ADS_UP/DOWN anim named `<weapon>_<optic>_ads_up/down` that keys `tag_ads` (the HMW way),
or (b) list explicit `animOverrides` per attachment (the crossbow way). If neither exists, the base
ADS anim plays and the optic will be off in game too.

In-game reference: `hmw-workbench\references\m4\m4-ads.png` exists but shows the **iron sights**
(no optic), and `m4-hip.png`. The requested M4A1 + red dot / + ACOG in-game ADS screenshots are
still **missing** — they are no longer needed for the rule (the anims + measurements answer it), but
they would be a useful final cross-check (§8).

## 6. Crossbow optics shortlist (from `review\reddots\reddots.md`)

| optic | ADS reticle | mount tag | verdict |
|---|---:|---|---|
| `reflex` (HMW MW2, `attach_h2_reflex_vm`) | 0 px | `tag_reflex` (real, fitted) | **keep** (already in the weapon json) |
| `acog` (HMW MW2, `attach_h2_acog_vm`) | 0 px | `tag_acog` (real, fitted) | **keep** (already in the weapon json) |
| `holo` (`attach_h2_holo_vm`) | 0 px | `tag_holo` (needs adding) | **optional** — add `tag_holo` if the user wants three optics; housing is large but clears the limbs/string |
| `mars` (`attach_h2_mars_vm`, TAR-21-only) | 0 px | `tag_tavor_scope` | **skip** — single-weapon model, not HMW's general set; needs a foreign tag |
| `reflexmwr` (`attach_h1_optic_reflex_vm`) | 131.6 px high | `tag_reflex` | **reject** — MWR housing on an MW2-fitted tag, no `hideTags`, pink/checker materials |
| `variant` (`attach_h1_optic_reflex_variant_vm_mp`) | blocked | `tag_reflex` | **reject** — its surface set is missing from all dumps |
| thermal | not tested | `tag_thermal_scope` exists on the crossbow | **open** — test in W4 if wanted; HMW `thermal` has a `hideTags` for the ring |

Proposal: ship the crossbow with **`reflex` + `acog`** (as now). Add **`holo`** only if the user
asks (one new tag, 0 px). No `reflexmwr`/`mars`/`variant`.

## 7. Sticks and Stones conflict (`mod.ff` also defines `iw5_exocrossbow_mp`)

Facts: `GAME\zone_source\mod.csv` (internal zone name `mod`) builds the crossbow weapon + models +
fx/sounds into `mods\sticksandstones\mod.ff`; `mods.md` documents the mod. MWX loads zone `mod` last
(`patches.cpp`). If `hmw_aw_weapons.ff` also contains the same asset name, the same asset exists in
two loaded zones — order-dependent and unsafe (H1 does not guarantee which wins).

**Proposal (one owner):** the crossbow moves to `hmw_aw_weapons.ff`; `mod.ff` stops shipping its
own copy and only references it:

```text
// GAME\zone_source\mod.csv after W1
weapon,,iw5_exocrossbow_mp
```

and the model/fx/sound lines are dropped (they come from `hmw_aw_weapons`). Because MWX always loads
`hmw_aw_weapons` before `mod`, Sticks and Stones keeps working. The mod is rebuilt once in W1, in
the approved time slot, and tested (sas gametype on a map, crossbow given, kill feed, bankrupt
logic). `mod.json`'s `WEAPON_EXPLOSIVE_EXO_CROSSBOW` string override becomes redundant but harmless.
Fallback if a reference-only weapon in `mod.ff` does not resolve: keep the crossbow in `mod.ff` for
the mod and ship a **second, separate** crossbow asset in `hmw_aw_weapons` under a different internal
name (e.g. `iw5_exocrossbow_mp` vs `iw5_exocrossbow_cac_mp`) — ugly (two definitions, two CAC rows)
and only if the reference fails. Test it in W1.

## 8. Isolation setup for W1 (approved 25 Nov 2026: **local clone**, not a worktree)

The other agent works in `C:\Games\cod-hwm\cod-mwx` and owns `mwx.exe`, `GAME\mwx`,
`GAME\players2`, the MCP server and the game. W1 used:

1. **Clone/branch:** `git clone C:\Games\cod-hwm\cod-mwx C:\Games\cod-hwm\cod-hmw-aw` (local clone, hardlinked
   objects) then `git checkout -b aw-weapons origin/mwx`. Nothing in `cod-mwx` is written (no worktree metadata, no
   checkout, no build). All W1 commits go to `aw-weapons`; Claude merges it later.
2. **Exe name:** `GAME\hmw-aw.exe`. Build in the clone with `premake5 vs2022` + `msbuild build\hmw-mod.sln`
   (**not** the build script: its install step overwrites `GAME\mwx.exe` and mirrors `data\mwx` ? `GAME\mwx` with
   `/MIR`), then copy `build\bin\x64\Release\mwx.exe` ? `GAME\hmw-aw.exe`.
3. **Data copy:** `data\mwx` ? `GAME\hmw-aw`, zone ? `GAME\hmw-aw\zone`. The client is started with
   `-datadir hmw-aw -logdir hmw-aw-logs -debugport 27079` (test-only commit); `GAME\mwx`, `GAME\mwx.exe` and
   `GAME\players2` stay untouched. The zone list loads `hmw_aw_weapons` from `GAME\hmw-aw\zone` (data path).
4. **When to test:** only in a time slot the user confirms the game/dedicated server/MCP are free. Never start or
   stop their processes. New `zone_source\hmw_aw_weapons.csv` and the `zone_source\mod.csv` change (our own mod,
   `.bak` first) are the only zone_source edits.
5. **Test artifacts:** screenshots/logs to `mwx-debug\aw-weapons\W1\` (new folder); zone source in
   `GAME\zonetool\hmw_aw_weapons`; the built .ff/.pak to the test data dir and later to
   `cod-mwx\data\mwx\zone\` for the normal build.
6. `players2`: the first run of `hmw-aw.exe` writes the same profile the game uses; the user tests, so this is
   their call (a copy of `GAME\players2` before the first test is optional).
7. **Merge:** after W1 passes, Claude merges `aw-weapons` into `mwx`; the normal build then installs `mwx.exe` +
   `GAME\mwx\zone\hmw_aw_weapons.ff`.

## 9. Decisions (answered by the user on 25 Nov 2026)

1. Crossbow optics: **`reflex` + `acog` only**. 2. Crossbow: **secondary -> Launchers**. 3. Combat knife:
   **secondary Melee**, W4. 4. Sticks and Stones: **crossbow moves into `hmw_aw_weapons.ff`; `mod.ff` references it**
   (rebuilt in W1). 5. UTS-19/Blunderbuss: **secondary Shotguns** (later milestones). 6. HBRa3 = **CZ 805 BREN**.
   7. Isolation: **local clone** (`cod-hmw-aw`), test client `hmw-aw.exe` + `GAME\hmw-aw`. 8. No in-game M4 optic
   screenshots needed; the <= 3 px ADS test with auto-align OFF stands. Also corrected: the Lever Action is a
   **Browning BLR** (not a Winchester).

## 10. W1 starting checklist (done in W1 - see §11)

- Crossbow zone: copy `GAME\zonetool\mod` → `GAME\zonetool\hmw_aw_weapons` (weapon json with the
  fitted tags + `attachments = ["acog","reflex"]`, models, anims, sounds, fx), new
  `zone_source\hmw_aw_weapons.csv`, `buildzone hmw_aw_weapons`.
- `stock_weapons_data.hpp` row for the crossbow (`iw5_exocrossbow`, class `weapon_projectile`,
  `WEAPON_EXPLOSIVE_EXO_CROSSBOW` → "Crossbow"); `customweapons.csv` row comes from the same patch —
  **add the missing type column** (`"Secondary"`) in `stock_weapons.cpp` (see §3).
- GSC: extend `isvalidsecondary` with `iw5_exocrossbow` (and, if the user picks it, melee with the
  knife later) via `replacefunc` in `data\mwx\scripts\mp\`.
- `patches.cpp`: load `hmw_aw_weapons` after `hmw_*`, before the mod zone.
- `mod.ff`: reference-only crossbow (decision 4).
- Test matrix from the plan (CAC snapshot, no optic/reflex/acog, spawn/fire/reload/ADS/kill feed,
  bot match, restart), `load_times.log` before/after.


## 11. W1 - the crossbow natively in Create-a-Class (25 Nov 2026)

Status: **done, built and installed; in-game verification is the user's step** (this agent must not launch the game).
Commits:
- client: `cod-hmw-aw` branch `aw-weapons` - `60012370` (feature) and `6da28dfe` (test-only flags). No push.
- docs/review: hmw-workbench `f2436b1` (W1 report, guide/aw updates, review\aw-weapons\W1).
Zone built: `hmw_aw_weapons.ff` 19,877,776 B + `hmw_aw_weapons.pak` 9,236,400 B, **0 FATAL / 0 ERROR**, 11 image
warnings (the known missing crossbow scope/glass textures, see 11.6).

### 11.1 Decisions taken (user, 25 Nov 2026)

1. Crossbow optics: `reflex` + `acog` only (no `holo`). 2. Crossbow: secondary -> Launchers.
3. Combat knife: secondary Melee in W4. 4. Sticks and Stones: crossbow moves into `hmw_aw_weapons.ff`, `mod.ff`
referencing it. 5. UTS-15/Blunderbuss: secondary Shotguns later. 6. HBRa3 = CZ 805 BREN.
7. Isolation: **local clone** (not a worktree). 8. No M4 optic screenshots needed; the <= 3 px ADS test stands.
Naming: the new fastfile is **`hmw_aw_weapons.ff`**; the test client is `hmw-aw.exe` + `GAME\hmw-aw`; the repo is
`cod-hmw-aw`. Existing paths (`cod-mwx`, `GAME\mwx`, `mwx.exe`) keep their names until the user renames them.

### 11.2 Isolation actually used

- `git clone C:\Games\cod-hwm\cod-mwx C:\Games\cod-hwm\cod-hmw-aw` (local clone; the shared objects are
  hardlinked, no copy of the working tree), then `git checkout -b aw-weapons origin/mwx` (5cdcf64b). Nothing in
  `cod-mwx` was written: no branch, no checkout, no build, no edit.
- The clone had the `deps/imgui` submodule empty; it was initialised from the parent's module repo
  (`git -c protocol.file.allow=always submodule update --init deps/imgui`, URL temporarily pointed at
  `cod-mwx\.git\modules\deps\imgui`). The other submodules are vendored files in this repo. **imgui is not part of
  the Release build** (no reference in premake5.lua).
- Build without the install step: `tools\premake5 vs2022` + `msbuild build\hmw-mod.sln /p:Configuration=Release
  /p:Platform=x64` (build script *not* used, it would copy into `GAME\mwx.exe` and mirror `/MIR` into `GAME\mwx`).
  Output `build\bin\x64\Release\mwx.exe` copied manually to `GAME\hmw-aw.exe`; `data\mwx` mirrored to
  `GAME\hmw-aw`; the zone copied to `GAME\hmw-aw\zone`.
- Test client start line:
  `GAME\hmw-aw.exe -datadir hmw-aw -logdir hmw-aw-logs -debugport 27079`
  (flags from the test-only commit; the debug port keeps it away from the other instance's 27099/27098).
- `GAME\mwx.exe`, `GAME\mwx`, `GAME\players2` untouched by us; the game was not started.

### 11.3 Zone

- Source: `GAME\zonetool\hmw_aw_weapons` (copy of the fixed `GAME\zonetool\mod` crossbow source: weapon json with
  `attachments = ["acog","reflex"]`, the fitted view/world model tags, 64 xanims, sounds, fx) and
  `GAME\zone_source\hmw_aw_weapons.csv` (new file; same content as the old mod.csv).
- Build: `GAME\x64zt.exe` (H1), `buildzone hmw_aw_weapons`, 8.5 s. Writes `GAME\zone\hmw_aw_weapons.ff` + `.pak`
  (the pak carries the streamed images; **the .pak must be installed next to the .ff** - the old Sticks and Stones
  install was missing `mod.pak`, which is why its streamed crossbow textures never loaded).
- Install: `GAME\hmw-aw\zone\hmw_aw_weapons.ff` + `hmw_aw_weapons.pak`. The build outputs were then removed from
  `GAME\zone` (they are not game files; rebuild = build then copy again).
- Load: `patches.cpp::ui_init_stub()` explicit zone vector, entry `"hmw_aw_weapons"` after `hmw_dlc_killstreaks`
  and before `zones.emplace_back("mod")`. The client has **no glob/prefix zone scan** - `fastfiles.cpp` also loads
  the fixed engine zones (`hmw_pre_gfx`, `hmw_files`, `hmw_post_gfx`, `hmw_common`), so the explicit list is the
  only place that can load it and it is loaded exactly once per `ui_init`, same as the other `hmw_*` zones.
- Shipping location when this merges: `cod-mwx\data\mwx\zone\hmw_aw_weapons.ff` (+ `.pak`), mirrored to
  `GAME\mwx\zone` by `build-mwx.bat`. **Caveat:** that mirror is `robocopy /MIR`, so a zone that exists only in
  `GAME\mwx` is deleted by the next build; it must be committed in `data\mwx\zone\` (or the packaging step must
  copy it). Not committed in W1 (build artifact), decide in W5.

### 11.4 Client changes (`cod-hmw-aw`, branch `aw-weapons`)

Commit `60012370` (feature):
- `src\client\component\patches.cpp`: `hmw_aw_weapons` in the zone list (see 11.3).
- `src\client\component\stock_weapons_data.hpp`: one 68-value row for `iw5_exocrossbow`
  (`weapon_projectile`, `WEAPON_EXPLOSIVE_EXO_CROSSBOW`, image `hud_icon_exocrossbow`, desc key
  `WEAPON_EXPLOSIVE_EXO_CROSSBOW_DESC`, attachments `reflex`,`acog` in columns 11/12, launcher-like display stats,
  GUID `-1` = no inventory item, class type derived from the class). `row_count` 43 -> 44.
- `src\client\component\stock_weapons.cpp`:
  - **type column fix:** `mp/customweapons.csv` rows now fill column 2 (`Primary`/`Secondary`, derived from the
    statstable class). Before this, the table got only `ref` + `class`; `create_a_class\__init__.lua` reads
    `[0]=ref, [1]=class, [2]=type` and its `pcall` loop aborts at the first row with an empty type, so appended
    rows did not reach the CAC. (The 43 MWR stock rows had the same problem; they now get their type too.)
  - a small component that sets the localized strings: `WEAPON_EXPLOSIVE_EXO_CROSSBOW` = "Crossbow",
    `WEAPON_EXPLOSIVE_EXO_CROSSBOW_DESC` = "Bolt-action crossbow. Fires explosive bolts that stick and detonate."
    (from AW's English string; HMW has no strings for AW assets).
- `data\mwx\scripts\mp\aw_weapons.gsc` (new, loaded from the data dir as a script module):
  - `init()` runs after every script's `main()` (the loader runs all `main()`s at `scr_load_structs`, all `init()`s
    at `scr_load_level` - `scripting.cpp`), so it wins over HMW's replacements.
  - `replacefunc(_class::isvalidsecondary, ::aw_isvalidsecondary)` - accepts `iw5_exocrossbow`, delegates
    everything else to `scripts\mp_patches\custom_weapons::isvalidsecondary`.
  - `replacefunc(_class::buildweaponname, ::aw_buildweaponname)` - **fixes an HMW bug:** HMW's
    `custom_weapons.gsc::buildweaponname` has no `return` in its non-`h1_/h2_` branch, so **any `iw5_*` weapon
    (with or without an attachment) got an undefined weapon name** from the class path. Our wrapper builds
    `<ref>_mp[_<attachment>]` for `iw5_*` (attachment only if it is in the weapon's statstable attachment list)
    and delegates everything else to HMW's function. Camo suffixes are deliberately not added (no AW `_camoNN`
    variants exist), so a selected camo is dropped instead of producing an unknown weapon.
- `data\mwx\ui_scripts\aw_weapons\__init__.lua` (new LUI module, loaded after all HMW zone ui_scripts):
  - appends `reflex`/`acog` to `Cac.GetValidAttachKits` for `iw5_exocrossbow` only (HMW filters the attachkit
    table by weapon-class token and a projectile `prj` row has no optics; HMW's own launchers have no attachments
    so nothing else changes). This is the safety net: if HMW's attachkit table does list optics for `prj`, the
    wrapper adds nothing (duplicate check).
  - makes sure the crossbow appears in the Secondary/`weapon_projectile` list even if HMW's item-lock filter
    would hide it (AW items have no HMW unlock/GUID entry).

Commit `6da28dfe` (test-only, droppable on merge):
- `filesystem.hpp/cpp`: `default_data_dir`/`default_log_dir` constants (one place for the `mwx` -> `hmw` rename) and
  `get_data_dir()`/`get_log_dir()` reading `-datadir`/`-logdir`.
- `filesystem.cpp` registers `get_data_dir()` instead of the literal `mwx`; `ui_scripting.cpp` loads the LUI
  modules from it; `gsc\script_loading.cpp` loads disk GSC only from it; `load_timing.cpp`, `debug_server.cpp` and
  the statstable dump use `get_log_dir()`.
- Note for the rename: with the test-only commit dropped, the hardcodes are the old ones again (`"mwx"` in
  `filesystem.cpp`, `ui_scripting.cpp`, `script_loading.cpp`, `mwx-logs` in `load_timing.cpp`, `debug_server.cpp`,
  `stock_weapons.cpp`). With it kept, the rename is the two constants in `filesystem.hpp` (or `-datadir`).

### 11.5 Sticks and Stones (`mod.ff`)

- `zone_source\mod.csv` backed up as `mod.csv.bak-20261125-104624`; new content is one reference line:
  `weapon,,iw5_exocrossbow_mp`.
- `buildzone mod` -> `mod.ff` **177 bytes**, 0 errors. Installed to `GAME\mods\sticksandstones\mod.ff`; the previous
  zone is kept as `mod.ff.bak-20261125-104819-w1` (the older `mod.ff.bak-20261125-070627` is untouched).
- **Risk, by design:** the mod zone now requires a client that loads `hmw_aw_weapons` (MWX does, before `mod`). A
  client without it (e.g. the other agent's build before this branch merges, or the official `hmw-mod.exe`) fails
  to load zone `mod` with `Could not load default asset 'void' for asset type 'weapon'`. Restore is one copy from
  the `.bak` above. The stale `GAME\zone\mod.pak` (old streamed images) is unused and left in place.
- `mod.json`'s `WEAPON_EXPLOSIVE_EXO_CROSSBOW` string is still set; it matches our localized override.

### 11.6 Missing crossbow images (still open)

The build warns about exactly these 11 images (all in AW `common_mp`, none in any H1-format dump):
`crossbow_scope_base_col`, `crossbow_scope_base_nml`, `~crossbow_scope_base_spc-rgb&~d8544476`,
`~crossbow_scope_base_occ-l`, `crossbow_glass_col`, `crossbow_glass_nml`,
`~crossbow_glass_spc-rgb&cross~a7515f30`, `crossbow_target_ems`, `wep_shared_gray_swatch_col`,
`weapon_red_cross_background`, `emblem_temp_col` (the player-emblem surface).
Their materials are also not in the zone (`mtl_crossbow_scope_base`, `ms/mtl_crossbow_scope_base`,
`mtl_crossbow_target`, `mtl_crossbow_glass_base`, `mtl_iron_sights_color`); they exist in
`AW\dump\common_mp\materials` but were not copied because the images are missing anyway. Fixing them needs an AW
image dump in H1 format: `dumpasset image <name>` writes S1 images, and `dumpzone h1 common_mp image` dumps the
whole zone (too big). Options for W4: targeted image extraction/conversion, or `dumpzone h1 common_mp image` into a
temporary folder (expect several GB; keep >= 20 GB free) and keep only these names. The HUD/kill icon is fine:
`hud_icon_exocrossbow` -> image `special_crossbow` is in the zone and has pixel data.

### 11.7 Workbench check (auto-align OFF, 1920x1016, `-posedump`)

`review\aw-weapons\W1\crossbow_reflex_ads.png/.txt`, `crossbow_acog_ads.png/.txt`:
- reflex: effective anim `vm_crossbow_reddot_ads_up`, reticle screen **(960.0, 508.0)** = **0.0 px**.
- acog: effective anim `vm_crossbow_acog_ads_up`, reticle screen **(960.0, 508.0)** = **0.0 px**.
Both within the 3 px acceptance test. The ring is hidden by the HMW attachments' `hideTags = tag_sight_front_on`
(the bone was renamed in W0/M5).

### 11.8 In-game test checklist (user)

Start the test client (from the game folder):
```
hmw-aw.exe -datadir hmw-aw -logdir hmw-aw-logs -debugport 27079
```
Optional console (after spawning): `unlockall 1`, `set mwx_gsc_print 1`.

1. **CAC list:** Play -> Private Match -> Create a Class -> Secondary -> **Launchers**: the list must show
   **Crossbow** (icon = crossbow HUD icon, description "Bolt-action crossbow..."). If it is missing, check
   `hmw-aw-logs\load_times.log` for `hmw_aw_weapons` and the console line `stock weapons: mp/customweapons.csv:
   added N ...`.
2. **Attachments:** with the crossbow equipped in CAC, open **Secondary -> Attachment**: "None", "Red Dot Sight"
   (`reflex`) and "ACOG Scope" (`acog`) must be selectable. If the two optics are missing, the
   `Cac.GetValidAttachKits` wrapper did not install (report it; fallback = patch the attachkit table for `prj`).
3. **Spawn:** start the match; the class must apply without a script error, with **TF141 viewhands/arms** on the
   crossbow (not AW exo arms). Fire, reload (1 in the clip, 6 bolts), sprint, melee.
4. **ADS no optic:** the built-in ring is visible, the sight roughly centred (known open issue: 125-178 px low in
   the workbench; check in game).
5. **ADS reflex:** white dot on the crosshair, the built-in ring hidden, optic sits on the top rail.
6. **ADS acog:** ACOG reticle centred, ring hidden.
7. **Kill feed:** a kill shows **Crossbow** (not `iw5_exocrossbow_mp`); the kill icon is the crossbow icon.
8. **Swap weapons:** switch to the secondary/primary and back; no arms glitch, no missing asset errors in the
   console.
9. **Restart the game, reopen CAC:** the class with the crossbow and its optic is still there.
10. **Camo:** the Secondary -> Camo tab shows HMW camos. Red wash (`camo043`) and blue jade (`camo046`) must
    apply to the crossbow body only (mask-limited); Christmas wrap (`camo085`) is rejected by HMW's own
    `isvalidcamo` (applies to every weapon, see §12.6). W1 dropped the suffix on purpose, W1.1 adds it.
11. **Sticks and Stones:** MODS -> Sticks and Stones -> start (sas gametype). The crossbow must be given and work
    (the mod zone only references it now); throwing-knife kills must still bankrupt.
12. Optional quick check in a match console:
    `give iw5_exocrossbow_mp`, `give iw5_exocrossbow_mp_reflex`, `give iw5_exocrossbow_mp_acog`.

### 11.9 Open issues / risks

- CAC listing and the two optics are the only unverified parts (no in-game run here). Both have a defined
  fallback in 11.4; the item-lock filter was the reason for the list wrapper.
- The crossbow's GUID is `-1` (no HMW inventory item). Unranked/private play does not check unlocks
  (`level.rankedmatch` only); if the CAC hides it, the list wrapper should still add it.
- `mod.ff` now needs `hmw_aw_weapons` (11.5). The other agent's current client/build cannot play Sticks and
  Stones until this branch is merged; the old zone is in `mod.ff.bak-20261125-104819-w1`.
- 11 missing images (11.6) - the scope/glass look red/checkered in ADS; materials+images to port in W4.
- The `hmw_aw_weapons` zone (20 MB) + pak (9 MB) are not committed to the repo yet (packaging decision in W5).
- Disk after W1: **28.4 GB** free (build objects, dumps and the test install included).

## 12. W1.1 - Camos on the crossbow (25 Nov 2026)

### 12.1 Goal and result

Goal: HMW camos on the AW crossbow (view model at least, world model if HMW does it for its own guns) and a
reusable recipe for every later AW weapon. Result:

- the material was **already camo-ready** (W0/M5 kept AW's layout); the only material defect was `gameFlags` 84 vs
  the 80 every HMW camo material uses, now fixed;
- the actual blocker was the **client name builder**: W1's `aw_buildweaponname` dropped the camo suffix, so the class
  never reached the engine with a camo. It now delegates to `_class::buildweaponnamecamo`;
- workbench: all three test camos show on the crossbow body and only where `crossbow_mask_02` allows it
  (`review\aw-weapons\W1.1\`), next to the H2 M4 control;
- in-game verification is the user's step (§12.6).

### 12.2 What blocked camos (findings)

1. **The name suffix (the blocker).** HMW gives the engine `<ref>_mp[_<attachment>]_camoNNN` plus the camo index
   (`_class.gsc` -> `_giveweapon(name, index)`). The suffix is **3 digits** (`buildweaponnamecamo`:
   `_camo00N` 1..9, `_camo0NN` 10..99, `_camoNNN` 100+), proven by a real HMW kill-log line
   `h2_scar_mp_silencerar_xmag_camo004`. No `_camo*` weapon asset exists in any HMW weapon dump, so the engine
   resolves the suffix to the base weapon itself. W1's replacement built `<ref>_mp[_<attachment>]` only.
2. **The material was already camo-ready.** `zonetool\hmw_aw_weapons\materials\m\mtl_crossbow_base.json` has the
   same 9 slots as `mtl_h2_m4a1_receiver_camo` (table in guide §4.2): `dc1=camo_transparent_col`,
   `ds1=~crossbow_spc-rgb&crossbow_cos-l-67`, `dn1=crossbow_nml`, `dna1=camo_nml`, `om0=~crossbow_occ-l`,
   `c=crossbow_col`, `m=crossbow_mask_02`, camo spec `~camo_spcmaps_01_spc-rgb&camo~b29a8732`, env LUT
   `~envbrdflut_ggx_16-rg`; `decalParams1 (2,2,0,0)`, `envMapParms (0.04,1,5,1)`. The mask is AW's own, so the
   camo coverage is AW's intent.
3. **All camo images are really in the zone.** The build log parses `camo_transparent_col`, `camo_nml`,
   `crossbow_mask_02`, `~camo_spcmaps_01_spc-rgb&camo~b29a8732`, `~envbrdflut_ggx_16-rg`, `cloak_detail3`,
   `crossbow_col`, `crossbow_nml`, `~crossbow_spc-rgb&crossbow_cos-l-67`, `~crossbow_occ-l`. The 11 warnings are the
   same missing scope/glass/emblem images as W1 (§11.6), none of them camo.
4. **One material defect: `gameFlags`.** The ported material had 84; **all 369** HMW materials with the camo layout
   have 80 (checked `hmw_patch_common_mp` + `hmw_ar1` dumps). Bit 0x4 (AW's) appears in those dumps only on
   `nodraw_notsolid`, `mtl_ret_h1_reddot` and `mtl_wpn_h1_sni_tac330_ret` (no-draw/reticle materials), never on a
   weapon body material. Set to **80**.
5. **Technique kept.** AW's `m_l_sm_lmpb_dc1dn1ds1dg1dna1_r0c0n0sd0sr0om0_nfwpf_ctq_s1` vs HMW's
   `..._r0c0n0sd0om0_nfwpf`: same family, the lit techniques bind `dc1` + `m` identically; `sr0` (SSR) and the
   `_ctq_s1` cloak pass (`cloak_detail3`, `cloakShaderP` x2, `reflectionRa`) are AW-only and unused in HMW (the
   `TECHNIQUE_*_CTQ` entries exist in the H1 techset). No reason to change it; the techset is in the zone.
6. **AW's selector does not exist in H1.** AW weapon json: `camoViewModelMaterialOverride`/`camoWorldModelMaterialOverride`
   = `mtl_crossbow_base`. H1's `WeaponDef` (`cod-mwx\src\client\game\database.hpp`) has no camo or material-override
   field, so the port drops them and HMW derives support from the material layout. `vm_crossbow_base` and
   `npc_crossbow_base` both use `mtl_crossbow_base`, so both are camo-ready (HMW cams the view model; the world model
   follows if it cams world models).
7. **CAC gating is fine for the crossbow.** `LauncherInUseCheck`/`MeleeInUseCheck` hardcode the five launchers and
   five melee refs; `iw5_exocrossbow` is not among them, so Secondary -> Camo is visible. The camo list is not
   filtered per weapon. **But** HMW's own `isvalidcamo` (in `custom_weapons.gsc`) only allows `none`,
   `camo016`..`camo062`, `gold`, `golddiamond`, `toxicwaste`; camos above 062 (e.g. `camo085` Christmas wrap) are
   replaced by the default in `applyloadout` for **every** weapon, H2 M4 included.
8. **AW vs HMW difference (summary).** AW: per-weapon `camo*MaterialOverride` selects the material, and AW's design
   materials (`camo_digital_01_col`, technique `l_sm_lmpb_dc1_heat_...`) put the design in `dc1` with
   `camo_reference_col` in `c` - structurally the same as HMW's `camo_h1_*`, so the *design* side is compatible.
   HMW: no weapon field, the engine finds camo-ready materials and the class name must carry `_camoNNN`. AW loot
   skins (`mtl_crossbow_loot_*_skn_01`, `weapon_crossbow_gold`) stay AW-only; the crossbow has no native variants.

### 12.3 Changes

Zone source (`GAME\zonetool\hmw_aw_weapons`, not the repo):
- `materials\m\mtl_crossbow_base.json`: `gameFlags` **84 -> 80** (backup `mtl_crossbow_base.json.bak-w1.1`). Nothing
  else changed; `me\mtl_crossbow_base_trans.json` (camo layout, no model uses it) and the `npc` model were left
  alone - HMW does not camo its own transparent/lens parts.

Build (`GAME\x64zt.exe`, interactive `buildzone hmw_aw_weapons`, 3-4 s after the 85 s start-up):
- `hmw_aw_weapons.ff` **19,877,687 B** (SHA-256 `06C84F0BD205704BC9952936C2F1389CD1FF3BB251633F3836A240341C43FC2A`)
  + `hmw_aw_weapons.pak` **9,236,400 B** (`8A6521BDB8B3F0240B9754CE5009CBC9C090464D2D10C927FC537083D7E608A0`),
  **0 FATAL / 0 ERROR**, 11 image warnings (the same 11 as W1).
- **Not byte-reproducible:** two builds from the identical source produced 19,877,687 B and 19,877,740 B (different
  hashes). W1 had 19,877,776 B. Compare the asset logs, not the bytes.
- Install: `GAME\hmw-aw\zone` only (ff+pak); the build outputs in `GAME\zone` were deleted again. W1's files are kept
  as `hmw_aw_weapons.ff.bak-w1` / `.pak.bak-w1` in the test zone. `GAME\mwx*`, `GAME\zone`, `GAME\mods`,
  `GAME\players2` untouched.

Client (`cod-hmw-aw`, branch `aw-weapons`, commit **`87a878f9`**):
- `data\mwx\scripts\mp\aw_weapons.gsc`: `aw_buildweaponname` now appends
  `maps\mp\gametypes\_class::buildweaponnamecamo(name, camo)` when `isdefined(camo) && camo > 0`, after the
  attachment; header comment updated.
- Copied to `GAME\hmw-aw\scripts\mp\aw_weapons.gsc`. No exe rebuild: HMW compiles disk GSC at start-up, the exe is
  unchanged (`GAME\hmw-aw.exe` 8,043,008 B from W1).

### 12.4 Workbench (auto-align default, 1280x720 orbit; commands in `review\aw-weapons\W1.1\previews.txt`)

- `crossbow_red_wash.png`, `crossbow_blue_jade.png`, `crossbow_christmas_wrap.png` next to
  `h2_m4_red_wash.png`, `h2_m4_blue_jade.png`, `h2_m4_christmas_wrap.png` - same camera, so mask and strength are
  comparable. `crossbow_nocamo.png` is the baseline, `crossbow_christmas_wrap_fp_reflex.png` shows the optic and
  hands in first person.
- Coverage matches the mask: body/stock/grip/sight housing camo; limbs, rails, string, optic and glass base. The
  workbench's two warnings (`camo specular/emissive: no texture slot`) come from the camo *design* material
  (red_wash has no spec/emissive map) and appear for `h2_m4` too.

### 12.5 Disk

Before W1.1: 28.36 GB free. After: **28.39 GB** free (the rebuild reused the zone source; +29 MB kept backups in
`GAME\hmw-aw\zone`, +2.6 MB previews). No new large copies were made.

### 12.6 In-game test checklist (user)

Start (as in W1): `hmw-aw.exe -datadir hmw-aw -logdir hmw-aw-logs -debugport 27079`. `unlockall 1` is not needed for
private play.

1. **CAC camo path (primary test).** Private Match -> Create a Class -> Secondary -> Launchers -> Crossbow ->
   **Camo**. Pick **Red Wash** (`camo043`) or **Blue Jade** (`camo046`), back out, spawn. Expected: the body/stock is
   camo; rails, limbs, string and the optic are not; no script error in the console. If nothing changes, check that
   the loaded class script is the new one (`GAME\hmw-aw\scripts\mp\aw_weapons.gsc`, 2,685 B) and the console for
   `Replacing invalid camo`.
2. **CAC limitation (expected, not the port).** Christmas Wrap (`camo085`) is rejected by HMW's `isvalidcamo` for
   every weapon (the H2 M4 control too). Do not report it as a crossbow bug.
3. **Christmas wrap via console** (bypasses the class check): `give iw5_exocrossbow_mp_camo084`, then switch to the
   weapon with the normal weapon-switch key (or `give` it again while it is already held).
4. **With optics** (attachments before the camo, HMW's own order):
   `give iw5_exocrossbow_mp_reflex_camo042`, `give iw5_exocrossbow_mp_acog_camo045`; ADS: reticle centred, camo on
   the body, optic housing/glass not camo.
5. **Control:** the same `give h2_m4_mp_camo042` and `h2_m4_mp_camo084` on HMW's M4. It must show the camo on
   stock/grip/handguard; that proves the engine and the camo index, and shows what "correct" coverage looks like.
6. **World model** (only if HMW cams it): drop the crossbow (or look at a bot with it) - `npc_crossbow_base` uses the
   same material, so it should show the camo too.
7. **Persistence:** restart, reopen CAC - the camo is still selected on the class and applies on spawn.
8. **Sticks and Stones:** unchanged from W1 (§11.8 point 11); the mod still needs the `hmw_aw_weapons` zone.

Expected/known: the crossbow has no AW camo variants, so `camo_h1_*` designs only; the scope/glass still use the 11
missing images (red/checkered in ADS), unrelated to camos.

### 12.7 Open issues

- In-game verification of the suffix path and of the material coverage is pending (no game run here). If a camo is
  selected but the crossbow stays un-camoed although the name in the console shows `_camoNNN`, the next hypothesis is
  that HMW keys on the **material name** (`*_camo`); the fallback is then to add a `mtl_crossbow_base_camo` copy and
  repoint the model surfaces (xmb material name), documented in guide §4.3 but not needed unless the test fails.
- The AW cloak techset (`_ctq_s1`) is kept; switch to HMW's exact `..._nfwpf` template only if the test shows a
  lighting difference.
- The 11 missing images (§11.6) are still W4 work; they also affect the optic/glass in ADS.
- `mod.ff` (Sticks and Stones) still requires a client that loads `hmw_aw_weapons` (§11.5).
- `hmw_aw_weapons.ff`/`.pak` are still test-only (not committed); packaging is W5.

### 12.8 Commits

- `cod-hmw-aw` `aw-weapons`: **`87a878f9`** ("aw weapons: append the camo suffix in buildweaponname (W1.1)").
- `hmw-workbench` `main`: **`2b1af69`** (guide §4 + error rows, `aw.md` camo section, `aw-weapons-port.md` §12,
  `review\aw-weapons\W1.1\`). Not pushed.

## 13. W1.2 - Crossbow finish (25 Nov 2026)

Status: **done in the workbench and in the built zone; the in-game screenshot is blocked by a
t1-build listen-host crash (13.4).** The zone builds with **0 FATAL / 0 ERROR / 0 image
warnings**, and the built-in sight is centred in ADS.

### 13.1 Built-in sight alignment (fixed)

Measured with hmw-workbench at 1920x1016, ADS, auto-align OFF:
- before: `tag_scope` screen y **643.9** (135.9 px below the centre 508); `tag_sight_front_on`
  633.1 (125.1 px), `tag_red_dot` 685.9 (177.9 px) - the 125-178 px range from the plan.
- cause: the crossbow is an AW weapon on HMW hands. The base AW ADS anim `vm_crossbow_ads_up`
  keys only `tag_ads`; its Z minimum is 0.0, which put the AW scope below the HMW camera. The
  optic overrides (`vm_crossbow_reddot_ads_up`, `vm_crossbow_acog_ads_up`) are separate and were
  already 0.0 px.
- fix: add **+0.72** to the Z minimum of the normal-translation block in `vm_crossbow_ads_up` and
  `vm_crossbow_ads_down` (the `dataInts` min/size block read by `xanim.cpp`; the compressed key
  bytes stay untouched, so every key shifts by +0.72). After: `tag_scope` **(960.0, 507.3)** =
  **0.7 px**; `tag_sight_front_on` 539.6, `tag_red_dot` 556.9 (these are the iron-sight / mount
  tags, not the scope picture). `reflex` and `acog` remain **0.0 px**.
- The two xab files are changed in `GAME\zonetool\hmw_aw_weapons\xanim\` (backups
  `.bak-w1.2`) and in `GAME\zonetool\mod\xanim\` (the workbench source; same backups).
- Tool: `mwx-debug\aw-weapons\W1.2\xanim_shift.py <in.xab> <out.xab> <delta_z>` - parses the H1
  xab stream, finds the 6-float translation min/size block and adds delta to min.z. Reusable for
  every AW weapon whose built-in sight sits low on HMW hands.

### 13.2 Scope/glass/target textures (fixed)

The 11 missing images from W1 section 11.6 are now in the zone. The extraction path that works:
- `dumpasset image <name>` writes only the **streamed image header** (`.s1Image`, 130-260 B) for a
  streamed AW image; the pixels live in the big `imagefileN.pak` stream files. The
  `-dump_streamed_image` flag writes the header + `.pixels` for `dumpasset` only when the stream
  file index is set; for a single asset it is not, so the `.pixels` are absent.
- `dumpzone h1 common_mp image` (**S1 tool, h1 target**) converts and writes the H1
  `images\<name>.h1Image` **and** `streamed_images\<name>.h1Image` +
  `streamed_images\<name>_streamN.pixels` (the converter's `dump_streamed_image` always writes
  the pixels). The full zone is large (~7.5 GB of streamed pixels before we stopped); the
  crossbow images are near the end of the asset order. We kept only the 12 needed files and
  deleted the rest. The disk stayed >= 20 GB (it reached 20.97 GB before the cleanup).
- `emblem_temp_col` is in `patch_code_post_gfx_mp`, not `common_mp`:
  `dumpzone h1 patch_code_post_gfx_mp image` (7.5 MB) writes it.
- The 12 files were copied into `GAME\zonetool\hmw_aw_weapons\images` /
  `streamed_images`, and the images were added to `GAME\zone_source\hmw_aw_weapons.csv`
  (`image,<name>` lines, backup `.bak-w1.2`). The H1 build finds the materials in
  `GAME\zonetool_paths\common_mp_materials` and packs the images.

Build (`GAME\x64zt.exe`, `buildzone hmw_aw_weapons`, 3.8 s):
- `hmw_aw_weapons.ff` **19,879,343 B**, `hmw_aw_weapons.pak` **15,708,188 B**,
  **0 FATAL / 0 ERROR / 0 image warnings** (the previous 11 warnings are gone).
- Installed to `GAME\hmw-aw\zone` (W1.1 files kept as `.bak-w1.1`). `GAME\zone` build outputs
  removed after the copy. `GAME\mwx*`, `GAME\mods`, `GAME\players2` untouched.
- The workbench preview source `zonetool\mod` also got the 12 images + the +0.72 anim, so
  `review\aw-weapons\W1.2\` shows the fixed scope. A small checkered rectangle remains in the
  workbench preview under the scope: it is the 4-vertex `m/mtl_player_emblem` surface (the AW
  ammo/emblem display), not a scope/glass image. It should resolve in game to HMW's
  player-emblem material; it will not show a live AW ammo count without a separate port.

### 13.3 Workbench evidence (`mwx-debug\aw-weapons\W1.2\`)

- `crossbow_ads_noscope.png` - before (scope checkered, 136 px low).
- `ads_shift072.png` - after the +0.72 shift: scope centred, still checkered (pre-texture).
- `crossbow_ads_fixed_final.png` - after textures + shift: scope centred and textured.
- `ads_reflex_fixed.txt` / `ads_acog_fixed.txt` - reticles still (960, 508) = 0.0 px.

### 13.4 Open

- In-game tests (ADS no-optic/optics/camo, CAC entry, kill feed, Sticks and Stones): **the user
  will do these**. The zone is installed in `GAME\hmw-aw\zone`; `GAME\hmw-aw.exe.bak-w1.1` is the
  W1 client that can load it before I1. The t1-build `hmw-aw.exe` has the test interface but
  crashes in `gsc::load_custom_script` (`script_loading.cpp:210`) during a listen-host gametype
  load (`0x1405A4E96`; minidump `GAME\minidumps\hmw-mod-crash-1-2026-11-25-15-14-36.zip`); the
  `hmw_aw_weapons` zone itself loads in 3.1 s, so this is a t1-build/test-branch issue, not the
  crossbow. The dedicated tests and the workbench are unaffected.
- The workbench-only emblem square (13.2); verify in game.
- The final `hmw_aw_weapons.ff` is still a test artifact (not committed); packaging is W5.

## 14. T1 - Automated crossbow test (25 Nov 2026)

Status: **crossbow test green; Sticks and Stones entry BLOCKED by the reference-only `mod.ff`.**

### 14.1 Where the test lives

- `cod-hmw-aw` `aw-weapons`: `data\mwx\tests\aw_weapon.gsc` (generic, dvar-driven), `aw_crossbow.gsc`,
  `aw_crossbow_sas.gsc`, `_test.gsc` (the framework helper), `tests.json` (the 26-test framework
  manifest + `aw_crossbow`), and `tests-aw-sas.json` (the manual sas entry, not in `run_all_tests`).
- `aw_weapon.gsc` parameters (tests.json dvars): `mwx_aw_weapon` (base ref without `_mp`),
  `mwx_aw_class` (`primary`/`secondary`), `mwx_aw_attachments` (comma list), `mwx_aw_camo`
  (index), `mwx_aw_combat` (1 = combat checks, default 0 for the class test).
- Checks: asset exists (`weaponclipsize`), the class accepts the ref (`isvalidsecondary`), the
  name builder (`buildweaponname`) for no optic / `reflex` / `acog` / `_camo042`, then give each
  variant to a bot and assert `hasweapon` + a non-zero clip. The class path adds a projectile
  after the bot's class loadout, so `getcurrentweapon()` reports `none` for it; the inventory and
  clip checks are the meaningful ones (logged). Visual ADS/camo checks remain MCP screenshots.
- The combat half (fire/reload/switch/kill) is in `aw_sas_test` / the manual sas entry, where the
  crossbow is the mode-spawned weapon and therefore current.

### 14.2 Running it without I1

`aw-weapons` is based on `origin/mwx` (`5cdcf64b`) and has no test framework. T1 was therefore
validated on a **local-only** `t1-build` branch (`origin/opencode/autonomous` + merge `aw-weapons`,
not pushed, no `mwx` change):
- `data/mwx/tests/` must be loaded from the `-datadir` root; the framework's
  `script_loading.cpp` hardcoded `mwx/tests/...`. The temp branch changes it to
  `filesystem::get_data_dir() + "/tests/"`; the **I1 rebase must keep that**.
- The test includes are data-dir relative (`#include tests\_test;`), not `mwx\tests\...`, so the
  same files work with `-datadir mwx` and `-datadir hmw-aw`.
- No BOM in the test GSC: PowerShell `Set-Content -Encoding UTF8` writes a BOM, and the GSC
  compiler fails the whole script with `bad token: '?'` at 1:1.
- `aw-weapons` lacks the test `mwx_test`/`events` implementation in the debug server; the test
  client is the t1-build `hmw-aw.exe` with `-datadir hmw-aw -logdir hmw-aw-logs -debugport 27300`,
  manifest `GAME\hmw-aw\tests\tests.json`, net ports 27116+.

### 14.3 Results

- `aw_crossbow` (war, mp_shipment, 4 bots): **PASS 15/15, 0 script errors** (68 s). Covers the
  asset, `isvalidsecondary`, the four names (`iw5_exocrossbow_mp`, `_reflex`, `_acog`,
  `_camo042`), and the give/has/clip of each variant.
- Full suite with the zone loaded (25 existing tests + `aw_crossbow`; `sas` and the manual sas
  entry excluded): **26 run, 25 green, 1 known failure** (`survival` wave 1 -> 2, the documented
  prototype issue). The run was interrupted by a harness restart at `war_terrace` after 21 green;
  the four remaining tests were rerun with fresh ports: `reinforce` **8/8**, `infect_rust`
  **28/28**, `war_terrace` **12/12**, `aw_crossbow` **15/15**, all PASS. `infect_rust` had one
  `scavenger bag dropped` failure in the interrupted run and passed 28/28 on the rerun (known
  RNG flake); `reinforce` had one 416 s server-load stall and passed 8/8 on the rerun.
- `tools\gsc-check\check-all.ps1` on `t1-build`: **42 files, 543 cross-file calls, 0 problems**
  (the AW test includes resolve; the aw-weapons branch itself has the older checker without the
  test-builtin list, so the merged branch's checker is the meaningful gate).
- Sticks and Stones: **BLOCKED**. The reference-only `GAME\mods\sticksandstones\mod.ff` (177 B,
  `weapon,,iw5_exocrossbow_mp`) crashes in `fastfiles::db_try_load_x_file_internal`
  (`0xC0000005` at `0x1405A51A9`) while loading `mod`, on a dedicated server **and** on a listen
  host, with the AW zone loaded. The self-contained `mod.ff` (19.8 MB) plus the AW zone crashes
  the same way (both define `iw5_exocrossbow_mp`). The crash dumps are
  `GAME\minidumps\hmw-mod-crash-2-2026-11-25-13-56-41.zip` and `...-14-02-50.zip`.
  - **Next idea:** W1.5's fallback - keep the mod self-contained and have the client skip the
    `hmw_aw_weapons` zone when `mods/sticksandstones` is loaded (conditional entry in
    `patches.cpp`), or give the mod's copy a separate internal name. This is an integration
    decision for I1, not a T1 test fix. Until then the self-contained `mod.ff` is restored so the
    normal (non-AW-zone) Sticks and Stones still works; `tests-aw-sas.json` is the manual entry
    to run once the mod/zone ownership is decided.
- Note: the existing `sas` test in the game-mode manifest also loads the mod. With the AW zone
  loaded it hits the same crash; it was green in the previous full runs on `opencode/autonomous`
  because that branch does not load `hmw_aw_weapons`. T1 therefore runs the full suite without
  `sas` and reports the mod as the one blocked existing test.

### 14.4 Commits

- `cod-hmw-aw` `aw-weapons`: **`aab9eab6`** (T1 tests + manifest), `60fd2b1f` (data-dir relative
  includes), **`4d8c4466`** (CAC checks after level init), **`b1e675f3`** (final crossbow test;
  sas entry moved to `tests-aw-sas.json`). No push.
- `t1-build` (local only): merge `aw-weapons`, `15caf122` (test script from `-datadir`),
  `c2a8e656` (includes), `a445976f`/`ff78a8fc`/`67166159` (test fixes).

### 14.5 Open

- Sticks and Stones mod/zone ownership (14.3) - I1 decision.
- The full suite must be re-run on the merged `mwx` after I1, with the `sas` entry and the
  manual sas test.
- In-game tests/screenshots (ADS no-optic, optics, camo, CAC entry, kill feed, Sticks and Stones)
  are the user's step; the zone is installed in `GAME\hmw-aw\zone`.
