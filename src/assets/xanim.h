#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <DirectXMath.h>

namespace wb::assets {

struct AnimQuatKey {
	std::uint32_t frame = 0;
	DirectX::XMFLOAT4 value{0.0f, 0.0f, 0.0f, 1.0f};
};

struct AnimVec3Key {
	std::uint32_t frame = 0;
	DirectX::XMFLOAT3 value{0.0f, 0.0f, 0.0f};
};

// One bone of the animation skeleton. CoD stores parent-relative (local) rotations
// and translations as quantized shorts/bytes; the reader converts them to floats.
struct AnimChannel {
	std::vector<AnimQuatKey> rotations;
	std::vector<AnimVec3Key> translations;

	bool HasRotation() const { return !rotations.empty(); }
	bool HasTranslation() const { return !translations.empty(); }
};

struct AnimNotify {
	std::string name;
	float fraction = 0.0f; // normalized [0,1] as stored in the asset
	float time = 0.0f;     // seconds (fraction * duration)
	std::uint32_t frame = 0;
};

struct XAnimData {
	std::string name;
	std::filesystem::path path;

	// Frame keys are 0..frameCount inclusive (frameCount + 1 keys), matching the
	// notetrack fraction convention used by MWR/H1.
	std::uint16_t frameCount = 0;
	float frameRate = 30.0f;
	std::uint8_t flags = 0;     // 1 = looping, 2 = delta
	std::uint8_t assetType = 0; // 6 = additive in H1
	bool looping = false;
	bool delta = false;
	bool additive = false;

	std::vector<std::string> boneNames;
	std::vector<AnimChannel> channels; // same order as boneNames
	std::unordered_map<std::string, std::size_t> boneLookup;

	// Root motion from XAnimDeltaPart, applied to the skeleton root when present.
	std::vector<AnimQuatKey> deltaRotations;
	std::vector<AnimVec3Key> deltaTranslations;

	std::vector<AnimNotify> notifies;

	float Duration() const { return frameRate > 0.0f ? static_cast<float>(frameCount) / frameRate : 0.0f; }

	std::size_t FindBone(const std::string& bone) const;
	const AnimChannel* FindChannel(const std::string& bone) const;

	// Samples the channel at `time` seconds. Keys with a sparse frame layout are
	// interpolated linearly (slerp for rotations). Returns false when the channel
	// has no keys.
	bool SampleRotation(const std::string& bone, float time, DirectX::XMFLOAT4& out) const;
	bool SampleTranslation(const std::string& bone, float time, DirectX::XMFLOAT3& out) const;
};

std::optional<std::filesystem::path> FindAnimFile(const std::vector<std::filesystem::path>& roots,
                                                  const std::string& name);

// Reads an x64-zt H1 .xab (XAnimParts tagged stream) and decodes all rotation and
// translation channel forms used by H1: 2D/3D static and animated keys, byte/short
// translations, notetracks and delta parts.
XAnimData LoadXAnim(const std::filesystem::path& path);

} // namespace wb::assets
