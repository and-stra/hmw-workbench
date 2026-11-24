#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <DirectXMath.h>

namespace wb::assets {

struct BoneData {
	std::string name;
	int parent = -1;
	DirectX::XMFLOAT4 localQuat{0.0f, 0.0f, 0.0f, 1.0f};
	DirectX::XMFLOAT3 localPos{0.0f, 0.0f, 0.0f};
	DirectX::XMFLOAT4 worldQuat{0.0f, 0.0f, 0.0f, 1.0f};
	DirectX::XMFLOAT3 worldPos{0.0f, 0.0f, 0.0f};
	DirectX::XMFLOAT3 tagPosition{0.0f, 0.0f, 0.0f};
	bool hasTagPosition = false;

	bool IsTag() const { return name.rfind("tag_", 0) == 0; }
};

struct XModelData {
	std::string name;
	std::filesystem::path path;
	std::uint8_t numBones = 0;
	std::uint8_t numRootBones = 0;
	std::uint8_t numSurfs = 0;
	std::uint8_t numLods = 0;
	std::uint16_t flags = 0;
	float scale = 1.0f;
	DirectX::XMFLOAT3 boundsMid{0.0f, 0.0f, 0.0f};
	DirectX::XMFLOAT3 boundsHalf{0.0f, 0.0f, 0.0f};
	std::vector<BoneData> bones;
	std::vector<std::string> materialNames;
	std::array<std::string, 6> lodSurfaces;
	std::vector<std::uint8_t> partClassification;
};

// Reads an x64-zt H1 .xmb (tagged asset stream with the raw XModel struct followed by
// its arrays, strings and sub-asset names). World bone transforms are computed from the
// base pose (baseMat) for the static preview.
XModelData LoadXModel(const std::filesystem::path& path);

} // namespace wb::assets
