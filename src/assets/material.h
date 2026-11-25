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
constexpr std::uint32_t kTextureTypeSpecular1 = 887934131u; // ds1 (design specular)
constexpr std::uint32_t kTextureTypeNormal1 = 1507003663u;  // dn1 (design normal)
constexpr std::uint32_t kTextureTypeNormalA1 = 1518176625u; // dna1 (second/camo normal)
constexpr std::uint32_t kTextureTypeColorC = 2695565377u;  // "c" base colour
constexpr std::uint32_t kTextureTypeColorM = 3135443286u;  // "m" camo mask
constexpr std::uint32_t kTextureTypeSpecularCamo = 3426233914u; // camo specular
constexpr std::uint32_t kTextureTypeEnvironment = 3447584578u;  // env BRDF LUT
constexpr std::uint32_t kTextureTypeEmissive = 878789447u;      // H2 native camo emissive

struct MaterialData {
	std::string name;
	std::string techniqueSet;
	std::filesystem::path path;
	std::vector<MaterialTextureDef> textures;
	// decalParams1 and envMapParms from the constantTable. The camo-ready H1/H2
	// materials use decalParams1 = (2,2,0,0) and envMapParms = (0.04,1,5,1).
	float decalScaleU = 1.0f;
	float decalScaleV = 1.0f;
	float envMapScale = 0.04f;
};

MaterialData LoadMaterial(const std::filesystem::path& path);

const char* TextureSemanticName(std::uint8_t semantic);

} // namespace wb::assets
