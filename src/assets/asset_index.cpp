#include "assets/asset_index.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <unordered_set>

namespace wb {
namespace {

std::string ToLower(std::string text) {
	std::transform(text.begin(), text.end(), text.begin(),
	               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return text;
}

int& CountForKind(AssetRoot& root, AssetKind kind) {
	switch (kind) {
	case AssetKind::Weapon:
		return root.weaponCount;
	case AssetKind::Model:
		return root.modelCount;
	case AssetKind::Anim:
		return root.animCount;
	}
	return root.weaponCount;
}

bool HasExtension(const std::filesystem::path& path, const char* extension) {
	const std::string actual = path.extension().string();
	return _stricmp(actual.c_str(), extension) == 0;
}

void ScanFolder(const std::filesystem::path& rootPath, AssetKind kind, int rootIndex, AssetRoot& root,
                std::unordered_set<std::string>& seen, std::vector<AssetEntry>& entries) {
	const std::filesystem::path folder = rootPath / AssetKindFolder(kind);
	std::error_code ec;
	if (!std::filesystem::is_directory(folder, ec) || ec) {
		return;
	}

	std::vector<std::filesystem::path> files;
	for (std::filesystem::directory_iterator it(folder, ec), end; !ec && it != end; it.increment(ec)) {
		std::error_code fileEc;
		if (it->is_regular_file(fileEc) && HasExtension(it->path(), AssetKindExtension(kind))) {
			files.push_back(it->path());
		}
	}
	std::sort(files.begin(), files.end(),
	          [](const std::filesystem::path& a, const std::filesystem::path& b) { return a.string() < b.string(); });

	for (const auto& file : files) {
		std::string name = file.stem().string();
		const std::string key = std::to_string(static_cast<int>(kind)) + ":" + ToLower(name);
		if (!seen.insert(key).second) {
			continue;
		}
		AssetEntry entry;
		entry.kind = kind;
		entry.name = std::move(name);
		entry.path = file;
		entry.rootIndex = rootIndex;
		entries.push_back(std::move(entry));
		++CountForKind(root, kind);
	}
}

} // namespace

const char* AssetKindLabel(AssetKind kind) {
	switch (kind) {
	case AssetKind::Weapon:
		return "Weapon";
	case AssetKind::Model:
		return "Model";
	case AssetKind::Anim:
		return "Anim";
	}
	return "Unknown";
}

const char* AssetKindPlural(AssetKind kind) {
	switch (kind) {
	case AssetKind::Weapon:
		return "Weapons";
	case AssetKind::Model:
		return "Models";
	case AssetKind::Anim:
		return "Anims";
	}
	return "Unknown";
}

const char* AssetKindFolder(AssetKind kind) {
	switch (kind) {
	case AssetKind::Weapon:
		return "weapons";
	case AssetKind::Model:
		return "xmodel";
	case AssetKind::Anim:
		return "xanim";
	}
	return "";
}

const char* AssetKindExtension(AssetKind kind) {
	switch (kind) {
	case AssetKind::Weapon:
		return ".json";
	case AssetKind::Model:
		return ".xmb";
	case AssetKind::Anim:
		return ".xab";
	}
	return "";
}

void AssetIndex::Clear() {
	roots_.clear();
	entries_.clear();
}

void AssetIndex::Scan(const std::vector<std::filesystem::path>& roots) {
	Clear();
	roots_.reserve(roots.size());

	std::unordered_set<std::string> seen;
	for (size_t i = 0; i < roots.size(); ++i) {
		AssetRoot root;
		root.path = roots[i];
		std::error_code ec;
		root.exists = std::filesystem::is_directory(root.path, ec) && !ec;

		const int rootIndex = static_cast<int>(i);
		if (root.exists) {
			for (int k = 0; k < kAssetKindCount; ++k) {
				ScanFolder(root.path, static_cast<AssetKind>(k), rootIndex, root, seen, entries_);
			}
		}
		roots_.push_back(std::move(root));
	}

	std::sort(entries_.begin(), entries_.end(), [](const AssetEntry& a, const AssetEntry& b) {
		if (a.kind != b.kind) {
			return static_cast<int>(a.kind) < static_cast<int>(b.kind);
		}
		return a.name < b.name;
	});
}

int AssetIndex::Count(AssetKind kind) const {
	int count = 0;
	for (const AssetEntry& entry : entries_) {
		if (entry.kind == kind) {
			++count;
		}
	}
	return count;
}

const AssetEntry* AssetIndex::Find(AssetKind kind, const std::string& name) const {
	for (const AssetEntry& entry : entries_) {
		if (entry.kind == kind && _stricmp(entry.name.c_str(), name.c_str()) == 0) {
			return &entry;
		}
	}
	return nullptr;
}

} // namespace wb
