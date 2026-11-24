#include "app/application.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>

#include <imgui_internal.h>

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

float BoundsRadius(const DirectX::XMFLOAT3& halfSize) {
	return std::max({halfSize.x, halfSize.y, halfSize.z, 0.25f}) * 1.1f;
}

} // namespace

Application::Application(Settings& settings, AssetIndex& assets, Renderer& renderer,
                         std::filesystem::path exeDir, bool restoreLayout)
    : settings_(settings), assets_(assets), renderer_(renderer), exeDir_(std::move(exeDir)),
      layoutNeeded_(!restoreLayout) {
	camera_.SetYawPitch(settings_.camera.yaw, settings_.camera.pitch);
	camera_.SetDistance(settings_.camera.distance);
	camera_.SetFov(settings_.camera.fov);
	RebuildFilter();
}

void Application::Frame() {
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

	settings_.camera.yaw = camera_.Yaw();
	settings_.camera.pitch = camera_.Pitch();
	settings_.camera.distance = camera_.Distance();
	settings_.camera.fov = camera_.Fov();
}

void Application::BuildLayout(ImGuiID dockspaceId, const ImVec2& size) {
	ImGui::DockBuilderRemoveNode(dockspaceId);
	ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
	ImGui::DockBuilderSetNodeSize(dockspaceId, size);

	ImGuiID center = dockspaceId;
	const ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.20f, nullptr, &center);
	const ImGuiID right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.28f, nullptr, &center);
	const ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.25f, nullptr, &center);

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
		if (ImGui::MenuItem("Reload model", nullptr, false, !loadedModelName_.empty())) {
			ReloadModel();
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
					}
					if (entry.kind == AssetKind::Model) {
						LoadSelectedModel();
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

	if (!renderer_.BeginScene(pixelWidth, pixelHeight)) {
		ImGui::TextDisabled("Rendering failed.");
		ImGui::End();
		ImGui::PopStyleVar();
		return;
	}
	renderer_.RenderGrid(camera_);
	if (gpuModel_ && modelVisible_) {
		renderer_.RenderModel(*gpuModel_, camera_, wireframe_, &surfaceVisible_);
	}

	const ImVec2 imageOrigin = ImGui::GetCursorScreenPos();
	ImGui::Image(static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(renderer_.SceneTexture())), available);
	ImGui::SetCursorScreenPos(imageOrigin);
	ImGui::InvisibleButton("##viewport_input", available,
	                       ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight |
	                           ImGuiButtonFlags_MouseButtonMiddle);

	HandleViewportInput(available);

	if (showBones_ && !model_.model.bones.empty()) {
		DirectX::XMFLOAT4X4 viewProjection = {};
		DirectX::XMStoreFloat4x4(&viewProjection, camera_.ViewProjection(aspect));
		DrawModelOverlay(ImGui::GetWindowDrawList(), imageOrigin, available, viewProjection, model_.model,
		                 selectedBone_, showAllBoneNames_);
	}

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	char line[256];
	std::snprintf(line, sizeof(line), "yaw %.1f  pitch %.1f  dist %.2f  fov %.1f", camera_.Yaw(),
	              camera_.Pitch(), camera_.Distance(), camera_.Fov());
	const ImU32 textColor = IM_COL32(210, 214, 222, 220);
	drawList->AddText(ImVec2(imageOrigin.x + 10.0f, imageOrigin.y + 8.0f), textColor, line);
	drawList->AddText(ImVec2(imageOrigin.x + 10.0f, imageOrigin.y + 24.0f), IM_COL32(150, 155, 165, 200),
	                  "LMB orbit  |  RMB/MMB pan  |  wheel zoom  |  F frame camera");

	ImGui::End();
	ImGui::PopStyleVar();
}

void Application::HandleViewportInput(const ImVec2& viewportSize) {
	ImGuiIO& io = ImGui::GetIO();
	const bool active = ImGui::IsItemActive();
	const bool hovered = ImGui::IsItemHovered();

	if (active) {
		const ImVec2 delta = io.MouseDelta;
		if (delta.x != 0.0f || delta.y != 0.0f) {
			if (io.MouseDown[ImGuiMouseButton_Left]) {
				camera_.Orbit(delta.x, delta.y);
			} else if (io.MouseDown[ImGuiMouseButton_Right] || io.MouseDown[ImGuiMouseButton_Middle]) {
				camera_.Pan(delta.x, delta.y, viewportSize.y);
			}
		}
	}

	if (hovered && io.MouseWheel != 0.0f) {
		camera_.Zoom(io.MouseWheel);
	}

	if (hovered && ImGui::IsKeyPressed(ImGuiKey_F, false)) {
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
		ImGui::TextWrapped("Pick an xmodel in the Assets panel to load a model.");
	}

	ImGui::SeparatorText("Model");
	DrawModelInspector();

	ImGui::SeparatorText("Camera");
	ImGui::Text("yaw %.1f   pitch %.1f", camera_.Yaw(), camera_.Pitch());
	ImGui::Text("distance %.2f", camera_.Distance());
	float fov = camera_.Fov();
	if (ImGui::SliderFloat("FOV", &fov, 20.0f, 120.0f, "%.0f deg")) {
		camera_.SetFov(fov);
	}
	if (ImGui::Button("Frame camera (F)")) {
		FrameCamera();
	}

	ImGui::End();
}

void Application::DrawModelInspector() {
	if (!modelError_.empty()) {
		ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "%s", modelError_.c_str());
	}

	if (!gpuModel_) {
		ImGui::TextDisabled("Select an xmodel in Assets.");
		return;
	}

	ImGui::Text("%s", model_.model.name.c_str());
	if (ImGui::Button("Reload")) {
		ReloadModel();
	}
	ImGui::SameLine();
	if (ImGui::Button("Frame")) {
		FrameCamera();
	}
	ImGui::SameLine();
	ImGui::Checkbox("Visible", &modelVisible_);
	ImGui::Checkbox("Bones", &showBones_);
	ImGui::SameLine();
	ImGui::Checkbox("Wireframe", &wireframe_);
	ImGui::Checkbox("All bone names", &showAllBoneNames_);

	const assets::XModelData& data = model_.model;
	std::uint32_t totalVerts = 0;
	std::uint32_t totalTris = 0;
	for (const auto& surface : model_.surfaceSet.surfaces) {
		totalVerts += surface.vertexCount;
		totalTris += surface.triangleCount;
	}
	ImGui::TextDisabled("%u bones (%u roots)  |  %zu surfaces, %u verts, %u tris", data.numBones,
	                    data.numRootBones, model_.surfaceSet.surfaces.size(), totalVerts, totalTris);
	ImGui::TextDisabled("bounds mid %.2f %.2f %.2f  half %.2f %.2f %.2f", model_.boundsCenter.x,
	                    model_.boundsCenter.y, model_.boundsCenter.z, model_.boundsHalf.x, model_.boundsHalf.y,
	                    model_.boundsHalf.z);
	ImGui::TextDisabled("%s", model_.surfaceSet.path.string().c_str());

	if (!model_.warnings.empty()) {
		ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.3f, 1.0f), "%zu warnings", model_.warnings.size());
		if (ImGui::IsItemHovered()) {
			ImGui::BeginTooltip();
			for (const std::string& warning : model_.warnings) {
				ImGui::TextWrapped("%s", warning.c_str());
			}
			ImGui::EndTooltip();
		}
	}

	if (!data.bones.empty() && ImGui::CollapsingHeader("Bones", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::BeginChild("##bone_list", ImVec2(0.0f, 180.0f), ImGuiChildFlags_Borders);
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

	if (ImGui::CollapsingHeader("Surfaces", ImGuiTreeNodeFlags_DefaultOpen)) {
		if (surfaceVisible_.size() != model_.surfaceSet.surfaces.size()) {
			surfaceVisible_.assign(model_.surfaceSet.surfaces.size(), true);
		}
		ImGui::BeginChild("##surface_list", ImVec2(0.0f, 150.0f), ImGuiChildFlags_Borders);
		for (std::size_t i = 0; i < model_.surfaceSet.surfaces.size(); ++i) {
			const assets::SurfaceData& surface = model_.surfaceSet.surfaces[i];
			ImGui::PushID(static_cast<int>(i));
			bool visible = surfaceVisible_[i];
			if (ImGui::Checkbox("##visible", &visible)) {
				surfaceVisible_[i] = visible;
			}
			ImGui::SameLine();
			if (ImGui::TreeNode((void*)(uintptr_t)(i + 1), "%2zu  %u verts  %u tris  %s", i,
			                    surface.vertexCount, surface.triangleCount, surface.materialName.c_str())) {
				if (surface.materialIndex >= 0 &&
				    surface.materialIndex < static_cast<int>(model_.materials.size())) {
					const assets::MaterialSlot& slot =
					    model_.materials[static_cast<std::size_t>(surface.materialIndex)];
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

	if (!model_.materials.empty() && ImGui::CollapsingHeader("Materials")) {
		for (const assets::MaterialSlot& slot : model_.materials) {
			ImGui::PushID(slot.material.name.c_str());
			if (ImGui::TreeNode(slot.material.name.c_str())) {
				ImGui::TextDisabled("techset: %s", slot.material.techniqueSet.c_str());
				if (!slot.error.empty()) {
					ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "%s", slot.error.c_str());
				}
				for (const auto& texture : slot.textures) {
					ImGui::TextDisabled("  %-10s %s%s", assets::TextureSemanticName(texture.semantic),
					                    texture.imageName.c_str(),
					                    texture.fileFound ? "" : "  (missing)");
				}
				ImGui::TreePop();
			}
			ImGui::PopID();
		}
	}
}

void Application::DrawTimelinePanel() {
	ImGui::Begin(kTimelineWindow);
	ImGui::TextDisabled("No animation loaded.");
	ImGui::TextDisabled("Timeline, playback and notetracks arrive in M3.");
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
	assets_.Scan(ResolveAssetRoots(settings_, exeDir_));
	selected_ = -1;
	RebuildFilter();
}

void Application::FrameCamera() {
	if (gpuModel_) {
		camera_.Frame(model_.boundsCenter, BoundsRadius(model_.boundsHalf));
	} else {
		camera_.Frame(DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f), 7.0f);
	}
}

void Application::LoadModelByName(const std::string& name) {
	const std::vector<AssetEntry>& entries = assets_.Entries();
	for (std::size_t i = 0; i < entries.size(); ++i) {
		if (entries[i].kind == AssetKind::Model && entries[i].name == name) {
			selected_ = static_cast<int>(i);
			LoadSelectedModel();
			return;
		}
	}
	modelError_ = "model not found in the asset list: " + name;
}

void Application::LoadSelectedModel() {	const AssetEntry* entry = SelectedEntry();
	if (entry == nullptr || entry->kind != AssetKind::Model) {
		return;
	}
	if (entry->name == loadedModelName_ && !modelError_.empty()) {
		return;
	}
	if (entry->name == loadedModelName_ && gpuModel_) {
		return;
	}

	modelError_.clear();
	loadedModelName_ = entry->name;
	loadedModelPath_ = entry->path.string();
	selectedBone_ = -1;
	gpuModel_.reset();
	model_ = assets::ModelAsset{};

	std::string error;
	const std::vector<std::filesystem::path> roots = ResolveAssetRoots(settings_, exeDir_);
	if (!assets::LoadModelAsset(entry->name, roots, model_, error, true)) {
		modelError_ = error;
		return;
	}

	std::string uploadError;
	gpuModel_ = renderer_.UploadModel(model_, uploadError);
	if (!gpuModel_) {
		modelError_ = "GPU upload failed: " + uploadError;
		return;
	}

	surfaceVisible_.assign(model_.surfaceSet.surfaces.size(), true);
	FrameCamera();
}

void Application::ReloadModel() {
	if (loadedModelName_.empty()) {
		return;
	}
	gpuModel_.reset();
	const std::string name = loadedModelName_;
	loadedModelName_.clear();
	modelError_.clear();
	const AssetEntry* entry = SelectedEntry();
	if (entry != nullptr && entry->kind == AssetKind::Model && entry->name == name) {
		LoadSelectedModel();
	} else {
		// Keep the selection-independent reload path simple: find the model entry by name.
		for (const AssetEntry& candidate : assets_.Entries()) {
			if (candidate.kind == AssetKind::Model && candidate.name == name) {
				selected_ = static_cast<int>(&candidate - assets_.Entries().data());
				LoadSelectedModel();
				break;
			}
		}
	}
}

const AssetEntry* Application::SelectedEntry() const {
	const std::vector<AssetEntry>& entries = assets_.Entries();
	if (selected_ < 0 || selected_ >= static_cast<int>(entries.size())) {
		return nullptr;
	}
	return &entries[static_cast<size_t>(selected_)];
}

} // namespace wb
