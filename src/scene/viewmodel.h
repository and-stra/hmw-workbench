#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <DirectXMath.h>

#include "assets/model_asset.h"
#include "assets/weapon.h"

namespace wb::scene {

struct ViewmodelAsset {
	assets::WeaponAsset weapon;
	assets::ModelAsset gun;
	assets::ModelAsset hands;
	bool hasHands = false;
	std::string handsName;
	std::string handsSource; // "weapon", "faction" or "explicit"
	DirectX::XMFLOAT4X4 gunTransform{};
	DirectX::XMFLOAT3 boundsCenter{0.0f, 0.0f, 0.0f};
	DirectX::XMFLOAT3 boundsHalf{1.0f, 1.0f, 1.0f};
	DirectX::XMFLOAT3 viewOrigin{0.0f, 0.0f, 0.0f};
	bool viewOriginValid = false;
	std::vector<std::string> warnings;
};

// The weapon xmodel root bone (bone 0, `j_gun` in the AW port, `tag_weapon` in HMW models)
// is attached to the hands model's `tag_weapon` bone. Render the gun with gunTransform.
bool LoadViewmodel(const std::string& weaponName, const std::string& handsOverride,
                   const std::vector<std::filesystem::path>& roots, ViewmodelAsset& out,
                   std::string& error);

// Default faction hands for weapons whose handModel is the `viewmodel_base_viewhands`
// placeholder. Prefers viewhands_tf141 (the crossbow's hands), then any viewhands model.
std::string DefaultFactionHands(const std::vector<std::filesystem::path>& roots);

std::vector<std::string> ListHandsModels(const std::vector<std::filesystem::path>& roots);

} // namespace wb::scene
