#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "assets/image.h"
#include "assets/material.h"

namespace wb::assets {

// HMW/H1 weapon camo materials live next to the other materials as
// "camo_h1_<name>.json" (the camoTable name without its "wc_" prefix). The character
// camos are "camo_h1_char_*" and are not weapon camos.
//
// H2 native/animated material variants (e.g. mtl_h2_m4a1_receiver_fire) are exposed as
// "native:<material stem>" entries. The workbench shows their static frame.
//
// ListCamos() returns the sorted, de-duplicated entries from all roots.

struct CamoTextures {
	std::string materialName;
	std::string colorTextureName;
	ImageData color;
	ImageData normal;
	ImageData specular;
	ImageData environment;
	ImageData emissive;
	bool hasNormal = false;
	bool hasSpecular = false;
	bool hasEnvironment = false;
	bool hasEmissive = false;
	bool native = false;
	float decalScaleU = 1.0f;
	float decalScaleV = 1.0f;
	float envMapScale = 0.04f;
};

std::vector<std::string> ListCamos(const std::vector<std::filesystem::path>& roots);

std::optional<std::filesystem::path> FindCamoMaterial(const std::vector<std::filesystem::path>& roots,
                                                      const std::string& name);

// Loads the camo material and its full technique inputs: dc1 colour, dn1/dna1 normal,
// camo specular, the environment LUT, and decalParams1/envMapParms. Missing optional
// maps are reported in warnings; the colour map is required.
// Accepts "camo_h1_goldfish", "goldfish" or "native:mtl_h2_m4a1_receiver_fire".
bool LoadCamoTextures(const std::vector<std::filesystem::path>& roots, const std::string& name,
                      CamoTextures& out, std::vector<std::string>& warnings, std::string& error);

// The camo design colour image name from the material (for -info / reports), empty when not found.
std::string CamoTextureName(const std::vector<std::filesystem::path>& roots, const std::string& name);

// "native:<stem>" -> material stem and the base stem (mtl_h2_m4a1_receiver).
bool SplitNativeCamoName(const std::string& name, std::string& materialStem, std::string& baseStem);

} // namespace wb::assets
