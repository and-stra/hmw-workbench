#include "assets/attachment.h"

#include "assets/asset_stream.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <unordered_map>

#include <nlohmann/json.hpp>

namespace wb::assets {
namespace {

std::string ToLower(std::string text) {
	std::transform(text.begin(), text.end(), text.begin(),
	               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return text;
}

std::vector<std::string> Strings(const nlohmann::json& json, const char* key) {
	std::vector<std::string> out;
	if (!json.contains(key) || !json[key].is_array()) {
		return out;
	}
	for (const auto& value : json[key]) {
		if (value.is_string() && !value.get<std::string>().empty()) {
			out.push_back(value.get<std::string>());
		}
	}
	return out;
}

// Coarse weapon tag families used by HMW / AW optics and muzzle devices.
std::string TagFamily(const std::string& name) {
	std::string text = ToLower(name);
	if (text.rfind("tag_", 0) == 0) {
		text = text.substr(4);
	}
	const auto has = [&text](const char* needle) { return text.find(needle) != std::string::npos; };
	if (has("laser")) {
		return "laser";
	}
	if (has("reflex") || has("red_dot") || has("reddot") || has("eotech") || has("holo") ||
	    has("aimpoint") || has("micro")) {
		return "optic_reflex";
	}
	if (has("acog") || has("scope") || has("thermal") || has("vrd") || has("enhancer") ||
	    has("magnifier") || has("hybrid") || has("sniper")) {
		return "optic_scope";
	}
	if (has("flash") || has("silencer") || has("suppressor") || has("muzzle")) {
		return "muzzle";
	}
	if (has("grip") || has("bipod") || has("launcher") || has("m203") || has("gl")) {
		return "underbarrel";
	}
	if (has("clip") || has("mag")) {
		return "mag";
	}
	if (has("rail")) {
		return "rail";
	}
	if (has("sight")) {
		return "sight";
	}
	return {};
}

int SynonymBonus(const std::string& required, const std::string& candidate) {
	std::string a = ToLower(required);
	std::string b = ToLower(candidate);
	const auto pair = [&a, &b](const char* x, const char* y, int bonus) {
		const std::string sx(x);
		const std::string sy(y);
		const bool forward = a.find(sx) != std::string::npos && b.find(sy) != std::string::npos;
		const bool backward = a.find(sy) != std::string::npos && b.find(sx) != std::string::npos;
		return (forward || backward) ? bonus : 0;
	};
	int best = 0;
	best = std::max(best, pair("reflex", "red_dot", 40));
	best = std::max(best, pair("red_dot", "reddot", 40));
	best = std::max(best, pair("reflex", "eotech", 30));
	best = std::max(best, pair("reflex", "holo", 30));
	best = std::max(best, pair("reflex", "aimpoint", 25));
	best = std::max(best, pair("acog", "scope", 35));
	best = std::max(best, pair("thermal", "thermal_scope", 40));
	best = std::max(best, pair("acog", "thermal_scope", 10));
	best = std::max(best, pair("vrd", "scope", 20));
	best = std::max(best, pair("target_enhancer", "thermal", 20));
	best = std::max(best, pair("silencer", "suppressor", 40));
	return best;
}

std::vector<std::string> Tokens(const std::string& name) {
	std::string text = ToLower(name);
	if (text.rfind("tag_", 0) == 0) {
		text = text.substr(4);
	}
	std::vector<std::string> tokens;
	std::string current;
	for (const char character : text) {
		if (character == '_' || character == '-') {
			if (!current.empty()) {
				tokens.push_back(current);
				current.clear();
			}
		} else {
			current.push_back(character);
		}
	}
	if (!current.empty()) {
		tokens.push_back(current);
	}
	return tokens;
}

int TagScore(const std::string& required, const std::string& candidate) {
	if (required == candidate) {
		return 10000;
	}
	int score = 0;
	if (!TagFamily(required).empty() && TagFamily(required) == TagFamily(candidate)) {
		score += 100;
	}
	score += SynonymBonus(required, candidate);
	const std::vector<std::string> requiredTokens = Tokens(required);
	const std::vector<std::string> candidateTokens = Tokens(candidate);
	for (const std::string& token : requiredTokens) {
		if (std::find(candidateTokens.begin(), candidateTokens.end(), token) != candidateTokens.end()) {
			score += 30;
		}
	}
	std::string a = ToLower(required);
	std::string b = ToLower(candidate);
	if (a.rfind("tag_", 0) == 0) {
		a = a.substr(4);
	}
	if (b.rfind("tag_", 0) == 0) {
		b = b.substr(4);
	}
	std::size_t prefix = 0;
	while (prefix < a.size() && prefix < b.size() && a[prefix] == b[prefix]) {
		++prefix;
	}
	score += static_cast<int>(prefix) * 3;
	return score;
}

} // namespace

std::vector<std::filesystem::path> FindAttachmentDefinitions(
    const std::vector<std::filesystem::path>& roots, const std::string& name) {
	std::vector<std::filesystem::path> paths;
	for (const auto& root : roots) {
		const std::filesystem::path candidate = root / "attachments" / (name + ".json");
		std::error_code ec;
		if (std::filesystem::is_regular_file(candidate, ec) && !ec) {
			paths.push_back(candidate);
		}
	}
	return paths;
}

bool AttachmentHasModels(const std::filesystem::path& path) {
	std::ifstream in(path);
	if (!in) {
		return false;
	}
	nlohmann::json json;
	try {
		in >> json;
	} catch (const std::exception&) {
		return false;
	}
	const auto hasModel = [&json](const char* key) {
		if (!json.contains(key) || !json[key].is_array()) {
			return false;
		}
		for (const auto& value : json[key]) {
			if (value.is_string() && !value.get<std::string>().empty()) {
				return true;
			}
		}
		return false;
	};
	return hasModel("viewModels") || hasModel("worldModels");
}

std::optional<std::filesystem::path> FindAttachmentFile(const std::vector<std::filesystem::path>& roots,
                                                        const std::string& name) {
	const std::vector<std::filesystem::path> definitions = FindAttachmentDefinitions(roots, name);
	std::optional<std::filesystem::path> first;
	for (const std::filesystem::path& path : definitions) {
		if (!first.has_value()) {
			first = path;
		}
		if (AttachmentHasModels(path)) {
			return path;
		}
	}
	return first;
}

std::vector<std::string> ListAttachments(const std::vector<std::filesystem::path>& roots) {
	std::vector<std::string> names;
	for (const auto& root : roots) {
		const std::filesystem::path folder = root / "attachments";
		std::error_code ec;
		for (std::filesystem::directory_iterator it(folder, ec), end; !ec && it != end; it.increment(ec)) {
			std::error_code fileEc;
			if (!it->is_regular_file(fileEc) || it->path().extension() != ".json") {
				continue;
			}
			names.push_back(it->path().stem().string());
		}
	}
	std::sort(names.begin(), names.end());
	names.erase(std::unique(names.begin(), names.end()), names.end());
	return names;
}

AttachmentAsset LoadAttachment(const std::filesystem::path& path) {
	std::ifstream in(path);
	if (!in) {
		throw AssetError("cannot open attachment " + path.string());
	}
	nlohmann::json json;
	try {
		in >> json;
	} catch (const std::exception& e) {
		throw AssetError("attachment " + path.string() + ": " + e.what());
	}

	AttachmentAsset attachment;
	attachment.path = path;
	attachment.name = path.stem().string();
	if (json.contains("internalName") && json["internalName"].is_string()) {
		attachment.internalName = json["internalName"].get<std::string>();
	} else {
		attachment.internalName = attachment.name;
	}
	attachment.worldModels = Strings(json, "worldModels");
	attachment.viewModels = Strings(json, "viewModels");
	attachment.reticleViewModels = Strings(json, "reticleViewModels");
	attachment.hideTags = Strings(json, "hideTags");
	attachment.showTags = Strings(json, "showTags");
	if (json.contains("fields") && json["fields"].is_array()) {
		for (const auto& field : json["fields"]) {
			if (!field.is_object()) {
				continue;
			}
			AttachmentField entry;
			if (field.contains("name") && field["name"].is_string()) {
				entry.name = field["name"].get<std::string>();
			}
			if (field.contains("anim") && field["anim"].is_string()) {
				entry.anim = field["anim"].get<std::string>();
			}
			entry.type = field.value("type", 0);
			entry.code = field.value("code", 0);
			if (field.contains("value")) {
				if (field["value"].is_string()) {
					entry.value = field["value"].get<std::string>();
				} else if (field["value"].is_number()) {
					entry.value = field["value"].dump();
				} else if (field["value"].is_boolean()) {
					entry.value = field["value"].get<bool>() ? "true" : "false";
				}
			}
			if (!entry.name.empty()) {
				attachment.fields.push_back(std::move(entry));
			}
		}
	}
	return attachment;
}

std::vector<std::string> NearestTags(const std::string& requiredTag,
                                     const std::vector<std::string>& candidateTags,
                                     std::size_t maxResults) {
	std::vector<std::pair<int, std::string>> scored;
	scored.reserve(candidateTags.size());
	for (const std::string& candidate : candidateTags) {
		if (candidate.rfind("tag_", 0) != 0 || candidate == requiredTag) {
			continue;
		}
		const int score = TagScore(requiredTag, candidate);
		if (score > 0) {
			scored.emplace_back(score, candidate);
		}
	}
	std::sort(scored.begin(), scored.end(), [](const auto& a, const auto& b) {
		if (a.first != b.first) {
			return a.first > b.first;
		}
		return a.second < b.second;
	});
	std::vector<std::string> out;
	for (const auto& [score, name] : scored) {
		if (out.size() >= maxResults) {
			break;
		}
		(void)score;
		out.push_back(name);
	}
	return out;
}

std::string TagFamilyName(const std::string& tag) { return TagFamily(tag); }

int OpticAttachmentIndexForTag(const std::string& tag) {
	const std::string text = ToLower(tag);
	const auto has = [&text](const char* needle) { return text.find(needle) != std::string::npos; };
	if (has("acog")) {
		return 1;
	}
	if (has("eotech") || has("holo")) {
		return 2;
	}
	if (has("reflex") || has("red_dot") || has("reddot")) {
		return 3;
	}
	if (has("thermal")) {
		return 4;
	}
	if (has("vrd")) {
		return 5;
	}
	if (has("enhancer")) {
		return 6;
	}
	if (has("scope") || has("target")) {
		return 7;
	}
	return 0;
}

} // namespace wb::assets
