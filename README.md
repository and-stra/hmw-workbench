# hmw-workbench

A small Windows workbench for porting content to **HMW / MWR** (H1 / H2). It loads the zonetool
dumps directly, so you can inspect and fix a ported weapon before building the zone and testing
it in game:

- load a weapon json with its view model, viewhands and HMW attachments, or a standalone xmodel;
- play the weapon animations (hip and ADS) on the combined skeleton, orbit or first person;
- mount HMW attachments on their required tags, see found/missing in the fit report;
- **edit tags** (move/rotate/add/revert) with a gizmo or numeric fields, then save the `.xmb` in
  place with a `.bak` and a `.tags.json`;
- preview **camos** from the HMW camo materials on the view model;
- render offscreen screenshots and a text `-info` dump for review without a GPU desktop app.

Everything is exposed both in the UI and in the CLI/MCP command layer. The current state and the
design are in [docs/PLAN.md](docs/PLAN.md); the per-milestone reports are in
[docs/milestones/](docs/milestones/).

## Requirements

- Windows 10/11 x64, a GPU with Direct3D 11.
- Visual Studio 2022 Build Tools with the MSVC x64 toolset and the Windows SDK.
- Git; the dependencies are submodules (`deps/imgui`, `deps/ImGuizmo`, `deps/json`,
  `deps/DirectXTex`).
- The zonetool dumps (read-only input). See [Asset roots](#asset-roots).

## Build

From the repo root (`C:\Games\cod-hwm\hmw-workbench`):

```bat
tools\build\build.bat
```

The script initializes the submodules if needed, runs `tools\premake5 vs2022`, and builds Release
x64 with MSBuild. The binary is `build\bin\Release\hmw-workbench.exe`; the full build log is
`build.log`. Our code is compiled with `/W4` and the build must stay at **0 warnings**.

`generate.bat` only regenerates the Visual Studio solution (no build).

## Run

```bat
build\bin\Release\hmw-workbench.exe
```

The first run writes `workbench.json` next to the exe (ignored by git) and scans the asset roots.
Everything is read from the dumps; the app never writes game files by itself. The only writes
outside the exe folder are the explicit **Save model** action, which copies the original to
`<name>.xmb.bak` (only if no `.bak` exists) and writes `<model>.tags.json` next to it.

### Asset roots

`workbench.json` has an `assetRoots` array, searched in order (first hit wins). Relative paths
resolve against the exe folder, and a trailing `*` expands to every subfolder. Defaults:

1. `GAME\zonetool\mod` - the AW crossbow port (weapons, crossbow xmodel/xanims/materials)
2. `GAME\dump\workbench_hmw_patch_common_mp`
3. `GAME\dump\workbench_hmw_ar1` (H2 weapons, camo masks)
4. `GAME\dump\workbench_hmw_m4`
5. `GAME\dump\workbench_hmw_attachments`
6. `GAME\dump\workbench_hmw_post_gfx`
7. `refs\dumps\hmw_patch_common_mp` (HMW weapons, attachments, camo materials/textures)
8. `refs\dumps\patch_common_mp`
9. `refs\dumps\*`
10. `GAME\dump\*`

`GAME` = `C:\Games\cod-hwm\Call of Duty Horizon Modern Warfare\Call of Duty Modern Warfare
Remastered`. If a root is missing the Assets panel tooltip marks it and continues; missing
textures/materials show as checker/warnings, they do not abort a load. See
[docs/assets.md](docs/assets.md) for how the M2 dumps were produced and
[docs/viewmodel.md](docs/viewmodel.md) for the hands/weapon composition.

## UI overview

- **Assets** (left): search box and the weapons, xmodels and xanims from all roots with their
  source root. Click to load. `Rescan` re-scans the roots (use it after adding a dump).
- **Viewport** (center): orbit camera (LMB rotate, RMB/MMB pan, wheel zoom, `F` frame) or first
  person. Grid, bones/tags overlay, bone labels, wireframe, surface visibility toggles.
- **Inspector** (right): the loaded weapon (gun/hands/handModel, reload, view mode, grid,
  wireframe, ADS, FOV, hands dropdown, attachment list with found/missing and mount toggles),
  the standalone model, the **Camo** section (filter + dropdown, strength, UV scale), the **Tags
  (edit + save)** section (tag list, local position/quaternion, ImGuizmo move/rotate, ghost,
  Add tag from the tags HMW attachments need, Revert tag/all, Save model, dirty marker) and the
  **Animation** list/overrides.
- **Timeline** (bottom): play/pause, loop, speed, scrub, frame step, notetracks and the additive
  toggle.
- **Menu**: File (rescan, reload model/weapon, save settings, exit), View (reset layout/camera,
  grid, wireframe).

The Inspector's edit and save actions go through the same `commands::Workbench` layer as the CLI
and MCP, so a change made in the UI can be reproduced from a script.

## CLI reference

`hmw-workbench.exe -help` prints this list. All commands are offscreen where applicable; paths are
relative to the current working directory unless they are absolute.

| command | what it does |
|---|---|
| `-screenshot <file.png>` | render one frame/sheet offscreen and exit |
| `-model <xmodel>` | load a standalone model (orbit) |
| `-weapon <name>` | load a weapon json (with viewhands; first person with `-view firstperson`) |
| `-hands <viewhands>` | viewhands override |
| `-view firstperson\|orbit` | view mode |
| `-ads` | aim down sights (weapon) |
| `-anim <xanim>` / `-time <s>` / `-frac` | animation, time in seconds or normalized |
| `-sheet <n>` | contact sheet of n cells of the current animation |
| `-attach <name>` (repeatable) | mount an HMW attachment |
| `-hidesurface <i,i,...>` | hide weapon surfaces by index |
| `-camo <camo_h1_name\|name>` | apply a camo (M6); `-camo-strength 0..1`, `-camo-scale n` |
| `-world` | load the weapon's world model (no hands, orbit) and its attachments' world models |
| `-bones` / `-wire` / `-crosshair` / `-marktags` / `-grid` / `-nogrid` | overlays |
| `-yaw <deg>` / `-pitch <deg>` / `-dist <units>` / `-fov <vertical deg>` | camera |
| `-exposure <n>` | lighting exposure |
| `-width <n>` / `-height <n>` | image/sheet cell size |
| `-caption <text>` | caption in the top-left corner of a screenshot |
| `-no-ads-align` | disable automatic ADS reticle alignment (tag fitting) |
| `-posedump [file]` | text dump of the posed skeleton, animation coverage, bounds, tag screen positions |
| `-info <xmodel\|xanim>` | text dump (bones/tags/surfaces/materials or anim frames/notetracks) |
| `-fit-report [file] -weapon <name>` | attachment fit report (needed tag, found/missing, nearest tags; add `-world`) |
| `-validate` | parse every xmodel + surface set + material in all roots (exit 1 when known assets are missing) |
| `-roundtrip [report.txt]` | read/write every `.xmb` to `build\roundtrip` and byte-compare |
| `-fit-crossbow -weapon <name>` | add/fit `tag_reflex`, `tag_acog`, `tag_silencer` and save the crossbow (M5) |
| `-edit-test <src.xmb> <dst.xmb>` | add/move a tag and save/reload/validate a copy (test helper) |
| `-window-test [report.txt]` | scripted window run (crossbow + M4 + copy tag edit + close) for M7 |
| `-mcp` | stdio MCP server (see below) |

Examples:

```bat
hmw-workbench.exe -info vm_crossbow_base
hmw-workbench.exe -screenshot review\shot.png -weapon iw5_exocrossbow_mp -view firstperson ^
                  -anim vm_crossbow_idle -time 0 -attach reflexmwr -width 1920 -height 1016
hmw-workbench.exe -screenshot review\camo.png -model wpn_h2_m4a1_vm -view orbit ^
                  -yaw 200 -pitch 10 -dist 24 -camo camo_h1_blue_jade
hmw-workbench.exe -fit-report review\fit.txt -weapon iw5_exocrossbow_mp
hmw-workbench.exe -roundtrip review\roundtrip.txt
```

## MCP server

`hmw-workbench.exe -mcp` is a newline-delimited JSON-RPC MCP server over the same command layer
(load weapons/models, attachments, anims, tags, camos, screenshots, fit report, validate). Tools:
`list_assets`, `load_weapon`, `load_model`, `set_anim`, `screenshot`, `get_bones`, `get_tags`,
`posedump`, `attachment_fit_report`, `compare_tags`, `validate`, `info`, `move_tag`, `add_tag`,
`revert_tags`, `save_model`. Every tool and parameter, client configuration and a smoke test are
in [docs/mcp.md](docs/mcp.md).

## Porting a weapon

The step-by-step check (load, hands, fit report, add/move tags, ADS check, save, build the zone,
test in game) with the crossbow as the worked example is in
[docs/workflow-porting.md](docs/workflow-porting.md). Camo research and the preview pipeline are
in [docs/camos.md](docs/camos.md).

## Tests and review artifacts

`tools\test\` contains the scripted checks used by the milestones:

- `mcp-smoke.ps1` - every MCP tool once, writes `review\M4b\mcp-smoke.txt`.
- `m5-edit.ps1` - MCP move/add/revert on the real model (no save), `review\M5\mcp-edit.txt`.
- `m5-mcp-copy.ps1` - MCP add/move/save on a copy in `build\edit_test`, `review\M5\mcp-smoke.txt`.
- `m6-camo.ps1` - MCP camo on the M4/crossbow, `review\M6\mcp-camo.txt`.
- `m7-window.ps1` - scripted window run, `review\M7\window-test.txt`.
- `m7-regression.ps1` - `-validate`, `-roundtrip`, every M1-M6 review command and all the above,
  `review\M7\regression.txt` (large images under `review\M7\regression\`, gitignored).

## Repository layout

```
src\app      window, ImGui panels, command layer, CLI, MCP, info/review helpers
src\assets   readers (.xmb/.xsb/.xab/.h1Image/.json), camo/material/model/weapon/attachment, writers
src\render   D3D11 renderer, camera, shaders (inline HLSL)
src\scene    skeleton pose, viewmodel composition, animation sampling
tools\build  premake/MSBuild build script
tools\test   the PowerShell test scripts
docs\        plan, milestone reports, MCP/asset/porting/camo docs
review\      per-milestone screenshots and logs
```

Dependencies are git submodules in `deps/`; `.ref/` and `build/` are gitignored. The app never
touches `cod-mwx`, game `.ff`/`.pak`, zones or existing dumps; the only exception is the explicit
tag save described above.
