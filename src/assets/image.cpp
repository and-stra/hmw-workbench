#include "assets/image.h"

#include "assets/asset_stream.h"
#include "assets/h1_structs.h"

#include <algorithm>
#include <fstream>

namespace wb::assets {
namespace {

bool IsBlockCompressed(std::int32_t format) {
	switch (format) {
	case 70: // BC1_TYPELESS
	case 71: // BC1_UNORM
	case 72: // BC1_UNORM_SRGB
	case 73: // BC2_TYPELESS
	case 74: // BC2_UNORM
	case 75: // BC2_UNORM_SRGB
	case 76: // BC3_TYPELESS
	case 77: // BC3_UNORM
	case 78: // BC3_UNORM_SRGB
	case 79: // BC4_TYPELESS
	case 80: // BC4_UNORM
	case 81: // BC4_SNORM
	case 82: // BC5_TYPELESS
	case 83: // BC5_UNORM
	case 84: // BC5_SNORM
	case 94: // BC6H_TYPELESS
	case 95: // BC6H_UF16
	case 96: // BC6H_SF16
	case 97: // BC7_TYPELESS
	case 98: // BC7_UNORM
	case 99: // BC7_UNORM_SRGB
		return true;
	default:
		return false;
	}
}

std::uint32_t BlockBytes(std::int32_t format) {
	switch (format) {
	case 70:
	case 71:
	case 72:
	case 79:
	case 80:
	case 81:
		return 8;
	default:
		return 16;
	}
}

std::uint32_t BitsPerPixel(std::int32_t format) {
	switch (format) {
	case 28: // R8G8B8A8_UNORM
	case 29: // R8G8B8A8_UNORM_SRGB
	case 87: // B8G8R8A8_UNORM
	case 88: // B8G8R8X8_UNORM
	case 90: // B8G8R8A8_TYPELESS
	case 91: // B8G8R8A8_UNORM_SRGB
	case 93: // B8G8R8X8_UNORM_SRGB
	case 10: // R16G16B16A16_FLOAT
	case 11: // R16G16B16A16_UNORM
	case 12: // R16G16B16A16_SNORM
		return format >= 10 && format <= 12 ? 64 : 32;
	case 61: // R8_UNORM
		return 8;
	case 49: // R8G8_UNORM
		return 16;
	case 54: // R16_FLOAT
		return 16;
	case 34: // R16G16_TYPELESS (the env BRDF LUT; treated as R16G16_FLOAT)
	case 56: // R16G16_FLOAT
		return 32;
	case 41: // R32_FLOAT
		return 32;
	case 6: // R32G32B32_FLOAT
		return 96;
	case 2: // R32G32B32A32_FLOAT
		return 128;
	case 85: // B5G6R5_UNORM
	case 86: // B5G6R5_UNORM? (unused)
		return 16;
	case 115: // B8G8R8A8_UNORM_SRGB (older alias)
		return 32;
	default:
		return 0;
	}
}

std::filesystem::path CleanName(const std::string& name) {
	std::string clean = name;
	for (char& c : clean) {
		if (c == '*') {
			c = '_';
		}
	}
	return std::filesystem::path(clean);
}

std::uint64_t FileSize(const std::filesystem::path& path) {
	std::error_code ec;
	const std::uintmax_t size = std::filesystem::file_size(path, ec);
	return ec ? 0 : static_cast<std::uint64_t>(size);
}

bool ReadFilePrefix(const std::filesystem::path& path, std::uint64_t bytes, std::vector<std::uint8_t>& out) {
	if (bytes == 0 || bytes > 512ull * 1024ull * 1024ull) {
		return false;
	}
	std::ifstream file(path, std::ios::binary);
	if (!file) {
		return false;
	}
	out.resize(static_cast<std::size_t>(bytes));
	if (!file.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(bytes))) {
		out.clear();
		return false;
	}
	return true;
}

std::optional<std::filesystem::path> FindStreamFile(const std::filesystem::path& imagePath,
                                                    const std::string& name, int stream) {
	const std::filesystem::path clean = CleanName(name);
	const std::string suffix = "_stream" + std::to_string(stream) + ".pixels";
	std::vector<std::filesystem::path> candidates;
	candidates.push_back(imagePath.parent_path() / (clean.string() + suffix));
	candidates.push_back(imagePath.parent_path().parent_path() / "streamed_images" / (clean.string() + suffix));
	for (const auto& candidate : candidates) {
		std::error_code ec;
		if (std::filesystem::is_regular_file(candidate, ec) && !ec) {
			return candidate;
		}
	}
	return std::nullopt;
}

} // namespace

std::uint64_t ImageMipSize(std::int32_t format, std::uint32_t width, std::uint32_t height) {
	if (width == 0 || height == 0) {
		return 0;
	}
	if (IsBlockCompressed(format)) {
		const std::uint64_t blocksX = (width + 3) / 4;
		const std::uint64_t blocksY = (height + 3) / 4;
		return blocksX * blocksY * BlockBytes(format);
	}
	const std::uint32_t bits = BitsPerPixel(format);
	if (bits == 0) {
		return 0;
	}
	return static_cast<std::uint64_t>(width) * height * bits / 8;
}

std::optional<std::filesystem::path> FindImageFile(const std::vector<std::filesystem::path>& roots,
                                                   const std::string& name) {
	const std::string clean = CleanName(name).string();
	for (const auto& root : roots) {
		const std::filesystem::path inImages = root / "images" / (clean + ".h1Image");
		std::error_code ec;
		if (std::filesystem::is_regular_file(inImages, ec) && !ec) {
			return inImages;
		}
		const std::filesystem::path inStreamed = root / "streamed_images" / (clean + ".h1Image");
		if (std::filesystem::is_regular_file(inStreamed, ec) && !ec) {
			return inStreamed;
		}
	}
	return std::nullopt;
}

ImageData LoadImage(const std::filesystem::path& path) {
	ImageData image;
	image.path = path;

	AssetStream stream = AssetStream::FromFile(path);
	const std::vector<h1::GfxImageRaw> headers = stream.ReadSingle<h1::GfxImageRaw>();
	if (headers.empty()) {
		throw AssetError("missing GfxImage struct in " + path.string());
	}
	const h1::GfxImageRaw& raw = headers.front();
	image.name = stream.ReadString();
	if (image.name.empty()) {
		image.name = path.stem().string();
	}
	image.format = raw.imageFormat;
	image.streamed = raw.streamed != 0;
	image.mipLevels = raw.levelCount;

	std::vector<std::uint8_t> embeddedPixels;
	if (raw.pixelData != 0) {
		embeddedPixels = stream.ReadArray<std::uint8_t>();
	}

	if (image.streamed) {
		for (int streamIndex = 3; streamIndex >= 0; --streamIndex) {
			const h1::GfxImageStreamData& streamInfo = raw.streams[streamIndex];
			if (streamInfo.width == 0 || streamInfo.height == 0) {
				continue;
			}
			const std::optional<std::filesystem::path> pixelsPath = FindStreamFile(path, image.name, streamIndex);
			if (!pixelsPath.has_value()) {
				continue;
			}
			const std::uint64_t expected = ImageMipSize(image.format, streamInfo.width, streamInfo.height);
			if (expected == 0) {
				image.note = "unsupported image format " + std::to_string(image.format);
				break;
			}
			if (FileSize(*pixelsPath) < expected) {
				image.note = "stream " + std::to_string(streamIndex) + " is truncated";
				continue;
			}
			if (!ReadFilePrefix(*pixelsPath, expected, image.pixels)) {
				image.note = "cannot read " + pixelsPath->string();
				continue;
			}
			image.width = streamInfo.width;
			image.height = streamInfo.height;
			image.available = true;
			image.source = "stream" + std::to_string(streamIndex) + " " + std::to_string(streamInfo.width) + "x" +
			                std::to_string(streamInfo.height);
			break;
		}

		if (!image.available && !embeddedPixels.empty()) {
			const std::uint64_t expected = ImageMipSize(image.format, raw.width, raw.height);
			if (expected != 0 && embeddedPixels.size() >= expected) {
				image.width = raw.width;
				image.height = raw.height;
				image.pixels.assign(embeddedPixels.begin(), embeddedPixels.begin() + static_cast<std::ptrdiff_t>(expected));
				image.available = true;
				image.source = "embedded mip0";
			}
		}
		if (!image.available && image.note.empty()) {
			image.note = "streamed image, no .pixels data in dump";
		}
	} else {
		const std::uint64_t expected = ImageMipSize(image.format, raw.width, raw.height);
		image.width = raw.width;
		image.height = raw.height;
		if (expected == 0) {
			image.note = "unsupported image format " + std::to_string(image.format);
		} else if (raw.pixelData == 0 || embeddedPixels.empty()) {
			image.note = "no pixel data";
		} else if (embeddedPixels.size() < expected) {
			image.note = "pixel data is truncated";
		} else {
			image.pixels.assign(embeddedPixels.begin(), embeddedPixels.begin() + static_cast<std::ptrdiff_t>(expected));
			image.available = true;
			image.source = "image mip0";
		}
	}

	return image;
}

} // namespace wb::assets
