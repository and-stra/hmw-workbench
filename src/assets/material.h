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
};

struct MaterialData {
	std::string name;
	std::string techniqueSet;
	std::filesystem::path path;
	std::vector<MaterialTextureDef> textures;
};

MaterialData LoadMaterial(const std::filesystem::path& path);

const char* TextureSemanticName(std::uint8_t semantic);

} // namespace wb::assets
