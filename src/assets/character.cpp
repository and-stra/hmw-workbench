#include "assets/character.h"

#include <algorithm>

namespace wb::assets {
namespace {

const std::vector<CharacterFaction> kFactions = {
    // HMW multiplayer factions (custom_teams.gsc, assault variant; the body/head are the
    // H2 models named there). The *_mp body/head are the H1 ports used by the same script.
    {"tf141", "TF141", "body_h2_tf141_assault", "head_h2_tf141_assault", "viewhands_tf141"},
    {"opfor", "OpFor", "body_h2_opforce_assault", "head_h2_opforce_assault",
     "viewhands_h1_arab_desert_mp_camo"},
    {"spetsnaz", "Spetsnaz", "body_h2_airborne_assault", "head_h2_airborne_assault",
     "globalViewhands_mw2_airborne"},
    {"rangers", "Rangers", "body_h2_us_army_ar", "head_h2_us_army_assault", "viewhands_rangers"},
    {"seals", "SEALs", "body_h2_seal_assault", "head_h2_seal_assault", "viewhands_udt"},
    {"militia", "Militia", "body_h2_militia_assault", "head_h2_militia_assault",
     "globalViewhands_mw2_militia"},
    {"gign", "GIGN", "mp_body_gign_paris_assault", "head_gign_a", "viewhands_sas"},
    {"pmc", "PMC", "mp_body_pmc_africa_assault_a", "head_pmc_africa_a", "viewhands_pmc"},
    {"ic", "Inner Circle", "mp_body_henchmen_assault_a", "head_henchmen_a", "viewhands_henchmen"},
    {"africa", "African Militia", "mp_body_africa_militia_assault_a", "head_africa_militia_a_mp",
     "viewhands_african_militia"},
    {"delta", "Delta Force", "mp_body_delta_elite_assault_aa", "head_delta_elite_c",
     "viewhands_delta"},
    // H1 multiplayer body that is in the dumps (its xsurface is missing, see M9.md).
    {"usmc_desert", "USMC desert (H1)", "body_usmc_desert_assault_mp_camo", "",
     "viewhands_h1_usmc_desert_mp_camo"},
    // Full-body HMW "post gfx" characters (hazmat/infected); the body includes the head.
    {"hazmat", "Hazmat", "body_hazmat", "", "viewhands_hazmat"},
    {"infect", "Infected", "body_infect", "", "viewhands_infect"},
};

} // namespace

const std::vector<CharacterFaction>& CharacterFactions() { return kFactions; }

const CharacterFaction* FindCharacterFaction(const std::string& id) {
	for (const CharacterFaction& faction : kFactions) {
		if (faction.id == id || faction.display == id) {
			return &faction;
		}
	}
	return nullptr;
}

CharacterAvailability CheckCharacterFaction(const CharacterFaction& faction,
                                            const std::vector<std::filesystem::path>& roots) {
	CharacterAvailability availability;
	availability.bodyFound = FindModelFile(roots, faction.body).has_value();
	availability.headFound = faction.head.empty() || FindModelFile(roots, faction.head).has_value();
	availability.viewhandsFound =
	    faction.viewhands.empty() || FindModelFile(roots, faction.viewhands).has_value();
	availability.usable = availability.bodyFound;
	return availability;
}

const char* CharacterHeadBone() { return "j_head"; }

bool LoadCharacter(const CharacterFaction& faction, const std::vector<std::filesystem::path>& roots,
                   ModelAsset& body, ModelAsset& head, bool& hasHead, std::vector<std::string>& missing,
                   std::vector<std::string>& warnings, std::string& error) {
	body = ModelAsset{};
	head = ModelAsset{};
	hasHead = false;
	missing.clear();
	warnings.clear();

	if (!FindModelFile(roots, faction.body).has_value()) {
		missing.push_back("body " + faction.body);
		error = "character " + faction.display + ": body model " + faction.body +
		        " is not in any asset root";
		return false;
	}
	if (!LoadModelAsset(faction.body, roots, body, error, true)) {
		error = "character " + faction.display + " body " + faction.body + ": " + error;
		return false;
	}

	if (!faction.head.empty()) {
		if (!FindModelFile(roots, faction.head).has_value()) {
			missing.push_back("head " + faction.head);
			warnings.push_back("head model " + faction.head + " is not in any asset root; showing the body");
		} else if (!LoadModelAsset(faction.head, roots, head, error, true)) {
			missing.push_back("head " + faction.head);
			warnings.push_back("head model " + faction.head + ": " + error);
		} else {
			hasHead = true;
		}
	}
	if (!faction.viewhands.empty() && !FindModelFile(roots, faction.viewhands).has_value()) {
		missing.push_back("viewhands " + faction.viewhands);
	}
	return true;
}

} // namespace wb::assets
