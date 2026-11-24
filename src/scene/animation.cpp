#include "scene/animation.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace wb::scene {
namespace {

DirectX::XMFLOAT4 Normalize(DirectX::XMFLOAT4 q) {
	const float length = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
	if (length > 1.0e-8f) {
		q.x /= length;
		q.y /= length;
		q.z /= length;
		q.w /= length;
	} else {
		q = {0.0f, 0.0f, 0.0f, 1.0f};
	}
	return q;
}

DirectX::XMFLOAT4 Multiply(DirectX::XMFLOAT4 a, DirectX::XMFLOAT4 b) {
	DirectX::XMVECTOR result =
	    DirectX::XMQuaternionMultiply(DirectX::XMLoadFloat4(&a), DirectX::XMLoadFloat4(&b));
	DirectX::XMFLOAT4 out;
	DirectX::XMStoreFloat4(&out, result);
	return out;
}

DirectX::XMFLOAT4 Slerp(DirectX::XMFLOAT4 a, DirectX::XMFLOAT4 b, float t) {
	float dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
	if (dot < 0.0f) {
		b.x = -b.x;
		b.y = -b.y;
		b.z = -b.z;
		b.w = -b.w;
		dot = -dot;
	}
	if (dot > 0.9995f) {
		return Normalize({a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t,
		                  a.w + (b.w - a.w) * t});
	}
	const float theta = std::acos(std::clamp(dot, -1.0f, 1.0f));
	const float sinTheta = std::sin(theta);
	const float wa = std::sin((1.0f - t) * theta) / sinTheta;
	const float wb = std::sin(t * theta) / sinTheta;
	return Normalize({a.x * wa + b.x * wb, a.y * wa + b.y * wb, a.z * wa + b.z * wb,
	                  a.w * wa + b.w * wb});
}

bool SampleQuatKeys(const std::vector<assets::AnimQuatKey>& keys, float frame, DirectX::XMFLOAT4& out) {
	if (keys.empty()) {
		return false;
	}
	if (frame <= static_cast<float>(keys.front().frame)) {
		out = keys.front().value;
		return true;
	}
	if (frame >= static_cast<float>(keys.back().frame)) {
		out = keys.back().value;
		return true;
	}
	const auto next = std::upper_bound(keys.begin(), keys.end(), frame,
	                                   [](float value, const assets::AnimQuatKey& key) { return value < key.frame; });
	const assets::AnimQuatKey& b = *next;
	const assets::AnimQuatKey& a = *(next - 1);
	const float span = static_cast<float>(b.frame - a.frame);
	out = Slerp(a.value, b.value, span > 0.0f ? (frame - static_cast<float>(a.frame)) / span : 0.0f);
	return true;
}

bool SampleVecKeys(const std::vector<assets::AnimVec3Key>& keys, float frame, DirectX::XMFLOAT3& out) {
	if (keys.empty()) {
		return false;
	}
	if (frame <= static_cast<float>(keys.front().frame)) {
		out = keys.front().value;
		return true;
	}
	if (frame >= static_cast<float>(keys.back().frame)) {
		out = keys.back().value;
		return true;
	}
	const auto next = std::upper_bound(keys.begin(), keys.end(), frame,
	                                   [](float value, const assets::AnimVec3Key& key) { return value < key.frame; });
	const assets::AnimVec3Key& b = *next;
	const assets::AnimVec3Key& a = *(next - 1);
	const float span = static_cast<float>(b.frame - a.frame);
	const float t = span > 0.0f ? (frame - static_cast<float>(a.frame)) / span : 0.0f;
	out = {a.value.x + (b.value.x - a.value.x) * t, a.value.y + (b.value.y - a.value.y) * t,
	       a.value.z + (b.value.z - a.value.z) * t};
	return true;
}

DirectX::XMFLOAT3 Rotate(const DirectX::XMFLOAT3& value, const DirectX::XMFLOAT4& quaternion) {
	const DirectX::XMVECTOR result = DirectX::XMVector3Rotate(
	    DirectX::XMLoadFloat3(&value), DirectX::XMLoadFloat4(&quaternion));
	DirectX::XMFLOAT3 out;
	DirectX::XMStoreFloat3(&out, result);
	return out;
}

void SampleBone(const std::string& name, const assets::XAnimData* baseAnim, float time,
                const assets::XAnimData* additiveAnim, float additiveTime, PoseBone& bone) {
	if (baseAnim != nullptr) {
		DirectX::XMFLOAT4 rotation;
		if (baseAnim->SampleRotation(name, time, rotation)) {
			bone.localQuat = rotation;
		}
		DirectX::XMFLOAT3 translation;
		if (baseAnim->SampleTranslation(name, time, translation)) {
			bone.localPos = translation;
		}
	}
	if (additiveAnim != nullptr) {
		DirectX::XMFLOAT4 rotation;
		if (additiveAnim->SampleRotation(name, additiveTime, rotation)) {
			bone.localQuat = Normalize(Multiply(bone.localQuat, rotation));
		}
		DirectX::XMFLOAT3 translation;
		if (additiveAnim->SampleTranslation(name, additiveTime, translation)) {
			bone.localPos.x += translation.x;
			bone.localPos.y += translation.y;
			bone.localPos.z += translation.z;
		}
	}
}

void ApplyDelta(const assets::XAnimData* anim, float time, SkeletonPose& pose) {
	if (anim == nullptr || (anim->deltaRotations.empty() && anim->deltaTranslations.empty())) {
		return;
	}
	int root = -1;
	for (std::size_t i = 0; i < pose.bones.size(); ++i) {
		if (pose.bones[i].name == "tag_origin") {
			root = static_cast<int>(i);
			break;
		}
		if (root < 0 && pose.bones[i].parent < 0) {
			root = static_cast<int>(i);
		}
	}
	if (root < 0) {
		return;
	}
	const float frame = std::clamp(time * anim->frameRate, 0.0f, static_cast<float>(anim->frameCount));
	DirectX::XMFLOAT4 rotation;
	if (SampleQuatKeys(anim->deltaRotations, frame, rotation)) {
		pose.bones[static_cast<std::size_t>(root)].localQuat = rotation;
	}
	DirectX::XMFLOAT3 translation;
	if (SampleVecKeys(anim->deltaTranslations, frame, translation)) {
		pose.bones[static_cast<std::size_t>(root)].localPos = translation;
	}
}

void ComposeWorld(SkeletonPose& pose) {
	for (PoseBone& bone : pose.bones) {
		if (bone.parent < 0) {
			bone.worldQuat = Normalize(bone.localQuat);
			bone.worldPos = bone.localPos;
			continue;
		}
		const PoseBone& parent = pose.bones[static_cast<std::size_t>(bone.parent)];
		bone.worldQuat = Normalize(Multiply(Normalize(bone.localQuat), Normalize(parent.worldQuat)));
		const DirectX::XMFLOAT3 rotated = Rotate(bone.localPos, parent.worldQuat);
		bone.worldPos = {rotated.x + parent.worldPos.x, rotated.y + parent.worldPos.y,
		                 rotated.z + parent.worldPos.z};
	}
}

int FindPoseBone(const std::unordered_map<std::string, int>& lookup, const char* name) {
	const auto it = lookup.find(name);
	return it == lookup.end() ? -1 : it->second;
}

} // namespace

SkeletonPose BuildViewmodelPose(const ViewmodelAsset& viewmodel, const assets::XAnimData* baseAnim,
                                float time, const assets::XAnimData* additiveAnim, float additiveTime) {
	SkeletonPose pose;
	pose.animated = baseAnim != nullptr || additiveAnim != nullptr;
	pose.baseAnim = baseAnim;
	pose.additiveAnim = additiveAnim;

	std::unordered_map<std::string, int> lookup;
	pose.handsBoneToPose.assign(viewmodel.hands.model.bones.size(), -1);
	pose.bones.reserve(viewmodel.hands.model.bones.size() + viewmodel.gun.model.bones.size());

	for (std::size_t i = 0; i < viewmodel.hands.model.bones.size(); ++i) {
		const assets::BoneData& source = viewmodel.hands.model.bones[i];
		PoseBone bone;
		bone.name = source.name;
		bone.parent = source.parent;
		bone.localQuat = source.localQuat;
		bone.localPos = source.localPos;
		bone.fromHands = true;
		lookup.emplace(bone.name, static_cast<int>(pose.bones.size()));
		pose.handsBoneToPose[i] = static_cast<int>(pose.bones.size());
		pose.bones.push_back(std::move(bone));
	}

	int gunRootParent = FindPoseBone(lookup, "tag_weapon");
	if (gunRootParent < 0) {
		gunRootParent = FindPoseBone(lookup, "tag_weapon_right");
	}
	if (gunRootParent < 0) {
		gunRootParent = FindPoseBone(lookup, "tag_weapon_left");
	}

	pose.gunBoneToPose.assign(viewmodel.gun.model.bones.size(), -1);
	for (std::size_t i = 0; i < viewmodel.gun.model.bones.size(); ++i) {
		const assets::BoneData& source = viewmodel.gun.model.bones[i];
		const auto existing = lookup.find(source.name);
		if (existing != lookup.end()) {
			pose.gunBoneToPose[i] = existing->second;
			continue;
		}
		PoseBone bone;
		bone.name = source.name;
		bone.localQuat = source.localQuat;
		bone.localPos = source.localPos;
		if (source.parent < 0 || i == 0) {
			bone.parent = gunRootParent;
		} else {
			const std::size_t parentIndex = static_cast<std::size_t>(source.parent);
			bone.parent = parentIndex < pose.gunBoneToPose.size() ? pose.gunBoneToPose[parentIndex] : gunRootParent;
		}
		lookup.emplace(bone.name, static_cast<int>(pose.bones.size()));
		pose.gunBoneToPose[i] = static_cast<int>(pose.bones.size());
		pose.bones.push_back(std::move(bone));
	}

	for (PoseBone& bone : pose.bones) {
		SampleBone(bone.name, baseAnim, time, additiveAnim, additiveTime, bone);
	}
	ApplyDelta(baseAnim, time, pose);
	ComposeWorld(pose);
	return pose;
}

SkeletonPose BuildModelPose(const assets::XModelData& model, const assets::XAnimData* baseAnim,
                            float time, const assets::XAnimData* additiveAnim, float additiveTime) {
	SkeletonPose pose;
	pose.animated = baseAnim != nullptr || additiveAnim != nullptr;
	pose.baseAnim = baseAnim;
	pose.additiveAnim = additiveAnim;
	pose.modelBoneToPose.resize(model.bones.size());
	pose.bones.reserve(model.bones.size());
	for (std::size_t i = 0; i < model.bones.size(); ++i) {
		const assets::BoneData& source = model.bones[i];
		PoseBone bone;
		bone.name = source.name;
		bone.parent = source.parent;
		bone.localQuat = source.localQuat;
		bone.localPos = source.localPos;
		pose.modelBoneToPose[i] = static_cast<int>(pose.bones.size());
		pose.bones.push_back(std::move(bone));
	}
	for (PoseBone& bone : pose.bones) {
		SampleBone(bone.name, baseAnim, time, additiveAnim, additiveTime, bone);
	}
	ApplyDelta(baseAnim, time, pose);
	ComposeWorld(pose);
	return pose;
}

std::vector<DirectX::XMFLOAT4X4> BuildBonePalette(const assets::XModelData& model,
                                                  const SkeletonPose& pose,
                                                  const std::vector<int>& boneMap) {
	std::vector<DirectX::XMFLOAT4X4> palette(model.bones.size());
	for (std::size_t i = 0; i < model.bones.size(); ++i) {
		const assets::BoneData& bind = model.bones[i];
		const DirectX::XMMATRIX bindWorld =
		    DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&bind.worldQuat)) *
		    DirectX::XMMatrixTranslation(bind.worldPos.x, bind.worldPos.y, bind.worldPos.z);
		const int poseIndex = i < boneMap.size() ? boneMap[i] : -1;
		DirectX::XMMATRIX animatedWorld = bindWorld;
		if (poseIndex >= 0 && static_cast<std::size_t>(poseIndex) < pose.bones.size()) {
			const PoseBone& animated = pose.bones[static_cast<std::size_t>(poseIndex)];
			animatedWorld = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&animated.worldQuat)) *
			                DirectX::XMMatrixTranslation(animated.worldPos.x, animated.worldPos.y,
			                                             animated.worldPos.z);
		}
		const DirectX::XMMATRIX inverseBind = DirectX::XMMatrixInverse(nullptr, bindWorld);
		DirectX::XMMATRIX skin = DirectX::XMMatrixMultiply(inverseBind, animatedWorld);
		DirectX::XMStoreFloat4x4(&palette[i], skin);
	}
	return palette;
}

} // namespace wb::scene
