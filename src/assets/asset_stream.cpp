#include "assets/asset_stream.h"

#include "assets/h1_structs.h"

#include <cstddef>
#include <cstring>
#include <fstream>
#include <limits>

namespace wb::assets {
namespace {

constexpr std::uint8_t kTypeChar = 1;
constexpr std::uint8_t kTypeShort = 2;
constexpr std::uint8_t kTypeInt = 3;
constexpr std::uint8_t kTypeFloat = 4;
constexpr std::uint8_t kTypeInt64 = 5;
constexpr std::uint8_t kTypeString = 6;
constexpr std::uint8_t kTypeAsset = 7;
constexpr std::uint8_t kTypeArray = 8;
constexpr std::uint8_t kTypeOffset = 9;
constexpr std::uint8_t kTypeRaw = 10;

} // namespace

AssetStream::AssetStream(std::vector<std::uint8_t> data) : data_(std::move(data)) {}

AssetStream AssetStream::FromFile(const std::filesystem::path& path) {
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	if (!file) {
		throw AssetError("cannot open " + path.string());
	}
	const std::streamoff size = file.tellg();
	if (size <= 0) {
		throw AssetError("empty file: " + path.string());
	}
	file.seekg(0, std::ios::beg);
	std::vector<std::uint8_t> data(static_cast<std::size_t>(size));
	if (!file.read(reinterpret_cast<char*>(data.data()), size)) {
		throw AssetError("cannot read " + path.string());
	}
	return AssetStream(std::move(data));
}

void AssetStream::EnsureAvailable(std::size_t bytes, const char* what) const {
	if (position_ > data_.size() || bytes > data_.size() - position_) {
		throw AssetError(std::string("unexpected end of file while reading ") + what);
	}
}

std::uint8_t AssetStream::ReadType() {
	EnsureAvailable(1, "a type tag");
	return data_[position_++];
}

std::uint32_t AssetStream::ReadU32() {
	EnsureAvailable(sizeof(std::uint32_t), "a 32-bit value");
	std::uint32_t value = 0;
	std::memcpy(&value, data_.data() + position_, sizeof(value));
	position_ += sizeof(value);
	return value;
}

std::string AssetStream::ReadCString() {
	const std::size_t start = position_;
	std::size_t end = start;
	while (end < data_.size() && data_[end] != 0) {
		++end;
	}
	if (end >= data_.size()) {
		throw AssetError("unterminated string");
	}
	std::string value(reinterpret_cast<const char*>(data_.data() + start), end - start);
	position_ = end + 1;
	return value;
}

std::string AssetStream::ReadString() {
	const std::uint8_t type = ReadType();
	if (type == kTypeString) {
		EnsureAvailable(1, "a string header");
		const std::uint8_t existing = data_[position_++];
		if (!existing) {
			return {};
		}
		Entry entry;
		entry.kind = EntryKind::String;
		entry.offset = position_;
		entry.text = ReadCString();
		entries_.push_back(std::move(entry));
		return entries_.back().text;
	}
	if (type == kTypeOffset) {
		const std::uint32_t index = ReadU32();
		const std::uint32_t sub = ReadU32();
		if (index >= entries_.size()) {
			throw AssetError("string offset references an unknown entry");
		}
		const Entry& entry = entries_[index];
		if (entry.kind != EntryKind::String) {
			throw AssetError("string offset references a non-string entry");
		}
		if (sub != 0) {
			throw AssetError("string offset has a non-zero sub-index");
		}
		return entry.text;
	}
	throw AssetError("expected a string, got tag " + std::to_string(type));
}

std::string AssetStream::ReadAssetName() {
	const std::uint8_t type = ReadType();
	if (type == kTypeAsset) {
		EnsureAvailable(1, "an asset header");
		const std::uint8_t existing = data_[position_++];
		if (!existing) {
			return {};
		}
		Entry entry;
		entry.kind = EntryKind::Asset;
		entry.offset = position_;
		entry.text = ReadCString();
		entries_.push_back(std::move(entry));
		return entries_.back().text;
	}
	if (type == kTypeOffset) {
		const std::uint32_t index = ReadU32();
		const std::uint32_t sub = ReadU32();
		if (index >= entries_.size()) {
			throw AssetError("asset offset references an unknown entry");
		}
		const Entry& entry = entries_[index];
		if (entry.kind != EntryKind::Asset) {
			throw AssetError("asset offset references a non-asset entry");
		}
		if (sub != 0) {
			throw AssetError("asset offset has a non-zero sub-index");
		}
		return entry.text;
	}
	throw AssetError("expected an asset reference, got tag " + std::to_string(type));
}

template <typename T>
ArrayView<T> AssetStream::ReadArrayView() {
	static_assert(std::is_trivially_copyable_v<T>, "array element must be trivially copyable");
	const std::uint8_t type = ReadType();
	if (type == kTypeArray) {
		EnsureAvailable(1, "an array header");
		const std::uint8_t existing = data_[position_++];
		if (!existing) {
			return {};
		}
		const std::uint32_t count = ReadU32();
		const std::size_t bytes = static_cast<std::size_t>(count) * sizeof(T);
		EnsureAvailable(bytes, "an array body");
		const std::size_t offset = position_;
		position_ += bytes;

		Entry entry;
		entry.kind = EntryKind::Array;
		entry.offset = offset;
		entry.count = count;
		entry.elementSize = sizeof(T);
		entries_.push_back(entry);
		return {reinterpret_cast<const T*>(data_.data() + offset), count};
	}
	if (type == kTypeOffset) {
		const std::uint32_t index = ReadU32();
		const std::uint32_t sub = ReadU32();
		if (index >= entries_.size()) {
			throw AssetError("array offset references an unknown entry");
		}
		const Entry& entry = entries_[index];
		if (entry.kind != EntryKind::Array || entry.elementSize != sizeof(T)) {
			throw AssetError("array offset references an incompatible entry");
		}
		if (sub > entry.count) {
			throw AssetError("array offset is out of range");
		}
		return {reinterpret_cast<const T*>(data_.data() + entry.offset + static_cast<std::size_t>(sub) * sizeof(T)),
		        entry.count - sub};
	}
	throw AssetError("expected an array, got tag " + std::to_string(type));
}

std::vector<std::uint8_t> AssetStream::ReadRawBytes() {
	const std::uint8_t type = ReadType();
	if (type == kTypeRaw) {
		EnsureAvailable(1, "a raw header");
		const std::uint8_t existing = data_[position_++];
		if (!existing) {
			return {};
		}
		const std::uint32_t size = ReadU32();
		EnsureAvailable(size, "a raw body");
		const std::size_t offset = position_;
		position_ += size;

		Entry entry;
		entry.kind = EntryKind::Raw;
		entry.offset = offset;
		entry.count = size;
		entry.elementSize = 1;
		entries_.push_back(entry);
		return std::vector<std::uint8_t>(data_.begin() + static_cast<std::ptrdiff_t>(offset),
		                                 data_.begin() + static_cast<std::ptrdiff_t>(offset + size));
	}
	if (type == kTypeOffset) {
		const std::uint32_t index = ReadU32();
		const std::uint32_t sub = ReadU32();
		if (index >= entries_.size()) {
			throw AssetError("raw offset references an unknown entry");
		}
		const Entry& entry = entries_[index];
		if (entry.kind != EntryKind::Raw || sub != 0) {
			throw AssetError("raw offset references an incompatible entry");
		}
		return std::vector<std::uint8_t>(data_.begin() + static_cast<std::ptrdiff_t>(entry.offset),
		                                 data_.begin() + static_cast<std::ptrdiff_t>(entry.offset + entry.count));
	}
	throw AssetError("expected raw data, got tag " + std::to_string(type));
}

template <typename T>
ArrayView<T> AssetStream::ReadRawView() {
	static_assert(std::is_trivially_copyable_v<T>, "raw element must be trivially copyable");
	const std::uint8_t type = ReadType();
	if (type == kTypeRaw) {
		EnsureAvailable(1, "a raw header");
		const std::uint8_t existing = data_[position_++];
		if (!existing) {
			return {};
		}
		const std::uint32_t size = ReadU32();
		EnsureAvailable(size, "a raw body");
		const std::size_t offset = position_;
		position_ += size;

		Entry entry;
		entry.kind = EntryKind::Raw;
		entry.offset = offset;
		entry.count = size;
		entry.elementSize = sizeof(T);
		entries_.push_back(entry);
		return {reinterpret_cast<const T*>(data_.data() + offset), size / sizeof(T)};
	}
	if (type == kTypeOffset) {
		const std::uint32_t index = ReadU32();
		const std::uint32_t sub = ReadU32();
		if (index >= entries_.size()) {
			throw AssetError("raw offset references an unknown entry");
		}
		const Entry& entry = entries_[index];
		if (entry.kind != EntryKind::Raw || entry.elementSize != sizeof(T)) {
			throw AssetError("raw offset references an incompatible entry");
		}
		const std::uint32_t offset = sub * static_cast<std::uint32_t>(sizeof(T));
		if (offset > entry.count || entry.count - offset > std::numeric_limits<std::uint32_t>::max()) {
			throw AssetError("raw offset is out of range");
		}
		return {reinterpret_cast<const T*>(data_.data() + entry.offset + offset), (entry.count - offset) / sizeof(T)};
	}
	throw AssetError("expected raw data, got tag " + std::to_string(type));
}

template ArrayView<std::uint8_t> AssetStream::ReadArrayView<std::uint8_t>();
template ArrayView<std::int16_t> AssetStream::ReadArrayView<std::int16_t>();
template ArrayView<std::uint16_t> AssetStream::ReadArrayView<std::uint16_t>();
template ArrayView<std::int32_t> AssetStream::ReadArrayView<std::int32_t>();
template ArrayView<std::uint32_t> AssetStream::ReadArrayView<std::uint32_t>();
template ArrayView<std::uint64_t> AssetStream::ReadArrayView<std::uint64_t>();
template ArrayView<float> AssetStream::ReadArrayView<float>();
template ArrayView<h1::XModelAngle> AssetStream::ReadArrayView<h1::XModelAngle>();
template ArrayView<h1::XModelTagPos> AssetStream::ReadArrayView<h1::XModelTagPos>();
template ArrayView<h1::DObjAnimMat> AssetStream::ReadArrayView<h1::DObjAnimMat>();
template ArrayView<h1::ReactiveMotionModelPart> AssetStream::ReadArrayView<h1::ReactiveMotionModelPart>();
template ArrayView<h1::ReactiveMotionModelTweaks> AssetStream::ReadArrayView<h1::ReactiveMotionModelTweaks>();
template ArrayView<h1::XModelCollSurf> AssetStream::ReadArrayView<h1::XModelCollSurf>();
template ArrayView<h1::XBoneInfo> AssetStream::ReadArrayView<h1::XBoneInfo>();
template ArrayView<h1::BlendShapeWeightMap> AssetStream::ReadArrayView<h1::BlendShapeWeightMap>();
template ArrayView<h1::MdaoVolume> AssetStream::ReadArrayView<h1::MdaoVolume>();
template ArrayView<h1::XPhysBoneInfo> AssetStream::ReadArrayView<h1::XPhysBoneInfo>();
template ArrayView<h1::XModelSurfsRaw> AssetStream::ReadArrayView<h1::XModelSurfsRaw>();
template ArrayView<h1::XSurfaceRaw> AssetStream::ReadArrayView<h1::XSurfaceRaw>();
template ArrayView<h1::GfxPackedVertex> AssetStream::ReadArrayView<h1::GfxPackedVertex>();
template ArrayView<h1::Face> AssetStream::ReadArrayView<h1::Face>();
template ArrayView<h1::XRigidVertList> AssetStream::ReadArrayView<h1::XRigidVertList>();
template ArrayView<h1::XSurfaceCollisionTree> AssetStream::ReadArrayView<h1::XSurfaceCollisionTree>();
template ArrayView<h1::XSurfaceCollisionNode> AssetStream::ReadArrayView<h1::XSurfaceCollisionNode>();
template ArrayView<h1::XSurfaceSubdivInfoRaw> AssetStream::ReadArrayView<h1::XSurfaceSubdivInfoRaw>();
template ArrayView<h1::XSurfaceSubdivLevelRaw> AssetStream::ReadArrayView<h1::XSurfaceSubdivLevelRaw>();
template ArrayView<h1::BlendShape> AssetStream::ReadArrayView<h1::BlendShape>();
template ArrayView<h1::BlendShapeVert> AssetStream::ReadArrayView<h1::BlendShapeVert>();
template ArrayView<h1::GfxImageRaw> AssetStream::ReadArrayView<h1::GfxImageRaw>();
template ArrayView<h1::XModelRaw> AssetStream::ReadArrayView<h1::XModelRaw>();

template ArrayView<std::uint8_t> AssetStream::ReadRawView<std::uint8_t>();
template ArrayView<std::uint16_t> AssetStream::ReadRawView<std::uint16_t>();
template ArrayView<std::uint32_t> AssetStream::ReadRawView<std::uint32_t>();
template ArrayView<float> AssetStream::ReadRawView<float>();

} // namespace wb::assets
