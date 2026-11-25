# Finish task: open items after the overnight run (written by Claude, 25 Nov 2026)

Work through the items below in order, autonomously; one commit per item. Same rules as AGENTS.md and
docs\OVERNIGHT-TASK.md (backups, allowed writes, never touch cod-mwx, rename a locked exe instead of killing the
Claude app's MCP server). Keep a progress log in docs\milestones\FINISH.md (resume from it after a context reset).
Maps stay out of scope.

## 1. ADS auto-align off by default (commit "ads auto-align off by default")
Claude already changed the source (uncommitted): `adsAutoAlign_ = false` in src\app\commands.h and the MCP
`ads_align` description in src\app\mcp.cpp. Finish it:
- CLI: add `-ads-align` (turns it on, debug only); `-no-ads-align` stays accepted as a no-op for old commands.
- UI: the checkbox (if any) starts unchecked and is labelled "debug: snap ADS to reticle (hides misalignment)".
- README.md and docs\workflow-porting.md: describe the new default; remove the advice to pass `-no-ads-align`.
- Find every review/self-check result since M4 that was measured with auto-align ON (M4 "m4_reflex_ads_end",
  M5 ADS checks, M7 regression) and re-measure with it OFF. Write the before/after numbers into the commit's
  milestone note (docs\milestones\FINISH.md). If any reticle is now off centre by more than 3 px, report it
  prominently.

## 2. Player model data (M9 gap)
- Check free space (>= 20 GB must remain). Dump the HMW faction bodies and heads with zonetool into NEW folders
  `GAME\dump\workbench_<zone>\` only (never into existing dumps, never modify .ff/.pak). Find which zones hold
  `body_h2_*`, `head_h2_*`, `mp_body_*`, `head_*` used by `custom_teams.gsc`; prefer single-asset dumps. Also get
  `usmc_desert_assault_body_mp_lod0300.xsb`. Record commands and sizes in docs\assets.md; add the roots.
- Then: every faction in the table renders textured with head; test the head attachment code (`j_head`/`tag_head`)
  that was untested. Screenshots in review\finish\factions\. List what is still missing.

## 3. Camo fidelity (M6 gap)
- Use the full camo technique, not only the diffuse swap: camo normal (`camo_nml` / design `dn1`), camo specular,
  environment term, `decalParams1` scale, as the materials define them (see docs\camos.md).
- Expose the H2 animated/native camo material variants (e.g. `mtl_h2_m4a1_receiver_fire`) if they are in the
  dumps; static frame is fine.
- No camo selected must stay pixel-identical (repeat the M6 SHA check).
- Screenshots: same 3 camos on H2 M4 and crossbow, before/after, review\finish\camos\.

## 4. Camos in game: test commands
- Find in the HMW/H1 scripts (dumps, refs\h1_dump, refs\iw5-scripts) exactly how a weapon with a camo is given
  (weapon-name suffix `_camo<NNN>` via buildweaponnamecamo, loadout camo name → camoTable index, and/or
  overrideviewmodelmaterial). Write the console/GSC commands to get the crossbow (and the H2 M4 as control) with
  each of the 3 test camos into docs\porting\aw.md, section "In-game test (camos)". Note whether the crossbow's
  material (`mtl_crossbow_base`, has the camo slots + `crossbow_mask_02`) should take them and what could stop it
  (e.g. weapon name not in camoTable / a whitelist in the HMW class scripts).

## 5. Comfort gaps (M10)
- Compare mode: a live split viewport in the window (two weapons, synced camera and anim time), not only the
  offscreen composition.
- Hot reload: also reload when material jsons or images of the loaded model change, and pick up new files in the
  roots (asset index refresh, debounced).

## At the end
docs\milestones\FINISH.md: per item done / BLOCKED, commits, measurements, files written outside the repo (only new
GAME\dump\workbench_* folders), free disk space, open questions. Commit, then stop and reply with a short summary.
