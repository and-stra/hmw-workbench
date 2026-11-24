# Overnight task (written by Claude, 24 Nov 2026)

The user is asleep. Work autonomously through M5, M6, M7, M8, M9, M10 below. Do NOT stop between milestones and do
not wait for answers; Claude reviews everything in the morning. Maps are NOT part of this run.

**This file is the task. If your context was reset or summarized, read this file, AGENTS.md, docs\PLAN.md and
docs\milestones\OVERNIGHT.md (your progress log) again and continue where the log says you are.**

## General rules

- Read AGENTS.md and docs\PLAN.md first. docs\porting\aw.md was updated by Claude: commit it with M5.
- One commit per milestone ("M5: tag editing", "M6: camos", "M7: hardening", "M8: world models",
  "M9: player models", "M10: comfort features"), each with a clean build (0 warnings) and its
  docs\milestones\M<n>.md. Never leave the repo uncommitted or broken.
- **Progress log:** after every milestone (and when you start one) update docs\milestones\OVERNIGHT.md: current
  milestone, done / BLOCKED / not started per milestone, commit hashes, self-check results. Commit it with the
  milestone. This is how you resume after a context reset.
- A milestone is done only if every self-check passes. If a check can't pass, write exactly why in the milestone file,
  mark it "BLOCKED: <reason>", and continue with the next milestone if it doesn't depend on it.
- If something blocks you for more than ~45 minutes, write down what you tried and move on.
- The Claude desktop app runs build\bin\Release\hmw-workbench.exe as an MCP server, so the exe may be locked. If
  linking fails because the file is in use, rename the running exe to hmw-workbench-prev-<n>.exe (renaming a running
  exe works) and build again. Never kill processes you didn't start.
- Writes outside the repo: ONLY GAME\zonetool\mod\xmodel\vm_crossbow_base.xmb (+ .bak + tags.json) in M5. Never touch
  cod-mwx, game files, .ff/.pak, zone_source, weapon jsons or any other dump file. Keep >= 20 GB free on C:.
- If time runs out, stop cleanly after the last finished milestone (committed, docs written) and list the rest in
  OVERNIGHT.md as "not started".

## M5 – Tag editing and saving

1. **.xmb writer + round-trip FIRST:** read and write every .xmb in all roots into a gitignored temp folder in the
   repo and byte-compare. 100% identical before any edit feature is enabled. `-roundtrip` CLI prints the result.
2. **Editing in the command layer** (UI and MCP use the same code):
   - move/rotate an existing tag (local to its parent): numeric fields + ImGuizmo gizmo, ghost of the original;
   - add a tag: name (dropdown of the tags HMW attachments need, from the fit report), parent bone, start transform
     copied from another tag or typed in. Append new bones at the end so parents stay before children; update every
     per-bone array the XModel has (names, parentList, partClassification, baseMat, tag positions/angles, bone
     info/collision, counts) – check x64-zt for the complete list. New tags have no skinned vertices;
   - edits apply live while anims play, hip and ADS, first person and orbit;
   - revert (per tag and all), dirty marker.
3. **Save** in place after copying the original to `<name>.xmb.bak` (never overwrite an existing .bak); write
   `<weapon>.tags.json` next to it (tag, parent, local pos, local rot). Re-read the saved file and validate it.
4. **MCP write tools:** move_tag, add_tag, revert_tags, save_model (save only when called; return the .bak path).
5. **Crossbow fit** (see docs\porting\aw.md):
   - add tag_reflex (start: tag_red_dot), tag_acog (start: tag_acog_2), tag_silencer (start: tag_flash) to
     vm_crossbow_base;
   - tune tag_reflex so that reflexmwr with vm_crossbow_reddot_ads_up (family map from M4) puts the red dot at screen
     centre at the end of ADS (within 3 px, -crosshair); tag_acog the same with acogmwr and vm_crossbow_acog_ads_up;
     suppressor aligned with the barrel (orbit side + top view);
   - save vm_crossbow_base in GAME\zonetool\mod with .bak + tags.json. Report the final offsets relative to
     tag_red_dot / tag_acog_2 / tag_flash. Nothing else in the port changes.

Self-check M5:
- [ ] roundtrip.txt: all files identical (count printed)
- [ ] saved vm_crossbow_base.xmb re-reads, -validate has no new failures, .bak exists and equals the original
      (byte-compare with the round-trip copy)
- [ ] after reload the fit report shows tag_reflex / tag_acog / tag_silencer found
- [ ] ADS-end screenshots with -crosshair: dot/reticle within 3 px of the centre (write the measured numbers)
- [ ] idle/fire/reload anims still play with the new tags (sheet), no exploded vertices
- [ ] MCP smoke test extended with the write tools, run on a COPY of a model in the temp folder, not the real file

Review files in review\M5\: roundtrip.txt, crossbow_reflex_ads_end.png, crossbow_acog_ads_end.png,
crossbow_reflex_hip.png, crossbow_silencer_side.png, crossbow_silencer_top.png, crossbow_reload_sheet.png,
crossbow_tags.json, fit_crossbow_after.txt, mcp-smoke.txt.

## M6 – Camos

1. Research first, write docs\camos.md: how HMW applies camos to view models (weapon camoViewModelMaterialOverride,
   "_camo" model/material names like wpn_h1_asl_m4a1_vm_camo, camo materials/images/constants/techsets in the
   workbench_* dumps, how a camo index becomes a material at runtime – H1 decomp, HMW GSC/Lua/statstable in the
   dumps). List the camos that exist.
2. Camo dropdown, -camo CLI, MCP parameter (load_weapon camo). Approximate the look in the shader as the materials
   define it (camo texture, mask/blend, scale).

Self-check M6:
- [ ] docs\camos.md cites the file/function each statement comes from
- [ ] at least 3 camos on the H2 M4 and on the crossbow (if the crossbow can't take camos, explain why)
- [ ] no camo selected = identical image to before M6 (pixel compare)
- Note which camos the user should compare in game. Review files in review\M6\.

## M7 – Hardening

- README.md: what the tool is, build, run, UI overview, CLI reference, link to docs\mcp.md, asset roots.
- docs\workflow-porting.md: step by step how to check a newly ported weapon (load, anims on all faction hands, fit
  report, add/move tags, ADS check, save, build zone, test in game), with the crossbow as the example.
- Run -validate, -roundtrip, the MCP smoke test and all review commands of M1–M6 again; fix regressions.
- Window app: start, load crossbow and M4, play anims, attach, edit a tag on a COPY, close cleanly.

Self-check M7: [ ] everything above passes, [ ] no warnings, [ ] git status clean after the commit.

## M8 – World models

- Weapon worldModel (+ world attachment models via the attachments' worldModels, same tag rule) in orbit view.
- Toggle view model / world model; -world CLI, MCP load_weapon parameter world=true.
- Fit report for world models too (crossbow npc_crossbow_base vs HMW attachment world models).

Self-check: [ ] crossbow + H2 M4 world models render textured, [ ] world fit report for the crossbow,
[ ] screenshots in review\M8\.

## M9 – Player models

- Load an HMW character: body + head (find how HMW builds characters: character/costume scripts in the dumps,
  custom_teams.gsc, playercostume). Faction dropdown like the hands one.
- Bind pose first; if third-person player anims (pb_*/mp_* xanims) exist in the dumps, play idle and a weapon-hold
  anim with the world weapon on the hand tag (verify which tag: tag_weapon_right / tag_inhand).
- MCP: load_character (faction or body + head).

Self-check: [ ] at least 3 factions render textured (otherwise list the missing assets), [ ] world weapon in hand
if anims exist, [ ] screenshots in review\M9\.

## M10 – Comfort features

- Hot reload: watch the asset roots, reload the current weapon/model when a dump file changes (debounced).
- Compare mode: two weapons/models side by side or overlaid (ghost), same camera (e.g. crossbow vs H2 M4);
  MCP compare_screenshot.
- Bone-name compare and a simple bone rename map for anims (for future IW7/T7 ports): show which anim channels don't
  match the loaded rig; optional rename-table json applied at load time (no file writes).

Self-check: [ ] changing a dump file copy in the temp folder triggers a reload, [ ] compare screenshots in
review\M10\, [ ] rename-map test with a made-up mapping on a copy.

## At the end

Finish docs\milestones\OVERNIGHT.md: per milestone done / BLOCKED / not started, commit hashes, every self-check with
its result, the files changed outside the repo (should be exactly vm_crossbow_base.xmb + .bak + tags.json), free disk
space, and a list of questions for the user. Commit it, then stop.
