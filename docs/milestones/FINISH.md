# Finish task progress log

Task: `docs\FINISH-TASK.md` (25 Nov 2026). One commit per item; this file is updated with each
item's result and is the resume point after a context reset.

## Status

| item | state | commit | notes |
|---|---|---|---|
| 1 ADS auto-align off by default | **done** | `343cc96` | M4 reflex after: 22.1 px off (reported) |
| 2 player model data | **done** | item 2 commit (hash in final log) | all 14 factions textured with head; head attach bug fixed |
| 3 camo fidelity | not started | - | |
| 4 camos in game: test commands | not started | - | |
| 5 comfort gaps | not started | - | |

Extra (not in the task list): red-dot comparison on the crossbow, commit `41348f9`
(`review: red dot comparison on the crossbow`), files in `review\reddots\`; 4 of 5 red dots done,
`variant` is BLOCKED (missing `vm_h1_optic_reflex_variant_ads_mp_lodvm100.xsb`).

## Item 1

- `adsAutoAlign_` default false (Claude's source change); CLI `-ads-align` turns it on for debug,
  `-no-ads-align` is accepted as a no-op. UI checkbox starts unchecked and is labelled
  "debug: snap ADS to reticle (hides misalignment)". README and workflow-porting updated; the
  workflow no longer tells reviewers to pass `-no-ads-align`.
- Re-measured with auto-align **OFF** (1920x1016, reticle aim from `-posedump`; before = auto-align
  ON, after = new default OFF):
  - H2 M4 + `reflexmwrm4`: before **(960.0, 508.0)** -> after **(957.9, 486.7)** = 2.1 px left,
    21.3 px high, **22.1 px total - over the 3 px limit**.
  - H2 M4 + `acogmwrm4`: before **(960.0, 508.0)** -> after **(958.2, 526.0)** = 1.8 px left,
    18.0 px low, 18.1 px total - over 3 px.
  - Crossbow + HMW `reflex` (`vm_crossbow_reddot_ads_up`): before/after **(960.0, 508.0)**, 0 px.
  - Crossbow + HMW `acog` (`vm_crossbow_acog_ads_up`): before/after **(960.0, 508.0)**, 0 px.
  - Crossbow via the old M5 `reflexmwr` name: the current weapon json's `attachments` list is
    `["acog","reflex"]`, so `reflexmwr` no longer selects an override (base `vm_crossbow_ads_up`)
    and the H1 reticle is at (960.0, 376.4), 131.6 px high. This is a name mapping consequence,
    not an auto-align result: use `reflex`/`acog` for the current port. `acogmwr` similarly:
    (960.0, 527.3), 19.3 px low, because only the base ADS anim applies.
- Review images: `review\finish\m4_reflex_{on,off}.png`, `m4_acog_{on,off}.png`,
  `crossbow_{reflex,acog}_off.png`, `crossbow_{reflexmwr,acogmwr}_off.png`; pose dumps alongside.
- Build 0 warnings.

## Item 2

Status: **done**. All dumps are new `GAME\dump\workbench_*` folders; no existing dump, `.ff`, `.pak`
or zone was modified.

Dumps and sizes (full commands in `docs\assets.md`):

| folder | contents | size |
|---|---|---|
| `workbench_hmw_rangers` | HMW body_h2_* / head_h2_* bodies, heads, materials, techsets, streamed images | 1,072.2 MB / 2,230 files |
| `workbench_hmw_dlc_factions` | H1 port `mp_body_*` + `head_*` bodies/heads and materials/images | 854.7 MB / 3,340 files |
| `workbench_hmw_patch_common_mp_xsurface` | xsurface-only dump of `hmw_patch_common_mp` | 33.8 MB / 185 files |
| `workbench_hmw_ar1\xsurface` | the 5 `usmc_desert_assault_body_mp_lod{0..4}300.xsb` files | +7.4 MB / 5 files |

Roots added to `settings.cpp` after `workbench_hmw_post_gfx`: `workbench_hmw_rangers`,
`workbench_hmw_dlc_factions`, `workbench_hmw_patch_common_mp_xsurface` (docs\assets.md updated).
Free disk during the dumps: 30.8 GB (limit 20 GB).

Streamed images: `hmw_rangers` is special. Its colour/normal maps are in `hmw_rangers.pak`, and
zonetool only reads that pak when both the `.ff` and `.pak` are hardlinked into the dump source
folder. Without the pak it writes 141-byte `*.h1Image` metadata stubs and `body_h2_airborne_assault`
renders as the missing-texture checker. With both linked, the `.pixels` streams are written and the
body is textured.

Head attachment (the previously untested code) had two real bugs:

1. All H1/H2 head models are authored on the body skeleton with root `j_spine4`, not `j_head`.
   The old code attached the head root at body `j_head` (and applied the spine pose), so the head
   was displaced far above/in front of the body. The head palette already maps the head bones onto
   the body pose, so the head world matrix must stay identity when the head root exists in the body
   pose. The neck/head-tag fallback is kept for heads that are not on the body skeleton.
2. `BuildBonePalette` used an identity skin for bones without a pose channel, which left their
   vertices at the bind origin. The H2 scarf/face surfaces stretched from the head down to the floor.
   Unmapped bones now inherit the nearest animated ancestor's skin.

Verification: `tools\test\finish-factions.ps1` renders all 14 faction entries, writes
`review\finish\factions\report.txt` and the contact sheet; every line reports
`head attach: bone j_spine4 | root j_spine4`. Screenshots: `review\finish\factions\<id>.png` and
`factions_sheet.png`. The H2 M4 / crossbow ADS renders are byte-identical before/after the palette
fix (SHA-256 checked on `reflex_ads.png`), so the viewmodel path did not regress.

Still missing / not covered:

- The table only lists the assault variant per faction. The SMG/LMG/shotgun bodies and heads are in
  the same dump folders but are not separate table entries.
- `usmc_desert` is a single H1 body with the head included; `hazmat`/`infect` are full-body HMW
  models, also with the head included. They have no separate head model to attach.
- No anim is loaded for the faction screenshots; they are bind-pose renders against the body
  skeleton. The 14 table entries are all textured and all heads are present.
