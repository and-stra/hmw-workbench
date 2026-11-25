# MCP server (`-mcp`)

`hmw-workbench.exe -mcp` is a stdio MCP server over the shared command layer ("hmw-workbench"
tools). It has no window: D3D11 renders into an offscreen target exactly like `-screenshot`.
State (loaded weapon/model, animation, ADS, camera) persists between calls, so a client can load
once and then pose and render repeatedly. Every call answers well under 30 s (`validate` ~10 s,
`attachment_fit_report` ~5 s, screenshots 1-2 s).

## Client configuration

`opencode.json` in the repo registers it:

```json
{
  "$schema": "https://opencode.ai/config.json",
  "mcp": {
    "servers": {
      "hmw-workbench": {
        "type": "local",
        "command": ["C:\\Games\\cod-hwm\\hmw-workbench\\build\\bin\\Release\\hmw-workbench.exe", "-mcp"]
      }
    }
  }
}
```

opencode only discovers project config from its own working directory upwards. When it runs in
`C:\Games\cod-hwm` (one level above this repo), add the same server there (like the `mwx`
entry) or run `opencode mcp add hmw-workbench -- <exe> -mcp` from that directory.

Protocol: newline-delimited JSON-RPC 2.0 (MCP `2025-06-18`): `initialize`, `notifications/initialized`,
`ping`, `tools/list`, `tools/call`. No Content-Length framing. Logs go to stderr, never stdout.

## Tools

Positions are game/viewmodel space (X forward, Y left, Z up), inches. "Posed" means after the
current animation/ADS pose. Screenshot paths are relative to the repo root unless absolute.

### `list_assets`

List scanned assets from every root (weapons, xmodels, xanims).

| parameter | type | default | meaning |
|---|---|---|---|
| `kind` | string | `all` | `weapon`, `model`, `anim` or `all` |
| `name` | string | – | case-insensitive name substring |
| `limit` | integer | 200 | maximum entries (1..1000) |

Returns `{count, truncated, assets:[{name,kind,root}]}`.

### `load_weapon`

Load a weapon json, its hands and attachments in first person (or orbit).

| parameter | type | default | meaning |
|---|---|---|---|
| `weapon` | string | required | e.g. `iw5_exocrossbow_mp` |
| `hands` | string | weapon `handModel` / faction default | viewhands override |
| `attachments` | string[] | `[]` | attachment asset names (`reflexmwr`, `acogmwrm4`, ...) |
| `camo` | string | – | camo material (`camo_h1_blue_jade`) or bare name (`blue_jade`); applied to the gun + attachment view models |
| `world` | bool | `false` | load `worldModel` instead of `gunModel` (no hands, orbit); attachments use their `worldModels` |
| `first_person` | bool | `true` | first-person view (orbit otherwise; `world` forces orbit) |

Returns `{weapon, gun_model, loaded_model, world_model, hands, hands_source, first_person,
camo:{name,texture,strength}, attachments:[{name,view_model,required_tag,found}], warnings}`. A
missing tag is reported with `found:false`; the attachment is still mounted (red, at the weapon
origin) so a screenshot shows the problem. An unknown camo reports `camo:{name,error}` and leaves
the weapon loaded without a camo (see `docs/camos.md`).

### `load_model`

Load a standalone xmodel (unloads the weapon, switches to orbit).

`{"model":"vm_crossbow_base"}` → `{model,bones,surfaces,materials,camo,bounds_mid,bounds_half}`.
The optional `camo` parameter works like `load_weapon` (`"none"` clears it).

### `load_character`

Load an HMW player character from a faction or from explicit body/head xmodels, optionally mount
a world weapon in the hand and play a third-person anim.

| parameter | type | meaning |
|---|---|---|
| `faction` | string | faction id: `tf141`, `opfor`, `spetsnaz`, `rangers`, `seals`, `militia`, `gign`, `pmc`, `ic`, `africa`, `delta`, `usmc_desert`, `hazmat`, `infect` |
| `body` / `head` | string | explicit xmodel names (when `faction` is omitted; head optional) |
| `weapon` | string | weapon json whose `worldModel` is mounted |
| `anim` / `time` | string/number | third-person xanim and seconds |

Returns `{faction, display, body, head, weapon, weapon_tag, weapon_tag_world, weapon_found,
missing, warnings, anim?, frames?, time?}`. The weapon mounts on `tag_weapon_right` (the right
wrist tag; `tag_inhand` is the chest/in-hand tag) with the same root-bone rule as attachments.
The faction table and which bodies are actually in the dumps are in `docs/milestones/M9.md`.

### `set_anim`

Play an xanim, or a weapon state name (`IDLE`, `ADS_UP`, `RELOAD`, ...) with the weapon's
anim-overrides/attachment anim suffixes applied.

| parameter | type | meaning |
|---|---|---|
| `name` | string | xanim name or weapon state |
| `time` | number | seconds (after playback starts) |
| `frac` | number | normalized 0..1 (used when `time` is absent) |
| `ads` | bool | ADS overlay on/off |

Returns `{requested, anim, frames, fps, duration, time, frame, ads, looping}`.

### `screenshot`

Render the current state offscreen and return the PNG as an MCP image content block plus a text
block with the saved path and size.

| parameter | type | default | meaning |
|---|---|---|---|
| `path` | string | `review/mcp/shot_NNN.png` | output PNG |
| `view` | string | keep current | `firstperson` or `orbit` |
| `width` / `height` | int | 1920 / 1016 | image size; cell size when `sheet` is set |
| `yaw` / `pitch` / `dist` / `fov` | number | current | camera |
| `bones` | bool | false | standalone-model bone overlay |
| `wire` | bool | false | wireframe |
| `crosshair` | bool | false | crosshair at the screen centre |
| `marktags` | bool | false | label every weapon tag |
| `caption` | string | – | text in the top-left corner |
| `sheet` | int | 0 | contact sheet of N cells of the current animation |
| `ads` | bool | – | set ADS before rendering |
| `ads_align` | bool | false | **debug only**: snap `tag_ads` to the reticle (hides real misalignment) |

### `get_bones`

Posed skeleton in viewmodel space.

| parameter | type | default |
|---|---|---|
| `name` | string | – (substring filter) |
| `tags_only` | bool | false |
| `limit` | integer | 1000 |

Each bone: `{name,parent,source,local_pos,world_pos,local_quat,world_quat}`; `source` is
`hands`, `gun` or `model`.

### `get_tags`

Posed `tag_*` bones: `{name,local_pos,world_pos}` (viewmodel space). Use `name` to filter.

### `posedump`

Text dump of the posed skeleton, the animation channels for the root/viewmodel bones, gun bounds,
posed tag screen positions and the animation channel coverage. Parameters `width`/`height`
(default 1920x1016) only affect the projected screen positions. Returns `{text}`.

### `attachment_fit_report`

Plain-data fit report for all attachment assets (or the given subset): view model, required weapon
tag, found/missing, nearest weapon tags.

| parameter | type | default |
|---|---|---|
| `weapon` | string | the loaded weapon |
| `hands` | string | resolved like `load_weapon` |
| `world` | bool | `false`: use the weapon's world model and the attachments' world models |
| `attachments` | string[] | all attachment assets in the roots |

Returns `{weapon,gun_model,world,hands,weapon_tags,[{name,view_model,required_tag,found,nearest,note}]}`.

### `compare_tags`

Compare the bind-pose tags of two weapons or models (weapon names resolve to their `gunModel`).

`{"a":"iw5_exocrossbow_mp","b":"h2_m4_mp"}` → `{a,a_model,b,b_model,shared:[{name,a,b,delta,distance}],only_a,only_b}`.

### `validate`

Parses every `.xmb` in every root plus `.xsb` surface sets and material json (same as
`-validate`). Returns `{failures, summary, output}`. The current dumps have 0 parse failures and
77 known missing assets (47 surface files, 30 material json).

### `info`

Text info for a weapon, xmodel or xanim (the `-info` CLI dump).

| parameter | type | meaning |
|---|---|---|
| `asset` | string | asset name |
| `kind` | string | `weapon`, `model`, `anim` (optional; detected otherwise) |

Weapon output includes `gunModel`, `worldModel`, `handModel`, `adsZoomFov`, `hideTags`, the
attachment list and every state + anim override with its mapped state.

### `move_tag`

Move/rotate a tag on the loaded weapon/model. The transform is **local to the tag's parent bone**
unless `world` is true (viewmodel space).

| parameter | type | meaning |
|---|---|---|
| `tag` | string | tag bone name |
| `pos` | number[3] | position |
| `rot` | number[4] | quaternion x,y,z,w (optional, default identity) |
| `world` | bool | interpret `pos`/`rot` in viewmodel space |

Returns the tag's parent, local position/rotation, world position and the dirty flag.

### `add_tag`

Append a tag bone at the end (parents stay before children), updates all per-bone arrays
(names + string list, parentList, tagAngles, tagPositions, partClassification, baseMat, boneInfo,
counts). New tags have no skinned vertices.

| parameter | type | meaning |
|---|---|---|
| `tag` | string | new tag name (`tag_reflex`, ...) |
| `parent` | string | parent bone (default `j_gun` / `tag_weapon`) |
| `copy_from` | string | copy the local transform of an existing tag |
| `pos` / `rot` | numbers | local transform when `copy_from` is omitted |

### `rename_tag`

Rename an existing bone/tag. The name string item is rewritten (inline) and the stored
`scr_string_t` placeholder is kept, exactly how x64-zt pairs them; `save_model` writes the file
with the `.bak` rule.

`{"old":"tag_sight_on","new":"tag_sight_front_on"}` → `{old,new,dirty}`.

### `fit_tag`

Attach an attachment, set ADS and move a tag in its parent space until the mounted reticle's aim
is on the camera axis at the end of ADS (first person). Uses the weapon json's anim overrides.

`{"tag":"tag_reflex","attachment":"reflex"}` → `{tag,attachment,anim,source_local,local_pos,
local_delta,world_pos,screen,iterations}`. Does not save; call `save_model` afterwards.

### `revert_tags`

Revert one tag to the loaded/saved state, or all tags when `tag` is omitted.

`{"tag":"tag_reflex"}` → `{reverted:"tag_reflex", dirty:false}`.

### `save_model`

Save the edited model **in place**: copies the original to `<name>.xmb.bak` (only if no `.bak`
exists), writes `<model>.tags.json` next to it, re-reads the file and validates it.

Returns `{saved, backup, tags_json, validation, dirty}`.

### `compare_screenshot`

Render two weapons/models with the same camera and return a PNG: side by side (one image with a
white divider) or `b` ghosted over `a`.

| parameter | type | default | meaning |
|---|---|---|---|
| `a` / `b` | string | required | weapon json or xmodel names |
| `mode` | string | `side` | `side` or `ghost` |
| `alpha` | number | 0.45 | ghost blend |
| `view` | string | – | `firstperson` or `orbit` |
| `width` / `height` | int | 1600 / 900 | panel size (side output is `2*width+4`) |
| `yaw` / `pitch` / `dist` / `fov` | number | – | camera (same for both) |
| `world` | bool | false | use the world models |
| `camo` | string | – | camo applied to both |
| `path` | string | `review/mcp/compare_NNN.png` | output |

### `hot_reload`

The server polls the loaded weapon/model/character files on every tool call (400 ms debounce) and
reloads them when a file changes. `{"action":"poll"}` forces a poll; `{"enabled":false}` stops
watching; returns `{enabled,pending,reloads,status,watched}`.

### `rename_map`

Load a bone rename table `{"old_anim_bone":"new_model_bone"}` used while sampling animations
(`{"path":"review/M10/rename_map_test.json"}`), or `{"clear":true}`. Returns the table and entry
count.

### `compare_bones`

`{"anim":"mp_jug_maniac_idle","model":"vm_crossbow_base","rename_map":"..."}` → channels,
model bones, matched/renamed/missing counts and the channel names. `rename_map` is optional; the
current `rename_map` table is used otherwise.

## Example session

```json
{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-06-18"}}
{"jsonrpc":"2.0","method":"notifications/initialized"}
{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"load_weapon","arguments":{"weapon":"iw5_exocrossbow_mp","hands":"viewhands_tf141","attachments":["reflexmwr"]}}}
{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"attachment_fit_report","arguments":{"attachments":["reflexmwr"]}}}
{"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"set_anim","arguments":{"name":"IDLE","time":0}}}
{"jsonrpc":"2.0","id":5,"method":"tools/call","params":{"name":"screenshot","arguments":{"path":"review/M4b/crossbow_reflexmwr_via_mcp.png","view":"firstperson","pitch":-18,"crosshair":false,"caption":"weapon has no tag_reflex"}}}
```

The fit report answers `tag_reflex` found `false` with nearest `tag_red_dot`, `tag_eotech`;
the screenshot content contains the PNG (red attachment at the weapon origin) and a text block
with the saved path.

Tag editing (unsaved until `save_model`):

```json
{"jsonrpc":"2.0","id":6,"method":"tools/call","params":{"name":"add_tag","arguments":{"tag":"tag_reflex","parent":"j_gun","copy_from":"tag_red_dot"}}}
{"jsonrpc":"2.0","id":7,"method":"tools/call","params":{"name":"move_tag","arguments":{"tag":"tag_reflex","pos":[3.243,0.0,6.092],"rot":[0,0,0,1]}}}
{"jsonrpc":"2.0","id":8,"method":"tools/call","params":{"name":"revert_tags","arguments":{"tag":"tag_reflex"}}}
{"jsonrpc":"2.0","id":9,"method":"tools/call","params":{"name":"save_model","arguments":{}}}
```

## Smoke test

```powershell
powershell -ExecutionPolicy Bypass -File tools\test\mcp-smoke.ps1
```

The script starts the server, initializes it, calls every tool once, checks the crossbow + reflex
results, saves the "done when" screenshots and writes `review\M4b\mcp-smoke.txt` (all calls and
short results). Exit code 0 means every check passed.
