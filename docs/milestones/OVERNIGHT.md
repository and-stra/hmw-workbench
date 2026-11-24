# Overnight progress log - FINAL

Run: 24-25 Nov 2026, autonomous. Tasks: M5 (one open check), M6, M7, M8, M9, M10. Maps were not
part of this run.

**State: all requested milestones are attempted and committed. Repo is clean except the
untracked `review\claude\` (Claude's own artifact, intentionally left untracked).**

## Status per milestone

| milestone | state | commit | summary |
|---|---|---|---|
| M0-M4b | done | e4ba51a ... 5f6cec6 | previous sessions |
| M5 tag editing | **done** | 23c650c, bc8365e | copy-save check closed 14/14 |
| M6 camos | **done** | a419bbb | research + preview, 3 camos on M4/crossbow |
| M7 hardening | **done** | 33595c1 | README/workflow, 80/80 regression, window test 5/5 |
| M8 world models | **done** | 0779ae7 | world model + attachments, -world/MCP, world fit report |
| M9 player models | **done with asset gap** | d77140e | faction table + body/head + hand weapon + anims; 2 complete factions in dumps, missing assets listed |
| M10 comfort features | **done** | cf17c46, 49c86c7 | hot reload, compare mode, bone compare + rename map |

Maps: not started (explicitly out of scope).

## Self-checks

**M5** (all pass, `review\M5\`):
- roundtrip 523/523 identical, 0 errors.
- saved crossbow re-reads (39 bones), `-validate` unchanged, `.bak` = 4911 B original.
- fit report finds `tag_reflex`, `tag_acog`, `tag_silencer`.
- ADS end reflex/ACOG reticle aim (960.0, 508.0) at 1920x1016.
- idle/fire/reload sheets clean.
- MCP copy-save test **14/14** on `build\edit_test` (39 -> 40 bones, tag persisted, .bak
  pristine, real model sha256 unchanged) - `tools\test\m5-mcp-copy.ps1`.

**M6** (all pass, `review\M6\`):
- `docs\camos.md` cites every statement (script builtins, camoTable, `_camo` material slots,
  dumps, 355 weapon camos / 550 with character camos).
- 3 camos (`christmas_wrap`, `blue_jade`, `red_wash`) on H2 M4 and crossbow.
- no-camo frames SHA256-identical to the pre-M6 exe (M4 model, crossbow model, M4 first person).
- MCP camo test **6/6**.

**M7** (all pass, `review\M7\`):
- `-validate` 523/0 xmodels, 298/47 surfaces, 189/30 materials (known missing only).
- `-roundtrip` 523/523 identical.
- every M1-M6 review command + all MCP scripts: **80/80** (`review\M7\regression.txt`).
- fixed the outdated MCP smoke expectation (reflexmwr is fitted since M5): 42/42.
- scripted window test **5/5** (load crossbow/M4, anims, attachment, copy tag edit/save/reload,
  clean close; real model hash unchanged) - `review\M7\window-test.txt`.
- build 0 warnings; git clean after the commit.

**M8** (all pass, `review\M8\`):
- `npc_crossbow_base` and `wpn_h2_m4a1_npc` render textured; M4 world + camo too.
- world attachments by the same tag rule: M4 world reflex found on `tag_reflex`; crossbow world
  reflex MISSING (nearest `tag_red_dot`) because M5 fitted only the VM model - reported, not
  fixed (would be another game-file write).
- world fit reports: crossbow missing `tag_reflex`/`tag_acog`, M4 tags found.
- MCP world test **7/7**.

**M9** (done with a documented asset gap, `review\M9\`):
- faction table from `custom_teams.gsc`; body/head, viewhands availability and dropdown;
  world weapon on `tag_weapon_right` (verified: parent `j_wrist_ri`; `tag_inhand` is the chest
  tag); third-person `mp_jug_maniac_idle` / `mp_jug_maniac_stand_hold_throw` play on the
  player skeleton.
- **2 complete textured factions** (`hazmat`, `infect`) and the missing-asset list
  (`review\M9\missing-factions.txt`): `body_usmc_desert_assault_mp_camo` is present but its
  `.xsb` is missing; the HMW `body_h2_*`/`head_h2_*` and H1 `mp_body_*`/`head_*` models are not
  in the dumps. No outside-repo dump was allowed/written.
- world weapon in hand: `hazmat_m4_hand.png`, `hazmat_hold.png`, `infect_m4_hand.png`.
- MCP character test **8/8**.

**M10** (all pass, `review\M10\`):
- hot reload on a copy: **5/5** (39 -> 40 bones edit, replace file, debounced reload back to 39,
  real model untouched) - `review\M10\hot-reload.txt`.
- compare side and ghost screenshots (crossbow vs H2 M4) in `review\M10\`.
- rename-map test on the copy, made-up `{"j_mainroot":"j_gun"}`: compare-bones 2 matched / 1
  renamed / 91 missing, applied pose images differ - `review\M10\rename-map.txt`.
- MCP comfort test **5/5** (compare_screenshot side 1604x600, ghost 1200x900, compare_bones
  with/without map, rename_map) - `review\M10\mcp-comfort.txt`.
- final build 0 warnings.

## Files changed outside the repo

M5 (already done before this run; not touched again):
```
GAME\zonetool\mod\xmodel\vm_crossbow_base.xmb          5208 B
GAME\zonetool\mod\xmodel\vm_crossbow_base.xmb.bak      4911 B (original, 23 Nov 18:40)
GAME\zonetool\mod\xmodel\vm_crossbow_base.tags.json     7939 B
```
No other writes outside `C:\Games\cod-hwm\hmw-workbench` in M6-M10 (copy tests use
`build\edit_test`, gitignored). New dumps: none.

## Build / disk

- Clean Release x64 build, **0 warnings** (`build.log`).
- Free disk on C: **33.1 GB** (>= 20 GB).

## Known gaps / questions for the user

1. **M9 character assets:** only `body_hazmat` and `body_infect` are complete. To render the HMW
   factions (TF141/OpFor/Spetsnaz/Rangers/SEALs/Militia/GIGN/PMC/IC/Africa/Delta), the
   `body_h2_*` + `head_h2_*` (and H1 `mp_body_*`/`head_*`) models need to be dumped with
   zonetool into a new `GAME\dump\workbench_*` folder (outside-repo write, not allowed this
   run). The faction table/dropdown/code is already in place. `usmc_desert` also needs
   `usmc_desert_assault_body_mp_lod0300.xsb`.
2. **Crossbow world tags:** M5 added `tag_reflex`/`tag_acog` only to `vm_crossbow_base`. If world
   attachments are mounted from `npc_crossbow_base`, add the same tags there (tag edit + save).
3. **Reflex ring occlusion:** the crossbow's `tag_sight_on` ring still hides the reflex dot in
   ADS; fix is a `hideTags`/weapon-json change, not a tag fit issue.
4. **Camo fidelity:** the preview replaces the diffuse colour using the game's `m` mask; the real
   technique also swaps camo normal/spec/env. H2 native animated camo material variants
   (`mtl_h2_m4a1_receiver_fire` etc.) are not exposed.
5. **Compare / hot reload scope:** compare is an offscreen composition (side/ghost) rather than
   a live split viewport; hot reload watches the loaded asset files (weapon json/model/surface/
   hands/attachments/current anim), not arbitrary new files or material/image edits.
6. **M9 head attachment:** implemented for separate head xmodels (`j_head`/`tag_head`) but
   untested because no separate heads are dumped; the two available bodies include their heads.

## Resume note

`git log --oneline -8`:
```
49c86c7 M10: refresh review artifacts
cf17c46 M10: comfort features
d77140e M9: player models
0779ae7 M8: world models
33595c1 M7: hardening
a419bbb M6: camos
bc8365e M5: MCP copy-save self-check
e663b1d M5: record overnight checkpoint hash
```
The only untracked path is `review\claude\`, intentionally left alone.
