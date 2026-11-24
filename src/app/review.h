#pragma once

#include <filesystem>
#include <string>

namespace wb {

class Renderer;

namespace commands {
class Workbench;
}

// Offscreen review render shared by `-screenshot` and the MCP `screenshot` tool.
struct ShotOptions {
	int width = 1600;
	int height = 900;
	bool bones = false;
	bool wire = false;
	bool crosshair = false;
	bool markTags = false;
	std::string caption;
	int sheet = 0; // > 0: contact sheet of the current animation with this many cells
};

bool RenderShot(commands::Workbench& workbench, Renderer& renderer, const ShotOptions& options,
                const std::filesystem::path& output, std::string& error);

} // namespace wb
