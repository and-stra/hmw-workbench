#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "assets/image.h"
#include "assets/material.h"
#include "assets/xmodel.h"
#include "assets/xsurface.h"

namespace wb::assets {

struct MaterialTextureInfo {
	std::string imageName;
	std::uint8_t semantic = 0;
	std::uint32_t typeHash = 0;
	bool fileFound = false;
	int imageIndex = -1; // index into ModelAsset::images (only for loaded textures)
	bool dataAvailable = false;
};

struct MaterialSlot {
	MaterialData material;
	std::vector<MaterialTextureInfo> textures;
	int colorImage = -1;
	int normalImage = -1;
	int specularImage = -1;
	int camoMaskImage = -1; // "m" slot of a camo-ready material, -1 when absent
	bool jsonFound = false;
	std::string error;
};

struct ModelAsset {
	std::string name;
	XModelData model;
	XSurfaceSet surfaceSet;
	std::vector<MaterialSlot> materials;
	std::vector<ImageData> images;
	std::vector<std::string> warnings;
	DirectX::XMFLOAT3 boundsCenter{0.0f, 0.0f, 0.0f};
	DirectX::XMFLOAT3 boundsHalf{1.0f, 1.0f, 1.0f};
};

std::optional<std::filesystem::path> FindModelFile(const std::vector<std::filesystem::path>& roots,
                                                   const std::string& name);
std::optional<std::filesystem::path> FindXSurfaceFile(const std::vector<std::filesystem::path>& roots,
                                                      const std::string& name);
std::optional<std::filesystem::path> FindMaterialFile(const std::vector<std::filesystem::path>& roots,
                                                      const std::string& name);

// Loads .xmb + LOD0 .xsb + material json + the color/normal/specular images.
// Returns false and fills error on structural failures; missing textures and materials
// are reported in warnings and do not fail the load.
bool LoadModelAsset(const std::string& name, const std::vector<std::filesystem::path>& roots,
                    ModelAsset& out, std::string& error, bool loadImages = true);

// Same, but for a specific .xmb (used by -validate, which walks every file in every root).
bool LoadModelAssetFromFile(const std::filesystem::path& modelPath,
                            const std::vector<std::filesystem::path>& roots, ModelAsset& out,
                            std::string& error, bool loadImages = true);

} // namespace wb::assets
