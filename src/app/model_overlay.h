#pragma once

#include <imgui.h>
#include <DirectXMath.h>

#include "assets/xmodel.h"

namespace wb {

// Draws the bone/tag overlay with names into an ImGui draw list. Used by the windowed
// viewport and by the headless -bones screenshot path (origin (0,0), size = image size).
void DrawModelOverlay(ImDrawList* drawList, const ImVec2& origin, const ImVec2& size,
                      const DirectX::XMFLOAT4X4& viewProjection, const assets::XModelData& model,
                      int selectedBone, bool showAllNames);

} // namespace wb
