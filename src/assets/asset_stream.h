#pragma once

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace wb::assets {

class AssetError : public std::runtime_error {
public:
	using std::runtime_error::runtime_error;
};

template <typename T>
struct ArrayView {
	const T* data = nullptr;
	std::uint32_t count = 0;
};

// One read operation, used by the .xmb round-trip writer. Inline items keep their body and
// the entry index they got in the reader's reference table; offset items keep index/sub.
struct RecordedItem {
	std::uint8_t type = 0; // original type tag (6 string, 7 asset, 8 array, 9 offset, 10 raw)
	bool existing = false;
	std::uint32_t count = 0;
	std::uint32_t elementSize = 1;
	std::vector<std::uint8_t> body;
	std::uint32_t refIndex = 0;
	std::uint32_t refSub = 0;
	std::string text;
	std::uint32_t entryIndex = 0xFFFFFFFFu;
};

// Reader for the tagged stream written by x64-zt's assetmanager (see
// .ref/x64-zt/src/zonetool/zonetool/utils/io/assetmanager.hpp). Every dumped item is
// prefixed with a type byte (and an "existing" byte for container types). Offsets
// reference earlier items by their read order; the reader keeps that table so shared
// arrays, strings and asset names resolve correctly.
class AssetStream {
public:
	explicit AssetStream(std::vector<std::uint8_t> data);
	AssetStream() = default;

	static AssetStream FromFile(const std::filesystem::path& path);

	std::size_t Position() const { return position_; }
	std::size_t Size() const { return data_.size(); }
	bool AtEnd() const { return position_ >= data_.size(); }
	const std::vector<std::uint8_t>& Data() const { return data_; }

	template <typename T>
	T ReadStruct() {
		static_assert(std::is_trivially_copyable_v<T>, "struct read must be trivially copyable");
		if (position_ + sizeof(T) > data_.size()) {
			throw AssetError("unexpected end of file while reading a struct");
		}
		T value{};
		std::memcpy(&value, data_.data() + position_, sizeof(T));
		position_ += sizeof(T);
		return value;
	}

	std::string ReadString();      // tagged string
	std::string ReadAssetName();   // tagged asset reference (name only)

	template <typename T>
	std::vector<T> ReadArray() {
		const ArrayView<T> view = ReadArrayView<T>();
		if (view.count == 0) {
			return {};
		}
		return std::vector<T>(view.data, view.data + view.count);
	}

	template <typename T>
	std::vector<T> ReadSingle() {
		return ReadArray<T>();
	}

	template <typename T>
	ArrayView<T> ReadArrayView();

	template <typename T>
	ArrayView<T> ReadArrayViewAs() {
		return ReadArrayView<T>();
	}

	template <typename T>
	void SkipArray() {
		(void)ReadArrayView<T>();
	}

	std::vector<std::uint8_t> ReadRawBytes();

	void SetRecorder(std::vector<RecordedItem>* recorder) { recorder_ = recorder; }

	template <typename T>
	void SkipRaw() {
		ReadRawView<T>();
	}

	template <typename T>
	ArrayView<T> ReadRawView();

	template <typename T>
	void RequireSize(std::uint32_t count, std::uint32_t expected, const char* what) {
		if (count != expected) {
			throw AssetError(std::string(what) + ": expected " + std::to_string(expected) + " elements, got " +
			                 std::to_string(count));
		}
	}

private:
	enum class EntryKind { Array, Raw, String, Asset };

	struct Entry {
		EntryKind kind = EntryKind::Array;
		std::size_t offset = 0;
		std::uint32_t count = 0;
		std::uint32_t elementSize = 1;
		std::string text;
	};

	std::uint8_t ReadType();
	std::string ReadCString();
	void EnsureAvailable(std::size_t bytes, const char* what) const;
	std::uint32_t ReadU32();
	void Record(RecordedItem item);

	std::vector<std::uint8_t> data_;
	std::size_t position_ = 0;
	std::vector<Entry> entries_;
	std::vector<RecordedItem>* recorder_ = nullptr;
};

} // namespace wb::assets
