#pragma once

#include <array>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <imgui.h>

#include "app/settings.h"
#include "assets/asset_index.h"
#include "assets/model_asset.h"
#include "render/camera.h"
#include "render/renderer.h"
#include "scene/viewmodel.h"

namespace wb {

class Application {
public:
	Application(Settings& settings, AssetIndex& assets, Renderer& renderer, std::filesystem::path exeDir,
	            bool restoreLayout);

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
	void BuildLayout(ImGuiID dockspaceId, const ImVec2& size);
	void HandleViewportInput(const ImVec2& viewportSize);
	void RebuildFilter();
	void RescanAssets();
	void FrameCamera();
	void LoadSelectedModel();
	void ReloadModel();
	void LoadWeaponByName(const std::string& name);
	void LoadSelectedWeapon();
	void ReloadWeapon();
	void RefreshHandsOptions();
	void UpdateViewCamera();
	void DrawWeaponInspector();

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

	assets::ModelAsset model_;
	std::unique_ptr<GpuModel> gpuModel_;
	std::string modelError_;
	std::string loadedModelName_;
	std::string loadedModelPath_;
	bool modelVisible_ = true;
	bool showBones_ = false;
	bool showAllBoneNames_ = true;
	bool wireframe_ = false;
	int selectedBone_ = -1;
	std::vector<bool> surfaceVisible_;

	scene::ViewmodelAsset viewmodel_;
	std::unique_ptr<GpuModel> gpuHands_;
	std::unique_ptr<GpuModel> gpuGun_;
	std::string loadedWeaponName_;
	std::string weaponError_;
	std::string handsOverride_;
	std::vector<std::string> handsOptions_;
	bool weaponFirstPerson_ = false;
	std::vector<bool> gunSurfaceVisible_;
	std::vector<bool> handsSurfaceVisible_;
};

} // namespace wb
