#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace wb {

struct CameraSettings {
	float yaw = 45.0f;
	float pitch = 25.0f;
	float distance = 20.0f;
	float fov = 60.0f;
};

struct Settings {
	std::vector<std::string> assetRoots;
	int windowWidth = 1600;
	int windowHeight = 900;
	std::string lastWeapon;
	CameraSettings camera;
	float exposure = 1.0f;
	// H1 cg_fov is the horizontal FOV for a 4:3 view; cg_fovScale scales the viewmodel.
	float firstPersonFov = 65.0f;
	float viewmodelFovScale = 1.0f;
};

// Defaults used when workbench.json does not exist yet.
Settings DefaultSettings();

// Reads <exeDir>\workbench.json. Creates the file with defaults when it is missing.
// On parse errors the defaults are used and a message is printed to stderr.
Settings LoadSettings(const std::filesystem::path& exeDir);

// Writes <exeDir>\workbench.json.
void SaveSettings(const std::filesystem::path& exeDir, const Settings& settings);

// Expands the configured roots: relative paths are resolved against exeDir, a trailing
// "*" component expands to every subdirectory (sorted). Duplicates are removed, the
// first occurrence keeps its order. Non-existing paths are returned as well, so the UI
// can report them.
std::vector<std::filesystem::path> ResolveAssetRoots(const Settings& settings,
                                                     const std::filesystem::path& exeDir);

} // namespace wb
