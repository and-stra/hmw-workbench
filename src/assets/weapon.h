#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace wb::assets {

struct WeaponAsset {
	std::string name;
	std::string internalName;
	std::string gunModel;
	std::string worldModel;
	std::string handModel;
	std::string camoViewModelMaterialOverride;
	std::vector<std::string> attachmentOverrides;
};

WeaponAsset LoadWeapon(const std::filesystem::path& path);
std::optional<std::filesystem::path> FindWeaponFile(const std::vector<std::filesystem::path>& roots,
                                                    const std::string& name);

} // namespace wb::assets
