#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "assets/model_asset.h"

namespace wb::assets {

// One HMW player faction as defined by the HMW costume script
// (`refs\dumps\hmw_files\scripts\mp_patches\custom_teams.gsc`, *_precache functions):
// the body and head xmodel names and the matching viewhands model.
struct CharacterFaction {
	std::string id;      // "tf141", "opfor", ...
	std::string display; // "TF141 (assault)"
	std::string body;    // body_h2_tf141_assault
	std::string head;    // head_h2_tf141_assault (empty when the body includes the head)
	std::string viewhands;
};

const std::vector<CharacterFaction>& CharacterFactions();
const CharacterFaction* FindCharacterFaction(const std::string& id);

// Availability in the current roots (for the faction dropdown).
struct CharacterAvailability {
	bool bodyFound = false;
	bool headFound = false;
	bool viewhandsFound = false;
	bool usable = false; // bodyFound
};

CharacterAvailability CheckCharacterFaction(const CharacterFaction& faction,
                                            const std::vector<std::filesystem::path>& roots);

// Loads the body and (when it exists) the head; missing head only warns. Returns false with
// `missing` filled when the body itself cannot be loaded.
bool LoadCharacter(const CharacterFaction& faction, const std::vector<std::filesystem::path>& roots,
                   ModelAsset& body, ModelAsset& head, bool& hasHead, std::vector<std::string>& missing,
                   std::vector<std::string>& warnings, std::string& error);

// Bone the head model attaches to, trying the usual H1 player names in order.
const char* CharacterHeadBone();

} // namespace wb::assets
