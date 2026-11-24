#include "assets/model_asset.h"

#include "assets/asset_stream.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <unordered_map>

#include <DirectXMath.h>

namespace wb::assets {
namespace {

std::string ToLower(std::string text) {
	std::transform(text.begin(), text.end(), text.begin(),
	               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return text;
}

std::optional<std::filesystem::path> FindRelativeFile(const std::vector<std::filesystem::path>& roots,
                                                      const std::filesystem::path& relative) {
	for (const auto& root : roots) {
		const std::filesystem::path candidate = root / relative;
		std::error_code ec;
		if (std::filesystem::is_regular_file(candidate, ec) && !ec) {
			return candidate;
		}
	}
	return std::nullopt;
}

int ScoreTexture(const std::string& image, std::uint8_t semantic) {
	const std::string lower = ToLower(image);
	int score = 0;
	const auto has = [&lower](const char* needle) { return lower.find(needle) != std::string::npos; };

	if (semantic == 2) {
		if (has("col")) {
			score += 6;
		}
		if (has("mask") || has("detail") || has("camo") || has("spc") || has("nml") || has("occ") ||
		    has("env")) {
			score -= 5;
		}
	} else if (semantic == 5) {
		if (has("nml") || has("normal")) {
			score += 6;
		}
		if (has("spc") || has("occ")) {
			score -= 4;
		}
	} else if (semantic == 8) {
		if (has("spc") || has("spec")) {
			score += 6;
		}
		if (has("occ") || has("env")) {
			score -= 4;
		}
	}
	return score;
}

int LoadImageCached(ModelAsset& model, std::map<std::string, int>& cache,
                    const std::filesystem::path& path) {
	const std::string key = ToLower(path.string());
	const auto cached = cache.find(key);
	if (cached != cache.end()) {
		return cached->second;
	}
	int index = -1;
	try {
		ImageData image = LoadImage(path);
		if (!image.available) {
			model.warnings.push_back("texture " + image.name + ": " + image.note);
		}
		index = static_cast<int>(model.images.size());
		model.images.push_back(std::move(image));
	} catch (const std::exception& e) {
		model.warnings.push_back(std::string("texture ") + path.filename().string() + ": " + e.what());
	}
	cache[key] = index;
	return index;
}

} // namespace

std::optional<std::filesystem::path> FindModelFile(const std::vector<std::filesystem::path>& roots,
                                                   const std::string& name) {
	return FindRelativeFile(roots, std::filesystem::path("xmodel") / (name + ".xmb"));
}

std::optional<std::filesystem::path> FindXSurfaceFile(const std::vector<std::filesystem::path>& roots,
                                                      const std::string& name) {
	return FindRelativeFile(roots, std::filesystem::path("xsurface") / (name + ".xsb"));
}

std::optional<std::filesystem::path> FindMaterialFile(const std::vector<std::filesystem::path>& roots,
                                                      const std::string& name) {
	std::optional<std::filesystem::path> path =
	    FindRelativeFile(roots, std::filesystem::path("materials") / (name + ".json"));
	if (path.has_value()) {
		return path;
	}
	// "ms/" sub-materials share the base material's json in these dumps.
	if (name.rfind("ms/", 0) == 0) {
		return FindRelativeFile(roots,
		                        std::filesystem::path("materials") / ("m/" + name.substr(3) + ".json"));
	}
	return std::nullopt;
}

bool LoadModelAsset(const std::string& name, const std::vector<std::filesystem::path>& roots,
                    ModelAsset& out, std::string& error, bool loadImages) {
	const std::optional<std::filesystem::path> modelPath = FindModelFile(roots, name);
	if (!modelPath.has_value()) {
		error = "model not found in any asset root (xmodel/" + name + ".xmb)";
		return false;
	}
	return LoadModelAssetFromFile(*modelPath, roots, out, error, loadImages);
}

bool LoadModelAssetFromFile(const std::filesystem::path& modelPath,
                            const std::vector<std::filesystem::path>& roots, ModelAsset& out,
                            std::string& error, bool loadImages) {
	out = ModelAsset{};
	out.name = modelPath.stem().string();

	try {
		out.model = LoadXModel(modelPath);
	} catch (const std::exception& e) {
		error = std::string("xmodel: ") + e.what();
		return false;
	}

	const std::string& surfaceName = out.model.lodSurfaces[0];
	if (surfaceName.empty()) {
		error = "xmodel has no LOD0 surface set";
		return false;
	}
	const std::optional<std::filesystem::path> surfacePath = FindXSurfaceFile(roots, surfaceName);
	if (!surfacePath.has_value()) {
		error = "surface set " + surfaceName + ".xsb not found in any asset root";
		return false;
	}

	try {
		out.surfaceSet = LoadXSurface(*surfacePath);
	} catch (const std::exception& e) {
		error = "xsurface " + surfaceName + ": " + e.what();
		return false;
	}

	// Bounds fallback if the xmodel does not carry useful bounds.
	DirectX::XMFLOAT3 minBounds(1.0e30f, 1.0e30f, 1.0e30f);
	DirectX::XMFLOAT3 maxBounds(-1.0e30f, -1.0e30f, -1.0e30f);
	bool haveVertices = false;
	for (const SurfaceData& surface : out.surfaceSet.surfaces) {
		for (const SurfaceVertex& vertex : surface.vertices) {
			minBounds.x = std::min(minBounds.x, vertex.position.x);
			minBounds.y = std::min(minBounds.y, vertex.position.y);
			minBounds.z = std::min(minBounds.z, vertex.position.z);
			maxBounds.x = std::max(maxBounds.x, vertex.position.x);
			maxBounds.y = std::max(maxBounds.y, vertex.position.y);
			maxBounds.z = std::max(maxBounds.z, vertex.position.z);
			haveVertices = true;
		}
	}
	const bool modelBoundsValid = out.model.boundsHalf.x > 1.0e-4f || out.model.boundsHalf.y > 1.0e-4f ||
	                              out.model.boundsHalf.z > 1.0e-4f;
	if (modelBoundsValid) {
		out.boundsCenter = out.model.boundsMid;
		out.boundsHalf = out.model.boundsHalf;
	} else if (haveVertices) {
		out.boundsCenter = DirectX::XMFLOAT3((minBounds.x + maxBounds.x) * 0.5f, (minBounds.y + maxBounds.y) * 0.5f,
		                                     (minBounds.z + maxBounds.z) * 0.5f);
		out.boundsHalf = DirectX::XMFLOAT3((maxBounds.x - minBounds.x) * 0.5f, (maxBounds.y - minBounds.y) * 0.5f,
		                                   (maxBounds.z - minBounds.z) * 0.5f);
	}

	// Material slots in surface order.
	std::unordered_map<std::string, int> materialIndexByName;
	for (std::size_t i = 0; i < out.surfaceSet.surfaces.size(); ++i) {
		SurfaceData& surface = out.surfaceSet.surfaces[i];
		if (i < out.model.materialNames.size()) {
			surface.materialName = out.model.materialNames[i];
		}
		if (surface.materialName.empty()) {
			continue;
		}
		auto found = materialIndexByName.find(surface.materialName);
		if (found == materialIndexByName.end()) {
			const int index = static_cast<int>(out.materials.size());
			materialIndexByName[surface.materialName] = index;
			out.materials.push_back(MaterialSlot{});
			MaterialSlot& slot = out.materials.back();
			slot.material.name = surface.materialName;

			const std::optional<std::filesystem::path> materialPath = FindMaterialFile(roots, surface.materialName);
			if (materialPath.has_value()) {
				try {
					slot.material = LoadMaterial(*materialPath);
					slot.jsonFound = true;
				} catch (const std::exception& e) {
					slot.error = e.what();
					out.warnings.push_back("material " + surface.materialName + ": " + e.what());
				}
			} else {
				slot.error = "material json not found";
				out.warnings.push_back("material " + surface.materialName + ": json not found");
			}
			found = materialIndexByName.find(surface.materialName);
		}
		surface.materialIndex = found->second;
	}

	// Pick color/normal/specular textures per material.
	std::map<std::string, int> imageCache;
	for (MaterialSlot& slot : out.materials) {
		slot.textures.reserve(slot.material.textures.size());
		for (const MaterialTextureDef& def : slot.material.textures) {
			MaterialTextureInfo info;
			info.imageName = def.image;
			info.semantic = def.semantic;
			const std::optional<std::filesystem::path> imagePath = FindImageFile(roots, def.image);
			info.fileFound = imagePath.has_value();
			slot.textures.push_back(std::move(info));
		}

		if (!loadImages) {
			continue;
		}

		struct Candidate {
			std::size_t index;
			int score;
		};
		for (const std::uint8_t role : {std::uint8_t{2}, std::uint8_t{5}, std::uint8_t{8}}) {
			std::vector<Candidate> candidates;
			for (std::size_t i = 0; i < slot.textures.size(); ++i) {
				if (slot.textures[i].semantic == role && slot.textures[i].fileFound) {
					candidates.push_back({i, ScoreTexture(slot.textures[i].imageName, role)});
				}
			}
			std::sort(candidates.begin(), candidates.end(),
			          [](const Candidate& a, const Candidate& b) { return a.score > b.score; });

			int chosen = -1;
			for (const Candidate& candidate : candidates) {
				MaterialTextureInfo& info = slot.textures[candidate.index];
				const std::optional<std::filesystem::path> imagePath = FindImageFile(roots, info.imageName);
				if (!imagePath.has_value()) {
					continue;
				}
				const int imageIndex = LoadImageCached(out, imageCache, *imagePath);
				if (imageIndex < 0) {
					continue;
				}
				info.imageIndex = imageIndex;
				info.dataAvailable = out.images[static_cast<std::size_t>(imageIndex)].available;
				if (chosen < 0 && info.dataAvailable) {
					chosen = imageIndex;
				}
			}
			if (role == 2) {
				slot.colorImage = chosen;
			} else if (role == 5) {
				slot.normalImage = chosen;
			} else {
				slot.specularImage = chosen;
			}
		}
	}

	return true;
}

} // namespace wb::assets
