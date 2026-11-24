#include "app/application.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include <imgui_internal.h>
#include <ImGuizmo.h>

#include "app/model_overlay.h"

namespace wb {
namespace {

constexpr const char* kAssetsWindow = "Assets";
constexpr const char* kViewportWindow = "Viewport";
constexpr const char* kInspectorWindow = "Inspector";
constexpr const char* kTimelineWindow = "Timeline";

std::string ToLower(std::string text) {
	std::transform(text.begin(), text.end(), text.begin(),
	               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return text;
}

bool MatchesFilter(const std::string& name, const std::string& needleLower) {
	return needleLower.empty() || ToLower(name).find(needleLower) != std::string::npos;
}

bool IsAdditiveName(const std::string& name) {
	const std::string lower = ToLower(name);
	return lower.rfind("additive_", 0) == 0 ||
	       (lower.size() >= 4 && lower.compare(lower.size() - 4, 4, "_add") == 0);
}

} // namespace

Application::Application(Settings& settings, AssetIndex& assets, Renderer& renderer,
                         commands::Workbench& workbench, std::filesystem::path exeDir, bool restoreLayout)
    : settings_(settings), assets_(assets), renderer_(renderer), workbench_(workbench),
      exeDir_(std::move(exeDir)), layoutNeeded_(!restoreLayout) {
	RebuildFilter();
	RefreshHandsOptions();
}

void Application::Frame() {
	workbench_.Update(ImGui::GetIO().DeltaTime);

	DrawMenuBar();

	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);
	ImGui::SetNextWindowViewport(viewport->ID);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

	const ImGuiWindowFlags hostFlags =
	    ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
	    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
	    ImGuiWindowFlags_NoNavFocus;

	ImGui::Begin("##DockHost", nullptr, hostFlags);
	ImGui::PopStyleVar(3);

	const ImGuiID dockspaceId = ImGui::GetID("MainDockSpace");
	if (layoutNeeded_) {
		BuildLayout(dockspaceId, ImGui::GetContentRegionAvail());
	}
	ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
	ImGui::End();

	DrawAssetsPanel();
	DrawViewportPanel();
	DrawInspectorPanel();
	DrawTimelinePanel();

	const Camera& camera = workbench_.GetCamera();
	settings_.camera.yaw = camera.Yaw();
	settings_.camera.pitch = camera.Pitch();
	settings_.camera.distance = camera.Distance();
	if (!camera.IsFirstPerson()) {
		settings_.camera.fov = camera.Fov();
	}
}

void Application::BuildLayout(ImGuiID dockspaceId, const ImVec2& size) {
	ImGui::DockBuilderRemoveNode(dockspaceId);
	ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
	ImGui::DockBuilderSetNodeSize(dockspaceId, size);

	ImGuiID center = dockspaceId;
	const ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.20f, nullptr, &center);
	const ImGuiID right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.30f, nullptr, &center);
	const ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.28f, nullptr, &center);

	ImGui::DockBuilderDockWindow(kAssetsWindow, left);
	ImGui::DockBuilderDockWindow(kInspectorWindow, right);
	ImGui::DockBuilderDockWindow(kTimelineWindow, bottom);
	ImGui::DockBuilderDockWindow(kViewportWindow, center);
	ImGui::DockBuilderFinish(dockspaceId);

	layoutNeeded_ = false;
}

void Application::DrawMenuBar() {
	if (!ImGui::BeginMainMenuBar()) {
		return;
	}

	if (ImGui::BeginMenu("File")) {
		if (ImGui::MenuItem("Rescan assets")) {
			RescanAssets();
		}
		if (ImGui::MenuItem("Reload model", nullptr, false, !workbench_.ModelName().empty())) {
			ReloadModel();
		}
		if (ImGui::MenuItem("Reload weapon", nullptr, false, !workbench_.WeaponName().empty())) {
			ReloadWeapon();
		}
		if (ImGui::MenuItem("Save settings now")) {
			SaveSettings(exeDir_, settings_);
		}
		ImGui::Separator();
		if (ImGui::MenuItem("Exit")) {
			wantsClose_ = true;
		}
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("View")) {
		if (ImGui::MenuItem("Reset layout")) {
			layoutNeeded_ = true;
		}
		if (ImGui::MenuItem("Reset camera")) {
			FrameCamera();
		}
		bool grid = workbench_.ShowGrid();
		if (ImGui::MenuItem("Grid", nullptr, &grid)) {
			workbench_.SetShowGrid(grid);
		}
		bool wire = workbench_.Wireframe();
		if (ImGui::MenuItem("Wireframe", nullptr, &wire)) {
			workbench_.SetWireframe(wire);
		}
		ImGui::EndMenu();
	}

	ImGui::EndMainMenuBar();
}

void Application::DrawAssetsPanel() {
	ImGui::Begin(kAssetsWindow);

	ImGui::SetNextItemWidth(-FLT_MIN);
	if (ImGui::InputTextWithHint("##asset_search", "Search weapons, models, anims...", search_,
	                             sizeof(search_))) {
		RebuildFilter();
	}

	const std::vector<AssetRoot>& roots = assets_.Roots();
	int existingRoots = 0;
	for (const AssetRoot& root : roots) {
		if (root.exists) {
			++existingRoots;
		}
	}

	ImGui::TextDisabled("%d roots (%d found)  |  %zu assets", static_cast<int>(roots.size()), existingRoots,
	                    assets_.Entries().size());
	if (ImGui::IsItemHovered()) {
		ImGui::BeginTooltip();
		for (size_t i = 0; i < roots.size(); ++i) {
			const AssetRoot& root = roots[i];
			if (root.exists) {
				ImGui::Text("%zu. %s", i + 1, root.path.string().c_str());
				ImGui::TextDisabled("    %d weapons, %d models, %d anims", root.weaponCount, root.modelCount,
				                    root.animCount);
			} else {
				ImGui::TextDisabled("%zu. %s (missing)", i + 1, root.path.string().c_str());
			}
		}
		ImGui::EndTooltip();
	}

	ImGui::SameLine();
	if (ImGui::SmallButton("Rescan")) {
		RescanAssets();
	}

	ImGui::Separator();

	ImGui::BeginChild("##asset_list", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
	const std::vector<AssetEntry>& entries = assets_.Entries();

	for (int kindIndex = 0; kindIndex < kAssetKindCount; ++kindIndex) {
		const AssetKind kind = static_cast<AssetKind>(kindIndex);
		const std::vector<int>& rows = filtered_[static_cast<size_t>(kindIndex)];

		char header[96];
		std::snprintf(header, sizeof(header), "%s (%zu)", AssetKindPlural(kind), rows.size());
		ImGui::SetNextItemOpen(true, ImGuiCond_Once);
		if (!ImGui::CollapsingHeader(header)) {
			continue;
		}

		ImGuiListClipper clipper;
		clipper.Begin(static_cast<int>(rows.size()));
		while (clipper.Step()) {
			for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
				const int entryIndex = rows[static_cast<size_t>(row)];
				const AssetEntry& entry = entries[static_cast<size_t>(entryIndex)];
				ImGui::PushID(entryIndex);
				const bool selected = entryIndex == selected_;
				if (ImGui::Selectable(entry.name.c_str(), selected)) {
					selected_ = entryIndex;
					if (entry.kind == AssetKind::Weapon) {
						settings_.lastWeapon = entry.name;
						LoadWeapon(entry.name, handsSelection_, firstPersonRequested_);
					} else if (entry.kind == AssetKind::Model) {
						LoadSelectedModel();
					} else if (entry.kind == AssetKind::Anim) {
						std::string error;
						workbench_.PlayAnim(entry.name, error);
					}
				}
				if (ImGui::IsItemHovered()) {
					const std::string& rootPath =
					    entry.rootIndex >= 0 && entry.rootIndex < static_cast<int>(roots.size())
					        ? roots[static_cast<size_t>(entry.rootIndex)].path.string()
					        : std::string("(unknown root)");
					ImGui::SetTooltip("%s\n%s", entry.path.string().c_str(), rootPath.c_str());
				}
				ImGui::PopID();
			}
		}
	}

	ImGui::EndChild();
	ImGui::End();
}

void Application::DrawViewportPanel() {
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::Begin(kViewportWindow, nullptr,
	             ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

	const ImVec2 available = ImGui::GetContentRegionAvail();
	if (available.x < 32.0f || available.y < 32.0f) {
		ImGui::TextDisabled("Viewport is too small.");
		ImGui::End();
		ImGui::PopStyleVar();
		return;
	}

	const ImVec2 framebufferScale = ImGui::GetIO().DisplayFramebufferScale;
	const int pixelWidth = std::max(1, static_cast<int>(available.x * framebufferScale.x));
	const int pixelHeight = std::max(1, static_cast<int>(available.y * framebufferScale.y));
	const float aspect = static_cast<float>(pixelWidth) / static_cast<float>(pixelHeight);

	if (!workbench_.RenderScene(pixelWidth, pixelHeight)) {
		ImGui::TextDisabled("Rendering failed.");
		ImGui::End();
		ImGui::PopStyleVar();
		return;
	}

	const Camera& camera = workbench_.GetCamera();
	const ImVec2 imageOrigin = ImGui::GetCursorScreenPos();
	ImGui::Image(static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(renderer_.SceneTexture())), available);
	ImGui::SetCursorScreenPos(imageOrigin);
	ImGui::InvisibleButton("##viewport_input", available,
	                       ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight |
	                           ImGuiButtonFlags_MouseButtonMiddle);

	HandleViewportInput(available);

	if (workbench_.HasEditableModel() && !selectedTag_.empty() && showGizmo_) {
		DrawGizmo(imageOrigin, available, aspect);
	}

	const assets::ModelAsset* standalone = workbench_.Model();
	if (showBones_ && standalone != nullptr && !standalone->model.bones.empty()) {
		DirectX::XMFLOAT4X4 viewProjection = {};
		DirectX::XMStoreFloat4x4(&viewProjection, camera.ViewProjection(aspect));
		DrawModelOverlay(ImGui::GetWindowDrawList(), imageOrigin, available, viewProjection, standalone->model,
		                 selectedBone_, showAllBoneNames_);
	}

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	char line[256];
	std::snprintf(line, sizeof(line), "%s  yaw %.1f  pitch %.1f  dist %.2f  fov %.1f",
	              workbench_.IsFirstPerson() ? "first person" : "orbit", camera.Yaw(), camera.Pitch(),
	              camera.Distance(), camera.Fov());
	const ImU32 textColor = IM_COL32(210, 214, 222, 220);
	drawList->AddText(ImVec2(imageOrigin.x + 10.0f, imageOrigin.y + 8.0f), textColor, line);
	if (!workbench_.AnimName().empty()) {
		char animLine[256];
		std::snprintf(animLine, sizeof(animLine), "anim %s  frame %.2f / %u  time %.3f s",
		              workbench_.AnimName().c_str(), workbench_.AnimFrame(),
		              workbench_.Anim() != nullptr ? workbench_.Anim()->frameCount : 0u,
		              workbench_.AnimTime());
		drawList->AddText(ImVec2(imageOrigin.x + 10.0f, imageOrigin.y + 24.0f),
		                  IM_COL32(180, 205, 240, 220), animLine);
	}
	drawList->AddText(ImVec2(imageOrigin.x + 10.0f, imageOrigin.y + 40.0f), IM_COL32(150, 155, 165, 200),
	                  "LMB orbit  |  RMB/MMB pan  |  wheel zoom  |  F frame camera");

	ImGui::End();
	ImGui::PopStyleVar();
}

void Application::HandleViewportInput(const ImVec2& viewportSize) {
	ImGuiIO& io = ImGui::GetIO();
	Camera& camera = workbench_.GetCamera();
	const bool active = ImGui::IsItemActive();
	const bool hovered = ImGui::IsItemHovered();
	if (ImGuizmo::IsUsing() || ImGuizmo::IsOver()) {
		return;
	}

	if (active) {
		const ImVec2 delta = io.MouseDelta;
		if (!camera.IsFirstPerson() && (delta.x != 0.0f || delta.y != 0.0f)) {
			if (io.MouseDown[ImGuiMouseButton_Left]) {
				camera.Orbit(delta.x, delta.y);
			} else if (io.MouseDown[ImGuiMouseButton_Right] || io.MouseDown[ImGuiMouseButton_Middle]) {
				camera.Pan(delta.x, delta.y, viewportSize.y);
			}
		}
	}

	if (hovered && io.MouseWheel != 0.0f && !camera.IsFirstPerson()) {
		camera.Zoom(io.MouseWheel);
	}

	if (hovered && ImGui::IsKeyPressed(ImGuiKey_F, false) && !camera.IsFirstPerson()) {
		FrameCamera();
	}
}

void Application::DrawInspectorPanel() {
	ImGui::Begin(kInspectorWindow);

	const AssetEntry* entry = SelectedEntry();
	if (entry != nullptr) {
		ImGui::Text("%s", entry->name.c_str());
		ImGui::TextDisabled("%s", AssetKindLabel(entry->kind));
		ImGui::SeparatorText("Source");
		ImGui::TextWrapped("%s", entry->path.string().c_str());
		if (entry->rootIndex >= 0 && entry->rootIndex < static_cast<int>(assets_.Roots().size())) {
			ImGui::TextDisabled("Root %d", entry->rootIndex + 1);
		}
	} else {
		ImGui::TextDisabled("No asset selected.");
		ImGui::TextWrapped("Pick an asset in the Assets panel.");
	}

	ImGui::SeparatorText("Weapon");
	DrawWeaponInspector();

	ImGui::SeparatorText("Model");
	DrawModelInspector();

	if (workbench_.GpuWeaponGun() != nullptr || workbench_.GpuStandaloneModel() != nullptr) {
		ImGui::SeparatorText("Camo");
		DrawCamoSection();
	}

	DrawTagEditor();

	ImGui::SeparatorText("Animation");
	DrawAnimInspector();

	ImGui::SeparatorText("Camera");
	Camera& camera = workbench_.GetCamera();
	ImGui::Text("yaw %.1f   pitch %.1f", camera.Yaw(), camera.Pitch());
	ImGui::Text("distance %.2f", camera.Distance());
	float fov = camera.Fov();
	if (!camera.IsFirstPerson() && ImGui::SliderFloat("FOV", &fov, 20.0f, 120.0f, "%.0f deg")) {
		camera.SetFov(fov);
	}
	float exposure = settings_.exposure;
	if (ImGui::SliderFloat("Exposure", &exposure, 0.1f, 4.0f, "%.2f")) {
		settings_.exposure = exposure;
	}
	if (ImGui::Button("Frame camera (F)")) {
		FrameCamera();
	}

	ImGui::End();
}

void Application::DrawModelInspector() {
	const assets::ModelAsset* model = workbench_.Model();
	if (!workbench_.ModelError().empty()) {
		ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "%s", workbench_.ModelError().c_str());
	}
	if (model == nullptr || workbench_.GpuStandaloneModel() == nullptr) {
		ImGui::TextDisabled("Select an xmodel in Assets.");
		return;
	}

	ImGui::Text("%s", model->model.name.c_str());
	if (ImGui::Button("Reload")) {
		ReloadModel();
	}
	ImGui::SameLine();
	if (ImGui::Button("Frame")) {
		FrameCamera();
	}
	ImGui::SameLine();
	bool modelVisible = workbench_.ModelVisible();
	if (ImGui::Checkbox("Visible", &modelVisible)) {
		workbench_.SetModelVisible(modelVisible);
	}
	ImGui::Checkbox("Bones", &showBones_);
	ImGui::SameLine();
	bool wire = workbench_.Wireframe();
	if (ImGui::Checkbox("Wireframe", &wire)) {
		workbench_.SetWireframe(wire);
	}
	ImGui::Checkbox("All bone names", &showAllBoneNames_);

	const assets::XModelData& data = model->model;
	std::uint32_t totalVerts = 0;
	std::uint32_t totalTris = 0;
	for (const auto& surface : model->surfaceSet.surfaces) {
		totalVerts += surface.vertexCount;
		totalTris += surface.triangleCount;
	}
	ImGui::TextDisabled("%u bones (%u roots)  |  %zu surfaces, %u verts, %u tris", data.numBones,
	                    data.numRootBones, model->surfaceSet.surfaces.size(), totalVerts, totalTris);
	ImGui::TextDisabled("bounds mid %.2f %.2f %.2f  half %.2f %.2f %.2f", model->boundsCenter.x,
	                    model->boundsCenter.y, model->boundsCenter.z, model->boundsHalf.x,
	                    model->boundsHalf.y, model->boundsHalf.z);
	ImGui::TextDisabled("%s", model->surfaceSet.path.string().c_str());

	if (!model->warnings.empty()) {
		ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.3f, 1.0f), "%zu warnings", model->warnings.size());
		if (ImGui::IsItemHovered()) {
			ImGui::BeginTooltip();
			for (const std::string& warning : model->warnings) {
				ImGui::TextWrapped("%s", warning.c_str());
			}
			ImGui::EndTooltip();
		}
	}

	if (!data.bones.empty() && ImGui::CollapsingHeader("Bones", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::BeginChild("##bone_list", ImVec2(0.0f, 150.0f), ImGuiChildFlags_Borders);
		for (std::size_t i = 0; i < data.bones.size(); ++i) {
			const assets::BoneData& bone = data.bones[i];
			ImGui::PushID(static_cast<int>(i));
			char label[160];
			std::snprintf(label, sizeof(label), "%2zu  %s%s", i, bone.name.c_str(),
			              bone.IsTag() ? "  [tag]" : "");
			if (ImGui::Selectable(label, selectedBone_ == static_cast<int>(i))) {
				selectedBone_ = static_cast<int>(i);
				showBones_ = true;
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("parent %d\nlocal pos %.3f %.3f %.3f\nworld pos %.3f %.3f %.3f", bone.parent,
				                  bone.localPos.x, bone.localPos.y, bone.localPos.z, bone.worldPos.x,
				                  bone.worldPos.y, bone.worldPos.z);
			}
			ImGui::PopID();
		}
		ImGui::EndChild();
	}

	if (ImGui::CollapsingHeader("Surfaces")) {
		std::vector<bool>& surfaceVisible = workbench_.ModelSurfaceVisible();
		if (surfaceVisible.size() != model->surfaceSet.surfaces.size()) {
			surfaceVisible.assign(model->surfaceSet.surfaces.size(), true);
		}
		ImGui::BeginChild("##surface_list", ImVec2(0.0f, 120.0f), ImGuiChildFlags_Borders);
		for (std::size_t i = 0; i < model->surfaceSet.surfaces.size(); ++i) {
			const assets::SurfaceData& surface = model->surfaceSet.surfaces[i];
			ImGui::PushID(static_cast<int>(i));
			bool visible = surfaceVisible[i];
			if (ImGui::Checkbox("##visible", &visible)) {
				surfaceVisible[i] = visible;
			}
			ImGui::SameLine();
			if (ImGui::TreeNode((void*)(uintptr_t)(i + 1), "%2zu  %u verts  %u tris  %s", i,
			                    surface.vertexCount, surface.triangleCount, surface.materialName.c_str())) {
				if (surface.materialIndex >= 0 &&
				    surface.materialIndex < static_cast<int>(model->materials.size())) {
					const assets::MaterialSlot& slot =
					    model->materials[static_cast<std::size_t>(surface.materialIndex)];
					ImGui::TextDisabled("material: %s", slot.material.name.c_str());
					for (const auto& texture : slot.textures) {
						ImGui::TextDisabled("  %-10s %s%s", assets::TextureSemanticName(texture.semantic),
						                    texture.imageName.c_str(),
						                    texture.fileFound ? "" : "  (missing)");
					}
				}
				ImGui::TreePop();
			}
			ImGui::PopID();
		}
		ImGui::EndChild();
	}
}

void Application::DrawCamoSection() {
	const std::vector<std::string>& names = workbench_.CamoNames();
	ImGui::TextDisabled("%zu weapon camo materials (camo_h1_*) in the asset roots", names.size());
	ImGui::SetNextItemWidth(-FLT_MIN);
	ImGui::InputTextWithHint("##camo_search", "Filter camos...", camoSearch_, sizeof(camoSearch_));
	const std::string needle = ToLower(camoSearch_);
	const std::string preview = workbench_.CamoName().empty() ? "(none)" : workbench_.CamoName().c_str();
	if (ImGui::BeginCombo("Camo", preview.c_str())) {
		if (ImGui::Selectable("(none)", workbench_.CamoName().empty())) {
			std::string error;
			if (!workbench_.SetCamo({}, error)) {
				ImGui::SetTooltip("%s", error.c_str());
			}
		}
		int shown = 0;
		for (const std::string& name : names) {
			if (!needle.empty() && ToLower(name).find(needle) == std::string::npos) {
				continue;
			}
			++shown;
			if (ImGui::Selectable(name.c_str(), name == workbench_.CamoName())) {
				std::string error;
				if (!workbench_.SetCamo(name, error)) {
					ImGui::SetTooltip("%s", error.c_str());
				}
			}
		}
		if (shown == 0) {
			ImGui::TextDisabled("No camo matches.");
		}
		ImGui::EndCombo();
	}
	if (!workbench_.CamoName().empty()) {
		ImGui::TextDisabled("texture: %s", workbench_.CamoTexture().c_str());
		ImGui::SameLine();
		if (ImGui::SmallButton("Clear")) {
			std::string error;
			workbench_.SetCamo({}, error);
		}
		float strength = workbench_.CamoStrength();
		if (ImGui::SliderFloat("Strength", &strength, 0.0f, 1.0f, "%.2f")) {
			workbench_.SetCamoStrength(strength);
		}
		float scale = workbench_.CamoScale();
		if (ImGui::SliderFloat("UV scale", &scale, 0.25f, 4.0f, "%.2f")) {
			workbench_.SetCamoScale(scale);
		}
	}
}

void Application::DrawWeaponInspector() {
	const scene::ViewmodelAsset* viewmodel = workbench_.Viewmodel();
	if (!workbench_.WeaponError().empty()) {
		ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "%s", workbench_.WeaponError().c_str());
	}
	if (viewmodel == nullptr || workbench_.GpuWeaponGun() == nullptr) {
		ImGui::TextDisabled("Select a weapon in Assets.");
		return;
	}

	ImGui::Text("%s", viewmodel->weapon.name.c_str());
	ImGui::TextDisabled("gun: %s", viewmodel->weapon.gunModel.c_str());
	ImGui::TextDisabled("handModel: %s", viewmodel->weapon.handModel.empty()
	                                         ? "(none)"
	                                         : viewmodel->weapon.handModel.c_str());
	if (viewmodel->weapon.handModel == "viewmodel_base_viewhands") {
		ImGui::TextWrapped("handModel is the placeholder viewmodel_base_viewhands; the game replaces it with "
		                   "the faction viewmodel via setviewmodel (see docs/viewmodel.md).");
	}
	ImGui::TextDisabled("hands: %s (%s)", viewmodel->handsName.empty() ? "(none)" : viewmodel->handsName.c_str(),
	                    viewmodel->handsSource.c_str());
	ImGui::TextDisabled("anim states: %zu   overrides: %zu", workbench_.AnimStates().size(),
	                    workbench_.AnimOverrides().size());

	if (ImGui::Button("Reload weapon")) {
		ReloadWeapon();
	}
	ImGui::SameLine();
	if (ImGui::Button("Frame weapon")) {
		FrameCamera();
	}

	int viewMode = workbench_.IsFirstPerson() ? 1 : 0;
	if (ImGui::RadioButton("Orbit", viewMode == 0)) {
		workbench_.SetViewMode(commands::ViewMode::Orbit);
	}
	ImGui::SameLine();
	if (ImGui::RadioButton("First person", viewMode == 1)) {
		workbench_.SetViewMode(commands::ViewMode::FirstPerson);
	}

	bool world = workbench_.IsWorldModel();
	if (ImGui::Checkbox("World model (orbit)", &world)) {
		std::string error;
		if (!workbench_.SetWorldModel(world, error)) {
			ImGui::SetTooltip("%s", error.c_str());
		} else if (world) {
			workbench_.SetViewMode(commands::ViewMode::Orbit);
		}
	}
	ImGui::SameLine();
	ImGui::TextDisabled("model: %s", viewmodel->gun.model.name.c_str());

	bool grid = workbench_.ShowGrid();
	if (ImGui::Checkbox("Grid", &grid)) {
		workbench_.SetShowGrid(grid);
	}
	ImGui::SameLine();
	bool wire = workbench_.Wireframe();
	if (ImGui::Checkbox("Wireframe", &wire)) {
		workbench_.SetWireframe(wire);
	}

	bool ads = workbench_.Aiming();
	if (ImGui::Checkbox("Aim down sights", &ads)) {
		workbench_.SetAds(ads);
	}
	ImGui::SameLine();
	ImGui::TextDisabled("ADS FOV %.1f (adsZoomFov)", viewmodel->weapon.adsZoomFov);

	if (workbench_.IsFirstPerson()) {
		float cgFov = settings_.firstPersonFov;
		if (ImGui::SliderFloat("cg_fov (4:3 H)", &cgFov, 40.0f, 120.0f, "%.1f deg")) {
			settings_.firstPersonFov = cgFov;
			workbench_.RefreshCamera();
		}
		float scale = settings_.viewmodelFovScale;
		if (ImGui::SliderFloat("cg_fovScale", &scale, 0.1f, 2.0f, "%.2f")) {
			settings_.viewmodelFovScale = scale;
			workbench_.RefreshCamera();
		}
		ImGui::TextDisabled("camera vertical FOV %.2f deg", workbench_.GetCamera().Fov());
	}

	const char* preview = handsSelection_.empty()
	                          ? (viewmodel->handsName.empty() ? "(auto)" : viewmodel->handsName.c_str())
	                          : handsSelection_.c_str();
	if (ImGui::BeginCombo("Hands", preview)) {
		if (ImGui::Selectable("(auto)", handsSelection_.empty())) {
			handsSelection_.clear();
			std::string error;
			workbench_.SetHands({}, error);
		}
		for (const std::string& name : handsOptions_) {
			const bool selected = name == handsSelection_;
			if (ImGui::Selectable(name.c_str(), selected)) {
				handsSelection_ = name;
				std::string error;
				workbench_.SetHands(name, error);
			}
		}
		ImGui::EndCombo();
	}

	if (!viewmodel->warnings.empty()) {
		ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.3f, 1.0f), "%zu warnings", viewmodel->warnings.size());
		if (ImGui::IsItemHovered()) {
			ImGui::BeginTooltip();
			for (const std::string& warning : viewmodel->warnings) {
				ImGui::TextWrapped("%s", warning.c_str());
			}
			ImGui::EndTooltip();
		}
	}

	ImGui::SeparatorText("Attachments");
	ImGui::SetNextItemWidth(-FLT_MIN);
	ImGui::InputTextWithHint("##attach_search", "Filter attachments...", attachmentSearch_,
	                         sizeof(attachmentSearch_));
	const std::vector<commands::AttachmentFitEntry>& catalog = workbench_.AttachmentCatalog();
	const std::string attachNeedle = ToLower(attachmentSearch_);
	ImGui::TextDisabled("%zu attachment assets  |  %zu mounted", catalog.size(),
	                    workbench_.Attachments().size());
	ImGui::SameLine();
	if (ImGui::SmallButton("Detach all")) {
		workbench_.DetachAll();
	}
	ImGui::BeginChild("##attachment_list", ImVec2(0.0f, 220.0f), ImGuiChildFlags_Borders);
	int shownAttachments = 0;
	std::string attachErrorText;
	for (const commands::AttachmentFitEntry& entry : catalog) {
		if (!MatchesFilter(entry.name, attachNeedle)) {
			continue;
		}
		++shownAttachments;
		ImGui::PushID(entry.name.c_str());
		bool attached = workbench_.IsAttached(entry.name);
		if (ImGui::Checkbox("##attached", &attached)) {
			std::string error;
			if (attached) {
				if (!workbench_.Attach(entry.name, error)) {
					attachErrorText = entry.name + ": " + error;
				}
			} else {
				workbench_.Detach(entry.name);
			}
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("mount / unmount this attachment");
		}
		ImGui::SameLine();
		if (entry.note.empty()) {
			if (entry.found) {
				ImGui::Text("%s", entry.name.c_str());
			} else {
				ImGui::TextColored(ImVec4(1.0f, 0.42f, 0.42f, 1.0f), "%s", entry.name.c_str());
			}
		} else {
			ImGui::TextDisabled("%s  (%s)", entry.name.c_str(), entry.note.c_str());
		}
		ImGui::Indent(26.0f);
		if (!entry.viewModel.empty()) {
			if (entry.found) {
				ImGui::TextDisabled("vm %s   needs %s   found", entry.viewModel.c_str(),
				                    entry.requiredTag.c_str());
			} else {
				std::string nearest;
				for (std::size_t i = 0; i < entry.nearest.size(); ++i) {
					if (i > 0) {
						nearest += ", ";
					}
					nearest += entry.nearest[i];
				}
				ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.45f, 1.0f),
				                   "vm %s   needs %s   MISSING (nearest %s)", entry.viewModel.c_str(),
				                   entry.requiredTag.c_str(), nearest.c_str());
			}
		}
		ImGui::Unindent(26.0f);
		ImGui::PopID();
	}
	if (shownAttachments == 0) {
		ImGui::TextDisabled("No attachments match.");
	}
	ImGui::EndChild();
	if (!attachErrorText.empty()) {
		ImGui::TextColored(ImVec4(1.0f, 0.42f, 0.42f, 1.0f), "%s", attachErrorText.c_str());
	}
	if (!workbench_.HiddenTags().empty()) {
		std::string hidden;
		for (const std::string& tag : workbench_.HiddenTags()) {
			hidden += hidden.empty() ? tag : (", " + tag);
		}
		ImGui::TextDisabled("hidden tags (hideTags/showTags): %s", hidden.c_str());
	}
	for (const commands::AttachmentMount& mount : workbench_.Attachments()) {
		if (!mount.tagFound) {
			ImGui::TextColored(ImVec4(1.0f, 0.42f, 0.42f, 1.0f), "weapon has no %s (%s drawn at the weapon origin)",
			                   mount.requiredTag.c_str(), mount.name.c_str());
		}
	}
}

void Application::DrawTagEditor() {
	if (!workbench_.HasEditableModel()) {
		return;
	}
	ImGui::SeparatorText("Tags (edit + save)");
	const std::vector<commands::TagEditInfo> tags = workbench_.EditableTags();
	const bool dirty = workbench_.HasTagChanges();
	if (dirty) {
		ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "%s  * modified *",
		                   workbench_.EditableModelName().c_str());
	} else {
		ImGui::Text("%s", workbench_.EditableModelName().c_str());
	}
	ImGui::TextDisabled("%zu tags  |  %s", tags.size(),
	                    workbench_.Document() ? workbench_.Document()->path.string().c_str() : "");

	const bool hasSelection = !selectedTag_.empty();
	if (ImGui::BeginChild("##tag_list", ImVec2(0.0f, 130.0f), ImGuiChildFlags_Borders)) {
		for (const commands::TagEditInfo& tag : tags) {
			ImGui::PushID(tag.name.c_str());
			char label[192];
			std::snprintf(label, sizeof(label), "%s%s", tag.name.c_str(), tag.added ? "  [added]" : "");
			if (ImGui::Selectable(label, tag.name == selectedTag_)) {
				selectedTag_ = tag.name;
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("parent %s\nlocal (%.3f %.3f %.3f)\nworld (%.3f %.3f %.3f)",
				                  tag.parent.c_str(), tag.localPos.x, tag.localPos.y, tag.localPos.z,
				                  tag.worldPos.x, tag.worldPos.y, tag.worldPos.z);
			}
			ImGui::PopID();
		}
	}
	ImGui::EndChild();

	if (hasSelection) {
		const commands::TagEditInfo* selected = nullptr;
		for (const commands::TagEditInfo& tag : tags) {
			if (tag.name == selectedTag_) {
				selected = &tag;
			}
		}
		if (selected == nullptr) {
			selectedTag_.clear();
		} else {
			float position[3] = {selected->localPos.x, selected->localPos.y, selected->localPos.z};
			float rotation[4] = {selected->localQuat.x, selected->localQuat.y, selected->localQuat.z,
			                     selected->localQuat.w};
			ImGui::Text("%s  (parent %s)", selected->name.c_str(), selected->parent.c_str());
			bool changed = false;
			ImGui::SetNextItemWidth(-FLT_MIN);
			changed |= ImGui::DragFloat3("local position", position, 0.01f, -1000.0f, 1000.0f, "%.3f");
			ImGui::SetNextItemWidth(-FLT_MIN);
			changed |= ImGui::DragFloat4("local rotation", rotation, 0.005f, -1.0f, 1.0f, "%.4f");
			if (changed) {
				const float length = std::sqrt(rotation[0] * rotation[0] + rotation[1] * rotation[1] +
				                               rotation[2] * rotation[2] + rotation[3] * rotation[3]);
				if (length > 1.0e-6f) {
					for (float& value : rotation) {
						value /= length;
					}
				} else {
					rotation[3] = 1.0f;
				}
				std::string error;
				if (!workbench_.MoveTagLocal(selected->name,
				                             {position[0], position[1], position[2]},
				                             {rotation[0], rotation[1], rotation[2], rotation[3]}, error)) {
					tagSaveStatus_ = error;
				}
			}
			ImGui::Checkbox("Gizmo", &showGizmo_);
			ImGui::SameLine();
			ImGui::Checkbox("Ghost of original", &showGhost_);
			ImGui::SameLine();
			if (ImGui::RadioButton("Move", gizmoMode_ == 0)) {
				gizmoMode_ = 0;
			}
			ImGui::SameLine();
			if (ImGui::RadioButton("Rotate", gizmoMode_ == 1)) {
				gizmoMode_ = 1;
			}
			if (ImGui::Button("Revert tag")) {
				std::string error;
				if (!workbench_.RevertTag(selected->name, error)) {
					tagSaveStatus_ = error;
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Revert all")) {
				workbench_.RevertAllTags();
				tagSaveStatus_.clear();
			}
			ImGui::SameLine();
			if (ImGui::Button("Save model")) {
				commands::ModelSaveResult saved;
				std::string error;
				if (workbench_.SaveEditedModel(saved, error)) {
					tagSaveStatus_ = "saved " + saved.path + "  (.bak: " + saved.backup + ")  " +
					                 saved.validation;
				} else {
					tagSaveStatus_ = error;
				}
			}
			if (!tagSaveStatus_.empty()) {
				ImGui::TextWrapped("%s", tagSaveStatus_.c_str());
			}
		}
	}

	if (ImGui::CollapsingHeader("Add tag")) {
		ImGui::InputTextWithHint("##new_tag_name", "tag_reflex", newTagName_, sizeof(newTagName_));
		// Tags HMW attachments need, from the fit catalog.
		std::vector<std::string> needed;
		for (const commands::AttachmentFitEntry& entry : workbench_.AttachmentCatalog()) {
			if (!entry.requiredTag.empty() &&
			    std::find(needed.begin(), needed.end(), entry.requiredTag) == needed.end()) {
				needed.push_back(entry.requiredTag);
			}
		}
		std::sort(needed.begin(), needed.end());
		ImGui::SetNextItemWidth(-FLT_MIN);
		if (ImGui::BeginCombo("needed tag", "(pick from the fit report)")) {
			for (const std::string& name : needed) {
				if (ImGui::Selectable(name.c_str())) {
					std::snprintf(newTagName_, sizeof(newTagName_), "%s", name.c_str());
				}
			}
			ImGui::EndCombo();
		}
		std::vector<std::string> bones;
		if (const assets::XModelDocument* document = workbench_.Document()) {
			for (const assets::BoneData& bone : document->model.bones) {
				bones.push_back(bone.name);
			}
		}
		ImGui::SetNextItemWidth(-FLT_MIN);
		if (ImGui::BeginCombo("parent", newTagParent_.empty() ? "(j_gun / tag_weapon)" : newTagParent_.c_str())) {
			if (ImGui::Selectable("(j_gun / tag_weapon)", newTagParent_.empty())) {
				newTagParent_.clear();
			}
			for (const std::string& name : bones) {
				if (ImGui::Selectable(name.c_str(), newTagParent_ == name)) {
					newTagParent_ = name;
				}
			}
			ImGui::EndCombo();
		}
		ImGui::SetNextItemWidth(-FLT_MIN);
		if (ImGui::BeginCombo("copy transform from", newTagCopy_)) {
			for (const commands::TagEditInfo& tag : tags) {
				if (ImGui::Selectable(tag.name.c_str(), std::string(newTagCopy_) == tag.name)) {
					std::snprintf(newTagCopy_, sizeof(newTagCopy_), "%s", tag.name.c_str());
				}
			}
			ImGui::EndCombo();
		}
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::DragFloat3("##new_tag_pos", newTagPos_, 0.01f, -1000.0f, 1000.0f, "pos %.3f");
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::DragFloat4("##new_tag_rot", newTagRot_, 0.005f, -1.0f, 1.0f, "rot %.4f");
		if (ImGui::Button("Add tag")) {
			std::string error;
			if (workbench_.AddTag(newTagName_, newTagParent_, newTagCopy_,
			                      {newTagPos_[0], newTagPos_[1], newTagPos_[2]},
			                      {newTagRot_[0], newTagRot_[1], newTagRot_[2], newTagRot_[3]}, error)) {
				selectedTag_ = newTagName_;
				tagSaveStatus_.clear();
			} else {
				tagSaveStatus_ = error;
			}
		}
	}
}

void Application::DrawGizmo(const ImVec2& origin, const ImVec2& size, float aspect) {
	if (!workbench_.HasEditableModel() || selectedTag_.empty() || !showGizmo_) {
		return;
	}
	DirectX::XMFLOAT4X4 pose = {};
	if (!workbench_.TagPoseWorld(selectedTag_, pose)) {
		return;
	}
	const Camera& camera = workbench_.GetCamera();
	const auto columnMajor = [](const DirectX::XMMATRIX& matrix, float out[16]) {
		DirectX::XMFLOAT4X4 transposed;
		DirectX::XMStoreFloat4x4(&transposed, DirectX::XMMatrixTranspose(matrix));
		std::memcpy(out, &transposed, sizeof(transposed));
	};
	float view[16] = {};
	float projection[16] = {};
	float model[16] = {};
	columnMajor(camera.View(), view);
	columnMajor(camera.Projection(aspect), projection);
	columnMajor(DirectX::XMLoadFloat4x4(&pose), model);

	ImGuizmo::SetOrthographic(false);
	ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
	ImGuizmo::SetRect(origin.x, origin.y, size.x, size.y);
	const ImGuizmo::OPERATION operation =
	    gizmoMode_ == 1 ? ImGuizmo::ROTATE : ImGuizmo::TRANSLATE;
	ImGuizmo::Manipulate(view, projection, operation, ImGuizmo::LOCAL, model);
	if (ImGuizmo::IsUsing()) {
		DirectX::XMFLOAT4X4 transposed;
		std::memcpy(&transposed, model, sizeof(transposed));
		const DirectX::XMMATRIX world =
		    DirectX::XMMatrixTranspose(DirectX::XMLoadFloat4x4(&transposed));
		DirectX::XMVECTOR scale;
		DirectX::XMVECTOR rotation;
		DirectX::XMVECTOR translation;
		if (DirectX::XMMatrixDecompose(&scale, &rotation, &translation, world)) {
			DirectX::XMFLOAT3 position;
			DirectX::XMFLOAT4 quat;
			DirectX::XMStoreFloat3(&position, translation);
			DirectX::XMStoreFloat4(&quat, DirectX::XMQuaternionNormalize(rotation));
			std::string error;
			if (!workbench_.MoveTagWorld(selectedTag_, position, quat, error)) {
				tagSaveStatus_ = error;
			}
		}
	}

	if (showGhost_) {
		DirectX::XMFLOAT3 ghost;
		if (workbench_.TagGhostWorld(selectedTag_, ghost)) {
			const DirectX::XMMATRIX viewProjection = camera.ViewProjection(aspect);
			const DirectX::XMVECTOR clip =
			    DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&ghost), viewProjection);
			DirectX::XMFLOAT3 ndc;
			DirectX::XMStoreFloat3(&ndc, clip);
			const float x = origin.x + (ndc.x * 0.5f + 0.5f) * size.x;
			const float y = origin.y + (1.0f - (ndc.y * 0.5f + 0.5f)) * size.y;
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			drawList->AddCircle(ImVec2(x, y), 7.0f, IM_COL32(255, 180, 60, 220), 16, 2.0f);
			drawList->AddLine(ImVec2(x - 10.0f, y), ImVec2(x + 10.0f, y),
			                  IM_COL32(255, 180, 60, 180), 1.0f);
			drawList->AddLine(ImVec2(x, y - 10.0f), ImVec2(x, y + 10.0f),
			                  IM_COL32(255, 180, 60, 180), 1.0f);
			drawList->AddText(ImVec2(x + 9.0f, y + 4.0f), IM_COL32(255, 200, 120, 220), "original");
		}
	}
}

void Application::DrawAnimInspector() {
	const assets::XAnimData* anim = workbench_.Anim();
	if (anim != nullptr) {
		ImGui::Text("%s", anim->name.c_str());
		ImGui::TextDisabled("%u frames (0..%u)  |  %.1f fps  |  %.3f s", anim->frameCount, anim->frameCount,
		                    anim->frameRate, anim->Duration());
		ImGui::TextDisabled("bones %zu  |  notetracks %zu%s%s", anim->boneNames.size(),
		                    anim->notifies.size(), anim->looping ? "  |  loop" : "",
		                    anim->additive ? "  |  additive asset" : "");
		if (workbench_.AdditiveEnabled()) {
			ImGui::TextColored(ImVec4(0.55f, 0.80f, 1.0f, 1.0f), "additive over IDLE");
		}
	} else {
		ImGui::TextDisabled("No animation loaded.");
	}

	if (ImGui::CollapsingHeader("Weapon anim states", ImGuiTreeNodeFlags_DefaultOpen)) {
		if (workbench_.AnimStates().empty()) {
			ImGui::TextDisabled("No weapon states (load a weapon).");
		}
		ImGui::BeginChild("##anim_states", ImVec2(0.0f, 170.0f), ImGuiChildFlags_Borders);
		for (const commands::AnimStateEntry& entry : workbench_.AnimStates()) {
			ImGui::PushID(entry.state.c_str());
			const bool active = entry.anim == workbench_.AnimName();
			char label[256];
			std::snprintf(label, sizeof(label), "%-20s  %s", entry.state.c_str(), entry.anim.c_str());
			if (ImGui::Selectable(label, active)) {
				std::string error;
				workbench_.PlayAnim(entry.anim, error);
			}
			if (IsAdditiveName(entry.anim)) {
				ImGui::SameLine();
				ImGui::TextDisabled("[add]");
			}
			ImGui::PopID();
		}
		ImGui::EndChild();
	}

	if (ImGui::CollapsingHeader("animOverrides")) {
		if (workbench_.AnimOverrides().empty()) {
			ImGui::TextDisabled("No overrides.");
		}
		ImGui::BeginChild("##anim_overrides", ImVec2(0.0f, 150.0f), ImGuiChildFlags_Borders);
		for (const assets::WeaponAnimOverride& entry : workbench_.AnimOverrides()) {
			ImGui::PushID(entry.anim.c_str());
			const std::string state = commands::AnimStateForTreeType(entry.animTreeType);
			const bool active = workbench_.IsAnimOverrideActive(entry);
			char label[360];
			std::snprintf(label, sizeof(label), "att %d  tree %d%s%s%s  %s", entry.attachment1,
			              entry.animTreeType, state.empty() ? "" : " -> ", state.c_str(),
			              active ? "  [active]" : "", entry.anim.c_str());
			if (ImGui::Selectable(label, entry.anim == workbench_.AnimName())) {
				std::string error;
				workbench_.PlayAnim(entry.anim, error);
			}
			ImGui::PopID();
		}
		ImGui::EndChild();
	}

	if (ImGui::CollapsingHeader("All anims in roots")) {
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::InputTextWithHint("##anim_search", "Filter anims...", animSearch_, sizeof(animSearch_));
		const std::string needle = ToLower(animSearch_);
		ImGui::BeginChild("##all_anims", ImVec2(0.0f, 170.0f), ImGuiChildFlags_Borders);
		int shown = 0;
		for (const AssetEntry& entry : assets_.Entries()) {
			if (entry.kind != AssetKind::Anim || !MatchesFilter(entry.name, needle)) {
				continue;
			}
			++shown;
			ImGui::PushID(entry.name.c_str());
			if (ImGui::Selectable(entry.name.c_str(), entry.name == workbench_.AnimName())) {
				std::string error;
				workbench_.PlayAnim(entry.name, error);
			}
			ImGui::PopID();
		}
		if (shown == 0) {
			ImGui::TextDisabled("No anims match.");
		}
		ImGui::EndChild();
	}
}

void Application::DrawTimelinePanel() {
	ImGui::Begin(kTimelineWindow);

	const assets::XAnimData* anim = workbench_.Anim();
	if (anim == nullptr) {
		ImGui::TextDisabled("No animation loaded.");
		ImGui::TextDisabled("Load a weapon or pick an anim; timeline and notetracks appear here.");
		ImGui::End();
		return;
	}

	ImGui::Text("%s", anim->name.c_str());

	if (ImGui::Button(workbench_.AnimPlaying() ? "Pause" : "Play")) {
		workbench_.SetAnimPlaying(!workbench_.AnimPlaying());
	}
	ImGui::SameLine();
	if (ImGui::Button("Stop")) {
		workbench_.StopAnim();
		ImGui::End();
		return;
	}
	ImGui::SameLine();
	if (ImGui::Button("|<")) {
		workbench_.SetAnimFrame(0);
	}
	ImGui::SameLine();
	if (ImGui::Button("<")) {
		workbench_.SetAnimFrame(static_cast<int>(std::floor(workbench_.AnimFrame())) - 1);
	}
	ImGui::SameLine();
	if (ImGui::Button(">")) {
		workbench_.SetAnimFrame(static_cast<int>(std::floor(workbench_.AnimFrame())) + 1);
	}
	ImGui::SameLine();
	if (ImGui::Button(">|")) {
		workbench_.SetAnimFrame(static_cast<int>(anim->frameCount));
	}
	ImGui::SameLine();
	bool loop = workbench_.AnimLooping();
	if (ImGui::Checkbox("Loop", &loop)) {
		workbench_.SetAnimLooping(loop);
	}
	ImGui::SameLine();
	bool additive = workbench_.AdditiveEnabled();
	if (ImGui::Checkbox("Additive over IDLE", &additive)) {
		workbench_.SetAdditive(additive);
	}

	float speed = workbench_.AnimSpeed();
	ImGui::SetNextItemWidth(160.0f);
	if (ImGui::SliderFloat("Speed", &speed, 0.1f, 2.0f, "%.2fx")) {
		workbench_.SetAnimSpeed(speed);
	}
	ImGui::SameLine();
	ImGui::TextDisabled("frame %.2f / %u   |   %.3f / %.3f s", workbench_.AnimFrame(), anim->frameCount,
	                    workbench_.AnimTime(), workbench_.AnimDuration());

	float progress = workbench_.AnimProgress();
	ImGui::SetNextItemWidth(-FLT_MIN);
	if (ImGui::SliderFloat("##anim_scrub", &progress, 0.0f, 1.0f, "")) {
		workbench_.SetAnimFraction(progress);
	}
	const ImVec2 sliderMin = ImGui::GetItemRectMin();
	const ImVec2 sliderMax = ImGui::GetItemRectMax();
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	for (const assets::AnimNotify& note : anim->notifies) {
		const float x = sliderMin.x + std::clamp(note.fraction, 0.0f, 1.0f) * (sliderMax.x - sliderMin.x);
		drawList->AddLine(ImVec2(x, sliderMin.y - 2.0f), ImVec2(x, sliderMax.y + 2.0f),
		                  IM_COL32(255, 190, 90, 220), 1.5f);
		if (ImGui::IsItemHovered() && ImGui::GetIO().MousePos.x > x - 4.0f &&
		    ImGui::GetIO().MousePos.x < x + 4.0f) {
			ImGui::SetTooltip("%s\nframe %u  |  %.3f s", note.name.c_str(), note.frame, note.time);
		}
	}

	if (workbench_.AdditiveEnabled()) {
		if (workbench_.BaseAnim() == nullptr) {
			ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.35f, 1.0f),
			                   "No IDLE animation found for the additive base pose.");
		} else {
			ImGui::TextDisabled("base: %s  (%.3f s)", workbench_.BaseAnim()->name.c_str(),
			                    workbench_.BaseAnim()->Duration());
		}
	}

	if (!anim->notifies.empty()) {
		ImGui::SeparatorText("Notetracks");
		for (const assets::AnimNotify& note : anim->notifies) {
			ImGui::Text("frame %3u  %6.3f s  %s", note.frame, note.time, note.name.c_str());
		}
	}

	ImGui::End();
}

void Application::RebuildFilter() {
	lastFilter_ = ToLower(search_);
	for (std::vector<int>& rows : filtered_) {
		rows.clear();
	}

	const std::vector<AssetEntry>& entries = assets_.Entries();
	for (size_t i = 0; i < entries.size(); ++i) {
		const AssetEntry& entry = entries[i];
		if (MatchesFilter(entry.name, lastFilter_)) {
			filtered_[static_cast<size_t>(entry.kind)].push_back(static_cast<int>(i));
		}
	}
}

void Application::RescanAssets() {
	const std::vector<std::filesystem::path> roots = ResolveAssetRoots(settings_, exeDir_);
	assets_.Scan(roots);
	workbench_.SetRoots(roots);
	selected_ = -1;
	RebuildFilter();
	RefreshHandsOptions();
}

void Application::RefreshHandsOptions() {
	const std::vector<std::filesystem::path> roots = ResolveAssetRoots(settings_, exeDir_);
	handsOptions_ = scene::ListHandsModels(roots);
	if (!handsSelection_.empty() &&
	    std::find(handsOptions_.begin(), handsOptions_.end(), handsSelection_) == handsOptions_.end()) {
		handsOptions_.insert(handsOptions_.begin(), handsSelection_);
	}
}

void Application::LoadWeapon(const std::string& name, const std::string& hands, bool firstPerson) {
	firstPersonRequested_ = firstPerson;
	handsSelection_ = hands;
	std::string error;
	workbench_.LoadWeapon(name, hands, firstPerson, error);
	for (std::size_t i = 0; i < assets_.Entries().size(); ++i) {
		const AssetEntry& entry = assets_.Entries()[i];
		if (entry.kind == AssetKind::Weapon && entry.name == name) {
			selected_ = static_cast<int>(i);
			break;
		}
	}
}

void Application::LoadSelectedWeapon() {
	const AssetEntry* entry = SelectedEntry();
	if (entry != nullptr && entry->kind == AssetKind::Weapon) {
		LoadWeapon(entry->name, handsSelection_, firstPersonRequested_);
	}
}

void Application::ReloadWeapon() {
	if (workbench_.WeaponName().empty()) {
		return;
	}
	std::string error;
	workbench_.ReloadWeapon(error);
}

void Application::FrameCamera() { workbench_.FrameCamera(); }

void Application::LoadModelByName(const std::string& name) {
	for (std::size_t i = 0; i < assets_.Entries().size(); ++i) {
		const AssetEntry& entry = assets_.Entries()[i];
		if (entry.kind == AssetKind::Model && entry.name == name) {
			selected_ = static_cast<int>(i);
			break;
		}
	}
	std::string error;
	workbench_.LoadModel(name, error);
}

void Application::LoadSelectedModel() {
	const AssetEntry* entry = SelectedEntry();
	if (entry == nullptr || entry->kind != AssetKind::Model) {
		return;
	}
	selectedBone_ = -1;
	std::string error;
	workbench_.LoadModel(entry->name, error);
}

void Application::ReloadModel() {
	if (workbench_.ModelName().empty()) {
		return;
	}
	std::string error;
	workbench_.ReloadModel(error);
}

const AssetEntry* Application::SelectedEntry() const {
	const std::vector<AssetEntry>& entries = assets_.Entries();
	if (selected_ < 0 || selected_ >= static_cast<int>(entries.size())) {
		return nullptr;
	}
	return &entries[static_cast<size_t>(selected_)];
}

} // namespace wb
