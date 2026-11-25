#pragma once

#include <array>
#include <filesystem>
#include <memory>
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
	void LoadCharacter(const std::string& faction, const std::string& weapon);
	void LoadCompareLive(const std::string& name);

private:
	void DrawMenuBar();
	void DrawAssetsPanel();
	void DrawViewportPanel();
	void DrawInspectorPanel();
	void DrawTimelinePanel();
	void DrawModelInspector();
	void DrawWeaponInspector();
	void DrawCamoSection();
	void DrawCharacterSection();
	void DrawCompareSection();
	void DrawRigTools();
	void DrawTagEditor();
	void DrawGizmo(const ImVec2& origin, const ImVec2& size, float aspect);
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
	void LoadCompareAsset(const std::string& name);
	void SyncCompareState();

	const AssetEntry* SelectedEntry() const;

	Settings& settings_;
	AssetIndex& assets_;
	Renderer& renderer_;
	commands::Workbench& workbench_;
	std::filesystem::path exeDir_;

	bool wantsClose_ = false;
	bool layoutNeeded_ = false;
	int lastAssetCount_ = -1;

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
	char attachmentSearch_[96] = {};
	char camoSearch_[64] = {};
	char characterWeapon_[64] = {};
	char renameMapPath_[256] = {};
	std::string characterFaction_;
	std::string characterError_;
	std::string rigCompareStatus_;
	std::string selectedTag_;
	int gizmoMode_ = 0; // 0 = translate, 1 = rotate
	bool showGizmo_ = true;
	bool showGhost_ = true;
	std::string tagSaveStatus_;
	char newTagName_[64] = "tag_";
	char newTagCopy_[64] = "tag_red_dot";
	std::string newTagParent_;
	float newTagPos_[3] = {0.0f, 0.0f, 0.0f};
	float newTagRot_[4] = {0.0f, 0.0f, 0.0f, 1.0f};

	std::unique_ptr<commands::Workbench> compareWorkbench_;
	bool compareLive_ = false;
	bool compareLoaded_ = false;
	char compareAsset_[128] = {};
	std::string compareStatus_;
};

} // namespace wb
