# Asset dumps

This page documents how the M2 asset dumps were produced, where they live and which asset roots the
workbench uses. All dumps are new folders under `GAME\dump\workbench_*`; no existing dump folder, `.ff`,
`.pak` or zone was modified.

`GAME` = `C:\Games\cod-hwm\Call of Duty Horizon Modern Warfare\Call of Duty Modern Warfare Remastered`

## Free space check

Before dumping: 51.6 GB free on C: (>= 20 GB, so the dump was allowed).

## Zonetool build

`C:\Games\cod-hwm\Call of Duty Horizon Modern Warfare\Call of Duty Modern Warfare Remastered\zonetool.exe`
(X64-ZoneTool 0.0.0.491, 35 MB) was used. The newer build in
`refs\tools\x64-zt\release\zonetool.exe` (0.0.0.560, `latest-2-g3802db4`) was used for reading/validating the
dumps but did **not** work from a sandbox (see below). Both are the H1 zonetool.

## Dump method

Zonetool must run with the game folder as its working directory, and it writes each zone to
`dump\<fastfile>\`. HMW zones are not in `GAME\zone` but in `GAME\hmw-mod\zone`, so a plain
`-dumpzone hmw_attachments` says "zone could not be found".

Running from a sandbox folder with hardlinks/junctions of the game (to avoid touching `GAME\dump`) did not
work: the game stops during initialization with `MENU_CONTENT_NOT_AVAILABLE` (and zonetool never reaches its
command handler). This was reproduced with hardlinked and with copied game files; it is a location/Steam
check in the game, not a zonetool issue.

Working method: a **zone path traversal** in the zone name. `DB_FileExists` resolves `zone\<name>.ff`, so
`..\dump\workbench_data\<zone>` points to a hardlink of the HMW `.ff` inside a new workbench folder. Zonetool's
dump path then resolves to `dump\..\dump\workbench_data\<zone>` = `GAME\dump\workbench_data\<zone>`. It has the
side effect that `<zone>.pak` is also looked up below `workbench_data`, which is where the HMW stream pak is
hardlinked, so `-dump_streamed_image` can read it.

The H1 M4 viewmodel is **not** in `hmw_ar1` (that has `wpn_h2_m4a1_vm`, the H2 M4). `h1_m4_mp` references
`wpn_h1_asl_m4a1_vm_camo`, which is in the retail transient zone `mp_vm_m4_base_tr.ff`. That name ends in
`_tr`, and zonetool redirects `_tr` dumps to `dump\transient_assets`, so the `.ff` was hardlinked as
`workbench_data\mp_vm_m4_base.ff` (without `_tr`) and dumped under that name; asset names inside stay original.
Dumping the transient zone through the older game-folder zonetool hung; the refs x64-zt build (0.0.0.560)
dumps it quickly to `dump\weapon_models` (the `mp_vm_` prefix path), which was moved to
`GAME\dump\workbench_hmw_m4`. It contains `wpn_h1_asl_m4a1_vm_camo.xmb` + its `.xsb` (2.2 MB). Its materials
are not in any available dump, so this model renders with the checker; the M2 M4 screenshots use the H2 M4
from `hmw_ar1` instead.

Staging folder (new, allowed): `GAME\dump\workbench_data\` with hardlinks of the zone `.ff` (and `.pak` when
the zone has one). The dump output is then moved to `GAME\dump\workbench_<zone>\`.

All commands were run from `GAME`:

```
zonetool.exe -unbuffered-io -dump_streamed_image -dumpzone ..\dump\workbench_data\hmw_attachments
```

The dumps contain every asset type in the zone, including streamed image pixels (`.pixels`) because of
`-dump_streamed_image`.

## Dumps

| zone | source `.ff` | command (from `GAME`) | output folder | size |
|---|---|---|---|---|
| hmw_attachments | 354,742,864 B (338 MiB) | `zonetool.exe -unbuffered-io -dump_streamed_image -dumpzone ..\dump\workbench_data\hmw_attachments` | `GAME\dump\workbench_hmw_attachments` | 950.9 MB / 3553 files |
| hmw_patch_common_mp | 531,834,332 B (507 MiB) | `zonetool.exe -unbuffered-io -dump_streamed_image -dumpzone ..\dump\workbench_data\hmw_patch_common_mp` | `GAME\dump\workbench_hmw_patch_common_mp` | 15.66 GB / 72,454 files |
| hmw_ar1 | 280,771,104 B (268 MiB) + 68 MB pak | `zonetool.exe -unbuffered-io -dump_streamed_image -dumpzone ..\dump\workbench_data\hmw_ar1` | `GAME\dump\workbench_hmw_ar1` | 775.4 MB / 5,570 files |
| mp_vm_m4_base_tr (H1 M4 viewmodel) | 1,376,300 B (1.3 MiB) | `zonetool.exe -unbuffered-io -dumpzone mp_vm_m4_base_tr` (refs x64-zt build, 0.0.0.560) | `GAME\dump\workbench_hmw_m4` | 2.2 MB / xmodel + xsurface |
| hmw_post_gfx | 209,512,052 B (200 MiB) + 209 MB pak | `zonetool.exe -unbuffered-io -dump_streamed_image -dumpzone ..\dump\workbench_data\hmw_post_gfx` | `GAME\dump\workbench_hmw_post_gfx` | 618.1 MB / 3,922 files |

`hmw_attachments` contains all asset types of the zone (39 xmodels, 43 xsurfaces, 127 materials, 226 images,
5 weapons, xanims, sounds, effects, techsets...). Its cross-zone texture references are placeholders; the real
image data is in `hmw_patch_common_mp`, which therefore has to be an asset root as well.

`hmw_patch_common_mp` contains the HMW weapon materials, images and streamed image pixels
(`streamed_images\*.pixels`, 23,964 files), the faction viewhands (`viewhands_h1_arab_desert_mp_camo`,
`viewhands_h1_usmc_desert_mp_camo`, `viewhands_h1_sas_woodland_ghillie_mp_camo`,
`viewhands_h1_spetsnaz_ghillie_mp_camo`), grenades/knife models and 19 weapon jsons. It does **not** contain
the H1 M4 viewmodel.

`hmw_ar1` contains the AR weapon zone: `wpn_h2_m4a1_vm` (the H2 M4), other H2 AR viewmodels, materials,
streamed images and weapon jsons.

`hmw_post_gfx` contains `viewhands_tf141` (the crossbow's hands). It has its own `.pak`, which is hardlinked
into `workbench_data` so streamed images can be extracted.

`workbench_hmw_m4` contains only the H1 M4 xmodel (`wpn_h1_asl_m4a1_vm_camo`) and its xsurface; the material
jsons are missing from all dumps (see M2 open issues).

## Asset roots

`workbench.json` next to the exe lists the roots in this order (first hit wins):

1. `GAME\zonetool\mod` (AW crossbow)
2. `GAME\dump\workbench_hmw_patch_common_mp`
3. `GAME\dump\workbench_hmw_ar1`
4. `GAME\dump\workbench_hmw_m4`
5. `GAME\dump\workbench_hmw_attachments`
6. `GAME\dump\workbench_hmw_post_gfx`
7. `C:\Games\cod-hwm\refs\dumps\hmw_patch_common_mp` (old, incomplete; fallback)
8. `C:\Games\cod-hwm\refs\dumps\patch_common_mp`
9. `C:\Games\cod-hwm\refs\dumps\*`
10. `GAME\dump\*`

## Validation

After adding the roots and a clean build:

```
xmodels:   ok 523, failed 0
xsurfaces: ok 298, failed 47
materials: ok 189, failed 30
missing surface files: 47, missing materials: 30
```

0 parse failures. The missing files are NPC LOD/attachment/character surfaces and material jsons not present in
any dump (including the H1 M4 materials). Full list: `review/M2/validate.txt`.
