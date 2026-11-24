#pragma once

#include <array>
#include <filesystem>
#include <string>
#include <vector>

#include <imgui.h>

#include "app/settings.h"
#include "assets/asset_index.h"
#include "render/camera.h"
#include "render/renderer.h"

namespace wb {

class Application {
public:
	Application(Settings& settings, AssetIndex& assets, Renderer& renderer, std::filesystem::path exeDir,
	            bool restoreLayout);

	void Frame();
	bool WantsClose() const { return wantsClose_; }

private:
	void DrawMenuBar();
	void DrawAssetsPanel();
	void DrawViewportPanel();
	void DrawInspectorPanel();
	void DrawTimelinePanel();
	void BuildLayout(ImGuiID dockspaceId, const ImVec2& size);
	void HandleViewportInput(const ImVec2& viewportSize);
	void RebuildFilter();
	void RescanAssets();
	void FrameCamera();

	const AssetEntry* SelectedEntry() const;

	Settings& settings_;
	AssetIndex& assets_;
	Renderer& renderer_;
	std::filesystem::path exeDir_;
	Camera camera_;

	bool wantsClose_ = false;
	bool layoutNeeded_ = false;

	int selected_ = -1;
	char search_[128] = {};
	std::string lastFilter_;
	std::array<std::vector<int>, kAssetKindCount> filtered_;
};

} // namespace wb
