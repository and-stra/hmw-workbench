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
// ListCamos() returns the sorted, de-duplicated weapon camo material names from all roots.
std::vector<std::string> ListCamos(const std::vector<std::filesystem::path>& roots);

std::optional<std::filesystem::path> FindCamoMaterial(const std::vector<std::filesystem::path>& roots,
                                                      const std::string& name);

// Loads the camo material and its dc1 colour texture (wpn_h1_camo_*.h1Image).
// Accepts the full material name ("camo_h1_goldfish") or the bare camo name ("goldfish").
bool LoadCamoImage(const std::vector<std::filesystem::path>& roots, const std::string& name,
                   ImageData& image, std::string& error);

// The camo design image name from the material (for -info / reports), empty when not found.
std::string CamoTextureName(const std::vector<std::filesystem::path>& roots, const std::string& name);

} // namespace wb::assets
