#pragma once

#include <map>
#include <string>
#include <vector>

#include <DirectXMath.h>

#include "assets/xanim.h"
#include "assets/xmodel.h"
#include "scene/viewmodel.h"

namespace wb::scene {

struct PoseBone {
	std::string name;
	int parent = -1;
	DirectX::XMFLOAT4 localQuat{0.0f, 0.0f, 0.0f, 1.0f};
	DirectX::XMFLOAT3 localPos{0.0f, 0.0f, 0.0f};
	DirectX::XMFLOAT4 worldQuat{0.0f, 0.0f, 0.0f, 1.0f};
	DirectX::XMFLOAT3 worldPos{0.0f, 0.0f, 0.0f};
	bool fromHands = false;
};

struct SkeletonPose {
	std::vector<PoseBone> bones;
	std::vector<int> handsBoneToPose;
	std::vector<int> gunBoneToPose;
	std::vector<int> modelBoneToPose;
	bool animated = false;
	const assets::XAnimData* baseAnim = nullptr;
	const assets::XAnimData* additiveAnim = nullptr;
};

// Builds the combined hands + gun skeleton. The gun root (`j_gun`/`tag_weapon*`) is
// parented to the hands `tag_weapon`; every animation channel is matched by bone name.
// Bones without channels keep their parent-relative base pose.
SkeletonPose BuildViewmodelPose(const ViewmodelAsset& viewmodel, const assets::XAnimData* baseAnim,
                                float time, const assets::XAnimData* additiveAnim, float additiveTime,
                                const std::map<std::string, std::string>* rename = nullptr);

// Single-model pose used by `-model` previews and the plain model inspector.
SkeletonPose BuildModelPose(const assets::XModelData& model, const assets::XAnimData* baseAnim,
                            float time, const assets::XAnimData* additiveAnim, float additiveTime,
                            const std::map<std::string, std::string>* rename = nullptr);

// Anim-channel -> model-bone matching used by `compare_bones`: counts channels that have a
// model bone, missing channels and channels rescued by the rename table (old anim bone -> new
// model bone).
struct BoneCompareResult {
	int animChannels = 0;
	int modelBones = 0;
	int matched = 0;
	int missing = 0;
	int renamed = 0;
	std::vector<std::string> missingChannels;
	std::vector<std::string> renamedChannels;
};

BoneCompareResult CompareAnimToModel(const assets::XAnimData& anim, const assets::XModelData& model,
                                     const std::map<std::string, std::string>& rename);

// Palette for GPU skinning: inverse(bind pose) * animated pose per model bone, in the
// model's own bone order. When no animation is active this is the identity (or, for the
// gun, the static hands attachment).
std::vector<DirectX::XMFLOAT4X4> BuildBonePalette(const assets::XModelData& model,
                                                  const SkeletonPose& pose,
                                                  const std::vector<int>& boneMap);

// Maps a second model's bones onto an existing pose by bone name (used for character heads,
// which are separate xmodels sharing the player skeleton). -1 for bones not in the pose.
std::vector<int> MapBonesByName(const assets::XModelData& model, const SkeletonPose& pose);

// Recomputes worldQuat/worldPos after a caller overrides a local bone transform
// (used for the ADS tag_ads overlay).
void RecomputeWorld(SkeletonPose& pose);

} // namespace wb::scene
