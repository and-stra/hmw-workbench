#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace wb::assets {

// One entry of WeaponAttachment::waFields (name/code/value). HMW uses
// `name = "szXAnims"` with `anim` = the weapon state and a space separated
// replace pair ("_ads _reflex_ads") applied to that state's anim name.
struct AttachmentField {
	std::string name;
	std::string anim;
	int type = 0;
	int code = 0;
	std::string value;
};

struct AttachmentAsset {
	std::string name;         // file stem, e.g. "reflexmwrm4"
	std::string internalName; // "internalName"
	std::vector<std::string> worldModels;
	std::vector<std::string> viewModels;
	std::vector<std::string> reticleViewModels;
	std::vector<std::string> hideTags;
	std::vector<std::string> showTags;
	std::vector<AttachmentField> fields;
	std::filesystem::path path;
};

std::optional<std::filesystem::path> FindAttachmentFile(const std::vector<std::filesystem::path>& roots,
                                                        const std::string& name);

// All definitions of an attachment name across the roots, in root order. HMW has stub
// definitions (viewModels/worldModels null) in some zones; FindAttachmentFile prefers a
// definition that actually has models.
std::vector<std::filesystem::path> FindAttachmentDefinitions(
    const std::vector<std::filesystem::path>& roots, const std::string& name);

// True when the attachment json has at least one view or world model.
bool AttachmentHasModels(const std::filesystem::path& path);

// All attachment json names in all roots (first hit wins, sorted, unique).
std::vector<std::string> ListAttachments(const std::vector<std::filesystem::path>& roots);

AttachmentAsset LoadAttachment(const std::filesystem::path& path);

// Tag similarity used for the "missing tag, nearest candidate" hint. Candidates are
// weapon tag bone names (tag_*). Same optic family ranks first, then known synonyms
// (reflex <-> red_dot, acog <-> scope, ...), then shared name tokens/prefix.
std::vector<std::string> NearestTags(const std::string& requiredTag,
                                     const std::vector<std::string>& candidateTags,
                                     std::size_t maxResults = 3);

// Coarse family for a tag ("optic_reflex", "optic_scope", "muzzle", ...).
std::string TagFamilyName(const std::string& tag);

// The AW crossbow's optic slot (animOverrides attachment1 1..7), derived from its
// override anim names: 1 acog, 2 eotech, 3 red_dot, 4 thermal, 5 vrd,
// 6 target_enhancer, 7 scope. Returns 0 for a non-optic tag.
int OpticAttachmentIndexForTag(const std::string& tag);

} // namespace wb::assets
