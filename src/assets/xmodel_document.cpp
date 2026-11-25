#include "assets/xmodel_document.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <fstream>

#include <nlohmann/json.hpp>

#include "assets/asset_stream.h"

namespace wb::assets {
namespace {

constexpr std::int16_t kAngleScale = 32767;

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

DirectX::XMFLOAT4 Multiply(const DirectX::XMFLOAT4& a, const DirectX::XMFLOAT4& b) {
	DirectX::XMFLOAT4 out;
	DirectX::XMStoreFloat4(&out,
	                       DirectX::XMQuaternionMultiply(DirectX::XMLoadFloat4(&a),
	                                                     DirectX::XMLoadFloat4(&b)));
	return out;
}

DirectX::XMFLOAT4 Inverse(const DirectX::XMFLOAT4& q) {
	DirectX::XMFLOAT4 out;
	DirectX::XMStoreFloat4(&out, DirectX::XMQuaternionInverse(DirectX::XMLoadFloat4(&q)));
	return out;
}

DirectX::XMFLOAT3 Rotate(const DirectX::XMFLOAT3& value, const DirectX::XMFLOAT4& q) {
	DirectX::XMFLOAT3 out;
	DirectX::XMStoreFloat3(&out,
	                       DirectX::XMVector3Rotate(DirectX::XMLoadFloat3(&value),
	                                                DirectX::XMLoadFloat4(&q)));
	return out;
}

h1::XModelAngle EncodeAngle(DirectX::XMFLOAT4 quat) {
	quat = Normalize(quat);
	if (quat.w < 0.0f) {
		quat.x = -quat.x;
		quat.y = -quat.y;
		quat.z = -quat.z;
		quat.w = -quat.w;
	}
	const auto encode = [](float value) {
		const long rounded = std::lround(value * static_cast<float>(kAngleScale));
		return static_cast<std::int16_t>(std::clamp(rounded, -32767L, 32767L));
	};
	h1::XModelAngle angle;
	angle.x = encode(quat.x);
	angle.y = encode(quat.y);
	angle.z = encode(quat.z);
	angle.base = encode(quat.w);
	return angle;
}

int FindBone(const XModelData& model, const std::string& name) {
	for (std::size_t i = 0; i < model.bones.size(); ++i) {
		if (model.bones[i].name == name) {
			return static_cast<int>(i);
		}
	}
	return -1;
}

bool SetBoneName(XModelDocument& document, std::size_t boneIndex, const std::string& name,
                 std::string& error) {
	if (boneIndex >= document.model.bones.size() || boneIndex >= document.boneNameItems.size()) {
		error = "bone index out of range";
		return false;
	}
	const std::size_t itemIndex = document.boneNameItems[boneIndex];
	if (itemIndex >= document.items.size()) {
		error = "bone name item out of range";
		return false;
	}
	RecordedItem& item = document.items[itemIndex];
	item.type = 6;
	item.existing = true;
	item.text = name;
	item.refIndex = 0;
	item.refSub = 0;
	document.model.bones[boneIndex].name = name;
	return true;
}

void ComputeWorld(const XModelData& model, std::size_t index, DirectX::XMFLOAT3& worldPos,
                  DirectX::XMFLOAT4& worldQuat) {
	const BoneData& bone = model.bones[index];
	if (bone.parent < 0) {
		worldPos = bone.localPos;
		worldQuat = Normalize(bone.localQuat);
		return;
	}
	const std::size_t parent = static_cast<std::size_t>(bone.parent);
	DirectX::XMFLOAT3 parentPos;
	DirectX::XMFLOAT4 parentQuat;
	ComputeWorld(model, parent, parentPos, parentQuat);
	const DirectX::XMFLOAT3 rotated = Rotate(bone.localPos, parentQuat);
	worldPos = {parentPos.x + rotated.x, parentPos.y + rotated.y, parentPos.z + rotated.z};
	worldQuat = Normalize(Multiply(bone.localQuat, parentQuat));
}

void SetLocal(XModelDocument& document, std::size_t index, const DirectX::XMFLOAT3& localPos,
              const DirectX::XMFLOAT4& localQuat) {
	BoneData& bone = document.model.bones[index];
	bone.localPos = localPos;
	bone.localQuat = Normalize(localQuat);
	ComputeWorld(document.model, index, bone.worldPos, bone.worldQuat);
	if (index < document.baseMat.size()) {
		h1::DObjAnimMat& matrix = document.baseMat[index];
		matrix.quat[0] = bone.worldQuat.x;
		matrix.quat[1] = bone.worldQuat.y;
		matrix.quat[2] = bone.worldQuat.z;
		matrix.quat[3] = bone.worldQuat.w;
		matrix.trans[0] = bone.worldPos.x;
		matrix.trans[1] = bone.worldPos.y;
		matrix.trans[2] = bone.worldPos.z;
	}
	const std::size_t tagIndex = index - document.model.numRootBones;
	if (tagIndex < document.tagAngles.size()) {
		document.tagAngles[tagIndex] = EncodeAngle(bone.localQuat);
	}
	if (tagIndex < document.tagPositions.size()) {
		document.tagPositions[tagIndex] = {bone.worldPos.x, bone.worldPos.y, bone.worldPos.z};
	}
}

void AppendBytes(std::vector<std::uint8_t>& out, const void* data, std::size_t size) {
	const auto* bytes = static_cast<const std::uint8_t*>(data);
	out.insert(out.end(), bytes, bytes + size);
}

void WriteU8(std::vector<std::uint8_t>& out, std::uint8_t value) { out.push_back(value); }

void WriteU32(std::vector<std::uint8_t>& out, std::uint32_t value) {
	AppendBytes(out, &value, sizeof(value));
}

void WriteString(std::vector<std::uint8_t>& out, const std::string& text) {
	out.insert(out.end(), text.begin(), text.end());
	out.push_back(0);
}

std::uint32_t CountEntriesBefore(const std::vector<RecordedItem>& items, std::size_t position) {
	std::uint32_t count = 0;
	for (std::size_t i = 0; i < position && i < items.size(); ++i) {
		if (items[i].type != 9 && items[i].existing) {
			++count;
		}
	}
	return count;
}

void RecomputeEntryIndices(std::vector<RecordedItem>& items) {
	std::uint32_t entry = 0;
	for (RecordedItem& item : items) {
		if (item.type == 9 || !item.existing) {
			item.entryIndex = 0xFFFFFFFFu;
		} else {
			item.entryIndex = entry++;
		}
	}
}

bool WriteArray(std::vector<std::uint8_t>& out, const void* data, std::uint32_t count,
                std::uint32_t elementSize) {
	const std::size_t bytes = static_cast<std::size_t>(count) * elementSize;
	WriteU8(out, 8);
	WriteU8(out, data != nullptr && count > 0 ? 1 : 0);
	if (data == nullptr || count == 0) {
		return true;
	}
	WriteU32(out, count);
	AppendBytes(out, data, bytes);
	return true;
}

template <typename T>
bool ReadArray(const RecordedItem& item, std::vector<T>& out, std::uint32_t expectedCount,
               const char* what, std::string& error) {
	if (item.type != 8 || !item.existing) {
		error = std::string("xmodel: ") + what + " is not an inline array";
		return false;
	}
	if (item.count != expectedCount || item.elementSize != sizeof(T) ||
	    item.body.size() != static_cast<std::size_t>(expectedCount) * sizeof(T)) {
		error = std::string("xmodel: ") + what + " has an unexpected size";
		return false;
	}
	out.resize(expectedCount);
	if (expectedCount > 0) {
		std::memcpy(out.data(), item.body.data(), item.body.size());
	}
	return true;
}

} // namespace

bool LoadXModelDocument(const std::filesystem::path& path, XModelDocument& out, std::string& error) {
	out = XModelDocument{};
	out.path = path;
	try {
		AssetStream stream = AssetStream::FromFile(path);
		std::vector<RecordedItem> items;
		stream.SetRecorder(&items);
		out.model = ParseXModel(stream);
		out.items = std::move(items);
	} catch (const std::exception& e) {
		error = e.what();
		return false;
	}

	const std::size_t boneCount = out.model.bones.size();
	const std::size_t rootCount = out.model.numRootBones;
	if (out.items.size() < 3 + boneCount) {
		error = "xmodel: truncated stream";
		return false;
	}
	if (out.items[0].type != 8 || !out.items[0].existing ||
	    out.items[0].body.size() != sizeof(h1::XModelRaw)) {
		error = "xmodel: unexpected header item";
		return false;
	}
	std::memcpy(&out.header, out.items[0].body.data(), sizeof(h1::XModelRaw));

	out.headerItem = 0;
	out.nameItem = 1;
	out.boneNamesItem = 2;
	out.firstBoneNameItem = 3;
	out.parentItem = 3 + boneCount;
	out.angleItem = out.parentItem + 1;
	out.positionItem = out.parentItem + 2;
	out.classificationItem = out.parentItem + 3;
	out.baseMatItem = out.parentItem + 4;
	// reactiveMotionParts, reactiveMotionTweaks, collSurfs, then boneInfo
	out.boneInfoItem = out.baseMatItem + 4;

	out.boneNameItems.resize(boneCount);
	for (std::size_t i = 0; i < boneCount; ++i) {
		out.boneNameItems[i] = 3 + i;
	}

	if (out.parentItem >= out.items.size() || out.boneInfoItem >= out.items.size()) {
		error = "xmodel: truncated bone arrays";
		return false;
	}
	const auto readOptional = [&](std::size_t itemIndex, auto& target, std::uint32_t count,
	                              const char* what, bool& inlineFlag) {
		const RecordedItem& item = out.items[itemIndex];
		if (item.type == 9) {
			inlineFlag = false;
			return true;
		}
		if (item.type != 8 || !item.existing) {
			inlineFlag = false;
			return true;
		}
		return ReadArray(item, target, count, what, error);
	};
	if (!readOptional(2, out.boneNameValues, static_cast<std::uint32_t>(boneCount), "boneNames",
	                  out.inlineBoneNames)) {
		return false;
	}
	if (!readOptional(out.parentItem, out.parentList,
	                  static_cast<std::uint32_t>(boneCount - rootCount), "parentList",
	                  out.inlineParentList)) {
		return false;
	}
	if (!readOptional(out.angleItem, out.tagAngles, static_cast<std::uint32_t>(boneCount - rootCount),
	                  "tagAngles", out.inlineTagAngles)) {
		return false;
	}
	if (!readOptional(out.positionItem, out.tagPositions,
	                  static_cast<std::uint32_t>(boneCount - rootCount), "tagPositions",
	                  out.inlineTagPositions)) {
		return false;
	}
	if (!readOptional(out.classificationItem, out.partClassification,
	                  static_cast<std::uint32_t>(boneCount), "partClassification",
	                  out.inlinePartClassification)) {
		return false;
	}
	if (!readOptional(out.baseMatItem, out.baseMat, static_cast<std::uint32_t>(boneCount), "baseMat",
	                  out.inlineBaseMat)) {
		return false;
	}
	const RecordedItem& boneInfoItem = out.items[out.boneInfoItem];
	if (boneInfoItem.type == 8 && boneInfoItem.existing) {
		if (!ReadArray(boneInfoItem, out.boneInfo, static_cast<std::uint32_t>(boneCount), "boneInfo",
		               error)) {
			return false;
		}
		out.hasBoneInfo = true;
	}

	out.originalModel = out.model;
	out.originalBaseMat = out.baseMat;
	out.originalTagAngles = out.tagAngles;
	out.originalTagPositions = out.tagPositions;
	out.addedBones.assign(boneCount, false);
	return true;
}

bool SaveXModelDocument(const XModelDocument& document, const std::filesystem::path& path,
                        std::string& error) {
	const std::size_t boneCount = document.model.bones.size();
	if (boneCount > 255) {
		error = "xmodel: more than 255 bones";
		return false;
	}

	std::vector<std::uint8_t> out;
	out.reserve(document.items.empty() ? 1024 : 65536);

	h1::XModelRaw header = document.header;
	header.numBones = static_cast<std::uint8_t>(boneCount);

	std::vector<std::uint32_t> nameValues = document.boneNameValues;
	nameValues.resize(boneCount, 0);

	for (std::size_t i = 0; i < document.items.size(); ++i) {
		const RecordedItem& item = document.items[i];
		if (i == document.headerItem) {
			WriteU8(out, 8);
			WriteU8(out, 1);
			WriteU32(out, 1);
			AppendBytes(out, &header, sizeof(header));
			continue;
		}
		if (i == document.boneNamesItem && document.inlineBoneNames) {
			WriteArray(out, nameValues.data(), static_cast<std::uint32_t>(boneCount),
			           sizeof(std::uint32_t));
			continue;
		}
		if (i >= document.firstBoneNameItem &&
		    i < document.firstBoneNameItem + document.boneNameItems.size()) {
			const std::size_t bone = i - document.firstBoneNameItem;
			if (item.type == 9) {
				WriteU8(out, 9);
				WriteU32(out, item.refIndex);
				WriteU32(out, item.refSub);
			} else {
				WriteU8(out, 6);
				WriteU8(out, 1);
				WriteString(out, document.model.bones[bone].name);
			}
			continue;
		}
		if (i == document.parentItem && document.inlineParentList) {
			WriteArray(out, document.parentList.data(),
			           static_cast<std::uint32_t>(document.parentList.size()), 1);
			continue;
		}
		if (i == document.angleItem && document.inlineTagAngles) {
			WriteArray(out, document.tagAngles.data(),
			           static_cast<std::uint32_t>(document.tagAngles.size()),
			           sizeof(h1::XModelAngle));
			continue;
		}
		if (i == document.positionItem && document.inlineTagPositions) {
			WriteArray(out, document.tagPositions.data(),
			           static_cast<std::uint32_t>(document.tagPositions.size()),
			           sizeof(h1::XModelTagPos));
			continue;
		}
		if (i == document.classificationItem && document.inlinePartClassification) {
			WriteArray(out, document.partClassification.data(),
			           static_cast<std::uint32_t>(document.partClassification.size()), 1);
			continue;
		}
		if (i == document.baseMatItem && document.inlineBaseMat) {
			WriteArray(out, document.baseMat.data(), static_cast<std::uint32_t>(document.baseMat.size()),
			           sizeof(h1::DObjAnimMat));
			continue;
		}
		if (i == document.boneInfoItem && document.hasBoneInfo) {
			WriteArray(out, document.boneInfo.data(), static_cast<std::uint32_t>(document.boneInfo.size()),
			           sizeof(h1::XBoneInfo));
			continue;
		}

		if (item.type == 9) {
			WriteU8(out, 9);
			WriteU32(out, item.refIndex);
			WriteU32(out, item.refSub);
			continue;
		}
		if (item.type == 6 || item.type == 7) {
			WriteU8(out, item.type);
			WriteU8(out, item.existing ? 1 : 0);
			if (item.existing) {
				WriteString(out, item.text);
			}
			continue;
		}
		if (item.type == 8) {
			WriteU8(out, 8);
			WriteU8(out, item.existing ? 1 : 0);
			if (item.existing) {
				WriteU32(out, item.count);
				AppendBytes(out, item.body.data(), item.body.size());
			}
			continue;
		}
		if (item.type == 10) {
			WriteU8(out, 10);
			WriteU8(out, item.existing ? 1 : 0);
			if (item.existing) {
				WriteU32(out, static_cast<std::uint32_t>(item.body.size()));
				AppendBytes(out, item.body.data(), item.body.size());
			}
			continue;
		}
		error = "xmodel: unknown stream item type " + std::to_string(item.type);
		return false;
	}

	std::error_code ec;
	const std::filesystem::path parent = path.parent_path();
	if (!parent.empty()) {
		std::filesystem::create_directories(parent, ec);
	}
	std::ofstream file(path, std::ios::binary | std::ios::trunc);
	if (!file) {
		error = "cannot write " + path.string();
		return false;
	}
	file.write(reinterpret_cast<const char*>(out.data()), static_cast<std::streamsize>(out.size()));
	if (!file) {
		error = "cannot write " + path.string();
		return false;
	}
	return true;
}

bool MoveTagLocal(XModelDocument& document, const std::string& tag, const DirectX::XMFLOAT3& localPos,
                  const DirectX::XMFLOAT4& localQuat, std::string& error) {
	if (!document.inlineParentList || !document.inlineTagAngles || !document.inlineTagPositions ||
	    !document.inlineBaseMat) {
		error = "xmodel uses shared bone arrays; editing is not supported";
		return false;
	}
	const int index = FindBone(document.model, tag);
	if (index < 0) {
		error = "tag not found: " + tag;
		return false;
	}
	if (index < document.model.numRootBones) {
		error = tag + " is a root bone, not a tag";
		return false;
	}
	SetLocal(document, static_cast<std::size_t>(index), localPos, localQuat);
	return true;
}

bool AddTag(XModelDocument& document, const std::string& name, const std::string& parent,
            const std::string& copyFrom, const DirectX::XMFLOAT3& localPos,
            const DirectX::XMFLOAT4& localQuat, std::string& error) {
	if (!document.inlineBoneNames || !document.inlineParentList || !document.inlineTagAngles ||
	    !document.inlineTagPositions || !document.inlinePartClassification ||
	    !document.inlineBaseMat) {
		error = "xmodel uses shared bone arrays; editing is not supported";
		return false;
	}
	if (name.empty()) {
		error = "tag name is empty";
		return false;
	}
	if (FindBone(document.model, name) >= 0) {
		error = "bone already exists: " + name;
		return false;
	}
	const std::size_t boneCount = document.model.bones.size();
	if (boneCount >= 255) {
		error = "xmodel already has 255 bones";
		return false;
	}
	const std::size_t rootCount = document.model.numRootBones;

	int parentIndex = -1;
	if (!parent.empty()) {
		parentIndex = FindBone(document.model, parent);
		if (parentIndex < 0) {
			error = "parent bone not found: " + parent;
			return false;
		}
	} else {
		for (const char* candidate : {"j_gun", "tag_weapon", "tag_origin"}) {
			parentIndex = FindBone(document.model, candidate);
			if (parentIndex >= 0) {
				break;
			}
		}
		if (parentIndex < 0) {
			parentIndex = 0;
		}
	}
	if (parentIndex >= static_cast<int>(boneCount)) {
		error = "invalid parent bone";
		return false;
	}

	int sourceIndex = -1;
	DirectX::XMFLOAT3 position = localPos;
	DirectX::XMFLOAT4 rotation = Normalize(localQuat);
	if (!copyFrom.empty()) {
		sourceIndex = FindBone(document.model, copyFrom);
		if (sourceIndex < 0) {
			error = "copy source tag not found: " + copyFrom;
			return false;
		}
		position = document.model.bones[static_cast<std::size_t>(sourceIndex)].localPos;
		rotation = document.model.bones[static_cast<std::size_t>(sourceIndex)].localQuat;
	}

	BoneData bone;
	bone.name = name;
	bone.parent = parentIndex;
	bone.localPos = position;
	bone.localQuat = Normalize(rotation);
	DirectX::XMFLOAT3 parentPos;
	DirectX::XMFLOAT4 parentQuat;
	ComputeWorld(document.model, static_cast<std::size_t>(parentIndex), parentPos, parentQuat);
	const DirectX::XMFLOAT3 rotated = Rotate(bone.localPos, parentQuat);
	bone.worldPos = {parentPos.x + rotated.x, parentPos.y + rotated.y, parentPos.z + rotated.z};
	bone.worldQuat = Normalize(Multiply(bone.localQuat, parentQuat));
	const h1::XModelAngle angle = EncodeAngle(bone.localQuat);
	bone.tagAngle[0] = angle.x;
	bone.tagAngle[1] = angle.y;
	bone.tagAngle[2] = angle.z;
	bone.tagAngle[3] = angle.base;
	bone.tagPosition = bone.worldPos;
	bone.hasTagPosition = true;
	if (sourceIndex >= 0) {
		bone.bindTransWeight = document.model.bones[static_cast<std::size_t>(sourceIndex)].bindTransWeight;
	}

	document.model.bones.push_back(bone);
	document.model.numBones = static_cast<std::uint8_t>(document.model.bones.size());

	document.boneNameValues.push_back(0);
	document.parentList.push_back(static_cast<std::uint8_t>(boneCount - static_cast<std::size_t>(parentIndex)));
	document.tagAngles.push_back(EncodeAngle(bone.localQuat));
	document.tagPositions.push_back(
	    {bone.worldPos.x, bone.worldPos.y, bone.worldPos.z});
	document.partClassification.push_back(
	    sourceIndex >= 0 ? document.partClassification[static_cast<std::size_t>(sourceIndex)] : 0);
	h1::DObjAnimMat matrix{};
	matrix.quat[0] = bone.worldQuat.x;
	matrix.quat[1] = bone.worldQuat.y;
	matrix.quat[2] = bone.worldQuat.z;
	matrix.quat[3] = bone.worldQuat.w;
	matrix.trans[0] = bone.worldPos.x;
	matrix.trans[1] = bone.worldPos.y;
	matrix.trans[2] = bone.worldPos.z;
	matrix.transWeight = bone.bindTransWeight;
	document.baseMat.push_back(matrix);
	if (document.hasBoneInfo) {
		if (sourceIndex >= 0) {
			document.boneInfo.push_back(document.boneInfo[static_cast<std::size_t>(sourceIndex)]);
		} else {
			document.boneInfo.push_back(h1::XBoneInfo{});
		}
	}

	// Insert the new bone name string after the last bone name string and remap offsets.
	const std::size_t insertAt = document.boneNameItems.empty()
	                                 ? document.firstBoneNameItem
	                                 : document.boneNameItems.back() + 1;
	const std::uint32_t entryBefore = CountEntriesBefore(document.items, insertAt);
	for (RecordedItem& item : document.items) {
		if (item.type == 9 && item.refIndex >= entryBefore) {
			++item.refIndex;
		}
	}
	RecordedItem nameItem;
	nameItem.type = 6;
	nameItem.existing = true;
	nameItem.text = name;
	nameItem.entryIndex = entryBefore;
	document.items.insert(document.items.begin() + static_cast<std::ptrdiff_t>(insertAt), nameItem);
	RecomputeEntryIndices(document.items);

	const auto shift = [insertAt](std::size_t& value) {
		if (value >= insertAt) {
			++value;
		}
	};
	shift(document.parentItem);
	shift(document.angleItem);
	shift(document.positionItem);
	shift(document.classificationItem);
	shift(document.baseMatItem);
	shift(document.boneInfoItem);
	for (std::size_t& item : document.boneNameItems) {
		if (item >= insertAt) {
			++item;
		}
	}
	document.boneNameItems.push_back(insertAt);
	document.addedBones.push_back(true);
	(void)rootCount;
	return true;
}

bool RenameTag(XModelDocument& document, const std::string& oldName, const std::string& newName,
               std::string& error) {
	if (newName.empty()) {
		error = "new tag name is empty";
		return false;
	}
	const int index = FindBone(document.model, oldName);
	if (index < 0) {
		error = "tag not found: " + oldName;
		return false;
	}
	std::string lower = newName;
	std::transform(lower.begin(), lower.end(), lower.begin(),
	               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	for (const BoneData& bone : document.model.bones) {
		std::string existing = bone.name;
		std::transform(existing.begin(), existing.end(), existing.begin(),
		               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		if (existing == lower) {
			error = "bone already exists: " + newName;
			return false;
		}
	}
	return SetBoneName(document, static_cast<std::size_t>(index), newName, error);
}

bool RevertTag(XModelDocument& document, const std::string& tag, std::string& error) {
	const int index = FindBone(document.model, tag);
	if (index < 0) {
		error = "tag not found: " + tag;
		return false;
	}
	const std::size_t boneIndex = static_cast<std::size_t>(index);
	if (index < document.model.numRootBones) {
		error = tag + " is a root bone";
		return false;
	}
	if (boneIndex >= document.addedBones.size() || !document.addedBones[boneIndex]) {
		if (boneIndex >= document.originalModel.bones.size() || boneIndex >= document.originalBaseMat.size()) {
			error = "tag is not in the original model";
			return false;
		}
		const BoneData& original = document.originalModel.bones[boneIndex];
		if (!SetBoneName(document, boneIndex, original.name, error)) {
			return false;
		}
		SetLocal(document, boneIndex, original.localPos, original.localQuat);
		return true;
	}

	for (std::size_t i = 0; i < document.model.bones.size(); ++i) {
		if (i != boneIndex && document.model.bones[i].parent == index) {
			error = tag + " is the parent of " + document.model.bones[i].name;
			return false;
		}
	}

	const std::size_t removeAt = document.boneNameItems[boneIndex];
	const std::uint32_t entryBefore = CountEntriesBefore(document.items, removeAt);
	for (RecordedItem& item : document.items) {
		if (item.type == 9) {
			if (item.refIndex == entryBefore) {
				error = "internal: removed tag string is referenced";
				return false;
			}
			if (item.refIndex > entryBefore) {
				--item.refIndex;
			}
		}
	}
	document.items.erase(document.items.begin() + static_cast<std::ptrdiff_t>(removeAt));
	RecomputeEntryIndices(document.items);

	const auto shiftDown = [removeAt](std::size_t& value) {
		if (value > removeAt) {
			--value;
		}
	};
	shiftDown(document.parentItem);
	shiftDown(document.angleItem);
	shiftDown(document.positionItem);
	shiftDown(document.classificationItem);
	shiftDown(document.baseMatItem);
	shiftDown(document.boneInfoItem);
	for (std::size_t& item : document.boneNameItems) {
		if (item > removeAt) {
			--item;
		}
	}
	document.boneNameItems.erase(document.boneNameItems.begin() + static_cast<std::ptrdiff_t>(boneIndex));

	const std::size_t boneCount = document.model.bones.size();
	for (std::size_t j = boneIndex + 1; j < boneCount; ++j) {
		BoneData& later = document.model.bones[j];
		if (later.parent > index) {
			--later.parent;
		}
		const std::size_t tagIndex = j - document.model.numRootBones;
		if (tagIndex < document.parentList.size()) {
			document.parentList[tagIndex] = static_cast<std::uint8_t>(
			    j - static_cast<std::size_t>(later.parent));
		}
	}
	document.model.bones.erase(document.model.bones.begin() + static_cast<std::ptrdiff_t>(boneIndex));
	document.model.numBones = static_cast<std::uint8_t>(document.model.bones.size());
	document.boneNameValues.erase(document.boneNameValues.begin() + static_cast<std::ptrdiff_t>(boneIndex));
	if (boneIndex >= document.model.numRootBones) {
		const std::size_t tagIndex = boneIndex - document.model.numRootBones;
		document.parentList.erase(document.parentList.begin() + static_cast<std::ptrdiff_t>(tagIndex));
		document.tagAngles.erase(document.tagAngles.begin() + static_cast<std::ptrdiff_t>(tagIndex));
		document.tagPositions.erase(document.tagPositions.begin() + static_cast<std::ptrdiff_t>(tagIndex));
	}
	document.partClassification.erase(document.partClassification.begin() +
	                                  static_cast<std::ptrdiff_t>(boneIndex));
	document.baseMat.erase(document.baseMat.begin() + static_cast<std::ptrdiff_t>(boneIndex));
	if (document.hasBoneInfo) {
		document.boneInfo.erase(document.boneInfo.begin() + static_cast<std::ptrdiff_t>(boneIndex));
	}
	document.addedBones.erase(document.addedBones.begin() + static_cast<std::ptrdiff_t>(boneIndex));
	return true;
}

bool HasTagChanges(const XModelDocument& document) {
	if (document.model.bones.size() != document.originalModel.bones.size()) {
		return true;
	}
	for (std::size_t i = 0; i < document.model.bones.size(); ++i) {
		const BoneData& a = document.model.bones[i];
		const BoneData& b = document.originalModel.bones[i];
		if (a.name != b.name) {
			return true;
		}
		const float dx = a.localPos.x - b.localPos.x;
		const float dy = a.localPos.y - b.localPos.y;
		const float dz = a.localPos.z - b.localPos.z;
		const float dq = std::fabs(a.localQuat.x - b.localQuat.x) + std::fabs(a.localQuat.y - b.localQuat.y) +
		                 std::fabs(a.localQuat.z - b.localQuat.z) + std::fabs(a.localQuat.w - b.localQuat.w);
		if (dx * dx + dy * dy + dz * dz > 1.0e-10f || dq > 1.0e-6f) {
			return true;
		}
	}
	return false;
}

bool WriteTagsJson(const XModelDocument& document, const std::filesystem::path& modelPath,
                   const std::filesystem::path& jsonPath, std::string& error) {
	nlohmann::json json;
	json["model"] = document.model.name;
	json["xmodel"] = modelPath.filename().string();
	json["boneCount"] = document.model.bones.size();
	json["tags"] = nlohmann::json::array();
	for (const BoneData& bone : document.model.bones) {
		if (!bone.IsTag()) {
			continue;
		}
		nlohmann::json tag;
		tag["name"] = bone.name;
		tag["parent"] =
		    bone.parent >= 0 ? document.model.bones[static_cast<std::size_t>(bone.parent)].name : "";
		tag["localPos"] = {bone.localPos.x, bone.localPos.y, bone.localPos.z};
		tag["localRot"] = {bone.localQuat.x, bone.localQuat.y, bone.localQuat.z, bone.localQuat.w};
		tag["worldPos"] = {bone.worldPos.x, bone.worldPos.y, bone.worldPos.z};
		bool added = false;
		for (std::size_t i = 0; i < document.addedBones.size() && i < document.model.bones.size(); ++i) {
			if (document.addedBones[i] && document.model.bones[i].name == bone.name) {
				added = true;
				break;
			}
		}
		tag["added"] = added;
		json["tags"].push_back(std::move(tag));
	}
	std::error_code ec;
	const std::filesystem::path parent = jsonPath.parent_path();
	if (!parent.empty()) {
		std::filesystem::create_directories(parent, ec);
	}
	std::ofstream file(jsonPath);
	if (!file) {
		error = "cannot write " + jsonPath.string();
		return false;
	}
	file << json.dump(2) << '\n';
	return static_cast<bool>(file);
}

} // namespace wb::assets
