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
identity rotation this reduces to `handsTagWeaponWorld`. This static transform is what M2 used (hands rendered
with identity, gun with `gunTransform`). M3 replaces it with the combined animated skeleton described below:
the same relationship is expressed per bone as `inverse(bindWorld) * animatedWorld`, so the gun follows the
animated `tag_weapon` and `j_gun` channels while both models are drawn with an identity world matrix.

## First person

The view origin is the hands model's `tag_view` bone (fallbacks `tag_camera`, `tag_cambone`). In the HMW hands
models `tag_view`/`tag_camera` are at `(0,0,60)` with identity rotation, i.e. 60 units above the origin in
viewmodel space, X forward, Y left, Z up. The workbench puts the camera at that point looking along +X, with
Z up and a near plane of 0.02 (viewmodel FOV default 65, adjustable). The weapon and hands stay in viewmodel
space; no additional first-person offset is applied. `-view firstperson` / the Inspector radio button switch
between this and the orbit camera (orbit target = combined weapon+hands bounds).

## Animated composition (M3)

The animation reader (`src/assets/xanim.cpp`) decodes the H1 `XAnimParts` channel layout:
none/2D/3D/static rotations and byte/short/static translations, notetracks and delta parts.
`.xab` frame keys run **0..numframes inclusive** (numframes=86 means 87 keys). Notetrack times
are stored as normalized fractions; seconds = fraction * duration, frame = fraction * numframes.

Verified rules for applying an animation to the combined skeleton:

- Every channel is **parent-relative** (local): `worldQuat = parentWorldQuat * localQuat`,
  `worldPos = parentWorldPos + rotate(localPos, parentWorldQuat)`. This was checked against the
  actual grip poses: `vm_crossbow_idle` puts the wrists at `(19.6,-1.5,52.2)` and
  `(5.2,-5.0,51.2)` while the gun root `j_gun` is at `(8.2,-3.2,51.7)`, i.e. both hands are on
  the weapon. `vm_h2_m4a1_idle` behaves the same with the HMW hands.
- The animation skeleton is a superset of the hands+gun skeleton. Channels are matched **by bone
  name**, not by index: the crossbow idle starts with `tag_camera`, the reload starts with
  `tag_torso`, and HMW anims use yet another order.
- Bones with a rotation/translation channel use the sampled value; every other bone keeps its
  parent-relative base pose (`localQuat`/`localPos` from the xmodel `baseMat`).
- The gun root (`j_gun`, or `tag_weapon*` for HMW weapons) is a child of the hands `tag_weapon`
  in the combined hierarchy. Its own animation channel is therefore relative to `tag_weapon`
  (HMW M4 idle has a small `j_gun` offset; most crossbow anims keep it at identity). The gun mesh
  is skinned in gun space, so the palette is `inverse(gunBindWorld) * animatedWorldInHandsSpace`.
  Rendering both models with an identity world matrix and per-model palettes replaces the static
  `gunTransform` from M2.
- Additive animations (`assetType == 6`, or `ADDITIVE_*` / `*_add` names) store deltas relative
  to identity. The workbench multiplies the sampled delta onto IDLE for the additive checkbox.
  This is an approximation: there is no reference-pose/blend-weight data in the `.xab`.
- Delta parts (`XAnimDeltaPart`) are decoded and applied to the skeleton root (`tag_origin`) when
  present. The crossbow and H2 M4 review anims have no delta parts.
- GPU skinning uses the blend weights read in M1: rigid vertices (`boneOffset / 64`) and
  weighted vertices with 1–4 influences (weight buffer `boneId / 64`, `weight / 65536`).
  Palette per model bone: `inverse(bindWorld) * animatedWorld`; bind poses are the xmodel
  `baseMat` world transforms.

## Limitations

- Additive animations are an approximation (delta local rotations/translations multiplied onto
  IDLE; no reference pose or per-bone blend weight from the `.xab`). See `docs/milestones/M3.md`.
- The transform palette is limited to 256 bones (constant buffer). H1 viewmodels are far below
  that; character skeletons near/above 256 are not supported yet.
- Animations with more than 255 frames use the long-index path, which is implemented but not
  exercised by the M3 review assets (all are ≤ 100 frames).
- The crossbow anims are IW5-authored and the workbench plays them on the MWR `viewhands_tf141`
  by matching bone names. Their rest pose/hierarchy must stay compatible; a rig with different
  bone names or parent order would need a retarget step.
- Only the gun root is attached. HMW attachment models and per-attachment anim overrides are
  listed but not mounted yet; that is M4.
- Left-handed/dual-wield weapons (`tag_weapon_left`) are placed on `tag_weapon` as well.
- The faction hands are a user choice in the workbench (Hands dropdown). The game's choice depends on the
  player's team and weapon class; the script table above is what the game uses.
