# M4b - MCP server (read-only) + faction hands check

Status: **done**. Commit: `M4b: MCP server`.

## Goal

1. Quick check for `docs\porting\aw.md`: render the crossbow with every faction hands model that
   exists in the roots (idle + reload sheet) and the H2 M4 idle as a control; report load, anim
   channel coverage and arms/grip.
2. M4b: `hmw-workbench.exe -mcp`, a stdio MCP server (newline-delimited JSON-RPC 2.0) whose tools
   are thin wrappers around the shared command layer, every call < 30 s; headless offscreen
   rendering; register it in `opencode.json`; document every tool in `docs\mcp.md`; smoke-test it
   and run the plan's "done when".

## Part 1 - faction hands check

The full table is in `docs\porting\aw.md` under "Still unexplained / next checks / Faction hands
check". Summary:

- 7 of the 12 `custom_teams.gsc` hands models exist in the roots (`viewhands_tf141`,
  `viewhands_h1_arab_desert_mp_camo`, `globalViewhands_mw2_airborne`, `viewhands_rangers`,
  `viewhands_udt`, `globalViewhands_mw2_militia`, `viewhands_h2_ghillie`); the other 5
  (african_militia, pmc, henchmen, delta, sas) are not in any dump.
- All 7 load. `vm_crossbow_idle` / `vm_crossbow_reload` match **70/75** channels on six rigs and
  **72/75** on the arab rig. The unmatched channels are the crossbow-only bones `flipdown`,
  `flipup`, `arm` (no bone in `vm_crossbow_base` either) plus `j_wristtwist_back_le/ri`, which
  only the arab rig has.
- The arms show and hold the grip on all 7 (left hand on the foregrip, right hand behind the
  receiver at the trigger), in the idle and in the reload sheet; the H2 M4 control behaves the
  same. The arab hands render white/untextured with both weapons (missing workbench
  material/texture, not an AW-anim issue).
- "No arms" is **not** reproduced with any hands model in the roots; the next check is the 5
  missing hands models and the game-side `setviewmodel`/material path.

Review files: `review\M4b\hands_<model>_crossbow_idle.png` (1280x678),
`hands_<model>_crossbow_reload_sheet.png` (8 cells of 480x270) and `hands_<model>_m4_idle.png`
(control), 7 models each.

## Part 2 - MCP server

```
hmw-workbench.exe -mcp
```

- No window: the D3D11 device and an offscreen 1920x1016 scene target are created once; each
  `screenshot` renders and saves a PNG exactly like `-screenshot` (`src\app\review.cpp` is now the
  single offscreen render path for the CLI and the MCP: single frame, overlays and contact
  sheets).
- Transport: newline-delimited JSON-RPC 2.0 (`initialize`, `notifications/initialized`, `ping`,
  `tools/list`, `tools/call`), MCP protocol `2025-06-18`. stdout is reserved for the protocol;
  diagnostics go to stderr, and `info`/`validate` output is captured at the fd level before it is
  returned as text.
- State persists between calls: load a weapon once, then set animations/camera and take several
  screenshots.
- 12 tools (thin wrappers; no asset loading or posing logic in the server):
  `list_assets`, `load_weapon`, `load_model`, `set_anim`, `screenshot`, `get_bones`, `get_tags`,
  `posedump`, `attachment_fit_report`, `compare_tags`, `validate`, `info`.
  - `load_weapon` takes `attachments[]` and reports per attachment `view_model`, `required_tag`,
    `found`; missing tags are mounted red at the weapon origin.
  - `screenshot` supports path, first-person/orbit, width/height, yaw/pitch/dist/fov, bones, wire,
    crosshair, marktags, caption and `sheet N`; it returns the PNG as MCP image content plus a
    text block with the saved path and size. Relative paths resolve against the repo root;
    without `path` it writes `review\mcp\shot_NNN.png`.
  - `get_bones`/`get_tags`/`posedump` all use the current posed skeleton (viewmodel space);
    `attachment_fit_report` is the same plain-data report as M4; `compare_tags` compares bind-pose
    tags of two weapons/models; `validate` runs the full asset check (~0.6-10 s, 0 parse failures,
    the known 77 missing assets).
- Every tool is in `docs\mcp.md` with parameters and an example session.
- Registered in `opencode.json` (repo root) as the local server `hmw-workbench`. opencode only
  discovers config from its working directory upwards; when it runs in `C:\Games\cod-hwm` the
  same entry must be added there (documented in `docs\mcp.md`).

### Smoke test

`tools\test\mcp-smoke.ps1` starts the server, sends `initialize` + `tools/list`, calls every tool
once, checks the crossbow+reflex results and writes `review\M4b\mcp-smoke.txt` (all calls and
short results) plus the "done when" screenshots.

```
powershell -ExecutionPolicy Bypass -File tools\test\mcp-smoke.ps1
```

Result: **42 checks passed, 0 failed**. The log contains the 12-tool list, the crossbow with
`reflexmwr` (`found:false`, needed `tag_reflex`), the fit report (`nearest: tag_red_dot,
tag_eotech`), the bone/tag dumps, the full `posedump`, the animation calls, the screenshots, the
model/anim/weapon `info` dumps, `compare_tags` (5 shared crossbow/M4 tags), `validate` and the
final crossbow-reflex and M4-reflex-ADS screenshots.

Plan "done when": crossbow + `reflexmwr`, fit report and screenshot via the MCP:
`review\M4b\crossbow_reflexmwr_via_mcp.png` (the red attachment at the weapon origin, caption
"weapon has no tag_reflex"), `fit` data in `mcp-smoke.txt`.

## Review files

```
review\M4b\hands_<model>_crossbow_idle.png           7 models
review\M4b\hands_<model>_crossbow_reload_sheet.png   7 models, 8 cells
review\M4b\hands_<model>_m4_idle.png                 7 models (control)
review\M4b\mcp-smoke.txt                             every MCP call and short result
review\M4b\crossbow_reflexmwr_via_mcp.png            done-when screenshot via MCP
review\M4b\crossbow_hip_via_mcp.png
review\M4b\crossbow_reload_sheet_via_mcp.png
review\M4b\m4_reflex_ads_via_mcp.png                 crosshair + dot on centre
```

## Commands run

```
tools\build\build.bat

hmw-workbench.exe -screenshot review\M4b\hands_viewhands_tf141_crossbow_idle.png ^
                  -weapon iw5_exocrossbow_mp -hands viewhands_tf141 -view firstperson ^
                  -anim vm_crossbow_idle -time 0 -width 1280 -height 678
hmw-workbench.exe -screenshot review\M4b\hands_viewhands_tf141_crossbow_reload_sheet.png ^
                  -weapon iw5_exocrossbow_mp -hands viewhands_tf141 -view firstperson ^
                  -anim vm_crossbow_reload -sheet 8 -width 480 -height 270
hmw-workbench.exe -screenshot review\M4b\hands_viewhands_tf141_m4_idle.png ^
                  -weapon h2_m4_mp -hands viewhands_tf141 -view firstperson ^
                  -anim vm_h2_m4a1_idle -time 0 -width 1280 -height 678
(repeated for the other 6 hands models)

powershell -ExecutionPolicy Bypass -File tools\test\mcp-smoke.ps1
```

## Open issues / deviations

- The 5 remaining faction hands models are not in the dumps; the in-game "no arms" may involve one
  of them (or the game-side setviewmodel/material path). The workbench cannot test them yet.
- MCP is one client at a time and read-only (no tag saving; M5). It has no background job/queue:
  `validate` and `attachment_fit_report` block the single request for their duration (well under
  the 30 s budget).
- `screenshot` puts the base64 PNG in the JSON response (the MCP content block requires it); very
  large sheets mean a large response, but the default sizes are small.
- `load_model` unloads the weapon (otherwise the weapon pose would win); `load_weapon` does not
  unload a standalone model explicitly, but the weapon pose is used for `get_bones`/`get_tags`.
- `info`/`validate` are captured through a temporary fd redirect; nothing is written to the
  protocol stream.
- The CLI keeps its M4 review flags; both the CLI and the MCP now render through
  `src\app\review.cpp` (no second copy of the contact-sheet/overlay code).
- `docs\porting\aw.md` (written by Claude) is committed with this milestone; the hands-check
  section was added under "Still unexplained / next checks".
- No game/port files were changed; only `hmw-workbench`.
