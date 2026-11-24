#include "assets/xsurface.h"

#include "assets/asset_stream.h"
#include "assets/h1_structs.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace wb::assets {
namespace {

DirectX::XMFLOAT3 UnpackUnitVec(std::uint32_t packed) {
	constexpr float kScale = 2.0f / 1023.0f;
	return DirectX::XMFLOAT3(static_cast<float>((packed >> 0) & 0x3FF) * kScale - 1.0f,
	                         static_cast<float>((packed >> 10) & 0x3FF) * kScale - 1.0f,
	                         static_cast<float>((packed >> 20) & 0x3FF) * kScale - 1.0f);
}

DirectX::XMFLOAT2 UnpackTexCoords(std::uint32_t packed) {
	const auto halfToFloat = [](std::uint16_t half) {
		const std::uint32_t sign = (half >> 15) & 1u;
		const std::uint32_t exponent = (half >> 10) & 0x1Fu;
		const std::uint32_t mantissa = half & 0x3FFu;
		float value = 0.0f;
		if (exponent == 0) {
			value = static_cast<float>(mantissa) * (1.0f / 16777216.0f);
		} else if (exponent == 31) {
			value = mantissa == 0 ? 65504.0f : 65504.0f;
		} else {
			value = (1.0f + static_cast<float>(mantissa) / 1024.0f) *
			        std::pow(2.0f, static_cast<float>(exponent) - 15.0f);
		}
		return sign ? -value : value;
	};
	return DirectX::XMFLOAT2(halfToFloat(static_cast<std::uint16_t>(packed & 0xFFFF)),
	                         halfToFloat(static_cast<std::uint16_t>((packed >> 16) & 0xFFFF)));
}

SurfaceVertex DecodeVertex(const h1::GfxPackedVertex& vertex) {
	SurfaceVertex out;
	out.position = DirectX::XMFLOAT3(vertex.xyz[0], vertex.xyz[1], vertex.xyz[2]);
	out.normal = UnpackUnitVec(vertex.normal);
	out.tangent = UnpackUnitVec(vertex.tangent);
	out.binormalSign = vertex.binormalSign;
	out.uv = UnpackTexCoords(vertex.texCoord);
	return out;
}

} // namespace

XSurfaceSet LoadXSurface(const std::filesystem::path& path) {
	AssetStream stream = AssetStream::FromFile(path);

	const std::vector<h1::XModelSurfsRaw> heads = stream.ReadSingle<h1::XModelSurfsRaw>();
	if (heads.empty()) {
		throw AssetError("missing XModelSurfs struct");
	}
	const h1::XModelSurfsRaw& head = heads.front();

	XSurfaceSet set;
	set.path = path;
	set.name = stream.ReadString();

	const std::vector<h1::XSurfaceRaw> rawSurfaces = stream.ReadArray<h1::XSurfaceRaw>();
	if (rawSurfaces.size() != head.numsurfs) {
		throw AssetError("XSurface count " + std::to_string(rawSurfaces.size()) + " does not match numsurfs " +
		                 std::to_string(head.numsurfs));
	}
	if (rawSurfaces.empty()) {
		throw AssetError("surface set has no surfaces");
	}

	set.surfaces.reserve(rawSurfaces.size());
	for (const h1::XSurfaceRaw& raw : rawSurfaces) {
		SurfaceData surface;
		surface.flags = raw.flags;
		surface.rigidVertListCount = raw.rigidVertListCount;
		std::copy(std::begin(raw.blendVertCounts), std::end(raw.blendVertCounts),
		          surface.blendVertCounts.begin());

		const std::vector<h1::GfxPackedVertex> packedVertices = stream.ReadArray<h1::GfxPackedVertex>();
		const std::vector<h1::Face> faces = stream.ReadArray<h1::Face>();
		(void)stream.ReadArray<h1::Face>(); // triIndices2

		const std::vector<h1::XRigidVertList> rigidLists = stream.ReadArray<h1::XRigidVertList>();
		for (const h1::XRigidVertList& rigid : rigidLists) {
			if (!rigid.collisionTree) {
				continue;
			}
			const std::vector<h1::XSurfaceCollisionTree> trees = stream.ReadSingle<h1::XSurfaceCollisionTree>();
			if (trees.empty()) {
				continue;
			}
			if (trees.front().leafs) {
				(void)stream.ReadArray<std::uint16_t>();
			}
			if (trees.front().nodes) {
				(void)stream.ReadArray<h1::XSurfaceCollisionNode>();
			}
		}

		stream.SkipRaw<std::uint8_t>(); // unknown0
		surface.blendWeightsRaw = stream.ReadRawBytes();
		surface.blendTableRaw = stream.ReadRawBytes();
		stream.SkipRaw<std::uint8_t>(); // lmapUnwrap
		stream.SkipRaw<std::uint8_t>(); // tensionData
		stream.SkipRaw<std::uint8_t>(); // tensionAccumTable

		if (raw.subdiv) {
			const std::vector<h1::XSurfaceSubdivInfoRaw> subdiv =
			    stream.ReadSingle<h1::XSurfaceSubdivInfoRaw>();
			if (!subdiv.empty() && subdiv.front().levels) {
				const std::vector<h1::XSurfaceSubdivLevelRaw> levels =
				    stream.ReadArray<h1::XSurfaceSubdivLevelRaw>();
				for (std::size_t level = 0; level < levels.size(); ++level) {
					for (int i = 0; i < 10; ++i) {
						stream.SkipRaw<std::uint8_t>();
					}
				}
			}
		}

		const std::vector<h1::BlendShape> blendShapes = stream.ReadArray<h1::BlendShape>();
		for (std::size_t i = 0; i < blendShapes.size(); ++i) {
			(void)stream.ReadArray<h1::BlendShapeVert>();
		}

		surface.vertexCount = static_cast<std::uint32_t>(packedVertices.size());
		surface.vertices.reserve(packedVertices.size());
		for (const h1::GfxPackedVertex& packed : packedVertices) {
			surface.vertices.push_back(DecodeVertex(packed));
		}

		// Blend weights: rigid vertices come first (one XRigidVertList entry per rigid
		// bone range), then the skinned vertices grouped by weight count. The weight
		// buffer stores bone indices pre-multiplied by 64.
		surface.vertexWeights.assign(packedVertices.size(), VertexWeights{});
		std::size_t rigidVertices = 0;
		for (const h1::XRigidVertList& rigid : rigidLists) {
			const std::uint16_t bone = static_cast<std::uint16_t>(rigid.boneOffset / 64);
			for (std::uint16_t i = 0; i < rigid.vertCount; ++i) {
				if (rigidVertices < surface.vertexWeights.size()) {
					surface.vertexWeights[rigidVertices].boneIndex[0] = bone;
					surface.vertexWeights[rigidVertices].weight[0] = 1.0f;
				}
				++rigidVertices;
			}
		}

		if (!surface.blendTableRaw.empty() && !surface.blendWeightsRaw.empty()) {
			const std::size_t tableCount = surface.blendTableRaw.size() / 32;
			std::vector<std::uint16_t> weightWords(surface.blendWeightsRaw.size() / sizeof(std::uint16_t));
			std::memcpy(weightWords.data(), surface.blendWeightsRaw.data(), surface.blendWeightsRaw.size());
			const auto tableClass = [&surface](std::size_t vertex) {
				return static_cast<std::uint16_t>(surface.blendTableRaw[vertex * 32 + 30] |
				                                  (surface.blendTableRaw[vertex * 32 + 31] << 8));
			};
			std::size_t cursor = 0;
			for (std::uint16_t weightCount = 1; weightCount <= 8; ++weightCount) {
				for (std::size_t vertex = rigidVertices; vertex < tableCount; ++vertex) {
					if (tableClass(vertex) != weightCount) {
						continue;
					}
					if (vertex >= surface.vertexWeights.size()) {
						continue;
					}
					VertexWeights& target = surface.vertexWeights[vertex];
					const std::size_t used = std::min<std::size_t>(weightCount, 4);
					float remaining = 1.0f;
					for (std::size_t w = 0; w < used; ++w) {
						if (cursor >= weightWords.size()) {
							break;
						}
						if (w == 0) {
							target.boneIndex[0] = static_cast<std::uint16_t>(weightWords[cursor] / 64);
							++cursor;
							if (used == 1) {
								target.weight[0] = 1.0f;
							}
						} else {
							if (cursor + 1 >= weightWords.size()) {
								break;
							}
							target.boneIndex[w] = static_cast<std::uint16_t>(weightWords[cursor] / 64);
							target.weight[w] = static_cast<float>(weightWords[cursor + 1]) / 65536.0f;
							cursor += 2;
							remaining -= target.weight[w];
						}
					}
					target.weight[0] = std::max(0.0f, remaining);
					// Consume the remaining ids/weights of animations with more than four influences.
					for (std::size_t w = 4; w < weightCount; ++w) {
						cursor += 2;
					}
				}
			}
		}

		for (std::size_t i = 0; i < surface.vertices.size() && i < surface.vertexWeights.size(); ++i) {
			for (int w = 0; w < 4; ++w) {
				surface.vertices[i].boneIndices[w] = surface.vertexWeights[i].boneIndex[w];
				surface.vertices[i].boneWeights[w] = surface.vertexWeights[i].weight[w];
			}
		}

		surface.indices.reserve(faces.size() * 3);
		for (const h1::Face& face : faces) {
			if (face.v1 >= surface.vertexCount || face.v2 >= surface.vertexCount ||
			    face.v3 >= surface.vertexCount) {
				continue;
			}
			surface.indices.push_back(face.v1);
			surface.indices.push_back(face.v2);
			surface.indices.push_back(face.v3);
		}
		surface.triangleCount = static_cast<std::uint32_t>(surface.indices.size() / 3);

		set.surfaces.push_back(std::move(surface));
	}

	if (!stream.AtEnd()) {
		throw AssetError("trailing bytes in .xsb (" + std::to_string(stream.Size() - stream.Position()) + ")");
	}

	return set;
}

} // namespace wb::assets
