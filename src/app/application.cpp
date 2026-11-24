#include "app/application.h"

#include <algorithm>
#include <cctype>
#include <cstdio>

#include <imgui_internal.h>

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

	DirectX::XMFLOAT4X4 viewProjection = {};
	DirectX::XMStoreFloat4x4(&viewProjection, camera_.ViewProjection(aspect));
	if (!renderer_.RenderGrid(viewProjection, pixelWidth, pixelHeight)) {
		ImGui::TextDisabled("Rendering failed.");
		ImGui::End();
		ImGui::PopStyleVar();
		return;
	}

	const ImVec2 imageOrigin = ImGui::GetCursorScreenPos();
	ImGui::Image(static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(renderer_.SceneTexture())), available);
	ImGui::SetCursorScreenPos(imageOrigin);
	ImGui::InvisibleButton("##viewport_input", available,
	                       ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight |
	                           ImGuiButtonFlags_MouseButtonMiddle);

	HandleViewportInput(available);

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

		ImGui::SeparatorText("Preview");
		ImGui::TextDisabled("Readers and previews arrive in M1.");
	} else {
		ImGui::TextDisabled("No asset selected.");
		ImGui::TextWrapped("Pick a weapon, xmodel or xanim in the Assets panel.");
	}

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
	camera_.Frame(7.0f);
}

const AssetEntry* Application::SelectedEntry() const {
	const std::vector<AssetEntry>& entries = assets_.Entries();
	if (selected_ < 0 || selected_ >= static_cast<int>(entries.size())) {
		return nullptr;
	}
	return &entries[static_cast<size_t>(selected_)];
}

} // namespace wb
