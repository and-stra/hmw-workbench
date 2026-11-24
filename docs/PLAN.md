# hmw-workbench: plan

A lightweight Windows tool for porting content to HMW (H1 / MWR). First target: weapons. Load a weapon from
zonetool dumps, show it on viewhands, play its animations, mount HMW attachments, fix their fit by
editing tags, and preview camos. Later: world models and player models.

Repo: `C:\Games\cod-hwm\hmw-workbench` (its own git repo). It must never touch `cod-mwx`.

## Facts the design is built on (checked 24 Nov 2026)

- **Assets are read from zonetool (x64-zt) dump folders**, the same files the zones are built from:
  - `xmodel/*.xmb` (XModel), `xsurface/*.xsb` (XModelSurfs), `xanim/*.xab` (XAnimParts),
    `materials/**/*.json`, `images/*.h1Image` + `streamed_images/*.h1Image` / `*_streamN.pixels`,
    `weapons/*.json`, `attachments/*.json`, `techsets/`.
  - The binary layout of `.xmb/.xsb/.xab/.h1Image` is defined by the x64-zt H1 dumper source
    (most likely `github.com/Joelrau/x64-zt`; confirm it matches the build in `refs\tools\x64-zt\release`).
    The game structs (`XModel`, `XSurface`, `XAnimParts`, `Material`, `GfxImage`, `WeaponAttachment`, `DObjAnimMat`)
    are in `cod-mwx\src\client\game\database.hpp`; that file is **read-only reference**: copy what you need.
- **Asset roots** (searched in order; the first hit wins):
  1. `GAME\zonetool\mod` – the AW crossbow port (`weapons\iw5_exocrossbow*_mp.json`, `xmodel\vm_crossbow_base*.xmb`, 64 xanims)
  2. `refs\dumps\hmw_patch_common_mp` – HMW weapons, 74 attachments, attachment models
  3. `refs\dumps\patch_common_mp`, other `refs\dumps\*`, `GAME\dump\*`
  (`GAME` = `C:\Games\cod-hwm\Call of Duty Horizon Modern Warfare\Call of Duty Modern Warfare Remastered`)
- **Weapon json**: `gunModel[0]` (view model), `worldModel[0]`, `handModel` (`viewhands_tf141` for the crossbow),
  `szXAnims.<STATE>` (IDLE, FIRE, RELOAD, RAISE, ADS_UP, SPRINT_LOOP, ADDITIVE_*, …),
  `animOverrides[]` (`attachment1` index + `animTreeType` + `overrideAnim`), `camoViewModelMaterialOverride`
  (`mtl_crossbow_base`), `hideTags`.
- **`viewhands_tf141` is not in any dump yet.** The crossbow zone references it from retail MWR. It has to be dumped
  with zonetool into a new folder under `GAME\dump\` (find which retail zone contains it).
- **Attachments** (`attachments/*.json`): `viewModels[0]`, `worldModels[0]`, `reticleViewModels[]`, `hideTags`, `showTags`,
  `fields[]` (anim suffix overrides). There are **no position offset fields** in H1 weapons or attachments.
  An attachment model's first bone is the tag it mounts on (`attach_h1_optic_reflex_vm` → `tag_reflex`).
  The crossbow viewmodel has AW tag names (`tag_red_dot`, `tag_eotech`, `tag_acog_2`, `tag_scope`, `tag_vrd`,
  `tag_thermal_scope`, `tag_target_enhancer`, `tag_flash`, `tag_clip`…), so HMW optics won't find their tag.
  **Fitting = add or move tags in the weapon xmodel** (and save the `.xmb`). Verify the mounting rule in H1 code before M4.

- **Coordinate system**: the game is Z-up (X forward, Y left, Z up), units are inches. The workbench keeps game
  coordinates everywhere (bones, tags, edits, `-info` output); only the camera/view converts. Viewmodels are about
  10–40 units big, so the grid needs 1/10/100 unit steps.
- **Reference source** may be cloned into `hmw-workbench\.ref\` (gitignored), never anywhere else.

## Tech

- C++20, Visual Studio 2022 Build Tools, premake5 (same setup as cod-mwx), Release x64, one `hmw-workbench.exe`.
- Win32 + Direct3D 11 (block-compressed textures upload directly), Dear ImGui (docking branch), ImGuizmo,
  nlohmann/json, DirectXTex (DDS/BC helpers), DirectXMath or glm. Dependencies as git submodules in `deps/`.
- Layout: `src/app` (window, UI panels), `src/assets` (readers/writers, no D3D), `src/render` (D3D11),
  `src/scene` (skeleton, pose, attachment composition), `tools/` (build script), `docs/`.
- Settings in `workbench.json` next to the exe: asset roots, last weapon, camera, FOV.

### Headless review mode (needed for every milestone)

Claude reviews from a Linux VM and can't run the exe. Every milestone adds to a CLI that renders without UI:

```
hmw-workbench.exe -screenshot out.png [-weapon iw5_exocrossbow_mp | -model vm_crossbow_base]
                  [-anim vm_crossbow_reload -time 0.5] [-attach reflexmwr] [-camo <name>]
                  [-view orbit|firstperson] [-yaw 30 -pitch 10 -dist 40] [-bones] [-wire]
hmw-workbench.exe -info <asset>   # text dump: bones (name, parent, local pos/quat), surfaces, materials, anim frames/notetracks
```

Screenshots and `-info` output go to `review\M<n>\` (committed only as small PNG/TXT files).

## Milestones

The agent stops after each milestone, writes `docs/milestones/M<n>.md` (what was done, how it was tested,
the review commands it ran, open issues, any deviation from this plan) and commits `M<n>: <summary>`.
It does not start the next milestone until it gets the go-ahead.

### M0 – Scaffold
- `git init`, `.gitignore`, submodules, `premake5.lua`, `generate.bat`, `tools\build\build.bat` (log to `build.log`).
- Win32 + D3D11 window, ImGui docking layout: **Assets** (search box + list of weapons/models/anims from all roots),
  **Viewport**, **Inspector**, **Timeline** (empty for now).
- Viewport: grid, axes, orbit camera (LMB rotate, RMB/MMB pan, wheel zoom, F = frame selection).
- Asset root scanning + `workbench.json`. `-screenshot` works with an empty scene.
- Done when: builds clean from a fresh clone with `tools\build\build.bat`; `review\M0\empty.png` shows the grid.

### M1 – Static models
- Readers: `.xmb`, `.xsb` (positions, normals, tangents, UVs, vertex weights/blend info, triangles, LOD 0),
  material json (texture names by semantic: color, normal, specular/gloss), `.h1Image` + streamed `.pixels`
  (pick the highest available mip/stream) → D3D11 texture.
- Bone hierarchy and base pose from the xmodel; skinning done on the GPU (or CPU first if simpler, then GPU).
- Render: color map, normal map, simple lighting (one key light + ambient + basic spec). Missing textures → checker.
- Overlays: bones/tags with names (toggle), wireframe, show/hide per surface.
- Inspector: bone list (click selects + highlights), surfaces with material + texture names.
- Done when: `vm_crossbow_base` and one HMW weapon viewmodel (e.g. the M4) render textured in orbit view;
  `-info vm_crossbow_base` lists all the `tag_*` names above; screenshots in `review\M1\`.

### M2 – Viewhands + weapon (first person)
- Dump `viewhands_tf141` from retail MWR with zonetool into `GAME\dump\workbench_viewhands\` (document the command in
  `docs/assets.md`); add that folder as an asset root.
- Load a weapon json: hands model + gun model combined the way the game does it (verify in H1 code which bone of the
  gun joins which bone of the hands, e.g. `tag_weapon`/`j_gun`; document it). Merge into one skeleton for posing.
- First-person view: camera at the view origin, FOV from settings (default 65, plus `cg_fovScale`-style slider),
  near plane small enough for viewmodels. Toggle first person / orbit at any time (orbit target = weapon).
- Done when: crossbow on viewhands in first person, base pose, `review\M2\fp.png` + `orbit.png`.

### M3 – Animations
- `.xab` reader: frame count, framerate, bone names, quantized/compressed rotations and translations, delta parts,
  notetracks. Apply to the combined skeleton by bone name.
- Anim list in Inspector: every `szXAnims` state of the weapon with its anim, plus `animOverrides` (show the
  attachment and tree type), plus "all anims in roots".
- Timeline: play/pause, loop, speed (0.1–2×), scrub, frame step, notetrack markers with names.
- Additive anims (`ADDITIVE_*`, `*_add`): play on top of IDLE (checkbox); if not finished, list it as an open issue.
- Done when: idle, fire, reload, sprint loop, raise, ADS up play correctly on the crossbow; screenshots at several
  times of `vm_crossbow_reload`; `-info vm_crossbow_reload` shows frames + notetracks.

### M4 – Attachments
- Verify the mounting rule (attachment model root bone = weapon tag name) in H1 code; document it.
- Load `attachments/*.json` from all roots. Inspector: attachment picker (multi-select), per attachment show the
  view model, the tag it needs, and **found / missing** on the current weapon.
- Mount view models (and reticle view models) on their tags; apply `hideTags`/`showTags`; use the weapon's
  `animOverrides` and the attachment's `fields` anim suffixes when an attachment is active.
- Missing tag: the attachment is drawn at the weapon origin in red, with a hint "weapon has no `tag_reflex`".
- Done when: crossbow + `reflexmwr` shows "missing tag_reflex"; an HMW weapon with its own attachments mounts them
  correctly; screenshots.

### M5 – Tag editing (fit attachments) and saving
- `.xmb` writer. **First** a round-trip test: read → write every `.xmb` in all roots to a temp folder → byte-compare.
  Must be identical before any editing feature is enabled.
- Select a tag → move/rotate with ImGuizmo or numeric fields (local to parent). Ghost of the original position.
- Add tag: name (dropdown of tags HMW attachments need), parent bone, start position (e.g. copy from `tag_red_dot`).
  Rename/alias isn't enough because several attachment types may need different tags.
- Changes apply live while animations play, in first person and orbit.
- Save: writes the `.xmb` in place after copying the original to `<name>.xmb.bak` (never overwrite an existing `.bak`);
  "Revert" restores. Also export a small `<weapon>.tags.json` (tag, parent, pos, rot) so edits can be re-applied.
- Done when: round-trip passes; `tag_reflex` added to `vm_crossbow_base`, saved, reloaded, reflex mounts;
  the user builds the zone with zonetool and checks in game (manual step, listed in the report).

### M6 – Camos
- Research how HMW applies camos to view models (the weapon's `camoViewModelMaterialOverride`, the camo materials and
  images in the HMW dumps, the camo constants/techset). Write findings to `docs/camos.md` before coding.
- Camo dropdown; approximate the camo look in the shader (camo texture + mask/blend as the material defines it).
- Done when: two or three HMW camos on the crossbow and on an HMW weapon; screenshots side by side with an in-game
  screenshot provided by the user.

### Later
World model view (`worldModel` + world attachments), player models (body + head), hot reload when dump files change,
compare two weapons side by side, other asset types.

## Rules for the agent
- Work only in `C:\Games\cod-hwm\hmw-workbench`. Read-only everywhere else. **Never edit `cod-mwx`** (other agents
  work there), never change game files, `.ff`, `.pak`, zone or dump files, except: new dump folders under `GAME\dump\`
  (M2), and `.xmb` saves in M5 with a `.bak`.
- Keep at least 20 GB free on C:. Never copy or extract outside `C:\Games\cod-hwm`.
- No vendored binaries from untrusted sites; dependencies via git submodules from their official repos.
- One commit per milestone (more commits inside a milestone are fine). No pushes unless the user adds a remote.
- If the plan turns out wrong (a format differs, a rule isn't true), stop and write it in the report rather than
  working around it silently.
