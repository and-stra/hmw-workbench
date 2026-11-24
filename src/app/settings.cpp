#include "app/settings.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <system_error>

#include <nlohmann/json.hpp>

namespace wb {
namespace {

constexpr const char* kGameDir =
    "C:\\Games\\cod-hwm\\Call of Duty Horizon Modern Warfare\\Call of Duty Modern Warfare Remastered";

std::string ToLower(std::string text) {
	std::transform(text.begin(), text.end(), text.begin(),
	               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return text;
}

void AddUniqueRoot(std::vector<std::filesystem::path>& roots, std::vector<std::string>& seen,
                   const std::filesystem::path& path) {
	std::error_code ec;
	std::filesystem::path canonical = std::filesystem::weakly_canonical(path, ec);
	if (ec) {
		canonical = path.lexically_normal();
		ec.clear();
	}
	const std::string key = ToLower(canonical.string());
	if (std::find(seen.begin(), seen.end(), key) != seen.end()) {
		return;
	}
	seen.push_back(key);
	roots.push_back(canonical);
}

int ClampInt(int value, int low, int high) {
	return std::max(low, std::min(value, high));
}

float ClampFloat(float value, float low, float high) {
	return std::max(low, std::min(value, high));
}

} // namespace

Settings DefaultSettings() {
	Settings settings;
	settings.assetRoots = {
	    std::string(kGameDir) + "\\zonetool\\mod",
	    "C:\\Games\\cod-hwm\\refs\\dumps\\hmw_patch_common_mp",
	    "C:\\Games\\cod-hwm\\refs\\dumps\\patch_common_mp",
	    "C:\\Games\\cod-hwm\\refs\\dumps\\*",
	    std::string(kGameDir) + "\\dump\\*",
	};
	return settings;
}

Settings LoadSettings(const std::filesystem::path& exeDir) {
	Settings settings = DefaultSettings();
	const std::filesystem::path path = exeDir / "workbench.json";

	if (!std::filesystem::exists(path)) {
		SaveSettings(exeDir, settings);
		return settings;
	}

	try {
		std::ifstream in(path);
		if (!in) {
			std::fprintf(stderr, "workbench.json: cannot open %s, using defaults\n", path.string().c_str());
			return settings;
		}

		nlohmann::json json;
		in >> json;

		if (json.contains("assetRoots") && json["assetRoots"].is_array()) {
			std::vector<std::string> roots;
			for (const auto& value : json["assetRoots"]) {
				if (value.is_string()) {
					roots.push_back(value.get<std::string>());
				}
			}
			if (!roots.empty()) {
				settings.assetRoots = roots;
			}
		}

		if (json.contains("window") && json["window"].is_object()) {
			const auto& window = json["window"];
			if (window.contains("width") && window["width"].is_number_integer()) {
				settings.windowWidth = window["width"].get<int>();
			}
			if (window.contains("height") && window["height"].is_number_integer()) {
				settings.windowHeight = window["height"].get<int>();
			}
		}

		if (json.contains("lastWeapon") && json["lastWeapon"].is_string()) {
			settings.lastWeapon = json["lastWeapon"].get<std::string>();
		}

		if (json.contains("camera") && json["camera"].is_object()) {
			const auto& camera = json["camera"];
			if (camera.contains("yaw") && camera["yaw"].is_number()) {
				settings.camera.yaw = camera["yaw"].get<float>();
			}
			if (camera.contains("pitch") && camera["pitch"].is_number()) {
				settings.camera.pitch = camera["pitch"].get<float>();
			}
			if (camera.contains("distance") && camera["distance"].is_number()) {
				settings.camera.distance = camera["distance"].get<float>();
			}
			if (camera.contains("fov") && camera["fov"].is_number()) {
				settings.camera.fov = camera["fov"].get<float>();
			}
		}
	} catch (const std::exception& e) {
		std::fprintf(stderr, "workbench.json: %s, using defaults\n", e.what());
		return DefaultSettings();
	}

	settings.windowWidth = ClampInt(settings.windowWidth, 640, 7680);
	settings.windowHeight = ClampInt(settings.windowHeight, 480, 4320);
	settings.camera.pitch = ClampFloat(settings.camera.pitch, -89.0f, 89.0f);
	settings.camera.distance = ClampFloat(settings.camera.distance, 0.1f, 1000.0f);
	settings.camera.fov = ClampFloat(settings.camera.fov, 10.0f, 140.0f);
	return settings;
}

void SaveSettings(const std::filesystem::path& exeDir, const Settings& settings) {
	nlohmann::json json;
	json["assetRoots"] = settings.assetRoots;
	json["window"] = {{"width", settings.windowWidth}, {"height", settings.windowHeight}};
	json["lastWeapon"] = settings.lastWeapon;
	json["camera"] = {
	    {"yaw", settings.camera.yaw},
	    {"pitch", settings.camera.pitch},
	    {"distance", settings.camera.distance},
	    {"fov", settings.camera.fov},
	};

	std::error_code ec;
	std::filesystem::create_directories(exeDir, ec);
	std::ofstream out(exeDir / "workbench.json");
	if (!out) {
		std::fprintf(stderr, "workbench.json: cannot write %s\n", (exeDir / "workbench.json").string().c_str());
		return;
	}
	out << json.dump(2) << '\n';
}

std::vector<std::filesystem::path> ResolveAssetRoots(const Settings& settings,
                                                     const std::filesystem::path& exeDir) {
	std::vector<std::filesystem::path> roots;
	std::vector<std::string> seen;

	for (const std::string& raw : settings.assetRoots) {
		if (raw.empty()) {
			continue;
		}

		std::filesystem::path path(raw);
		if (path.is_relative()) {
			path = exeDir / path;
		}

		if (path.filename() == "*") {
			const std::filesystem::path parent = path.parent_path();
			std::vector<std::filesystem::path> children;
			std::error_code ec;
			for (std::filesystem::directory_iterator it(parent, ec), end; !ec && it != end; it.increment(ec)) {
				std::error_code dirEc;
				if (it->is_directory(dirEc)) {
					children.push_back(it->path());
				}
			}
			std::sort(children.begin(), children.end(),
			          [](const std::filesystem::path& a, const std::filesystem::path& b) {
				          return a.string() < b.string();
			          });
			for (const auto& child : children) {
				AddUniqueRoot(roots, seen, child);
			}
		} else {
			AddUniqueRoot(roots, seen, path);
		}
	}

	return roots;
}

} // namespace wb
