# Finish task progress log

Task: `docs\FINISH-TASK.md` (25 Nov 2026). One commit per item; this file is updated with each
item's result and is the resume point after a context reset.

## Status

| item | state | commit | notes |
|---|---|---|---|
| 1 ADS auto-align off by default | in progress | – | measurements taken |
| 2 player model data | not started | – | |
| 3 camo fidelity | not started | – | |
| 4 camos in game: test commands | not started | – | |
| 5 comfort gaps | not started | – | |

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
- Build 0 warnings. Commit: see below.

