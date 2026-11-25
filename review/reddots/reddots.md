# HMW red dots on the AW crossbow

Comparison shots: 1920x1016, first person, `viewhands_tf141`, cg_fov 65 (ADS uses the weapon's
`adsZoomFov`), ADS auto-align **off**. All images are in `review\reddots\`. The crossbow model
itself was not saved: `reflex`/`reflexmwr`/`variant` use the real `tag_reflex`; `mars` and `holo`
use temporary tags on a copy in `build\reddot_test` and a temporary weapon json (copy of the real
one with the test attachment list + red-dot ADS overrides). No real game file was changed by
this task.

| name | model | mount tag | real / temp tag | tag offset vs `tag_red_dot` | ADS end reticle | housing above rail | notes |
|---|---|---|---:|---|---:|---:|---|
| `reflex` | `attach_h2_reflex_vm` (MW2) | `tag_reflex` | real | **+0.490906 Z** (3.242754, 0, 6.226662) | **(960.0, 508.0)** = 0 px | 1.78 units | white dot clear; built-in ring hidden by the attachment's `hideTags=tag_sight_front_on`; clean small housing |
| `reflexmwr` | `attach_h1_optic_reflex_vm` (MWR) | `tag_reflex` | real | **+0.490906 Z** (same tag) | **(960.0, 376.4)** = **131.6 px high** | 1.78 units | H1 housing renders checker/pink (its materials are not in the dumps); **built-in ring visible** because this attachment has no `hideTags`; the current weapon json's list is acog/reflex, so no override applies and the base `vm_crossbow_ads_up` path is used; H1 optic on the H2-fitted tag sits far high |
| `mars` | `attach_h2_mars_vm` (TAR-21) | `tag_tavor_scope` | temp | **-0.011128 Z** (3.242754, 0, 5.724628) | **(960.0, 508.0)** = 0 px | 2.57 units | tall/wide housing; clears the bow limbs; the string crosses the rail at its base; ring hidden (`tag_sight_front_on`); H1 reflex reticle |
| `holo` | `attach_h2_holo_vm` (holographic) | `tag_holo` | temp | **+0.121460 Z** (3.242754, 0, 5.857216) | **(960.0, 508.0)** = 0 px | 2.15 units | large square housing, clears the limbs/string; ring hidden; holographic reticle is dark in the workbench (crosshair marks the centre) |
| `variant` | `attach_h1_optic_reflex_variant_vm_mp` | `tag_reflex` | real | +0.490906 Z | **BLOCKED** | – | model is in `hmw_patch_common_mp`, but its surface set `vm_h1_optic_reflex_variant_ads_mp_lodvm100.xsb` is **not in any asset root nor in a dedicated xsurface dump of the zone**; the workbench cannot load it. No shots. |

Housing height is the model's `bounds.mid.z + bounds.half.z` above the mount tag (game inches);
the side sheet is cropped to the optic/rail area for a direct comparison.

## Reticle measurements (1920x1016, `-posedump`, auto-align off)

| red dot | effective ADS anim | reticle aim screen | offset from (960,508) |
|---|---|---:|---:|
| reflex | `vm_crossbow_reddot_ads_up` | (960.0, 508.0) | 0.0 px |
| reflexmwr | `vm_crossbow_ads_up` (no override in the current weapon json) | (960.0, 376.4) | 131.6 px high |
| mars | `vm_crossbow_reddot_ads_up` (temp json override) | (960.0, 508.0) | 0.0 px |
| holo | `vm_crossbow_reddot_ads_up` (temp json override) | (960.0, 508.0) | 0.0 px |

## Files

```
review\reddots\<name>_hip.png      first person hip idle
review\reddots\<name>_ads.png      ADS end + crosshair
review\reddots\<name>_side.png     orbit side (yaw 90, pitch 0)
review\reddots\<name>_34.png       orbit 3/4 (yaw 35, pitch 15)
review\reddots\reddots_ads_sheet.png
review\reddots\reddots_side_sheet.png
review\reddots\setup.txt           temp copy setup/fit results
```
