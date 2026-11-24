#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace wb::assets {

struct WeaponAnimState {
	std::string state;
	std::string anim;
};

struct WeaponAnimOverride {
	std::string anim;
	std::string altmodeAnim;
	int attachment1 = 0;
	int attachment2 = 0;
	int animTreeType = 0;
	int animHand = 0;
	float animTime = 0.0f;
	float altTime = 0.0f;
};

struct WeaponAsset {
	std::string name;
	std::string internalName;
	std::string gunModel;
	std::string worldModel;
	std::string handModel;
	std::string camoViewModelMaterialOverride;
	float adsZoomFov = 50.0f;
	std::vector<std::string> attachmentOverrides;
	std::vector<WeaponAnimState> animStates;
	std::vector<WeaponAnimOverride> animOverrides;
};

WeaponAsset LoadWeapon(const std::filesystem::path& path);
std::optional<std::filesystem::path> FindWeaponFile(const std::vector<std::filesystem::path>& roots,
                                                    const std::string& name);

} // namespace wb::assets
