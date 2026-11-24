#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <DirectXMath.h>

namespace wb::assets {

struct SurfaceVertex {
	DirectX::XMFLOAT3 position{0.0f, 0.0f, 0.0f};
	DirectX::XMFLOAT3 normal{0.0f, 0.0f, 1.0f};
	DirectX::XMFLOAT3 tangent{1.0f, 0.0f, 0.0f};
	float binormalSign = 1.0f;
	DirectX::XMFLOAT2 uv{0.0f, 0.0f};
};

struct SurfaceData {
	std::uint16_t flags = 0;
	std::uint32_t vertexCount = 0;
	std::uint32_t triangleCount = 0;
	std::uint8_t rigidVertListCount = 0;
	std::array<std::int16_t, 8> blendVertCounts{};
	std::vector<std::uint8_t> blendWeightsRaw; // XBlendInfo weights (uint16 per entry)
	std::vector<std::uint8_t> blendTableRaw;   // 32 bytes per vertex
	std::vector<SurfaceVertex> vertices;
	std::vector<std::uint16_t> indices;
	std::string materialName;
	int materialIndex = -1;
};

struct XSurfaceSet {
	std::string name;
	std::filesystem::path path;
	std::vector<SurfaceData> surfaces;
};

// Reads an x64-zt H1 .xsb (XModelSurfs + XSurface array + vertex/index/index data).
// Only LOD geometry is decoded; collision, subdiv, blend-shape, tension and lightmap
// streams are parsed and skipped so validation sees the whole file.
XSurfaceSet LoadXSurface(const std::filesystem::path& path);

} // namespace wb::assets
