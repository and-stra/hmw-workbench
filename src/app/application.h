#pragma once

#include <array>
#include <filesystem>
#include <string>
#include <vector>

#include <imgui.h>

#include "app/commands.h"
#include "app/settings.h"
#include "assets/asset_index.h"
#include "render/camera.h"
#include "render/renderer.h"

namespace wb {

class Application {
public:
	Application(Settings& settings, AssetIndex& assets, Renderer& renderer, commands::Workbench& workbench,
	            std::filesystem::path exeDir, bool restoreLayout);

	void Frame();
	bool WantsClose() const { return wantsClose_; }
	void LoadModelByName(const std::string& name);
	void LoadWeapon(const std::string& name, const std::string& hands, bool firstPerson);

private:
	void DrawMenuBar();
	void DrawAssetsPanel();
	void DrawViewportPanel();
	void DrawInspectorPanel();
	void DrawTimelinePanel();
	void DrawModelInspector();
	void DrawWeaponInspector();
	void DrawAnimInspector();
	void BuildLayout(ImGuiID dockspaceId, const ImVec2& size);
	void HandleViewportInput(const ImVec2& viewportSize);
	void RebuildFilter();
	void RescanAssets();
	void FrameCamera();
	void LoadSelectedModel();
	void ReloadModel();
	void LoadSelectedWeapon();
	void ReloadWeapon();
	void RefreshHandsOptions();
	void DrawPlaybackControls(float width);

	const AssetEntry* SelectedEntry() const;

	Settings& settings_;
	AssetIndex& assets_;
	Renderer& renderer_;
	commands::Workbench& workbench_;
	std::filesystem::path exeDir_;

	bool wantsClose_ = false;
	bool layoutNeeded_ = false;

	int selected_ = -1;
	char search_[128] = {};
	char animSearch_[96] = {};
	std::string lastFilter_;
	std::array<std::vector<int>, kAssetKindCount> filtered_;

	bool showBones_ = false;	bool showAllBoneNames_ = true;
	int selectedBone_ = -1;

	std::string handsSelection_;
	std::vector<std::string> handsOptions_;
	bool firstPersonRequested_ = false;
};

} // namespace wb
