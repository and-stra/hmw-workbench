#include "assets/camo.h"

#include <algorithm>
#include <cctype>

#include "assets/model_asset.h"

namespace wb::assets {
namespace {

constexpr const char* kCamoPrefix = "camo_h1_";
constexpr const char* kCharacterCamoPrefix = "camo_h1_char_";
constexpr const char* kNativePrefix = "native:";

std::string ToLower(std::string text) {
	std::transform(text.begin(), text.end(), text.begin(),
	               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return text;
}

std::string Trim(const std::string& text) {
	std::string trimmed = text;
	while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.front()))) {
		trimmed.erase(trimmed.begin());
	}
	while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.back()))) {
		trimmed.pop_back();
	}
	return trimmed;
}

bool IsNativeName(const std::string& name) { return ToLower(name).rfind(kNativePrefix, 0) == 0; }

// Accept "camo_h1_x" as well as the bare camo name "x".
std::string MaterialNameFor(const std::string& name) {
	const std::string trimmed = Trim(name);
	if (trimmed.empty() || ToLower(trimmed) == "none") {
		return {};
	}
	const std::string lower = ToLower(trimmed);
	if (lower.rfind(kCamoPrefix, 0) == 0) {
		return trimmed;
	}
	return std::string(kCamoPrefix) + trimmed;
}

const MaterialTextureDef* FindTexture(const MaterialData& material, std::uint32_t typeHash) {
	for (const MaterialTextureDef& def : material.textures) {
		if (def.typeHash == typeHash) {
			return &def;
		}
	}
	return nullptr;
}

bool LoadTexture(const std::vector<std::filesystem::path>& roots, const char* role,
                 const MaterialTextureDef* def, ImageData& image, std::vector<std::string>& warnings) {
	if (def == nullptr) {
		warnings.push_back(std::string(role) + ": no texture slot in the material");
		return false;
	}
	const std::optional<std::filesystem::path> path = FindImageFile(roots, def->image);
	if (!path.has_value()) {
		warnings.push_back(std::string(role) + ": image " + def->image + " not in any asset root");
		return false;
	}
	try {
		image = LoadImage(*path);
	} catch (const std::exception& e) {
		warnings.push_back(std::string(role) + ": " + e.what());
		return false;
	}
	if (!image.available || image.pixels.empty()) {
		warnings.push_back(std::string(role) + ": " + (image.note.empty() ? "no pixel data" : image.note));
		return false;
	}
	return true;
}

bool IsNativeVariantStem(const std::string& stem) {
	const std::string lower = ToLower(stem);
	return lower.size() > 5 && (lower.ends_with("_fire") || lower.ends_with("_doomsday"));
}

} // namespace

std::vector<std::string> ListCamos(const std::vector<std::filesystem::path>& roots) {
	std::vector<std::string> names;
	for (const auto& root : roots) {
		const std::filesystem::path materials = root / "materials";
		std::error_code ec;
		for (std::filesystem::directory_iterator it(materials, ec), end; !ec && it != end;
		     it.increment(ec)) {
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

		// H2 native/animated material variants live in materials/m and are selected as
		// "native:<material stem>" (static frame in the workbench).
		const std::filesystem::path nativeDir = materials / "m";
		std::error_code nativeEc;
		for (std::filesystem::directory_iterator it(nativeDir, nativeEc), end; !nativeEc && it != end;
		     it.increment(nativeEc)) {
			std::error_code fileEc;
			if (!it->is_regular_file(fileEc) || fileEc || it->path().extension() != ".json") {
				continue;
			}
			const std::string stem = it->path().stem().string();
			if (!IsNativeVariantStem(stem)) {
				continue;
			}
			const std::string name = std::string(kNativePrefix) + stem;
			if (std::find(names.begin(), names.end(), name) == names.end()) {
				names.push_back(name);
			}
		}
	}
	std::sort(names.begin(), names.end());
	return names;
}

std::optional<std::filesystem::path> FindCamoMaterial(const std::vector<std::filesystem::path>& roots,
                                                      const std::string& name) {
	if (IsNativeName(name)) {
		return FindMaterialFile(roots, "m/" + Trim(name).substr(std::string(kNativePrefix).size()));
	}
	const std::string material = MaterialNameFor(name);
	if (material.empty()) {
		return std::nullopt;
	}
	return FindMaterialFile(roots, material);
}

bool LoadCamoTextures(const std::vector<std::filesystem::path>& roots, const std::string& name,
                      CamoTextures& out, std::vector<std::string>& warnings, std::string& error) {
	out = CamoTextures{};
	const bool native = IsNativeName(name);
	std::string materialName;
	if (native) {
		materialName = "m/" + Trim(name).substr(std::string(kNativePrefix).size());
		out.native = true;
	} else {
		materialName = MaterialNameFor(name);
	}
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
	out.materialName = material.name.empty() ? materialName : material.name;
	out.decalScaleU = material.decalScaleU > 0.0f ? material.decalScaleU : 1.0f;
	out.decalScaleV = material.decalScaleV > 0.0f ? material.decalScaleV : 1.0f;
	out.envMapScale = material.envMapScale;

	const MaterialTextureDef* color = FindTexture(material, kTextureTypeColor1);
	if (!LoadTexture(roots, "camo colour", color, out.color, warnings)) {
		error = "camo material " + materialName + " has no loadable colour texture";
		return false;
	}
	out.colorTextureName = out.color.name.empty() && color != nullptr ? color->image : out.color.name;

	// Design normal: the camo material's dn1 (pattern relief); fall back to its dna1
	// (camo_nml) and finally to the universal camo_nml image.
	const MaterialTextureDef* normal = FindTexture(material, kTextureTypeNormal1);
	if (normal == nullptr || normal->image == "$identitynormalmap" ||
	    normal->image == "camo_nml") {
		const MaterialTextureDef* second = FindTexture(material, kTextureTypeNormalA1);
		if (second != nullptr) {
			normal = second;
		}
	}
	out.hasNormal = LoadTexture(roots, "camo normal", normal, out.normal, warnings);
	if (!out.hasNormal) {
		const std::optional<std::filesystem::path> universal = FindImageFile(roots, "camo_nml");
		if (universal.has_value()) {
			try {
				ImageData image = LoadImage(*universal);
				if (image.available && !image.pixels.empty()) {
					out.normal = std::move(image);
					out.hasNormal = true;
					warnings.push_back("camo normal: using the universal camo_nml");
				}
			} catch (const std::exception& e) {
				warnings.push_back(std::string("camo normal fallback: ") + e.what());
			}
		}
	}

	const MaterialTextureDef* specular = FindTexture(material, kTextureTypeSpecularCamo);
	if (specular == nullptr) {
		specular = FindTexture(material, kTextureTypeSpecular1);
	}
	out.hasSpecular = LoadTexture(roots, "camo specular", specular, out.specular, warnings);

	const MaterialTextureDef* environment = FindTexture(material, kTextureTypeEnvironment);
	out.hasEnvironment =
	    LoadTexture(roots, "camo environment", environment, out.environment, warnings);

	const MaterialTextureDef* emissive = FindTexture(material, kTextureTypeEmissive);
	out.hasEmissive = LoadTexture(roots, "camo emissive", emissive, out.emissive, warnings);
	return true;
}

std::string CamoTextureName(const std::vector<std::filesystem::path>& roots, const std::string& name) {
	const std::optional<std::filesystem::path> materialPath = FindCamoMaterial(roots, name);
	if (!materialPath.has_value()) {
		return {};
	}
	try {
		const MaterialData material = LoadMaterial(*materialPath);
		const MaterialTextureDef* color = FindTexture(material, kTextureTypeColor1);
		return color != nullptr ? color->image : std::string();
	} catch (const std::exception&) {
		return {};
	}
}

bool SplitNativeCamoName(const std::string& name, std::string& materialStem, std::string& baseStem) {
	if (!IsNativeName(name)) {
		return false;
	}
	materialStem = Trim(name).substr(std::string(kNativePrefix).size());
	baseStem = materialStem;
	for (const char* suffix : {"_fire", "_doomsday"}) {
		const std::string s = suffix;
		if (baseStem.size() > s.size() && baseStem.ends_with(s)) {
			baseStem.resize(baseStem.size() - s.size());
			return true;
		}
	}
	return false;
}

} // namespace wb::assets
