#include "app/commands.h"

#include <algorithm>
#include <cstring>
#include <vector>

#include <DirectXTex.h>

#include "assets/model_asset.h"

namespace wb::commands {
namespace {

struct CapturedShot {
	std::vector<std::uint8_t> pixels;
	int width = 0;
	int height = 0;
	DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
};

bool LoadCompareAsset(Workbench& workbench, const std::string& name, const CompareOptions& options,
                      std::string& error) {
	if (assets::FindWeaponFile(workbench.Roots(), name).has_value()) {
		if (!workbench.LoadWeapon(name, "", options.firstPerson, error, options.world)) {
			return false;
		}
	} else if (!workbench.LoadModel(name, error)) {
		return false;
	}
	if (!options.camo.empty()) {
		std::string camoError;
		if (!workbench.SetCamo(options.camo, camoError)) {
			error = "camo: " + camoError;
			return false;
		}
	}
	workbench.SetWireframe(false);
	return true;
}

void ApplyCompareCamera(Workbench& workbench, const CompareOptions& options) {
	if (!options.hasView) {
		workbench.FrameCamera();
		return;
	}
	if (options.firstPerson) {
		workbench.SetFirstPersonView(options.yaw, options.pitch, options.fov);
	} else {
		workbench.SetOrbitView(options.yaw, options.pitch, options.distance, options.fov);
	}
}

bool Capture(Workbench& workbench, Renderer& renderer, const CompareOptions& options, CapturedShot& shot,
             std::string& error) {
	if (!workbench.RenderScene(options.width, options.height)) {
		error = "compare: render failed";
		return false;
	}
	if (!renderer.CaptureScene(shot.pixels, shot.width, shot.height, shot.format)) {
		error = "compare: capture failed";
		return false;
	}
	return true;
}

} // namespace

bool RenderComparison(Workbench& workbench, Renderer& renderer, const std::string& a, const std::string& b,
                      const CompareOptions& options, const std::filesystem::path& output,
                      std::string& error) {
	CapturedShot shotA;
	CapturedShot shotB;
	if (!LoadCompareAsset(workbench, a, options, error)) {
		error = "compare " + a + ": " + error;
		return false;
	}
	ApplyCompareCamera(workbench, options);
	DirectX::XMFLOAT3 targetA = workbench.GetCamera().Target();
	if (!Capture(workbench, renderer, options, shotA, error)) {
		return false;
	}

	if (!LoadCompareAsset(workbench, b, options, error)) {
		error = "compare " + b + ": " + error;
		return false;
	}
	if (options.mode == CompareMode::Ghost && !options.firstPerson) {
		// Same orbit target so the two silhouettes line up; otherwise each model is framed
		// around its own bounds.
		workbench.GetCamera().SetTarget(targetA);
	}
	ApplyCompareCamera(workbench, options);
	if (!Capture(workbench, renderer, options, shotB, error)) {
		return false;
	}

	if (shotA.format != shotB.format || shotA.width != shotB.width || shotA.height != shotB.height) {
		error = "compare: the two renders have different sizes/formats";
		return false;
	}
	const UINT bpp = static_cast<UINT>(DirectX::BitsPerPixel(shotA.format) / 8);
	if (bpp != 4) {
		error = "compare: unsupported capture format";
		return false;
	}

	const int width = shotA.width;
	const int height = shotA.height;
	std::vector<std::uint8_t> pixels;
	int outWidth = width;
	int outHeight = height;
	if (options.mode == CompareMode::SideBySide) {
		const int divider = 4;
		outWidth = width * 2 + divider;
		outHeight = height;
		pixels.assign(static_cast<std::size_t>(outWidth) * outHeight * bpp, 255);
		const std::size_t sourceRow = static_cast<std::size_t>(width) * bpp;
		const std::size_t destRow = static_cast<std::size_t>(outWidth) * bpp;
		for (int y = 0; y < height; ++y) {
			std::memcpy(pixels.data() + static_cast<std::size_t>(y) * destRow,
			            shotA.pixels.data() + static_cast<std::size_t>(y) * sourceRow, sourceRow);
			std::memcpy(pixels.data() + static_cast<std::size_t>(y) * destRow +
			                static_cast<std::size_t>(width + divider) * bpp,
			            shotB.pixels.data() + static_cast<std::size_t>(y) * sourceRow, sourceRow);
		}
		for (int d = 0; d < 4; ++d) {
			for (int y = 0; y < height; ++y) {
				std::uint8_t* pixel = pixels.data() + static_cast<std::size_t>(y) * destRow +
				                      static_cast<std::size_t>(width + d) * bpp;
				pixel[0] = 245;
				pixel[1] = 245;
				pixel[2] = 245;
				pixel[3] = 255;
			}
		}
	} else {
		pixels = shotA.pixels;
		const float alpha = std::clamp(options.ghostAlpha, 0.05f, 0.95f);
		for (std::size_t i = 0; i + 3 < pixels.size(); i += bpp) {
			for (int c = 0; c < 3; ++c) {
				pixels[i + c] = static_cast<std::uint8_t>(
				    static_cast<float>(shotB.pixels[i + c]) * alpha +
				    static_cast<float>(pixels[i + c]) * (1.0f - alpha));
			}
		}
	}

	std::error_code ec;
	if (!output.parent_path().empty()) {
		std::filesystem::create_directories(output.parent_path(), ec);
	}
	if (!renderer.SavePixelsToPng(output, pixels, outWidth, outHeight, shotA.format)) {
		error = "compare: failed to save " + output.string();
		return false;
	}
	return true;
}

} // namespace wb::commands
