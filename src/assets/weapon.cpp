#include "assets/weapon.h"

#include "assets/asset_stream.h"

#include <fstream>

#include <nlohmann/json.hpp>

namespace wb::assets {

WeaponAsset LoadWeapon(const std::filesystem::path& path) {
	std::ifstream in(path);
	if (!in) {
		throw AssetError("cannot open weapon " + path.string());
	}
	nlohmann::json json;
	try {
		in >> json;
	} catch (const std::exception& e) {
		throw AssetError("weapon " + path.string() + ": " + e.what());
	}

	WeaponAsset weapon;
	weapon.name = path.stem().string();
	weapon.internalName = json.value("szInternalName", weapon.name);

	const auto firstString = [&json](const char* key) -> std::string {
		if (!json.contains(key)) {
			return {};
		}
		const auto& value = json[key];
		if (value.is_array() && !value.empty() && value[0].is_string()) {
			return value[0].get<std::string>();
		}
		if (value.is_string()) {
			return value.get<std::string>();
		}
		return {};
	};

	weapon.gunModel = firstString("gunModel");
	weapon.worldModel = firstString("worldModel");
	weapon.handModel = json.value("handModel", "");
	weapon.camoViewModelMaterialOverride = json.value("camoViewModelMaterialOverride", "");

	if (json.contains("animOverrides") && json["animOverrides"].is_array()) {
		for (const auto& override : json["animOverrides"]) {
			if (override.is_object()) {
				weapon.attachmentOverrides.push_back(override.value("overrideAnim", ""));
			}
		}
	}

	if (weapon.gunModel.empty()) {
		throw AssetError("weapon " + path.string() + " has no gunModel");
	}
	return weapon;
}

std::optional<std::filesystem::path> FindWeaponFile(const std::vector<std::filesystem::path>& roots,
                                                    const std::string& name) {
	for (const auto& root : roots) {
		const std::filesystem::path candidate = root / "weapons" / (name + ".json");
		std::error_code ec;
		if (std::filesystem::is_regular_file(candidate, ec) && !ec) {
			return candidate;
		}
	}
	return std::nullopt;
}

} // namespace wb::assets
