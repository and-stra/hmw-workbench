#pragma once

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
                                float time, const assets::XAnimData* additiveAnim, float additiveTime);

// Single-model pose used by `-model` previews and the plain model inspector.
SkeletonPose BuildModelPose(const assets::XModelData& model, const assets::XAnimData* baseAnim,
                            float time, const assets::XAnimData* additiveAnim, float additiveTime);

// Palette for GPU skinning: inverse(bind pose) * animated pose per model bone, in the
// model's own bone order. When no animation is active this is the identity (or, for the
// gun, the static hands attachment).
std::vector<DirectX::XMFLOAT4X4> BuildBonePalette(const assets::XModelData& model,
                                                  const SkeletonPose& pose,
                                                  const std::vector<int>& boneMap);

// Recomputes worldQuat/worldPos after a caller overrides a local bone transform
// (used for the ADS tag_ads overlay).
void RecomputeWorld(SkeletonPose& pose);

} // namespace wb::scene
