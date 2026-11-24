#include "assets/xmodel.h"

#include "assets/asset_stream.h"
#include "assets/h1_structs.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace wb::assets {
XModelData LoadXModel(const std::filesystem::path& path) {
	AssetStream stream = AssetStream::FromFile(path);

	const std::vector<h1::XModelRaw> header = stream.ReadSingle<h1::XModelRaw>();
	if (header.empty()) {
		throw AssetError("missing XModel struct");
	}
	const h1::XModelRaw& raw = header.front();

	XModelData model;
	model.path = path;
	model.name = stream.ReadString();
	model.numBones = raw.numBones;
	model.numRootBones = raw.numRootBones;
	model.numSurfs = raw.numsurfs;
	model.numLods = static_cast<std::uint8_t>(std::max<int>(raw.numLods, 0));
	model.flags = raw.flags;
	model.scale = raw.scale;
	model.boundsMid = DirectX::XMFLOAT3(raw.bounds.midPoint[0], raw.bounds.midPoint[1], raw.bounds.midPoint[2]);
	model.boundsHalf = DirectX::XMFLOAT3(raw.bounds.halfSize[0], raw.bounds.halfSize[1], raw.bounds.halfSize[2]);

	const std::vector<std::int32_t> boneNameHashes = stream.ReadArray<std::int32_t>();
	std::vector<std::string> boneNames;
	boneNames.reserve(boneNameHashes.size());
	for (std::size_t i = 0; i < boneNameHashes.size(); ++i) {
		boneNames.push_back(stream.ReadString());
	}
	if (boneNames.size() != raw.numBones) {
		throw AssetError("bone name count " + std::to_string(boneNames.size()) + " does not match numBones " +
		                 std::to_string(raw.numBones));
	}

	const std::vector<std::uint8_t> parents = stream.ReadArray<std::uint8_t>();
	const std::vector<h1::XModelAngle> tagAngles = stream.ReadArray<h1::XModelAngle>();
	const std::vector<h1::XModelTagPos> tagPositions = stream.ReadArray<h1::XModelTagPos>();
	model.partClassification = stream.ReadArray<std::uint8_t>();
	const std::vector<h1::DObjAnimMat> baseMat = stream.ReadArray<h1::DObjAnimMat>();
	(void)stream.ReadArray<h1::ReactiveMotionModelPart>();
	(void)stream.ReadSingle<h1::ReactiveMotionModelTweaks>();
	(void)stream.ReadArray<h1::XModelCollSurf>();
	(void)stream.ReadArray<h1::XBoneInfo>();
	(void)stream.ReadArray<std::uint16_t>();

	(void)stream.ReadArray<std::uint64_t>(); // material pointer array
	for (std::uint32_t i = 0; i < raw.numsurfs; ++i) {
		model.materialNames.push_back(stream.ReadAssetName());
	}
	for (int i = 0; i < 6; ++i) {
		model.lodSurfaces[static_cast<std::size_t>(i)] = stream.ReadAssetName();
	}
	(void)stream.ReadAssetName(); // physPreset
	(void)stream.ReadAssetName(); // physCollmap

	(void)stream.ReadArray<std::int32_t>(); // blend shape weight name hashes
	for (std::uint32_t i = 0; i < raw.numberOfBlendShapeWeights; ++i) {
		(void)stream.ReadString();
	}
	(void)stream.ReadArray<h1::BlendShapeWeightMap>();

	(void)stream.ReadArray<h1::MdaoVolume>();
	for (std::uint32_t i = 0; i < raw.mdaoVolumeCount; ++i) {
		(void)stream.ReadAssetName();
	}

	(void)stream.ReadArray<std::uint64_t>(); // composite model pointers
	for (std::uint32_t i = 0; i < raw.numCompositeModels; ++i) {
		(void)stream.ReadAssetName();
	}

	(void)stream.ReadAssetName(); // skeletonScript

	(void)stream.ReadArray<h1::XPhysBoneInfo>();
	for (std::uint32_t i = 0; i < raw.numBonePhysics; ++i) {
		(void)stream.ReadAssetName();
		(void)stream.ReadAssetName();
		(void)stream.ReadAssetName();
	}

	if (!stream.AtEnd()) {
		throw AssetError("trailing bytes in .xmb (" + std::to_string(stream.Size() - stream.Position()) + ")");
	}

	if (tagAngles.size() != 0 && tagAngles.size() + raw.numRootBones != raw.numBones) {
		// Tag angle count is informational; the layout is still valid.
	}
	if (!tagPositions.empty() && tagPositions.size() + raw.numRootBones > raw.numBones) {
		throw AssetError("tag position count exceeds bone count");
	}

	model.bones.resize(raw.numBones);
	for (std::size_t i = 0; i < model.bones.size(); ++i) {
		BoneData& bone = model.bones[i];
		bone.name = boneNames[i];
		if (i < raw.numRootBones) {
			bone.parent = -1;
		} else if (i - raw.numRootBones < parents.size()) {
			// parentList stores the distance back to the parent bone, not its absolute index.
			const int parentOffset = parents[i - raw.numRootBones];
			bone.parent = static_cast<int>(i) - parentOffset;
			if (bone.parent < 0 || bone.parent >= static_cast<int>(i)) {
				throw AssetError("bone parent offset out of range");
			}
		} else {
			bone.parent = -1;
		}

		if (i < baseMat.size()) {
			// baseMat is the model-space base pose (tagPositions confirm this for tag bones).
			const h1::DObjAnimMat& mat = baseMat[i];
			bone.worldQuat = DirectX::XMFLOAT4(mat.quat[0], mat.quat[1], mat.quat[2], mat.quat[3]);
			bone.worldPos = DirectX::XMFLOAT3(mat.trans[0], mat.trans[1], mat.trans[2]);
		}
		if (i >= raw.numRootBones) {
			const std::size_t tagIndex = i - raw.numRootBones;
			if (tagIndex < tagPositions.size()) {
				bone.tagPosition = DirectX::XMFLOAT3(tagPositions[tagIndex].x, tagPositions[tagIndex].y,
				                                     tagPositions[tagIndex].z);
				bone.hasTagPosition = true;
			}
		}

		// Some dumps store tag positions in tagPositions, others only in baseMat. Prefer
		// baseMat (model-space base pose); use tagPositions only when baseMat is empty for
		// this bone and the tag position is non-zero.
		if (bone.hasTagPosition) {
			const float baseLength = bone.worldPos.x * bone.worldPos.x + bone.worldPos.y * bone.worldPos.y +
			                         bone.worldPos.z * bone.worldPos.z;
			const float tagLength = bone.tagPosition.x * bone.tagPosition.x +
			                        bone.tagPosition.y * bone.tagPosition.y +
			                        bone.tagPosition.z * bone.tagPosition.z;
			if (baseLength < 1.0e-6f && tagLength > 1.0e-6f) {
				bone.worldPos = bone.tagPosition;
			}
		}
	}

	// Derive the parent-relative local pose from the model-space base pose.
	for (std::size_t i = 0; i < model.bones.size(); ++i) {
		BoneData& bone = model.bones[i];
		const DirectX::XMVECTOR worldQuat =
		    DirectX::XMQuaternionNormalize(DirectX::XMLoadFloat4(&bone.worldQuat));
		DirectX::XMStoreFloat4(&bone.worldQuat, worldQuat);

		if (bone.parent < 0) {
			bone.localQuat = bone.worldQuat;
			bone.localPos = bone.worldPos;
			continue;
		}

		const BoneData& parent = model.bones[static_cast<std::size_t>(bone.parent)];
		const DirectX::XMVECTOR parentQuat =
		    DirectX::XMQuaternionNormalize(DirectX::XMLoadFloat4(&parent.worldQuat));
		const DirectX::XMVECTOR inverseParent = DirectX::XMQuaternionInverse(parentQuat);

		const DirectX::XMVECTOR worldPos = DirectX::XMLoadFloat3(&bone.worldPos);
		const DirectX::XMVECTOR parentPos = DirectX::XMLoadFloat3(&parent.worldPos);
		const DirectX::XMVECTOR localOffset =
		    DirectX::XMVector3Rotate(DirectX::XMVectorSubtract(worldPos, parentPos), inverseParent);

		DirectX::XMStoreFloat3(&bone.localPos, localOffset);
		DirectX::XMStoreFloat4(&bone.localQuat, DirectX::XMQuaternionMultiply(worldQuat, inverseParent));
	}

	return model;
}

} // namespace wb::assets
