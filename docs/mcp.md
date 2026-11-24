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
| `first_person` | bool | `true` | first-person view (orbit otherwise) |

Returns `{weapon, gun_model, hands, hands_source, first_person, attachments:[{name,view_model,
required_tag,found}], warnings}`. A missing tag is reported with `found:false`; the attachment is
still mounted (red, at the weapon origin) so a screenshot shows the problem.

### `load_model`

Load a standalone xmodel (unloads the weapon, switches to orbit).

`{"model":"vm_crossbow_base"}` → `{model,bones,surfaces,materials,bounds_mid,bounds_half}`.

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
| `attachments` | string[] | all attachment assets in the roots |

Returns `{weapon,gun_model,hands,weapon_tags,[{name,view_model,required_tag,found,nearest,note}]}`.

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

## Smoke test

```powershell
powershell -ExecutionPolicy Bypass -File tools\test\mcp-smoke.ps1
```

The script starts the server, initializes it, calls every tool once, checks the crossbow + reflex
results, saves the "done when" screenshots and writes `review\M4b\mcp-smoke.txt` (all calls and
short results). Exit code 0 means every check passed.
