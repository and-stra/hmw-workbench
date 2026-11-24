# Viewmodel composition (hands + weapon)

This is the rule hmw-workbench uses to place a weapon on the hands, and the reasoning behind it.

## The hands model

A weapon JSON has a `handModel` field. HMW weapons use the placeholder
`viewmodel_base_viewhands`; e.g. `refs\dumps\hmw_patch_common_mp\weapons\h1_m4_mp.json` has
`"handModel": "viewmodel_base_viewhands"`.

The actual hands are chosen by HMW's team/costume script, not by the weapon. The script
`refs\dumps\hmw_files\scripts\mp_patches\custom_teams.gsc` replaces `_teams::playercostume` and calls
`self setviewmodel("<model>")` per faction and weapon class (`ASSAULT`/`SMG`/`LMG`, ghillie for snipers):

| faction / character set | viewhands model |
|---|---|
| tf141 | `viewhands_tf141` |
| opfor | `viewhands_h1_arab_desert_mp_camo` |
| spetsnaz | `globalViewhands_mw2_airborne` |
| rangers | `viewhands_rangers` |
| seals | `viewhands_udt` |
| militia | `globalViewhands_mw2_militia` |
| ghillie (allies/axis) | `viewhands_h2_ghillie` |
| africa | `viewhands_african_militia` |
| pmcmw3 | `viewhands_pmc` |
| ic | `viewhands_henchmen` |
| delta | `viewhands_delta` |
| gign | `viewhands_sas` |

`viewhands_tf141` is the crossbow's explicit `handModel` (the AW port sets it directly). It is present in
**`hmw_post_gfx.ff`**, not in a retail MWR zone (the plan's "retail MWR" note was wrong; a full text scan of
all retail zone fastfiles found no `viewhands_tf141`, only `hmw_post_gfx.ff` did). The other HMW hands in the
M2 dumps are in `hmw_patch_common_mp.ff`.

## How the gun joins the hands

The hands skeleton contains a `tag_weapon` bone (plus `tag_weapon_left` / `tag_weapon_right`), verified in
`viewhands_h1_arab_desert_mp_camo` (bone 13 `tag_weapon`, and the left/right variants at the wrists).
H1's own tag handling treats `tag_weapon_right` and `tag_knife_attach` as aliases for `tag_weapon`
(`FUN_16013d1bc` in `refs\hmw_decomp\decomp_001.c`: if the requested tag is `tag_weapon_right` or
`tag_knife_attach`, it retries with `tag_weapon`). That makes `tag_weapon` the canonical weapon mount.

The weapon xmodel is attached by its **root bone** (bone index 0), not by a named tag:

- AW crossbow `vm_crossbow_base`: root bone `j_gun`.
- HMW weapons (M4 etc.): root bone `tag_weapon` or `tag_weapon_left/right` depending on the model.

The workbench places the gun so that its root bone coincides with the hands' `tag_weapon` bone:

```
gunTransform = inverse(gunRootWorld) * handsTagWeaponWorld      (row-vector convention)
```

Both poses are model-space base poses (`baseMat`), as described in M1. For a weapon root at the origin with
identity rotation this reduces to `handsTagWeaponWorld`. Hands are rendered with identity; the gun is rendered
with `gunTransform`. The same transform is used for orbit framing (bounds of gun corners transformed plus the
hands bounds).

## First person

The view origin is the hands model's `tag_view` bone (fallbacks `tag_camera`, `tag_cambone`). In the HMW hands
models `tag_view`/`tag_camera` are at `(0,0,60)` with identity rotation, i.e. 60 units above the origin in
viewmodel space, X forward, Y left, Z up. The workbench puts the camera at that point looking along +X, with
Z up and a near plane of 0.02 (viewmodel FOV default 65, adjustable). The weapon and hands stay in viewmodel
space; no additional first-person offset is applied. `-view firstperson` / the Inspector radio button switch
between this and the orbit camera (orbit target = combined weapon+hands bounds).

## Limitations

- Base pose only in M2: hands and gun are rendered in their model-space base pose; no animation/skinning yet.
- Only the gun root is attached. HMW attachment models (M4) are not mounted yet; that is M4.
- Left-handed/dual-wield weapons (`tag_weapon_left`) are placed on `tag_weapon` as well; not needed for M2.
- The faction hands are a user choice in the workbench (Hands dropdown). The game's choice depends on the
  player's team and weapon class; the script table above is what the game uses.
