#pragma once

#include <filesystem>

namespace wb {

struct Settings;

// `hmw-workbench.exe -mcp`: stdio MCP server over the shared command layer.
int RunMcp(const std::filesystem::path& exeDir, Settings& settings);

} // namespace wb
