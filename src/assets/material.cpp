#include "assets/material.h"

#include "assets/asset_stream.h"

#include <fstream>

#include <nlohmann/json.hpp>

namespace wb::assets {

const char* TextureSemanticName(std::uint8_t semantic) {
	switch (semantic) {
	case 0:
		return "2d";
	case 2:
		return "color";
	case 3:
		return "detail";
	case 5:
		return "normal";
	case 8:
		return "specular";
	case 11:
		return "water";
	case 13:
		return "displacement";
	case 14:
		return "parallax";
	case 15:
		return "environment";
	case 16:
		return "occlusion";
	default:
		return "unknown";
	}
}

MaterialData LoadMaterial(const std::filesystem::path& path) {
	std::ifstream in(path);
	if (!in) {
		throw AssetError("cannot open material " + path.string());
	}

	nlohmann::json json;
	try {
		in >> json;
	} catch (const std::exception& e) {
		throw AssetError("material " + path.string() + ": " + e.what());
	}

	MaterialData material;
	material.path = path;
	material.name = json.value("name", path.stem().string());
	material.techniqueSet = json.value("techniqueSet->name", "");

	if (json.contains("textureTable") && json["textureTable"].is_array()) {
		for (const auto& texture : json["textureTable"]) {
			if (!texture.is_object()) {
				continue;
			}
			MaterialTextureDef def;
			def.image = texture.value("image", "");
			def.semantic = static_cast<std::uint8_t>(texture.value("semantic", 0));
			def.samplerState = static_cast<std::uint8_t>(texture.value("samplerState", 0));
			def.typeHash = texture.value("typeHash", 0u);
			if (!def.image.empty()) {
				material.textures.push_back(std::move(def));
			}
		}
	}

	if (json.contains("constantTable") && json["constantTable"].is_array()) {
		for (const auto& constant : json["constantTable"]) {
			if (!constant.is_object()) {
				continue;
			}
			const std::string name = constant.value("name", "");
			if (name != "decalParams1" && name != "envMapParms") {
				continue;
			}
			const auto& literal = constant["literal"];
			if (!literal.is_array() || literal.size() < 4) {
				continue;
			}
			if (name == "decalParams1") {
				material.decalScaleU = literal[0].get<float>();
				material.decalScaleV = literal[1].get<float>();
			} else {
				material.envMapScale = literal[0].get<float>();
			}
		}
	}

	return material;
}

} // namespace wb::assets
