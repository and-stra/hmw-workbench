#include "app/model_overlay.h"

#include <algorithm>
#include <cmath>

namespace wb {
namespace {

constexpr ImU32 kTagColor = IM_COL32(255, 210, 80, 255);
constexpr ImU32 kBoneColor = IM_COL32(140, 200, 255, 220);
constexpr ImU32 kSelectedColor = IM_COL32(0, 255, 255, 255);
constexpr ImU32 kTextShadow = IM_COL32(0, 0, 0, 200);

bool ProjectPoint(const DirectX::XMFLOAT4X4& viewProjection, const DirectX::XMFLOAT3& point,
                  const ImVec2& origin, const ImVec2& size, ImVec2& output) {
	const DirectX::XMVECTOR position = DirectX::XMVectorSet(point.x, point.y, point.z, 1.0f);
	const DirectX::XMVECTOR clip = DirectX::XMVector4Transform(position, DirectX::XMLoadFloat4x4(&viewProjection));
	const float w = DirectX::XMVectorGetW(clip);
	if (w <= 1.0e-5f) {
		return false;
	}
	const float ndcX = DirectX::XMVectorGetX(clip) / w;
	const float ndcY = DirectX::XMVectorGetY(clip) / w;
	if (ndcX < -1.2f || ndcX > 1.2f || ndcY < -1.2f || ndcY > 1.2f) {
		return false;
	}
	output.x = origin.x + (ndcX * 0.5f + 0.5f) * size.x;
	output.y = origin.y + (0.5f - ndcY * 0.5f) * size.y;
	return true;
}

void DrawCross(ImDrawList* drawList, const ImVec2& position, ImU32 color, float radius) {
	drawList->AddLine(ImVec2(position.x - radius, position.y), ImVec2(position.x + radius, position.y), color, 1.5f);
	drawList->AddLine(ImVec2(position.x, position.y - radius), ImVec2(position.x, position.y + radius), color, 1.5f);
}

void DrawLabel(ImDrawList* drawList, const ImVec2& position, ImU32 color, const std::string& text) {
	drawList->AddText(ImVec2(position.x + 1.0f, position.y + 1.0f), kTextShadow, text.c_str());
	drawList->AddText(position, color, text.c_str());
}

} // namespace

void DrawModelOverlay(ImDrawList* drawList, const ImVec2& origin, const ImVec2& size,
                      const DirectX::XMFLOAT4X4& viewProjection, const assets::XModelData& model,
                      int selectedBone, bool showAllNames) {
	if (drawList == nullptr) {
		return;
	}

	std::vector<ImVec2> screenPositions(model.bones.size());
	std::vector<bool> visible(model.bones.size(), false);
	for (std::size_t i = 0; i < model.bones.size(); ++i) {
		visible[i] = ProjectPoint(viewProjection, model.bones[i].worldPos, origin, size, screenPositions[i]);
	}

	// Bones first, then tags and names on top.
	for (std::size_t i = 0; i < model.bones.size(); ++i) {
		const assets::BoneData& bone = model.bones[i];
		if (bone.parent < 0 || !visible[i] || !visible[static_cast<std::size_t>(bone.parent)]) {
			continue;
		}
		const ImU32 color = bone.IsTag() ? kTagColor : kBoneColor;
		drawList->AddLine(screenPositions[static_cast<std::size_t>(bone.parent)], screenPositions[i], color, 1.0f);
	}

	for (std::size_t i = 0; i < model.bones.size(); ++i) {
		if (!visible[i]) {
			continue;
		}
		const assets::BoneData& bone = model.bones[i];
		const bool selected = static_cast<int>(i) == selectedBone;
		const ImU32 color = selected ? kSelectedColor : (bone.IsTag() ? kTagColor : kBoneColor);
		DrawCross(drawList, screenPositions[i], color, selected ? 6.0f : (bone.IsTag() ? 4.5f : 3.0f));

		if (showAllNames || bone.IsTag() || selected) {
			DrawLabel(drawList, ImVec2(screenPositions[i].x + 6.0f, screenPositions[i].y - 6.0f), color,
			          bone.name);
		}
	}
}

} // namespace wb
