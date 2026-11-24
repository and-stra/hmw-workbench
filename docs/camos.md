# Camos: how HMW applies them and how the workbench previews them

Research for M6 (24/25 Nov 2026). Every statement names the file it comes from; the workbench
approximation is in the last section.

## 1. Where a camo comes from at runtime

- The player's class stores a camo **name**, not a texture:
  `rankedMatchData/customClasses/<n>/weaponSetups/<n>/camo/none` is the default written by the
  HMW client (`refs\hmw_decomp\decomp_001.c:69554`, string table
  `refs\hmw_decomp\strings.tsv`).
- The scripts resolve that name through `mp/camoTable.csv`:
  `tablelookup( "mp/camoTable.csv", 1, self.loadoutprimarycamo, 0 )` in
  `refs\iw5-scripts\maps\mp\_utility.gsc:1394` (IW5's `_giveweapon` path) and the
  `mp/camoTable.csv` string in the H1 compiled scripts
  `refs\h1_dump\h1_dump\scripts\42852.gsc:6,21` and `42891.gsc:2..4`.
- HMW keeps the same script surface: `table_getweaponcamo`, `getweaponcamoname`,
  `weaponcamoorder`, `unlockedcamo`, `cac_getweaponcamo` are script builtins listed in
  `refs\hmw_decomp\strings.tsv:5537, 19279, 23865` (function names at 1607143f0,
  1607683f0, 160784a10, 160793d58, 1607b3480).
- The material is swapped on the **view model**, not on the world model. HMW exposes three
  script builtins for that: `overrideviewmodelmaterial`, `overrideviewmodelmaterialreset` and
  `setviewmodelmaterialscriptparam` (`refs\hmw_decomp\strings.tsv:7253, 7258, 7259`; native
  functions at 16071d680 / 16071d728 / 16071d748). This is the mechanism the plan called
  `camoViewModelMaterialOverride`: **the field name does not exist** in any HMW weapon json
  (`refs\dumps\*\weapons\*.json`), in the HMW decompiled string table or binary
  (`refs\hmw_bin\hmw-mod.exe` has no such string), or in the H1 struct reference
  (`cod-mwx\src\client\game\database.hpp`, `WeaponDef` has no camo member). What exists is the
  script override plus the `_camo` asset naming below.
- The H1 camo name list is in the H1 compiled class script
  `refs\h1_dump\h1_dump\scripts\42891.gsc:4`: the basic names (`darkmatter`, `diamond`,
  `neontiger`, `nickel`, `fde`, `odgreen`, `redhex`, `redtiger`, `bluetiger`, `digital`,
  `gold`, `woodland`, `desert`, `white`) followed by `wc_*` loot camos (`wc_super_reward`,
  `wc_aftermath_01`, ..., 279 unique `wc_*` names) and the numeric `camo089`..`camo001`
  slots. The same file builds the weapon name suffix used for the camo variant:
  `buildweaponnamecamo` with `_camo00`, `_camo0`, `_camo` (line 3).

## 2. The `_camo` asset convention in the dumps

HMW/MWR do not store one complete model per camo. They store a camo-ready model/material and a
camo texture set:

- **Camo-ready view model.** H1 M4's weapon json points straight at a `_camo` model:
  `refs\dumps\hmw_patch_common_mp\weapons\h1_m4_mp.json` has
  `gunModel: ["wpn_h1_asl_m4a1_vm_camo", ""]` and
  `worldModel: ["wpn_h1_asl_m4a1_npc_camo", ""]`. H2 M4 keeps the normal model
  `wpn_h2_m4a1_vm` (`GAME\dump\assets\weapons\h2_m4_mp.json`), but its materials are already
  the `_camo` variants: `-info wpn_h2_m4a1_vm` lists `m/mtl_h2_m4a1_foregrip`,
  `m/mtl_h2_m4a1_grip_camo`, `m/mtl_h2_m4a1_stock_camo`, 8x
  `m/mtl_h2_m4a1_receiver_camo` and 5x `m/mtl_h2_m4a1_handguard_camo`.
- **Camo-ready material.** Example
  `GAME\dump\workbench_hmw_ar1\materials\m\mtl_h2_m4a1_receiver_camo.json`
  (technique `m_l_sm_lmpb_dc1dn1ds1dg1dna1_r0c0n0sd0om0_nfwpf`). Its texture table is the
  template for every camo:

  | typeHash | role in the technique | image in the `_camo` material |
  |---|---|---|
  | 659983135 | `dc1` camo design (placeholder) | `camo_transparent_col` |
  | 887934131 | `ds1` specular | `i_h2_m4a1_receiver_s` |
  | 1507003663 | `dn1` normal | `i_h2_m4a1_receiver_n` |
  | 1518176625 | second normal (`dna1`) | `camo_nml` |
  | 1610742065 | `om0` occlusion | `i_h2_m4a1_receiver_o` |
  | 2695565377 | `dc2`/`c` base colour | `i_h2_m4a1_receiver_c` |
  | 3135443286 | `dm` camo mask | `i_h2_m4a1_receiver_m` |
  | 3426233914 | camo specular | `~camo_spcmaps_01_spc-rgb&camo~b29a8732` |
  | 3447584578 | environment LUT | `~envbrdflut_ggx_16-rg` |

  Constants: `decalParams1 = (2,2,0,0)`, `envMapParms = (0.04,1,5,1)`.
- **The crossbow port already follows the same layout.** `GAME\zonetool\mod\materials\m\
  mtl_crossbow_base.json` has `camo_transparent_col` in `dc1`, `crossbow_col` in the `c` slot
  and `crossbow_mask_02` in the `m` slot (plus `camo_nml`, `~camo_spcmaps_01...` and the
  environment LUT). So the AW crossbow can take HMW camo textures through the same mask, even
  though it has no `_camo` model variant.
- **Camo design material.** Every previewable camo is a material
  `refs\dumps\hmw_patch_common_mp\materials\camo_h1_<name>.json` with the same technique family
  and its design in `dc1`. Example `camo_h1_aftermath_epic.json`:
  `dc1 = wpn_h1_camo_aftermath_epic_01_col`, `ds1 = ~wpn_h1_camo_aftermath_epic_0~87ccdbce`,
  `dn1 = $identitynormalmap`, `c = camo_reference_col`, `m = fp_black_col`,
  `dg = ~wpn_h1_camo_aftermath_epic_0~87ccdbce`, env; `decalParams1 = (1,1,0,0)`.
  The design textures are 1024x1024 BC1 with streamed mips (`camo_h1_christmas_wrap` →
  `wpn_h1_camo_christmas_wrap_01_col`, `camo_h1_blue_jade` → `wpn_h1_camo_jade_col`).
- **Reference/placeholder images.** `camo_reference_col` and `camo_transparent_col` are
  32x32 BC2 textures in `refs\dumps\hmw_patch_common_mp\images\`; `camo_nml` is a 1024x1024
  BC5 second normal (`...\images\camo_nml.h1Image`, mips under
  `GAME\dump\workbench_hmw_patch_common_mp\streamed_images\camo_nml_stream*.pixels`).
- **Per-part H2 camo variants also exist**, e.g.
  `mtl_h2_m4a1_receiver_fire.json` and `mtl_h2_m4a1_receiver_doomsday.json`
  (`GAME\dump\workbench_hmw_ar1\materials\m\`): same `_camo` layout but with the design baked
  into `dc1` (`wpn_h2m_camo_fire_col`, `wpn_h2m_camo_doomsday_ems`) and an extra
  `_misva_eflow` technique. These are the animated H2 native camos; they are not in the
  `camo_h1_*` list and are not exposed by M6.

## 3. Which camos exist in these dumps

`refs\dumps\hmw_patch_common_mp\materials\` contains **550** `camo_h1_*.json` materials:

- **355 weapon camos** (`camo_h1_<family>_<tier>`), all with a resolvable `dc1` image and pixel
  data in `GAME\dump\workbench_hmw_patch_common_mp\streamed_images`. Families with 8 tiers each
  are e.g. `aftermath`, `amoeba`, `burnt`, `cosmos`, `decay`, `diablo`, `etch`, `exlibris`,
  `flow`, `foliage`, `funhouse`, `geist`, `geometric`, `hightech`, `hunter`, `iris`, `minerva`,
  `operator`, `pixel`, `rebel`, `reptile`, `super`, `sweettooth`, `texture`, `tomahawk`; single
  camos include `44magnum`, `arctic_wolf`, `blue_jade`, `christmas_wrap`, `goldfish`, `marble`,
  `peacock`, `red_wash`, `snake_skin`, `stars_and_stripes`, `tan_wash`, `urbanwarfare_01`,
  `urbanoperator_01`, `venom_01`, `wolfen_01`, ...
- **195 character camos** (`camo_h1_char_*`), e.g. `camo_h1_char_sas_woodland_013`; these skin
  uniforms, not weapons, and the workbench excludes them from the weapon dropdown.
- The basic MWR camos from the H1 class list (`gold`, `woodland`, `desert`, `white`, `digital`,
  `diamond`, `darkmatter`, ...) are **not** present as `camo_h1_*` materials in these dumps
  (only in the compiled H1 class script), so M6 lists the 355 materials that exist here.

## 4. Workbench approximation (M6)

The workbench does not run the game's technique (the `.techset` pixel shaders are not available
as source). It renders the same inputs the material defines:

1. Base colour = the material's `c` slot (`i_*_c`, `crossbow_col`, ...), picked by the existing
   material loader (`src\assets\model_asset.cpp`, `ScoreTexture`).
2. Mask = the material's `m` slot (typeHash `3135443286`), loaded once per material
   (`MaterialSlot::camoMaskImage`, sampled at the base UVs). White = camo, black = base colour.
   The crossbow mask (`crossbow_mask_02`) is 1024x1024 and leaves rails/tips untouched.
3. Camo design = the `camo_h1_*` material's `dc1` texture (typeHash `659983135`), 1024x1024
   BC1, sampled with the model UVs at scale 1 (the `decalParams1` scale of the camo material is
   1,1; the `_camo` material uses 2,2, which is a decal parameter, not the camo UV transform).
4. Shader (`src\render\renderer.cpp`, `PSMain`):
   `albedo = lerp(albedo, camoColor, saturate(mask * strength))`, then the existing lighting.
   Strength 1 by default; the UI has a strength slider. With no camo selected
   `gCamoParams.x == 0` and the branch is skipped, so the output is byte-identical to M5 (the
   self-check compares hashes).
5. Only the gun (and mounted attachment view models), not the hands, receives the camo - the
   game overrides the view-model material, not the viewhands.

This is an approximation: it keeps the base normal/specular and only replaces the diffuse
colour, while the real technique also swaps the camo normal (`dna1`), specular and env response.
The design textures are sampled once across the model UV atlas, which is how the game's `dc1`
slot works; there is no per-part UV scale in the material data.

## 5. Using it

- **UI:** Inspector → *Camo*: filter box + dropdown of all 355 weapon camo materials, strength
  and UV-scale sliders, Clear. Works on a loaded weapon and on a standalone model.
- **CLI:** `-camo camo_h1_christmas_wrap` (or the bare name `christmas_wrap`) with
  `-model`/`-weapon`; optional `-camo-strength 0..1`, `-camo-scale n`.
- **MCP:** `load_weapon {"weapon":"h2_m4_mp","camo":"camo_h1_blue_jade"}` and
  `load_model {"model":"vm_crossbow_base","camo":"camo_h1_red_wash"}`; the result contains
  `camo:{name,texture,strength}` (or `camo:{name,error}`).

## 6. Camos to compare in game

The three reviewed camos are visually distinct and use different masks/designs:

- `camo_h1_christmas_wrap` - bright green wrap with white snowflakes (best contrast on the
  dark crossbow rails and the M4 stock).
- `camo_h1_blue_jade` - teal zebra stripes (good on the M4 stock/handguard).
- `camo_h1_red_wash` - dark red wash with grunge (subtle; closest to a "worn" finish).

Compare them on the H2 M4 (`h2_m4_mp`) and the crossbow (`iw5_exocrossbow_mp`) in game; the
workbench images are in `review\M6\`.
