#include "assets/camo.h"

#include <algorithm>
#include <cctype>

#include "assets/model_asset.h"

namespace wb::assets {
namespace {

constexpr const char* kCamoPrefix = "camo_h1_";
constexpr const char* kCharacterCamoPrefix = "camo_h1_char_";

std::string ToLower(std::string text) {
	std::transform(text.begin(), text.end(), text.begin(),
	               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return text;
}

// Accept "camo_h1_x" as well as the bare camo name "x".
std::string MaterialNameFor(const std::string& name) {
	std::string trimmed = name;
	while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.front()))) {
		trimmed.erase(trimmed.begin());
	}
	while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.back()))) {
		trimmed.pop_back();
	}
	if (trimmed.empty() || ToLower(trimmed) == "none") {
		return {};
	}
	const std::string lower = ToLower(trimmed);
	if (lower.rfind(kCamoPrefix, 0) == 0) {
		return trimmed;
	}
	return std::string(kCamoPrefix) + trimmed;
}

const MaterialTextureDef* FindCamoColorDef(const MaterialData& material) {
	const MaterialTextureDef* fallback = nullptr;
	for (const MaterialTextureDef& def : material.textures) {
		if (def.semantic != 2) {
			continue;
		}
		if (def.typeHash == kTextureTypeColor1) {
			return &def;
		}
		if (fallback == nullptr && ToLower(def.image).find("camo") != std::string::npos) {
			fallback = &def;
		}
	}
	return fallback;
}

} // namespace

std::vector<std::string> ListCamos(const std::vector<std::filesystem::path>& roots) {
	std::vector<std::string> names;
	for (const auto& root : roots) {
		const std::filesystem::path materials = root / "materials";
		std::error_code ec;
		for (std::filesystem::directory_iterator it(materials, ec), end; !ec && it != end; it.increment(ec)) {
			std::error_code fileEc;
			if (!it->is_regular_file(fileEc) || fileEc) {
				continue;
			}
			const std::string stem = it->path().stem().string();
			const std::string lower = ToLower(stem);
			if (lower.rfind(kCamoPrefix, 0) != 0 || lower.rfind(kCharacterCamoPrefix, 0) == 0) {
				continue;
			}
			if (it->path().extension() != ".json") {
				continue;
			}
			if (std::find(names.begin(), names.end(), stem) == names.end()) {
				names.push_back(stem);
			}
		}
	}
	std::sort(names.begin(), names.end());
	return names;
}

std::optional<std::filesystem::path> FindCamoMaterial(const std::vector<std::filesystem::path>& roots,
                                                      const std::string& name) {
	const std::string material = MaterialNameFor(name);
	if (material.empty()) {
		return std::nullopt;
	}
	return FindMaterialFile(roots, material);
}

bool LoadCamoImage(const std::vector<std::filesystem::path>& roots, const std::string& name, ImageData& image,
                   std::string& error) {
	const std::string materialName = MaterialNameFor(name);
	if (materialName.empty()) {
		error = "no camo name";
		return false;
	}
	const std::optional<std::filesystem::path> materialPath = FindMaterialFile(roots, materialName);
	if (!materialPath.has_value()) {
		error = "camo material " + materialName + " not found in any asset root";
		return false;
	}
	MaterialData material;
	try {
		material = LoadMaterial(*materialPath);
	} catch (const std::exception& e) {
		error = std::string("camo material: ") + e.what();
		return false;
	}
	const MaterialTextureDef* color = FindCamoColorDef(material);
	if (color == nullptr) {
		error = "camo material " + materialName + " has no colour texture";
		return false;
	}
	const std::optional<std::filesystem::path> imagePath = FindImageFile(roots, color->image);
	if (!imagePath.has_value()) {
		error = "camo texture " + color->image + " not found in any asset root";
		return false;
	}
	try {
		image = LoadImage(*imagePath);
	} catch (const std::exception& e) {
		error = std::string("camo texture: ") + e.what();
		return false;
	}
	if (!image.available || image.pixels.empty()) {
		error = "camo texture " + color->image + " has no pixel data (" + image.note + ")";
		return false;
	}
	return true;
}

std::string CamoTextureName(const std::vector<std::filesystem::path>& roots, const std::string& name) {
	const std::string materialName = MaterialNameFor(name);
	if (materialName.empty()) {
		return {};
	}
	const std::optional<std::filesystem::path> materialPath = FindMaterialFile(roots, materialName);
	if (!materialPath.has_value()) {
		return {};
	}
	try {
		const MaterialData material = LoadMaterial(*materialPath);
		const MaterialTextureDef* color = FindCamoColorDef(material);
		return color != nullptr ? color->image : std::string();
	} catch (const std::exception&) {
		return {};
	}
}

} // namespace wb::assets
