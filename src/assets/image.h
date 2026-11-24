#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace wb::assets {

struct ImageData {
	std::string name;
	std::filesystem::path path;
	std::int32_t format = 0;
	std::uint32_t width = 0;
	std::uint32_t height = 0;
	std::uint32_t mipLevels = 1;
	bool streamed = false;
	bool available = false;
	std::string source; // e.g. "stream3 2048x2048" or "image mip0"
	std::string note;   // why the data is missing/incomplete
	std::vector<std::uint8_t> pixels; // base mip only
};

// Reads a dumped .h1Image. For streamed images the highest available
// <name>_streamN.pixels next to it (or in streamed_images/) is used.
ImageData LoadImage(const std::filesystem::path& path);

// Finds images/<name>.h1Image or streamed_images/<name>.h1Image in the roots.
// "*" in image names is replaced by "_" like x64-zt does for file names.
std::optional<std::filesystem::path> FindImageFile(const std::vector<std::filesystem::path>& roots,
                                                   const std::string& name);

// Number of bytes of one mip level; 0 for unsupported formats.
std::uint64_t ImageMipSize(std::int32_t format, std::uint32_t width, std::uint32_t height);

} // namespace wb::assets
