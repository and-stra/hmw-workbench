#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace wb::assets {

struct MaterialTextureDef {
	std::string image;
	std::uint8_t semantic = 0;
	std::uint8_t samplerState = 0;
	std::uint32_t typeHash = 0;
};

// Texture slot hashes used by the H1/HMW camo materials (constant across the dumps).
// The "c" slot is the untextured base colour, "m" the camo mask and dc1 the camo design.
constexpr std::uint32_t kTextureTypeColor1 = 659983135u;   // dc1 (camo design / placeholder)
constexpr std::uint32_t kTextureTypeColorC = 2695565377u;  // "c" base colour
constexpr std::uint32_t kTextureTypeColorM = 3135443286u;  // "m" camo mask

struct MaterialData {
	std::string name;
	std::string techniqueSet;
	std::filesystem::path path;
	std::vector<MaterialTextureDef> textures;
};

MaterialData LoadMaterial(const std::filesystem::path& path);

const char* TextureSemanticName(std::uint8_t semantic);

} // namespace wb::assets
