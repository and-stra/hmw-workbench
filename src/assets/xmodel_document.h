#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "assets/asset_stream.h"
#include "assets/h1_structs.h"
#include "assets/xmodel.h"

namespace wb::assets {

// A parsed .xmb that can be written back byte-identically (unchanged) or with tag edits.
// The original stream is kept as a token list so unknown arrays/assets survive untouched.
struct XModelDocument {
	std::filesystem::path path;
	std::vector<RecordedItem> items;
	h1::XModelRaw header{};
	XModelData model;
	XModelData originalModel;

	std::vector<std::uint32_t> boneNameValues;
	std::vector<std::uint8_t> parentList;
	std::vector<h1::XModelAngle> tagAngles;
	std::vector<h1::XModelTagPos> tagPositions;
	std::vector<std::uint8_t> partClassification;
	std::vector<h1::DObjAnimMat> baseMat;
	std::vector<h1::XBoneInfo> boneInfo;
	bool hasBoneInfo = false;
	bool inlineBoneNames = true;
	bool inlineParentList = true;
	bool inlineTagAngles = true;
	bool inlineTagPositions = true;
	bool inlinePartClassification = true;
	bool inlineBaseMat = true;

	std::vector<h1::DObjAnimMat> originalBaseMat;
	std::vector<h1::XModelAngle> originalTagAngles;
	std::vector<h1::XModelTagPos> originalTagPositions;

	std::size_t headerItem = 0;
	std::size_t nameItem = 1;
	std::size_t boneNamesItem = 2;
	std::size_t firstBoneNameItem = 3;
	std::size_t parentItem = 0;
	std::size_t angleItem = 0;
	std::size_t positionItem = 0;
	std::size_t classificationItem = 0;
	std::size_t baseMatItem = 0;
	std::size_t boneInfoItem = 0;
	std::vector<std::size_t> boneNameItems;
	std::vector<bool> addedBones;
};

// Loads the stream into a document. Builds the semantic model through ParseXModel while
// recording every read operation for the writer.
bool LoadXModelDocument(const std::filesystem::path& path, XModelDocument& out, std::string& error);

// Writes the document (all unchanged tokens replayed). Returns false and an error on I/O.
bool SaveXModelDocument(const XModelDocument& document, const std::filesystem::path& path,
                        std::string& error);

// Tag editing (local to the parent bone). `pos`/`quat` are the new local transform.
bool MoveTagLocal(XModelDocument& document, const std::string& tag, const DirectX::XMFLOAT3& localPos,
                  const DirectX::XMFLOAT4& localQuat, std::string& error);

// Appends a tag bone at the end. `copyFrom` copies the local transform and the per-bone
// classification/bone-info of an existing tag; otherwise `localPos`/`localQuat` are used.
// `parent` is a bone name (empty = j_gun / tag_weapon / bone 0).
bool AddTag(XModelDocument& document, const std::string& name, const std::string& parent,
            const std::string& copyFrom, const DirectX::XMFLOAT3& localPos,
            const DirectX::XMFLOAT4& localQuat, std::string& error);

// Removes an added tag or restores an existing tag to the loaded/saved state.
bool RevertTag(XModelDocument& document, const std::string& tag, std::string& error);

bool HasTagChanges(const XModelDocument& document);

// Writes <model>.tags.json next to a model path (all tags: name, parent, local pos/rot).
bool WriteTagsJson(const XModelDocument& document, const std::filesystem::path& modelPath,
                   const std::filesystem::path& jsonPath, std::string& error);

} // namespace wb::assets
