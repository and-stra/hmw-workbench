#include "app/review.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include <DirectXMath.h>
#include <DirectXTex.h>
#include <imgui.h>
#include <imgui_impl_dx11.h>

#include "app/commands.h"
#include "app/model_overlay.h"
#include "render/renderer.h"

namespace wb {
namespace {

bool BeginOverlay(Renderer& renderer, int width, int height, float fontScale = 1.0f) {
	const ImVec2 displaySize(static_cast<float>(width), static_cast<float>(height));
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.IniFilename = nullptr;
	io.DisplaySize = displaySize;
	io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
	io.FontGlobalScale = fontScale;
	if (!ImGui_ImplDX11_Init(renderer.Device(), renderer.Context())) {
		ImGui::DestroyContext();
		return false;
	}
	ImGui_ImplDX11_NewFrame();
	ImGui::NewFrame();
	return true;
}

void EndOverlay() {
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
	ImGui_ImplDX11_Shutdown();
	ImGui::DestroyContext();
}

void DrawBones(commands::Workbench& workbench, int width, int height) {
	const assets::ModelAsset* model = workbench.Model();
	if (model == nullptr || model->model.bones.empty()) {
		return;
	}
	const float aspect = static_cast<float>(width) / static_cast<float>(height);
	DirectX::XMFLOAT4X4 viewProjection = {};
	DirectX::XMStoreFloat4x4(&viewProjection, workbench.GetCamera().ViewProjection(aspect));
	DrawModelOverlay(ImGui::GetBackgroundDrawList(), ImVec2(0.0f, 0.0f),
	                 ImVec2(static_cast<float>(width), static_cast<float>(height)), viewProjection,
	                 model->model, -1, true);
}

void DrawDebug(commands::Workbench& workbench, int width, int height, bool crosshair, bool markTags) {
	ImDrawList* drawList = ImGui::GetBackgroundDrawList();
	if (crosshair) {
		const float cx = static_cast<float>(width) * 0.5f;
		const float cy = static_cast<float>(height) * 0.5f;
		const ImU32 colour = IM_COL32(120, 255, 120, 235);
		const float gap = 8.0f;
		const float arm = 22.0f;
		drawList->AddLine(ImVec2(cx - arm, cy), ImVec2(cx - gap, cy), colour, 1.5f);
		drawList->AddLine(ImVec2(cx + gap, cy), ImVec2(cx + arm, cy), colour, 1.5f);
		drawList->AddLine(ImVec2(cx, cy - arm), ImVec2(cx, cy - gap), colour, 1.5f);
		drawList->AddLine(ImVec2(cx, cy + gap), ImVec2(cx, cy + arm), colour, 1.5f);
		drawList->AddCircle(ImVec2(cx, cy), 2.0f, IM_COL32(255, 255, 255, 235), 8, 1.0f);
	}

	const scene::ViewmodelAsset* viewmodel = workbench.Viewmodel();
	if (!markTags || viewmodel == nullptr) {
		return;
	}
	const DirectX::XMMATRIX viewProjection =
	    workbench.GetCamera().ViewProjection(static_cast<float>(width) / static_cast<float>(height));
	const scene::SkeletonPose& pose = workbench.Pose();
	for (std::size_t i = 0; i < viewmodel->gun.model.bones.size(); ++i) {
		const assets::BoneData& bind = viewmodel->gun.model.bones[i];
		if (!bind.IsTag()) {
			continue;
		}
		const int poseIndex = i < pose.gunBoneToPose.size() ? pose.gunBoneToPose[i] : -1;
		if (poseIndex < 0 || static_cast<std::size_t>(poseIndex) >= pose.bones.size()) {
			continue;
		}
		const DirectX::XMFLOAT3& world = pose.bones[static_cast<std::size_t>(poseIndex)].worldPos;
		const DirectX::XMVECTOR clip =
		    DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&world), viewProjection);
		DirectX::XMFLOAT3 ndc;
		DirectX::XMStoreFloat3(&ndc, clip);
		const float x = (ndc.x * 0.5f + 0.5f) * static_cast<float>(width);
		const float y = (1.0f - (ndc.y * 0.5f + 0.5f)) * static_cast<float>(height);
		drawList->AddCircle(ImVec2(x, y), 5.0f, IM_COL32(255, 210, 80, 240), 12, 1.5f);
		drawList->AddText(ImVec2(x + 7.0f, y - 6.0f), IM_COL32(255, 220, 120, 240),
		                  bind.name.c_str());
	}
}

void DrawCaption(const std::string& caption) {
	ImGui::GetBackgroundDrawList()->AddText(ImVec2(10.0f, 8.0f), IM_COL32(255, 255, 255, 235),
	                                        caption.c_str());
}

bool DrawTextOverlay(Renderer& renderer, int width, int height, const std::string& text) {
	if (!BeginOverlay(renderer, width, height, 1.4f)) {
		return false;
	}
	DrawCaption(text);
	EndOverlay();
	return true;
}

bool RenderContactSheet(commands::Workbench& workbench, Renderer& renderer, const ShotOptions& options,
                        const std::filesystem::path& output, std::string& error) {
	const assets::XAnimData* anim = workbench.Anim();
	if (anim == nullptr) {
		error = "-sheet needs an animation (set_anim first)";
		return false;
	}
	const int cells = options.sheet;
	if (cells <= 0) {
		return false;
	}
	const int columns = cells <= 2 ? cells : (cells == 4 ? 2 : (cells <= 6 ? 3 : 4));
	const int rows = (cells + columns - 1) / columns;
	const int cellWidth = std::max(options.width, 1);
	const int cellHeight = std::max(options.height, 1);
	const double duration = anim->Duration();

	std::vector<std::uint8_t> sheetPixels;
	int sheetWidth = 0;
	int sheetHeight = 0;
	DXGI_FORMAT sheetFormat = DXGI_FORMAT_UNKNOWN;

	for (int index = 0; index < cells; ++index) {
		double t = 0.0;
		if (cells > 1) {
			const double step =
			    anim->looping ? static_cast<double>(index) / cells : static_cast<double>(index) / (cells - 1);
			t = duration * step;
		}
		workbench.SetAnimTime(t);
		if (!workbench.RenderScene(cellWidth, cellHeight)) {
			error = "sheet: failed to render cell " + std::to_string(index);
			return false;
		}
		char caption[192];
		std::snprintf(caption, sizeof(caption), "%s   frame %.1f   %.3f s", anim->name.c_str(),
		              t * anim->frameRate, t);
		if (!DrawTextOverlay(renderer, cellWidth, cellHeight, caption)) {
			error = "sheet: overlay failed";
			return false;
		}

		std::vector<std::uint8_t> cell;
		int capturedWidth = 0;
		int capturedHeight = 0;
		DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
		if (!renderer.CaptureScene(cell, capturedWidth, capturedHeight, format)) {
			error = "sheet: failed to capture cell " + std::to_string(index);
			return false;
		}
		if (sheetPixels.empty()) {
			sheetWidth = columns * capturedWidth;
			sheetHeight = rows * capturedHeight;
			sheetFormat = format;
			const std::size_t bpp = DirectX::BitsPerPixel(format) / 8;
			sheetPixels.assign(static_cast<std::size_t>(sheetWidth) * sheetHeight * bpp, 0);
			if (bpp == 4) {
				for (std::size_t pixel = 0; pixel < static_cast<std::size_t>(sheetWidth) * sheetHeight;
				     ++pixel) {
					sheetPixels[pixel * bpp + 3] = 255;
				}
			}
		}
		const std::size_t bpp = DirectX::BitsPerPixel(format) / 8;
		const int column = index % columns;
		const int row = index / columns;
		const std::size_t sheetRowBytes = static_cast<std::size_t>(sheetWidth) * bpp;
		const std::size_t cellRowBytes = static_cast<std::size_t>(capturedWidth) * bpp;
		for (int y = 0; y < capturedHeight; ++y) {
			const std::size_t dest = (static_cast<std::size_t>(row) * capturedHeight +
			                          static_cast<std::size_t>(y)) * sheetRowBytes +
			                         static_cast<std::size_t>(column) * capturedWidth * bpp;
			const std::size_t source = static_cast<std::size_t>(y) * cellRowBytes;
			std::memcpy(sheetPixels.data() + dest, cell.data() + source, cellRowBytes);
		}
	}

	std::error_code ec;
	const std::filesystem::path parent = output.parent_path();
	if (!parent.empty()) {
		std::filesystem::create_directories(parent, ec);
	}
	if (!renderer.SavePixelsToPng(output, sheetPixels, sheetWidth, sheetHeight, sheetFormat)) {
		error = "sheet: failed to save " + output.string();
		return false;
	}
	return true;
}

} // namespace

bool RenderShot(commands::Workbench& workbench, Renderer& renderer, const ShotOptions& options,
                const std::filesystem::path& output, std::string& error) {
	const int width = std::max(options.width, 1);
	const int height = std::max(options.height, 1);
	workbench.SetWireframe(options.wire);

	if (options.sheet > 0) {
		return RenderContactSheet(workbench, renderer, options, output, error);
	}

	if (!workbench.RenderScene(width, height)) {
		error = "failed to render the scene";
		return false;
	}

	if (options.bones || options.crosshair || options.markTags || !options.caption.empty()) {
		if (!BeginOverlay(renderer, width, height, options.caption.empty() ? 1.0f : 1.4f)) {
			error = "overlay init failed";
			return false;
		}
		if (options.bones) {
			DrawBones(workbench, width, height);
		}
		if (options.crosshair || options.markTags) {
			DrawDebug(workbench, width, height, options.crosshair, options.markTags);
		}
		if (!options.caption.empty()) {
			DrawCaption(options.caption);
		}
		EndOverlay();
	}

	std::error_code ec;
	const std::filesystem::path parent = output.parent_path();
	if (!parent.empty()) {
		std::filesystem::create_directories(parent, ec);
	}
	if (!renderer.SaveSceneToPng(output)) {
		error = "failed to save " + output.string();
		return false;
	}
	return true;
}

} // namespace wb
