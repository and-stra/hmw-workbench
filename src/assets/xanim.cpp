#include "assets/xanim.h"

#include "assets/asset_stream.h"
#include "assets/h1_structs.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace wb::assets {
namespace {

float ReadFloatFromInt(std::int32_t value) {
	float out = 0.0f;
	std::memcpy(&out, &value, sizeof(out));
	return out;
}

DirectX::XMFLOAT4 NormalizeQuat(DirectX::XMFLOAT4 q) {
	const float length = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
	if (length > 1.0e-8f) {
		q.x /= length;
		q.y /= length;
		q.z /= length;
		q.w /= length;
	}
	return q;
}

DirectX::XMFLOAT4 Slerp(DirectX::XMFLOAT4 a, DirectX::XMFLOAT4 b, float t) {
	float dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
	if (dot < 0.0f) {
		b.x = -b.x;
		b.y = -b.y;
		b.z = -b.z;
		b.w = -b.w;
		dot = -dot;
	}
	if (dot > 0.9995f) {
		return NormalizeQuat(DirectX::XMFLOAT4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t,
		                                       a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t));
	}
	const float theta = std::acos(std::clamp(dot, -1.0f, 1.0f));
	const float sinTheta = std::sin(theta);
	const float wa = std::sin((1.0f - t) * theta) / sinTheta;
	const float wb = std::sin(t * theta) / sinTheta;
	return NormalizeQuat(DirectX::XMFLOAT4(a.x * wa + b.x * wb, a.y * wa + b.y * wb,
	                                       a.z * wa + b.z * wb, a.w * wa + b.w * wb));
}

std::uint8_t PeekTag(const AssetStream& stream) {
	if (stream.AtEnd()) {
		return 0;
	}
	return stream.Data()[stream.Position()];
}

void SkipNullArray(AssetStream& stream) {
	if (stream.AtEnd() || stream.Data()[stream.Position()] != 8) {
		return;
	}
	if (stream.Position() + 1 < stream.Size() && stream.Data()[stream.Position() + 1] == 0) {
		(void)stream.ReadArray<std::uint8_t>();
	}
}

template <typename T>
std::vector<T> ReadCountedArray(AssetStream& stream, std::uint32_t count) {
	if (count == 0) {
		SkipNullArray(stream);
		return {};
	}
	std::vector<T> values = stream.ReadArray<T>();
	if (values.size() != count) {
		throw AssetError("xanim array size mismatch: expected " + std::to_string(count) + ", got " +
		                 std::to_string(values.size()));
	}
	return values;
}

std::uint16_t ReadShort(const std::vector<std::int16_t>& dataShorts, std::size_t& shortCursor) {
	if (shortCursor >= dataShorts.size()) {
		throw AssetError("xanim data short out of range");
	}
	return static_cast<std::uint16_t>(dataShorts[shortCursor++]);
}

void SortChannel(AnimChannel& channel) {
	std::stable_sort(channel.rotations.begin(), channel.rotations.end(),
	                 [](const AnimQuatKey& a, const AnimQuatKey& b) { return a.frame < b.frame; });
	std::stable_sort(channel.translations.begin(), channel.translations.end(),
	                 [](const AnimVec3Key& a, const AnimVec3Key& b) { return a.frame < b.frame; });
}

} // namespace

std::size_t XAnimData::FindBone(const std::string& bone) const {
	const auto it = boneLookup.find(bone);
	return it == boneLookup.end() ? static_cast<std::size_t>(-1) : it->second;
}

const AnimChannel* XAnimData::FindChannel(const std::string& bone) const {
	const std::size_t index = FindBone(bone);
	return index == static_cast<std::size_t>(-1) ? nullptr : &channels[index];
}

bool XAnimData::SampleRotation(const std::string& bone, float time, DirectX::XMFLOAT4& out) const {
	const AnimChannel* channel = FindChannel(bone);
	if (channel == nullptr || channel->rotations.empty()) {
		return false;
	}
	const float clampTime = std::clamp(time, 0.0f, Duration());
	const float frame = frameRate > 0.0f ? clampTime * frameRate : 0.0f;
	const std::vector<AnimQuatKey>& keys = channel->rotations;
	if (frame <= static_cast<float>(keys.front().frame)) {
		out = keys.front().value;
		return true;
	}
	if (frame >= static_cast<float>(keys.back().frame)) {
		out = keys.back().value;
		return true;
	}
	const auto next = std::upper_bound(keys.begin(), keys.end(), frame,
	                                   [](float value, const AnimQuatKey& key) { return value < key.frame; });
	const AnimQuatKey& b = *next;
	const AnimQuatKey& a = *(next - 1);
	const float span = static_cast<float>(b.frame - a.frame);
	const float t = span > 0.0f ? (frame - static_cast<float>(a.frame)) / span : 0.0f;
	out = Slerp(a.value, b.value, t);
	return true;
}

bool XAnimData::SampleTranslation(const std::string& bone, float time, DirectX::XMFLOAT3& out) const {
	const AnimChannel* channel = FindChannel(bone);
	if (channel == nullptr || channel->translations.empty()) {
		return false;
	}
	const float clampTime = std::clamp(time, 0.0f, Duration());
	const float frame = frameRate > 0.0f ? clampTime * frameRate : 0.0f;
	const std::vector<AnimVec3Key>& keys = channel->translations;
	if (frame <= static_cast<float>(keys.front().frame)) {
		out = keys.front().value;
		return true;
	}
	if (frame >= static_cast<float>(keys.back().frame)) {
		out = keys.back().value;
		return true;
	}
	const auto next = std::upper_bound(keys.begin(), keys.end(), frame,
	                                   [](float value, const AnimVec3Key& key) { return value < key.frame; });
	const AnimVec3Key& b = *next;
	const AnimVec3Key& a = *(next - 1);
	const float span = static_cast<float>(b.frame - a.frame);
	const float t = span > 0.0f ? (frame - static_cast<float>(a.frame)) / span : 0.0f;
	out = DirectX::XMFLOAT3(a.value.x + (b.value.x - a.value.x) * t,
	                        a.value.y + (b.value.y - a.value.y) * t,
	                        a.value.z + (b.value.z - a.value.z) * t);
	return true;
}

std::optional<std::filesystem::path> FindAnimFile(const std::vector<std::filesystem::path>& roots,
                                                  const std::string& name) {
	for (const auto& root : roots) {
		const std::filesystem::path candidate = root / "xanim" / (name + ".xab");
		std::error_code ec;
		if (std::filesystem::is_regular_file(candidate, ec) && !ec) {
			return candidate;
		}
	}
	return std::nullopt;
}

XAnimData LoadXAnim(const std::filesystem::path& path) {
	AssetStream stream = AssetStream::FromFile(path);

	const std::vector<h1::XAnimPartsRaw> headers = stream.ReadSingle<h1::XAnimPartsRaw>();
	if (headers.empty()) {
		throw AssetError("missing XAnimParts struct");
	}
	const h1::XAnimPartsRaw& raw = headers.front();

	XAnimData anim;
	anim.path = path;
	anim.name = stream.ReadString();
	anim.frameCount = raw.numframes;
	anim.frameRate = raw.framerate > 0.0f ? raw.framerate : 30.0f;
	anim.flags = raw.flags;
	anim.assetType = raw.assetType;
	anim.looping = (raw.flags & 1u) != 0;
	anim.delta = (raw.flags & 2u) != 0;
	const std::string lowerName = [&anim]() {
		std::string lower = anim.name;
		std::transform(lower.begin(), lower.end(), lower.begin(),
		               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return lower;
	}();
	anim.additive = raw.assetType == 6 || lowerName.rfind("additive_", 0) == 0 ||
	                (lowerName.size() >= 4 && lowerName.compare(lowerName.size() - 4, 4, "_add") == 0);

	const std::size_t boneCount = raw.boneCount[9];
	anim.boneNames.reserve(boneCount);
	for (std::size_t i = 0; i < boneCount; ++i) {
		anim.boneNames.push_back(stream.ReadString());
		anim.boneLookup.emplace(anim.boneNames.back(), i);
	}
	anim.channels.resize(boneCount);

	const std::vector<std::uint8_t> dataBytes = ReadCountedArray<std::uint8_t>(stream, raw.dataByteCount);
	const std::vector<std::int16_t> dataShorts = ReadCountedArray<std::int16_t>(stream, raw.dataShortCount);
	const std::vector<std::int32_t> dataInts = ReadCountedArray<std::int32_t>(stream, raw.dataIntCount);
	const std::vector<std::int16_t> randomDataShorts =
	    ReadCountedArray<std::int16_t>(stream, raw.randomDataShortCount);
	const std::vector<std::uint8_t> randomDataBytes =
	    ReadCountedArray<std::uint8_t>(stream, raw.randomDataByteCount);
	const std::vector<std::int32_t> randomDataInts =
	    ReadCountedArray<std::int32_t>(stream, raw.randomDataIntCount);
	std::vector<std::uint16_t> longIndices;
	if (raw.indexCount > 0) {
		if (raw.numframes >= 256) {
			longIndices = ReadCountedArray<std::uint16_t>(stream, raw.indexCount);
		} else {
			(void)ReadCountedArray<std::uint8_t>(stream, raw.indexCount);
		}
	} else {
		SkipNullArray(stream);
	}

	if (raw.notifyCount > 0) {
		const std::vector<h1::XAnimNotifyInfoRaw> notify = ReadCountedArray<h1::XAnimNotifyInfoRaw>(stream, raw.notifyCount);
		anim.notifies.reserve(notify.size());
		for (std::size_t i = 0; i < notify.size(); ++i) {
			AnimNotify note;
			note.name = stream.ReadString();
			note.fraction = notify[i].time;
			note.time = note.fraction * anim.Duration();
			note.frame = static_cast<std::uint32_t>(std::lround(note.fraction * static_cast<float>(raw.numframes)));
			anim.notifies.push_back(std::move(note));
		}
	} else {
		SkipNullArray(stream);
	}

	// ---- rotation channels ----
	const bool wideFrames = raw.numframes >= 256;
	const bool wideBones = boneCount >= 256;
	std::size_t byteCursor = 0;
	std::size_t shortCursor = 0;
	std::size_t intCursor = 0;
	std::size_t randomShortCursor = 0;
	std::size_t randomByteCursor = 0;

	const auto readBoneId = [&]() -> std::size_t {
		if (wideBones) {
			if (shortCursor >= dataShorts.size()) {
				throw AssetError("xanim translation bone index out of range");
			}
			const std::size_t bone = static_cast<std::uint16_t>(dataShorts[shortCursor++]);
			return bone;
		}
		if (byteCursor >= dataBytes.size()) {
			throw AssetError("xanim translation bone index out of range");
		}
		return dataBytes[byteCursor++];
	};

	const auto skipInlineIndices = [&](std::uint32_t keyCount) {
		if (!wideFrames || keyCount < 0x40) {
			return;
		}
		while (shortCursor < dataShorts.size()) {
			const std::uint16_t value = static_cast<std::uint16_t>(dataShorts[shortCursor++]);
			if (value == raw.numframes) {
				break;
			}
		}
	};

	std::size_t longCursor = 0;
	const auto readFrameIndex = [&](std::uint32_t keyCount) -> std::uint32_t {
		if (wideFrames && keyCount >= 0x40 && longCursor < longIndices.size()) {
			return longIndices[longCursor++];
		}
		if (wideFrames) {
			if (shortCursor >= dataShorts.size()) {
				throw AssetError("xanim frame index out of range");
			}
			return static_cast<std::uint16_t>(dataShorts[shortCursor++]);
		}
		if (byteCursor >= dataBytes.size()) {
			throw AssetError("xanim frame index out of range");
		}
		return dataBytes[byteCursor++];
	};

	const std::size_t noneRot = raw.boneCount[0];
	const std::size_t twoDRot = raw.boneCount[1];
	const std::size_t normalRot = raw.boneCount[2];
	const std::size_t twoDStatic = raw.boneCount[3];
	const std::size_t normalStatic = raw.boneCount[4];
	const std::size_t normalTrans = raw.boneCount[5];
	const std::size_t preciseTrans = raw.boneCount[6];
	const std::size_t staticTrans = raw.boneCount[7];

	if (noneRot + twoDRot + normalRot + twoDStatic + normalStatic != boneCount) {
		throw AssetError("xanim rotation channel counts do not add up to the bone count");
	}

	std::size_t boneIndex = 0;
	for (std::size_t i = 0; i < noneRot; ++i, ++boneIndex) {
		anim.channels[boneIndex].rotations.push_back({0, {0.0f, 0.0f, 0.0f, 1.0f}});
	}

	for (std::size_t i = 0; i < twoDRot; ++i, ++boneIndex) {
		const std::uint32_t keyCount = ReadShort(dataShorts, shortCursor);
		skipInlineIndices(keyCount);
		for (std::uint32_t key = 0; key <= keyCount; ++key) {
			const std::uint32_t frame = readFrameIndex(keyCount);
			if (randomShortCursor + 1 >= randomDataShorts.size()) {
				throw AssetError("xanim 2D rotation data out of range");
			}
			const float z = static_cast<float>(randomDataShorts[randomShortCursor++]) / 32768.0f;
			const float w = static_cast<float>(randomDataShorts[randomShortCursor++]) / 32768.0f;
			anim.channels[boneIndex].rotations.push_back({frame, NormalizeQuat({0.0f, 0.0f, z, w})});
		}
	}

	for (std::size_t i = 0; i < normalRot; ++i, ++boneIndex) {
		const std::uint32_t keyCount = ReadShort(dataShorts, shortCursor);
		skipInlineIndices(keyCount);
		for (std::uint32_t key = 0; key <= keyCount; ++key) {
			const std::uint32_t frame = readFrameIndex(keyCount);
			if (randomShortCursor + 3 >= randomDataShorts.size()) {
				throw AssetError("xanim 3D rotation data out of range");
			}
			const float x = static_cast<float>(randomDataShorts[randomShortCursor++]) / 32768.0f;
			const float y = static_cast<float>(randomDataShorts[randomShortCursor++]) / 32768.0f;
			const float z = static_cast<float>(randomDataShorts[randomShortCursor++]) / 32768.0f;
			const float w = static_cast<float>(randomDataShorts[randomShortCursor++]) / 32768.0f;
			anim.channels[boneIndex].rotations.push_back({frame, NormalizeQuat({x, y, z, w})});
		}
	}

	for (std::size_t i = 0; i < twoDStatic; ++i, ++boneIndex) {
		if (shortCursor + 1 >= dataShorts.size()) {
			throw AssetError("xanim 2D static rotation data out of range");
		}
		const float z = static_cast<float>(dataShorts[shortCursor++]) / 32768.0f;
		const float w = static_cast<float>(dataShorts[shortCursor++]) / 32768.0f;
		anim.channels[boneIndex].rotations.push_back({0, NormalizeQuat({0.0f, 0.0f, z, w})});
	}

	for (std::size_t i = 0; i < normalStatic; ++i, ++boneIndex) {
		if (shortCursor + 3 >= dataShorts.size()) {
			throw AssetError("xanim 3D static rotation data out of range");
		}
		const float x = static_cast<float>(dataShorts[shortCursor++]) / 32768.0f;
		const float y = static_cast<float>(dataShorts[shortCursor++]) / 32768.0f;
		const float z = static_cast<float>(dataShorts[shortCursor++]) / 32768.0f;
		const float w = static_cast<float>(dataShorts[shortCursor++]) / 32768.0f;
		anim.channels[boneIndex].rotations.push_back({0, NormalizeQuat({x, y, z, w})});
	}

	// ---- translation channels ----
	const auto readMinSize = [&](DirectX::XMFLOAT3& mins, DirectX::XMFLOAT3& size) {
		if (intCursor + 5 >= dataInts.size()) {
			throw AssetError("xanim translation min/size table out of range");
		}
		mins = {ReadFloatFromInt(dataInts[intCursor]), ReadFloatFromInt(dataInts[intCursor + 1]),
		        ReadFloatFromInt(dataInts[intCursor + 2])};
		size = {ReadFloatFromInt(dataInts[intCursor + 3]), ReadFloatFromInt(dataInts[intCursor + 4]),
		        ReadFloatFromInt(dataInts[intCursor + 5])};
		intCursor += 6;
	};

	for (std::size_t i = 0; i < normalTrans; ++i) {
		const std::size_t bone = readBoneId();
		if (bone >= boneCount) {
			throw AssetError("xanim normal translation bone out of range");
		}
		const std::uint32_t keyCount = ReadShort(dataShorts, shortCursor);
		skipInlineIndices(keyCount);
		DirectX::XMFLOAT3 mins;
		DirectX::XMFLOAT3 size;
		readMinSize(mins, size);
		for (std::uint32_t key = 0; key <= keyCount; ++key) {
			const std::uint32_t frame = readFrameIndex(keyCount);
			if (randomByteCursor + 2 >= randomDataBytes.size()) {
				throw AssetError("xanim normal translation data out of range");
			}
			const float x = size.x * randomDataBytes[randomByteCursor++] + mins.x;
			const float y = size.y * randomDataBytes[randomByteCursor++] + mins.y;
			const float z = size.z * randomDataBytes[randomByteCursor++] + mins.z;
			anim.channels[bone].translations.push_back({frame, {x, y, z}});
		}
	}

	for (std::size_t i = 0; i < preciseTrans; ++i) {
		const std::size_t bone = readBoneId();
		if (bone >= boneCount) {
			throw AssetError("xanim precise translation bone out of range");
		}
		const std::uint32_t keyCount = ReadShort(dataShorts, shortCursor);
		skipInlineIndices(keyCount);
		DirectX::XMFLOAT3 mins;
		DirectX::XMFLOAT3 size;
		readMinSize(mins, size);
		for (std::uint32_t key = 0; key <= keyCount; ++key) {
			const std::uint32_t frame = readFrameIndex(keyCount);
			if (randomShortCursor + 2 >= randomDataShorts.size()) {
				throw AssetError("xanim precise translation data out of range");
			}
			const float x = size.x * static_cast<std::uint16_t>(randomDataShorts[randomShortCursor++]) + mins.x;
			const float y = size.y * static_cast<std::uint16_t>(randomDataShorts[randomShortCursor++]) + mins.y;
			const float z = size.z * static_cast<std::uint16_t>(randomDataShorts[randomShortCursor++]) + mins.z;
			anim.channels[bone].translations.push_back({frame, {x, y, z}});
		}
	}

	for (std::size_t i = 0; i < staticTrans; ++i) {
		if (intCursor + 2 >= dataInts.size()) {
			throw AssetError("xanim static translation data out of range");
		}
		const DirectX::XMFLOAT3 value = {ReadFloatFromInt(dataInts[intCursor]),
		                                 ReadFloatFromInt(dataInts[intCursor + 1]),
		                                 ReadFloatFromInt(dataInts[intCursor + 2])};
		intCursor += 3;
		const std::size_t bone = readBoneId();
		if (bone >= boneCount) {
			throw AssetError("xanim static translation bone out of range");
		}
		anim.channels[bone].translations.push_back({0, value});
	}
	(void)randomDataInts;

	// ---- delta parts ----
	if (raw.deltaPart != 0) {
		const std::vector<h1::XAnimDeltaPartRaw> delta = ReadCountedArray<h1::XAnimDeltaPartRaw>(stream, 1);
		if (!delta.empty()) {
			const h1::XAnimDeltaPartRaw& parts = delta.front();
			if (parts.trans != 0) {
				const std::vector<std::uint8_t> trans = stream.ReadRawBytes();
				if (trans.size() >= 40) {
					const std::uint16_t frameCount = static_cast<std::uint16_t>(trans[0] | (trans[1] << 8));
					const std::uint8_t smallTrans = trans[2];
					DirectX::XMFLOAT3 mins;
					DirectX::XMFLOAT3 maxs;
					std::memcpy(&mins, trans.data() + 8, sizeof(mins));
					std::memcpy(&maxs, trans.data() + 20, sizeof(maxs));
					if (frameCount == 0) {
						anim.deltaTranslations.push_back({0, mins});
					} else {
						std::uint64_t frameDataPtr = 0;
						std::memcpy(&frameDataPtr, trans.data() + 32, sizeof(frameDataPtr));
						const std::uint8_t* indices = trans.data() + 40;
						if (frameDataPtr != 0) {
							const std::vector<std::uint8_t> frames = stream.ReadRawBytes();
							const std::size_t stride = smallTrans ? 3 : 6;
							for (std::size_t key = 0; key <= frameCount; ++key) {
								const std::uint32_t frame = wideFrames
								                                ? static_cast<std::uint32_t>(indices[key * 2] |
								                                                             (indices[key * 2 + 1] << 8))
								                                : indices[key];
								if ((key + 1) * stride > frames.size()) {
									break;
								}
								const std::uint8_t* values = frames.data() + key * stride;
								DirectX::XMFLOAT3 value;
								if (smallTrans) {
									value = {maxs.x * values[0] + mins.x, maxs.y * values[1] + mins.y,
									         maxs.z * values[2] + mins.z};
								} else {
									std::uint16_t x = 0;
									std::uint16_t y = 0;
									std::uint16_t z = 0;
									std::memcpy(&x, values, 2);
									std::memcpy(&y, values + 2, 2);
									std::memcpy(&z, values + 4, 2);
									value = {maxs.x * x + mins.x, maxs.y * y + mins.y, maxs.z * z + mins.z};
								}
								anim.deltaTranslations.push_back({frame, value});
							}
						}
					}
				}
			}
			if (parts.quat2 != 0) {
				const std::vector<std::uint8_t> quat = stream.ReadRawBytes();
				if (quat.size() >= 16) {
					const std::uint16_t frameCount = static_cast<std::uint16_t>(quat[0] | (quat[1] << 8));
					if (frameCount == 0) {
						std::int16_t z = 0;
						std::int16_t w = 0;
						std::memcpy(&z, quat.data() + 8, 2);
						std::memcpy(&w, quat.data() + 10, 2);
						anim.deltaRotations.push_back(
						    {0, NormalizeQuat({0.0f, 0.0f, z / 32768.0f, w / 32768.0f})});
					} else {
						std::uint64_t frameDataPtr = 0;
						std::memcpy(&frameDataPtr, quat.data() + 8, sizeof(frameDataPtr));
						const std::uint8_t* indices = quat.data() + 16;
						if (frameDataPtr != 0) {
							const std::vector<std::uint8_t> frames = stream.ReadRawBytes();
							for (std::size_t key = 0; key <= frameCount; ++key) {
								const std::uint32_t frame = wideFrames
								                                ? static_cast<std::uint32_t>(indices[key * 2] |
								                                                             (indices[key * 2 + 1] << 8))
								                                : indices[key];
								if ((key + 1) * 4 > frames.size()) {
									break;
								}
								std::int16_t z = 0;
								std::int16_t w = 0;
								std::memcpy(&z, frames.data() + key * 4, 2);
								std::memcpy(&w, frames.data() + key * 4 + 2, 2);
								anim.deltaRotations.push_back(
								    {frame, NormalizeQuat({0.0f, 0.0f, z / 32768.0f, w / 32768.0f})});
							}
						}
					}
				}
			}
			if (parts.quat != 0) {
				const std::vector<std::uint8_t> quat = stream.ReadRawBytes();
				if (quat.size() >= 16) {
					const std::uint16_t frameCount = static_cast<std::uint16_t>(quat[0] | (quat[1] << 8));
					if (frameCount == 0) {
						std::int16_t x = 0;
						std::int16_t y = 0;
						std::int16_t z = 0;
						std::int16_t w = 0;
						std::memcpy(&x, quat.data() + 8, 2);
						std::memcpy(&y, quat.data() + 10, 2);
						std::memcpy(&z, quat.data() + 12, 2);
						std::memcpy(&w, quat.data() + 14, 2);
						anim.deltaRotations.push_back(
						    {0, NormalizeQuat({x / 32768.0f, y / 32768.0f, z / 32768.0f, w / 32768.0f})});
					} else {
						std::uint64_t frameDataPtr = 0;
						std::memcpy(&frameDataPtr, quat.data() + 8, sizeof(frameDataPtr));
						const std::uint8_t* indices = quat.data() + 16;
						if (frameDataPtr != 0) {
							const std::vector<std::uint8_t> frames = stream.ReadRawBytes();
							for (std::size_t key = 0; key <= frameCount; ++key) {
								const std::uint32_t frame = wideFrames
								                                ? static_cast<std::uint32_t>(indices[key * 2] |
								                                                             (indices[key * 2 + 1] << 8))
								                                : indices[key];
								if ((key + 1) * 8 > frames.size()) {
									break;
								}
								std::int16_t x = 0;
								std::int16_t y = 0;
								std::int16_t z = 0;
								std::int16_t w = 0;
								std::memcpy(&x, frames.data() + key * 8, 2);
								std::memcpy(&y, frames.data() + key * 8 + 2, 2);
								std::memcpy(&z, frames.data() + key * 8 + 4, 2);
								std::memcpy(&w, frames.data() + key * 8 + 6, 2);
								anim.deltaRotations.push_back(
								    {frame, NormalizeQuat({x / 32768.0f, y / 32768.0f, z / 32768.0f, w / 32768.0f})});
							}
						}
					}
				}
			}
		}
	} else {
		SkipNullArray(stream);
	}

	if (raw.secondaryName != 0) {
		(void)stream.ReadString();
	} else {
		SkipNullArray(stream);
	}

	if (raw.blendShapeCount > 0) {
		for (std::uint16_t i = 0; i < raw.blendShapeCount; ++i) {
			(void)stream.ReadString();
		}
		for (int i = 0; i < 5; ++i) {
			if (!stream.AtEnd() && (PeekTag(stream) == 8 || PeekTag(stream) == 9 || PeekTag(stream) == 10)) {
				if (PeekTag(stream) == 10) {
					(void)stream.ReadRawBytes();
				} else if (PeekTag(stream) == 9) {
					(void)stream.ReadRawBytes();
				} else {
					(void)stream.ReadArray<std::uint8_t>();
				}
			}
		}
	}

	for (AnimChannel& channel : anim.channels) {
		SortChannel(channel);
	}
	std::stable_sort(anim.deltaRotations.begin(), anim.deltaRotations.end(),
	                 [](const AnimQuatKey& a, const AnimQuatKey& b) { return a.frame < b.frame; });
	std::stable_sort(anim.deltaTranslations.begin(), anim.deltaTranslations.end(),
	                 [](const AnimVec3Key& a, const AnimVec3Key& b) { return a.frame < b.frame; });

	return anim;
}

} // namespace wb::assets
