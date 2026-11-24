#include "app/diag.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

#include "app/commands.h"
#include "assets/xanim.h"

namespace wb {
namespace {

const char* SourceName(const scene::PoseBone& bone) { return bone.fromHands ? "hands" : "gun"; }

int FindPoseBone(const scene::SkeletonPose& pose, const std::string& name) {
	for (std::size_t i = 0; i < pose.bones.size(); ++i) {
		if (pose.bones[i].name == name) {
			return static_cast<int>(i);
		}
	}
	return -1;
}

void PrintVec3(std::FILE* out, const char* label, const DirectX::XMFLOAT3& value) {
	std::fprintf(out, "%-12s (%8.3f %8.3f %8.3f)", label, value.x, value.y, value.z);
}

void PrintQuat(std::FILE* out, const DirectX::XMFLOAT4& value) {
	std::fprintf(out, " quat (%7.4f %7.4f %7.4f %7.4f)", value.x, value.y, value.z, value.w);
}

const char* ChannelLabel(const assets::AnimChannel& channel) {
	if (channel.HasRotation() && channel.HasTranslation()) {
		return "rot+trans";
	}
	if (channel.HasRotation()) {
		return "rot";
	}
	if (channel.HasTranslation()) {
		return "trans";
	}
	return "none";
}

void PrintChannel(std::FILE* out, const assets::XAnimData& anim, const std::string& name, float time) {
	const assets::AnimChannel* channel = anim.FindChannel(name);
	if (channel == nullptr) {
		std::fprintf(out, "  %-16s anim: no channel\n", name.c_str());
		return;
	}
	std::fprintf(out, "  %-16s anim: %-9s rot keys %-3zu trans keys %-3zu", name.c_str(),
	             ChannelLabel(*channel), channel->rotations.size(), channel->translations.size());
	if (channel->HasRotation()) {
		std::fprintf(out, "  frames %u..%u", channel->rotations.front().frame,
		             channel->rotations.back().frame);
	}
	if (channel->HasTranslation()) {
		std::fprintf(out, "  trans frames %u..%u", channel->translations.front().frame,
		             channel->translations.back().frame);
	}
	std::fprintf(out, "\n");

	DirectX::XMFLOAT4 rotation;
	DirectX::XMFLOAT3 translation;
	const bool hasRotation = anim.SampleRotation(name, time, rotation);
	const bool hasTranslation = anim.SampleTranslation(name, time, translation);
	std::fprintf(out, "  %-16s at %.3fs  ", name.c_str(), time);
	if (hasRotation) {
		PrintQuat(out, rotation);
	} else {
		std::fprintf(out, " rot (none)");
	}
	if (hasTranslation) {
		PrintVec3(out, " trans", translation);
	} else {
		std::fprintf(out, " trans (none)");
	}
	std::fprintf(out, "\n");
	if (channel->HasRotation() && channel->rotations.size() > 1) {
		const auto& first = channel->rotations.front().value;
		const auto& last = channel->rotations.back().value;
		std::fprintf(out, "  %-16s first frame %u", name.c_str(), channel->rotations.front().frame);
		PrintQuat(out, first);
		std::fprintf(out, "\n  %-16s last  frame %u", name.c_str(), channel->rotations.back().frame);
		PrintQuat(out, last);
		std::fprintf(out, "\n");
	}
	if (channel->HasTranslation() && channel->translations.size() > 1) {
		const auto& first = channel->translations.front().value;
		const auto& last = channel->translations.back().value;
		std::fprintf(out, "  %-16s first frame %u", name.c_str(), channel->translations.front().frame);
		PrintVec3(out, " trans", first);
		std::fprintf(out, "\n  %-16s last  frame %u", name.c_str(), channel->translations.back().frame);
		PrintVec3(out, " trans", last);
		std::fprintf(out, "\n");
	}
}

} // namespace

std::vector<std::string> DefaultPoseDumpBones() {
	return {"tag_view",   "tag_camera", "tag_torso",  "tag_player", "tag_weapon",
	        "tag_ads",    "j_gun",      "j_wrist_le", "j_wrist_ri"};
}

void PrintPoseDump(const commands::Workbench& workbench, const std::vector<std::string>& names,
                   int width, int height, std::FILE* out) {
	const scene::ViewmodelAsset* viewmodel = workbench.Viewmodel();
	if (viewmodel == nullptr) {
		std::fprintf(out, "posedump: no weapon loaded\n");
		return;
	}
	const int screenWidth = std::max(width, 1);
	const int screenHeight = std::max(height, 1);
	const DirectX::XMMATRIX viewProjection =
	    workbench.GetCamera().ViewProjection(static_cast<float>(screenWidth) / static_cast<float>(screenHeight));
	const auto project = [&viewProjection, screenWidth, screenHeight](const DirectX::XMFLOAT3& world,
	                                                                  float& x, float& y) {
		const DirectX::XMVECTOR clip =
		    DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&world), viewProjection);
		DirectX::XMFLOAT3 ndc;
		DirectX::XMStoreFloat3(&ndc, clip);
		x = (ndc.x * 0.5f + 0.5f) * static_cast<float>(screenWidth);
		y = (1.0f - (ndc.y * 0.5f + 0.5f)) * static_cast<float>(screenHeight);
	};

	std::fprintf(out, "weapon: %s\n", viewmodel->weapon.name.c_str());
	std::fprintf(out, "gun: %s   hands: %s (%s)\n", viewmodel->weapon.gunModel.c_str(),
	             viewmodel->handsName.empty() ? "(none)" : viewmodel->handsName.c_str(),
	             viewmodel->handsSource.c_str());
	if (const assets::XAnimData* anim = workbench.Anim()) {
		std::fprintf(out, "anim: %s  frames %u  %.3f fps  time %.4f s  frame %.2f%s\n",
		             anim->name.c_str(), anim->frameCount, anim->frameRate, workbench.AnimTime(),
		             workbench.AnimFrame(), workbench.AdditiveEnabled() ? "  (additive)" : "");
	} else {
		std::fprintf(out, "anim: (none)\n");
	}
	std::fprintf(out, "ads: %s   additive: %s\n", workbench.Aiming() ? "yes" : "no",
	             workbench.AdditiveEnabled() ? "yes" : "no");
	for (const char* state : {"IDLE", "ADS_UP"}) {
		const std::string effective = workbench.EffectiveAnimForState(state);
		if (!effective.empty()) {
			std::fprintf(out, "effective %-7s anim: %s\n", state, effective.c_str());
		}
	}
	std::fprintf(out, "camera eye (hands tag_view): (%.3f %.3f %.3f)\n", viewmodel->viewOrigin.x,
	             viewmodel->viewOrigin.y, viewmodel->viewOrigin.z);
	std::fprintf(out, "gun bind bounds: mid (%.3f %.3f %.3f) half (%.3f %.3f %.3f)\n",
	             viewmodel->gun.boundsCenter.x, viewmodel->gun.boundsCenter.y,
	             viewmodel->gun.boundsCenter.z, viewmodel->gun.boundsHalf.x,
	             viewmodel->gun.boundsHalf.y, viewmodel->gun.boundsHalf.z);
	std::fprintf(out, "\n");

	const scene::SkeletonPose& pose = workbench.Pose();
	std::fprintf(out, "posed bones (world = viewmodel space, screen px at %dx%d):\n", screenWidth,
	             screenHeight);
	std::fprintf(out, "  %-14s %-5s %-6s %-8s %-29s %-29s %s\n", "name", "idx", "src", "parent",
	             "local position", "world position", "screen");
	for (const std::string& name : names) {
		const int index = FindPoseBone(pose, name);
		if (index < 0) {
			std::fprintf(out, "  %-14s missing\n", name.c_str());
			continue;
		}
		const scene::PoseBone& bone = pose.bones[static_cast<std::size_t>(index)];
		const std::string parent =
		    bone.parent >= 0 && static_cast<std::size_t>(bone.parent) < pose.bones.size()
		        ? pose.bones[static_cast<std::size_t>(bone.parent)].name
		        : std::string("-");
		float screenX = 0.0f;
		float screenY = 0.0f;
		project(bone.worldPos, screenX, screenY);
		std::fprintf(out,
		             "  %-14s %-5d %-6s %-8s (%8.3f %8.3f %8.3f) (%8.3f %8.3f %8.3f) (%6.1f %6.1f)\n",
		             bone.name.c_str(), index, SourceName(bone), parent.c_str(), bone.localPos.x,
		             bone.localPos.y, bone.localPos.z, bone.worldPos.x, bone.worldPos.y,
		             bone.worldPos.z, screenX, screenY);
	}
	std::fprintf(out, "\n");

	// Gun bounds after the gun root's skin transform (the model bounds are rigid to the root).
	const std::vector<DirectX::XMFLOAT4X4>& palette = workbench.GunPalette();
	if (!palette.empty() && !viewmodel->gun.model.bones.empty()) {
		const DirectX::XMMATRIX root = DirectX::XMLoadFloat4x4(&palette.front());
		DirectX::XMFLOAT3 minBounds(1.0e30f, 1.0e30f, 1.0e30f);
		DirectX::XMFLOAT3 maxBounds(-1.0e30f, -1.0e30f, -1.0e30f);
		for (int corner = 0; corner < 8; ++corner) {
			const DirectX::XMFLOAT3 point = {
			    viewmodel->gun.boundsCenter.x + ((corner & 1) ? 1.0f : -1.0f) * viewmodel->gun.boundsHalf.x,
			    viewmodel->gun.boundsCenter.y + ((corner & 2) ? 1.0f : -1.0f) * viewmodel->gun.boundsHalf.y,
			    viewmodel->gun.boundsCenter.z + ((corner & 4) ? 1.0f : -1.0f) * viewmodel->gun.boundsHalf.z};
			DirectX::XMFLOAT3 transformed;
			DirectX::XMStoreFloat3(&transformed,
			                       DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&point), root));
			minBounds.x = std::min(minBounds.x, transformed.x);
			minBounds.y = std::min(minBounds.y, transformed.y);
			minBounds.z = std::min(minBounds.z, transformed.z);
			maxBounds.x = std::max(maxBounds.x, transformed.x);
			maxBounds.y = std::max(maxBounds.y, transformed.y);
			maxBounds.z = std::max(maxBounds.z, transformed.z);
		}
		std::fprintf(out, "gun posed bounds: min (%.3f %.3f %.3f) max (%.3f %.3f %.3f)  size (%.3f %.3f %.3f)\n",
		             minBounds.x, minBounds.y, minBounds.z, maxBounds.x, maxBounds.y, maxBounds.z,
		             maxBounds.x - minBounds.x, maxBounds.y - minBounds.y, maxBounds.z - minBounds.z);
		std::fprintf(out, "gun rear x %.3f (camera eye x %.3f), forward x %.3f\n", minBounds.x,
		             viewmodel->viewOrigin.x, maxBounds.x);
	}

	if (const assets::XAnimData* anim = workbench.Anim()) {
		std::fprintf(out, "\nanim channels for the root/viewmodel bones (anim %s):\n", anim->name.c_str());
		for (const std::string& name : names) {
			PrintChannel(out, *anim, name, 0.0f);
		}
	}
	std::fprintf(out, "\ngun tags (posed, viewmodel space), distance to the camera eye and screen px:\n");
	for (std::size_t i = 0; i < viewmodel->gun.model.bones.size(); ++i) {
		const assets::BoneData& bind = viewmodel->gun.model.bones[i];
		if (!bind.IsTag()) {
			continue;
		}
		const int poseIndex =
		    i < pose.gunBoneToPose.size() ? pose.gunBoneToPose[i] : -1;
		if (poseIndex < 0 || static_cast<std::size_t>(poseIndex) >= pose.bones.size()) {
			continue;
		}
		const DirectX::XMFLOAT3& world = pose.bones[static_cast<std::size_t>(poseIndex)].worldPos;
		const float dx = world.x - viewmodel->viewOrigin.x;
		const float dy = world.y - viewmodel->viewOrigin.y;
		const float dz = world.z - viewmodel->viewOrigin.z;
		float screenX = 0.0f;
		float screenY = 0.0f;
		project(world, screenX, screenY);
		std::fprintf(out, "  %-24s (%8.3f %8.3f %8.3f)  dist %7.3f  screen (%6.1f %6.1f)\n",
		             bind.name.c_str(), world.x, world.y, world.z,
		             std::sqrt(dx * dx + dy * dy + dz * dz), screenX, screenY);
	}
	std::fprintf(out, "screen centre: (%d %d)\n", screenWidth / 2, screenHeight / 2);
	{
		const Camera& camera = workbench.GetCamera();
		const DirectX::XMFLOAT3 eye = camera.Eye();
		const DirectX::XMFLOAT3 target = camera.LookTarget();
		std::fprintf(out, "camera: eye (%.3f %.3f %.3f) target (%.3f %.3f %.3f) fov %.3f deg\n", eye.x,
		             eye.y, eye.z, target.x, target.y, target.z, camera.Fov());
	}
	std::fprintf(out, "\n");
	if (!workbench.Attachments().empty()) {
		std::fprintf(out, "attachments:\n");
		for (const commands::AttachmentMount& mount : workbench.Attachments()) {
			float worldX = 0.0f;
			float worldY = 0.0f;
			project(DirectX::XMFLOAT3(mount.world.m[3][0], mount.world.m[3][1], mount.world.m[3][2]),
			        worldX, worldY);
			std::fprintf(out, "  %-24s vm %-36s tag %-20s %s  world origin screen (%6.1f %6.1f)%s\n",
			             mount.name.c_str(), mount.viewModel.c_str(), mount.requiredTag.c_str(),
			             mount.tagFound ? "found" : "MISSING", worldX, worldY,
			             mount.hidden ? "  hidden" : "");
			for (std::size_t i = 0; i < mount.reticleWorlds.size() && i < mount.reticles.size(); ++i) {
				const DirectX::XMMATRIX world = DirectX::XMLoadFloat4x4(&mount.reticleWorlds[i]);
				const DirectX::XMVECTOR aim = DirectX::XMVector3Transform(
				    DirectX::XMLoadFloat3(&mount.reticles[i].boundsCenter), world);
				DirectX::XMFLOAT3 aimPoint;
				DirectX::XMStoreFloat3(&aimPoint, aim);
				float aimX = 0.0f;
				float aimY = 0.0f;
				project(aimPoint, aimX, aimY);
				std::fprintf(out, "    reticle %-32s aim (%8.3f %8.3f %8.3f) screen (%6.1f %6.1f)\n",
				             mount.reticles[i].name.c_str(), aimPoint.x, aimPoint.y, aimPoint.z, aimX,
				             aimY);
			}
		}
		std::fprintf(out, "\n");
	}
}

} // namespace wb
