# Overnight progress log

Last update: 24 Nov 2026, ~23:00. The user asked to document the state and stop; the next session
starts with a fresh context.

**On resume:** read `docs\OVERNIGHT-TASK.md` (the task), `AGENTS.md`, `docs\PLAN.md`, this file
and `git log --oneline -5`; then continue with the first open item below. Do not redo M5.

## Status per milestone

| milestone | state | commit | notes |
|---|---|---|---|
| M0-M4b | done | e4ba51a ... 5f6cec6 | previous sessions |
| M5 tag editing | **done** | 23c650c + follow-up | all checks pass, copy-save 14/14 |
| M6 camos | not started | – | |
| M7 hardening | not started | – | |
| M8 world models | not started | – | |
| M9 player models | not started | – | |
| M10 comfort features | not started | – | |

Commit hash of this checkpoint: **23c650c** (`M5: tag editing`). The commit includes
`docs\porting\aw.md` updated by Claude and `docs\OVERNIGHT-TASK.md`.

## M5 - what is done

- **Writer + round-trip: 523/523 `.xmb` byte-identical, 0 errors** (`review\M5\roundtrip.txt`).
  `-roundtrip [report.txt]` writes into `build\roundtrip` (gitignored). The writer replays the
  original stream tokens and supports edits; models with shared/offset bone arrays are refused
  for editing but still round-trip.
- **Editing command layer** (UI + MCP): `MoveTagLocal`/`MoveTagWorld`, `AddTag` (append, all
  per-bone arrays + name string token + counts), `RevertTag`, `RevertAllTags`, `HasTagChanges`,
  `SaveEditedModel`, tag pose/ghost accessors. Inspector has the tag list, numeric pos/quat,
  ImGuizmo move/rotate, ghost, add-tag panel (fit-needed tag dropdown, parent, copy-from), revert,
  save, dirty marker. Edits apply live in hip/ADS, FP/orbit.
- **Save**: `.bak` created once, in-place write, `<model>.tags.json` (all tags with
  name/parent/local pos/rot/world/added), reload + `-validate`-style check.
- **MCP write tools**: `move_tag`, `add_tag`, `revert_tags`, `save_model`; `get_tags` reports
  `added`; `screenshot` has `ads_align`. Documented in `docs\mcp.md`.
- **Crossbow fit**: `vm_crossbow_base` now has `tag_reflex` (+0.356197 Z from `tag_red_dot`),
  `tag_acog` (+0.109879 Z from `tag_acog_2`) and `tag_silencer` (exactly `tag_flash`);
  36 -> 39 bones. ADS reticle aim **960.0 / 508.0 px** (centre) for both
  `vm_crossbow_reddot_ads_up` and `vm_crossbow_acog_ads_up`.
- Files changed outside the repo (only these): `GAME\zonetool\mod\xmodel\vm_crossbow_base.xmb`
  (5208 B), `vm_crossbow_base.xmb.bak` (4911 B, original) and `vm_crossbow_base.tags.json`
  (7939 B). No weapon json/zone csv/other port file touched.

## M5 self-checks

- [x] roundtrip: 523/523 identical, 0 errors.
- [x] Saved model re-reads (39 bones, new tags present); `-validate` unchanged (523/0, 298/47,
  189/30; no new failures). `.bak` is the 4911 B original and re-reads as the 36-bone model.
- [x] `fit_crossbow_after.txt` shows `tag_reflex`, `tag_acog`, `tag_silencer` found.
- [x] ADS-end with `-crosshair`: reflex and ACOG reticle aim (960.0, 508.0), 0 px off.
- [x] Idle/fire/reload sheets with the new tags, no exploded vertices.
- [x] **Copy-save check (closed 25 Nov):** MCP write tools save **on a copy of a model in
  `build\edit_test\`** (not the real file): `tools\test\m5-mcp-copy.ps1` -> `review\M5\mcp-smoke.txt`,
  **14/14 passed**. 39 -> 40 bones after reload, `tag_m5_copy` persisted at the moved local
  position, `.bak` byte-identical to the pristine copy, saved copy 5208 -> 5308 B, `.tags.json`
  contains the tag, real model sha256 unchanged.

## M5 open issues / notes

- The crossbow's built-in `tag_sight_on` ring hides the reflex dot in ADS. The official
  `crossbow_reflex_ads_end.png` hides surfaces 0,1,3,18,19 (`-hidesurface`) to show the centred
  dot; `crossbow_reflex_ads_end_ring.png` is the same without hiding. The tag fit itself is
  correct; the in-game fix is a `hideTags`/weapon-json change (not allowed in M5, M6/M7 candidate).
- ACOG reticle is dark in the workbench (no alpha reticle rendering); the crosshair marks centre.
- `tags.json` marks the three tags `added:true` as saved. Saving again re-baselines them
  (`added:false`); name/parent/local transform remain.
- UI gizmo/ghost compile and the window loads the crossbow, but the interactive gizmo could not be
  driven headlessly; numeric fields and the command layer are tested via MCP.
- Free disk: 33.3 GB.

## Next steps (new context)

1. Finish the one M5 MCP copy-save check, update `docs\milestones\M5.md` + this log, amend
   `M5: tag editing` (clean build first).
2. M6 camos: read OVERNIGHT-TASK.md §M6, write `docs\camos.md` research first, then the camo
   dropdown/CLI/MCP and the shader approximation. One commit `M6: camos`.
3. M7 hardening, then M8 world models, M9 player models, M10 comfort features, one commit each,
   updating this file after every milestone. Maps are not part of the run.

