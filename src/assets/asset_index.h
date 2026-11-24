#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace wb {

enum class AssetKind { Weapon = 0, Model = 1, Anim = 2 };

constexpr int kAssetKindCount = 3;

const char* AssetKindLabel(AssetKind kind);
const char* AssetKindPlural(AssetKind kind);
const char* AssetKindFolder(AssetKind kind);
const char* AssetKindExtension(AssetKind kind);

struct AssetEntry {
	AssetKind kind = AssetKind::Weapon;
	std::string name;
	std::filesystem::path path;
	int rootIndex = -1;
};

struct AssetRoot {
	std::filesystem::path path;
	bool exists = false;
	int weaponCount = 0;
	int modelCount = 0;
	int animCount = 0;
};

// Scans the "weapons", "xmodel" and "xanim" folders of every root. Binary files are
// listed only, not parsed (M0). A name found in an earlier root shadows the same name
// in later roots (the plan's "first hit wins" rule).
class AssetIndex {
public:
	void Scan(const std::vector<std::filesystem::path>& roots);
	void Clear();

	const std::vector<AssetEntry>& Entries() const { return entries_; }
	const std::vector<AssetRoot>& Roots() const { return roots_; }

	int Count(AssetKind kind) const;
	int TotalCount() const { return static_cast<int>(entries_.size()); }

	const AssetEntry* Find(AssetKind kind, const std::string& name) const;

private:
	std::vector<AssetRoot> roots_;
	std::vector<AssetEntry> entries_;
};

} // namespace wb
