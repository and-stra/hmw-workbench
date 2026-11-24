#include "scene/viewmodel.h"

#include "assets/model_asset.h"
#include "assets/weapon.h"

#include <algorithm>
#include <cctype>

namespace wb::scene {
namespace {

DirectX::XMMATRIX BoneMatrix(const assets::BoneData& bone) {
	DirectX::XMVECTOR quaternion = DirectX::XMLoadFloat4(&bone.worldQuat);
	if (DirectX::XMVectorGetX(DirectX::XMVector4LengthSq(quaternion)) < 1.0e-8f) {
		quaternion = DirectX::XMQuaternionIdentity();
	}
	quaternion = DirectX::XMQuaternionNormalize(quaternion);
	return DirectX::XMMatrixRotationQuaternion(quaternion) *
	       DirectX::XMMatrixTranslation(bone.worldPos.x, bone.worldPos.y, bone.worldPos.z);
}

int FindBone(const assets::XModelData& model, const char* name) {
	for (std::size_t i = 0; i < model.bones.size(); ++i) {
		if (model.bones[i].name == name) {
			return static_cast<int>(i);
		}
	}
	return -1;
}

std::string ToLower(std::string text) {
	std::transform(text.begin(), text.end(), text.begin(),
	               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return text;
}

bool IsHandsName(const std::string& name) {
	const std::string lower = ToLower(name);
	return lower.rfind("viewhands", 0) == 0 || lower.rfind("globalviewhands", 0) == 0;
}

void ExtendBounds(const DirectX::XMFLOAT3& center, const DirectX::XMFLOAT3& half,
                  const DirectX::XMMATRIX& transform, DirectX::XMFLOAT3& minBounds,
                  DirectX::XMFLOAT3& maxBounds, bool& initialized) {
	using DirectX::XMFLOAT3;
	const XMFLOAT3 corners[8] = {
	    {center.x - half.x, center.y - half.y, center.z - half.z},
	    {center.x + half.x, center.y - half.y, center.z - half.z},
	    {center.x - half.x, center.y + half.y, center.z - half.z},
	    {center.x + half.x, center.y + half.y, center.z - half.z},
	    {center.x - half.x, center.y - half.y, center.z + half.z},
	    {center.x + half.x, center.y - half.y, center.z + half.z},
	    {center.x - half.x, center.y + half.y, center.z + half.z},
	    {center.x + half.x, center.y + half.y, center.z + half.z},
	};
	for (const XMFLOAT3& corner : corners) {
		const DirectX::XMVECTOR point = DirectX::XMVector3Transform(
		    DirectX::XMLoadFloat3(&corner), transform);
		DirectX::XMFLOAT3 value;
		DirectX::XMStoreFloat3(&value, point);
		if (!initialized) {
			minBounds = maxBounds = value;
			initialized = true;
		} else {
			minBounds.x = std::min(minBounds.x, value.x);
			minBounds.y = std::min(minBounds.y, value.y);
			minBounds.z = std::min(minBounds.z, value.z);
			maxBounds.x = std::max(maxBounds.x, value.x);
			maxBounds.y = std::max(maxBounds.y, value.y);
			maxBounds.z = std::max(maxBounds.z, value.z);
		}
	}
}

} // namespace

std::vector<std::string> ListHandsModels(const std::vector<std::filesystem::path>& roots) {
	std::vector<std::string> names;
	for (const auto& root : roots) {
		const std::filesystem::path folder = root / "xmodel";
		std::error_code ec;
		for (std::filesystem::directory_iterator it(folder, ec), end; !ec && it != end; it.increment(ec)) {
			std::error_code fileEc;
			if (!it->is_regular_file(fileEc) || it->path().extension() != ".xmb") {
				continue;
			}
			const std::string name = it->path().stem().string();
			if (IsHandsName(name)) {
				names.push_back(name);
			}
		}
	}
	std::sort(names.begin(), names.end());
	names.erase(std::unique(names.begin(), names.end()), names.end());
	return names;
}

std::string DefaultFactionHands(const std::vector<std::filesystem::path>& roots) {
	const std::vector<std::string> preferred = {
	    "viewhands_tf141",
	    "viewhands_h1_arab_desert_mp_camo",
	    "viewhands_h1_usmc_desert_mp_camo",
	    "globalViewhands_mw2_airborne",
	    "viewhands_rangers",
	    "viewhands_udt",
	};
	for (const std::string& name : preferred) {
		if (assets::FindModelFile(roots, name).has_value()) {
			return name;
		}
	}
	const std::vector<std::string> available = ListHandsModels(roots);
	return available.empty() ? std::string() : available.front();
}

bool LoadViewmodel(const std::string& weaponName, const std::string& handsOverride,
                   const std::vector<std::filesystem::path>& roots, ViewmodelAsset& out,
                   std::string& error, bool world) {
	out = ViewmodelAsset{};
	out.isWorldModel = world;
	const std::optional<std::filesystem::path> weaponPath = assets::FindWeaponFile(roots, weaponName);
	if (!weaponPath.has_value()) {
		error = "weapon json not found: weapons/" + weaponName + ".json";
		return false;
	}

	try {
		out.weapon = assets::LoadWeapon(*weaponPath);
	} catch (const std::exception& e) {
		error = e.what();
		return false;
	}

	const std::string modelName =
	    world ? (out.weapon.worldModel.empty() ? out.weapon.gunModel : out.weapon.worldModel)
	          : out.weapon.gunModel;
	if (world && out.weapon.worldModel.empty()) {
		out.warnings.push_back("weapon has no worldModel; using gunModel " + out.weapon.gunModel);
	}
	try {
		if (!assets::LoadModelAsset(modelName, roots, out.gun, error, true)) {
			error = (world ? "world model " : "gun model ") + modelName + ": " + error;
			return false;
		}
	} catch (const std::exception& e) {
		error = std::string(world ? "world model: " : "gun model: ") + e.what();
		return false;
	}

	if (world) {
		// World models are authored in world space: no hands, no viewmodel join.
		out.handsName.clear();
		out.handsSource = "world";
		out.hasHands = false;
		DirectX::XMStoreFloat4x4(&out.gunTransform, DirectX::XMMatrixIdentity());
		out.boundsCenter = out.gun.boundsCenter;
		out.boundsHalf = out.gun.boundsHalf;
		return true;
	}

	const std::string placeholder = "viewmodel_base_viewhands";
	if (!handsOverride.empty()) {
		out.handsName = handsOverride;
		out.handsSource = "explicit";
	} else if (!out.weapon.handModel.empty() && out.weapon.handModel != placeholder) {
		out.handsName = out.weapon.handModel;
		out.handsSource = "weapon";
	} else {
		out.handsName = DefaultFactionHands(roots);
		out.handsSource = "faction";
	}

	if (!out.handsName.empty()) {
		std::string handsError;
		if (assets::LoadModelAsset(out.handsName, roots, out.hands, handsError, true)) {
			out.hasHands = true;
		} else {
			out.warnings.push_back("hands " + out.handsName + ": " + handsError);
		}
	} else {
		out.warnings.push_back("no hands model found in any asset root");
	}

	DirectX::XMMATRIX gunRoot = DirectX::XMMatrixIdentity();
	if (!out.gun.model.bones.empty()) {
		gunRoot = BoneMatrix(out.gun.model.bones.front());
	}
	DirectX::XMMATRIX attach = DirectX::XMMatrixIdentity();
	if (out.hasHands) {
		int handsTag = FindBone(out.hands.model, "tag_weapon");
		if (handsTag < 0) {
			handsTag = FindBone(out.hands.model, "tag_weapon_right");
		}
		if (handsTag < 0) {
			handsTag = FindBone(out.hands.model, "tag_weapon_left");
		}
		if (handsTag >= 0) {
			const DirectX::XMMATRIX handsTagMatrix =
			    BoneMatrix(out.hands.model.bones[static_cast<std::size_t>(handsTag)]);
			const DirectX::XMMATRIX inverseRoot = DirectX::XMMatrixInverse(nullptr, gunRoot);
			attach = inverseRoot * handsTagMatrix;
		} else {
			out.warnings.push_back("hands " + out.handsName + " has no tag_weapon bone");
		}

		int viewBone = FindBone(out.hands.model, "tag_view");
		if (viewBone < 0) {
			viewBone = FindBone(out.hands.model, "tag_camera");
		}
		if (viewBone < 0) {
			viewBone = FindBone(out.hands.model, "tag_cambone");
		}
		if (viewBone >= 0) {
			out.viewOrigin = out.hands.model.bones[static_cast<std::size_t>(viewBone)].worldPos;
			out.viewOriginValid = true;
		}
	}
	DirectX::XMStoreFloat4x4(&out.gunTransform, attach);

	DirectX::XMFLOAT3 minBounds{0.0f, 0.0f, 0.0f};
	DirectX::XMFLOAT3 maxBounds{0.0f, 0.0f, 0.0f};
	bool haveBounds = false;
	ExtendBounds(out.gun.boundsCenter, out.gun.boundsHalf, attach, minBounds, maxBounds, haveBounds);
	if (out.hasHands) {
		ExtendBounds(out.hands.boundsCenter, out.hands.boundsHalf, DirectX::XMMatrixIdentity(), minBounds,
		             maxBounds, haveBounds);
	}
	if (haveBounds) {
		out.boundsCenter = DirectX::XMFLOAT3((minBounds.x + maxBounds.x) * 0.5f,
		                                     (minBounds.y + maxBounds.y) * 0.5f,
		                                     (minBounds.z + maxBounds.z) * 0.5f);
		out.boundsHalf = DirectX::XMFLOAT3((maxBounds.x - minBounds.x) * 0.5f,
		                                   (maxBounds.y - minBounds.y) * 0.5f,
		                                   (maxBounds.z - minBounds.z) * 0.5f);
	}
	return true;
}

} // namespace wb::scene
