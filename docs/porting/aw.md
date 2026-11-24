# Advanced Warfare (S1) → HMW porting: state and findings

Central overview for porting AW content to HMW (H1). Detailed logs stay where they were written:
- `cod-mwx\docs\todo-aw-porting.md` (paused TODO list, 24 Nov 2026), `cod-mwx\docs\aw-weapon-porting.md`,
  `cod-mwx\docs\aw-map-porting.md`, `cod-mwx\tools\aw-weapon-port\README.md`
- `hmw-workbench\docs\milestones\M4.md` ("Crossbow" section), `hmw-workbench\docs\viewmodel.md`,
  review images in `hmw-workbench\review\M4\`

Last update: 24 Nov 2026.

## Pipeline

AW zone (`S1ff`) → x64-zt (S1 build in the AW folder) dump → x64-zt S1→H1 converter → H1 dump
(`GAME\zonetool\<zone>`) → zone CSV (`GAME\zone_source\<zone>.csv`) → x64-zt H1 build (`x64zt.exe` in GAME) → `.ff`.
The crossbow lives in `GAME\zonetool\mod` / `zone_source\mod.csv` (Sticks and Stones mod, zone name `mod`).
`hmw-workbench` reads the converted H1 dump, so every port can be checked before the zone is built.

Disk: plan 60+ GB working space (full AW `common_mp` dump 9.3 GB, `patch_common_mp` 7.2 GB, one map ~2.7 GB + 0.9 GB
build output). Continue on the new machine.

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

### Port fixes to try (none applied yet)

1. Restore the weapon's `attachments` list (or remap `animOverrides` to HMW attachments) and convert AW
   `animTreeType` values to H1's.
2. Add HMW tags to `vm_crossbow_base` (`tag_reflex` near `tag_red_dot`, `tag_acog` near `tag_acog_2`, `tag_silencer`
   at `tag_flash`) – workbench M5 (tag editing).
3. Hide the built-in sight (`tag_sight_on` surfaces) when an optic is mounted (`hideTags`).
4. Fix ADS alignment without optic (sight 178 px low) – check what AW does for the no-optic case first.
5. Dump the 11 missing images (AW `common_mp`) in H1 format.

## AW maps

28 of 30 community ports load and play with bots (see `todo-aw-porting.md`). Open: install the rest (disk), generic
helicopter paths, missing classic geometry (ladders/clips), own map pipeline fixes, `mp_seoul2`, `mp_solar`,
`mp_torqued` UAV issue. The workbench maps track (map inspector: mapents, collision, path nodes, heli points) is
meant for these checks.

## Other source games

x64-zt converts S1 (AW), IW6 (Ghosts), IW7 (Infinite Warfare), T7 (Black Ops III) and H2 (MW2CR) to H1. IW5 (MW3) has
no converter: ZoneTool (Joelrau/zonetool, used by Plutonium) dumps IW5, the IW5→H1 step is missing. MW2 maps look
complete in HMW; missing MW3 maps: Carbon, Exchange, Interchange, Outpost, Village and most DLC maps.
