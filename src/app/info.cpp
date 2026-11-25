#include "app/info.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <set>
#include <string>
#include <vector>

#include "app/commands.h"
#include "assets/attachment.h"
#include "assets/material.h"
#include "assets/model_asset.h"
#include "assets/weapon.h"
#include "assets/xanim.h"
#include "assets/xmodel_document.h"
#include "assets/xsurface.h"

namespace wb {
namespace {

void PrintBounds(const assets::ModelAsset& model) {
	std::printf("bounds: mid (%.3f %.3f %.3f) half (%.3f %.3f %.3f)\n", model.boundsCenter.x,
	            model.boundsCenter.y, model.boundsCenter.z, model.boundsHalf.x, model.boundsHalf.y,
	            model.boundsHalf.z);
}

} // namespace

bool PrintModelInfo(const std::string& name, const std::vector<std::filesystem::path>& roots) {
	assets::ModelAsset model;
	std::string error;
	if (!commands::LoadModelForInfo(roots, name, model, error)) {
		std::fprintf(stderr, "-info %s: %s\n", name.c_str(), error.c_str());
		return false;
	}

	const assets::XModelData& data = model.model;
	std::uint32_t totalVerts = 0;
	std::uint32_t totalTris = 0;
	for (const auto& surface : model.surfaceSet.surfaces) {
		totalVerts += surface.vertexCount;
		totalTris += surface.triangleCount;
	}

	std::printf("model: %s\n", data.name.c_str());
	std::printf("xmodel: %s\n", data.path.string().c_str());
	std::printf("bones: %u (%u root%s)\n", data.numBones, data.numRootBones, data.numRootBones == 1 ? "" : "s");
	std::printf("surfaces: %zu (%u vertices, %u triangles)\n", model.surfaceSet.surfaces.size(), totalVerts,
	            totalTris);
	std::printf("scale: %.4f  flags: 0x%04X\n", data.scale, data.flags);
	PrintBounds(model);
	std::printf("lod0 surfaces: %s\n", data.lodSurfaces[0].c_str());
	std::printf("xsurface: %s\n", model.surfaceSet.path.string().c_str());
	std::printf("\n");

	int tagCount = 0;
	for (const auto& bone : data.bones) {
		if (bone.IsTag()) {
			++tagCount;
		}
	}
	std::printf("tags: %d\n", tagCount);
	for (std::size_t i = 0; i < data.bones.size(); ++i) {
		const assets::BoneData& bone = data.bones[i];
		if (!bone.IsTag()) {
			continue;
		}
		std::printf("  %-28s bone %-3zu world (%.3f %.3f %.3f)  tagpos (%.3f %.3f %.3f)  angle (%d %d %d %d)\n",
		            bone.name.c_str(), i, bone.worldPos.x, bone.worldPos.y, bone.worldPos.z,
		            bone.tagPosition.x, bone.tagPosition.y, bone.tagPosition.z, bone.tagAngle[0],
		            bone.tagAngle[1], bone.tagAngle[2], bone.tagAngle[3]);
	}
	std::printf("\n");

	std::printf("bones:\n");
	std::printf("  %-4s %-28s %-6s %-29s %-27s %s\n", "idx", "name", "parent", "local position",
	            "local quaternion", "world position");
	for (std::size_t i = 0; i < data.bones.size(); ++i) {
		const assets::BoneData& bone = data.bones[i];
		std::printf("  %-4zu %-28s %-6d (%.3f %.3f %.3f)%*s(%.4f %.4f %.4f %.4f)  (%.3f %.3f %.3f)\n", i,
		            bone.name.c_str(), bone.parent, bone.localPos.x, bone.localPos.y, bone.localPos.z,
		            2, "", bone.localQuat.x, bone.localQuat.y, bone.localQuat.z, bone.localQuat.w,
		            bone.worldPos.x, bone.worldPos.y, bone.worldPos.z);
	}
	std::printf("\n");

	std::printf("surfaces (LOD0):\n");
	for (std::size_t i = 0; i < model.surfaceSet.surfaces.size(); ++i) {
		const assets::SurfaceData& surface = model.surfaceSet.surfaces[i];
		std::printf("  [%2zu] verts=%-6u tris=%-6u flags=0x%04X rigid=%u  material=%s\n", i,
		            surface.vertexCount, surface.triangleCount, surface.flags, surface.rigidVertListCount,
		            surface.materialName.c_str());
		if (!surface.vertexWeights.empty() && !data.bones.empty()) {
			std::vector<double> totals(data.bones.size(), 0.0);
			for (const assets::VertexWeights& weights : surface.vertexWeights) {
				for (int influence = 0; influence < 4; ++influence) {
					if (weights.boneIndex[influence] < totals.size() && weights.weight[influence] > 0.0f) {
						totals[weights.boneIndex[influence]] += weights.weight[influence];
					}
				}
			}
			const auto dominant =
			    std::max_element(totals.begin(), totals.end());
			double sum = 0.0;
			for (double value : totals) {
				sum += value;
			}
			if (dominant != totals.end() && *dominant > 0.0 && sum > 0.0) {
				const std::size_t boneIndex = static_cast<std::size_t>(dominant - totals.begin());
				std::printf("       weights: %.0f%% bone %zu %s\n", 100.0 * *dominant / sum, boneIndex,
				            data.bones[boneIndex].name.c_str());
			}
		}
		if (!surface.blendWeightsRaw.empty() || !surface.blendTableRaw.empty()) {
			std::printf("       blend weights=%zu bytes (XBlendInfo), table=%zu bytes\n",
			            surface.blendWeightsRaw.size(), surface.blendTableRaw.size());
		}
	}
	std::printf("\n");

	std::printf("materials: %zu\n", model.materials.size());
	for (const assets::MaterialSlot& slot : model.materials) {
		std::printf("  %s\n", slot.material.name.c_str());
		if (!slot.jsonFound) {
			std::printf("    json: missing (%s)\n", slot.error.c_str());
			continue;
		}
		std::printf("    json: %s\n", slot.material.path.string().c_str());
		std::printf("    techset: %s\n", slot.material.techniqueSet.c_str());
		for (const assets::MaterialTextureInfo& texture : slot.textures) {
			std::printf("    %-10s %-44s %s\n", assets::TextureSemanticName(texture.semantic),
			            texture.imageName.c_str(), texture.fileFound ? "image file found" : "IMAGE MISSING");
		}
	}

	if (!model.warnings.empty()) {
		std::printf("\nwarnings (%zu):\n", model.warnings.size());
		for (const std::string& warning : model.warnings) {
			std::printf("  %s\n", warning.c_str());
		}
	}

	return true;
}

int RunValidate(const std::vector<std::filesystem::path>& roots) {
	std::vector<std::filesystem::path> modelFiles;
	for (const auto& root : roots) {
		const std::filesystem::path folder = root / "xmodel";
		std::error_code ec;
		for (std::filesystem::directory_iterator it(folder, ec), end; !ec && it != end; it.increment(ec)) {
			std::error_code fileEc;
			if (it->is_regular_file(fileEc) && it->path().extension() == ".xmb") {
				modelFiles.push_back(it->path());
			}
		}
	}
	std::sort(modelFiles.begin(), modelFiles.end());

	int modelOk = 0;
	int modelFailed = 0;
	int surfaceOk = 0;
	int surfaceFailed = 0;
	int materialOk = 0;
	int materialFailed = 0;
	std::set<std::string> checkedSurfaces;
	std::set<std::string> missingSurfaces;
	std::set<std::string> modelsMissingSurfaces;
	std::set<std::string> checkedMaterials;
	std::set<std::string> missingMaterials;

	const auto fail = [](const char* kind, const std::filesystem::path& path, const std::string& reason) {
		std::printf("FAIL %s %s: %s\n", kind, path.string().c_str(), reason.c_str());
	};

	for (const auto& modelPath : modelFiles) {
		assets::XModelData model;
		try {
			model = assets::LoadXModel(modelPath);
		} catch (const std::exception& e) {
			fail("xmodel", modelPath, e.what());
			++modelFailed;
			continue;
		}
		++modelOk;

		// Validate all LOD surface sets referenced by this model.
		for (const std::string& surfaceName : model.lodSurfaces) {
			if (surfaceName.empty()) {
				continue;
			}
			const std::optional<std::filesystem::path> surfacePath =
			    assets::FindXSurfaceFile(roots, surfaceName);
			if (!surfacePath.has_value()) {
				modelsMissingSurfaces.insert(modelPath.string());
				if (missingSurfaces.insert(surfaceName).second) {
					fail("xsurface", surfaceName, "file not found");
					++surfaceFailed;
				}
				continue;
			}
			const std::string key = surfacePath->string();
			if (!checkedSurfaces.insert(key).second) {
				continue;
			}
			try {
				const assets::XSurfaceSet set = assets::LoadXSurface(*surfacePath);
				++surfaceOk;
				for (std::size_t surfaceIndex = 0; surfaceIndex < set.surfaces.size(); ++surfaceIndex) {
					if (surfaceIndex >= model.materialNames.size()) {
						continue;
					}
					const std::string& materialName = model.materialNames[surfaceIndex];
					if (materialName.empty()) {
						continue;
					}
					const std::optional<std::filesystem::path> materialPath =
					    assets::FindMaterialFile(roots, materialName);
					if (!materialPath.has_value()) {
						if (missingMaterials.insert(materialName).second) {
							fail("material", materialName, "material json not found");
							++materialFailed;
						}
						continue;
					}
					if (!checkedMaterials.insert(materialPath->string()).second) {
						continue;
					}
					try {
						(void)assets::LoadMaterial(*materialPath);
						++materialOk;
					} catch (const std::exception& e) {
						fail("material", *materialPath, e.what());
						++materialFailed;
					}
				}
			} catch (const std::exception& e) {
				fail("xsurface", *surfacePath, e.what());
				++surfaceFailed;
			}
		}
	}

	int attachmentNames = 0;
	int duplicateAttachments = 0;
	{
		std::set<std::string> names;
		for (const auto& root : roots) {
			const std::filesystem::path folder = root / "attachments";
			std::error_code ec;
			for (std::filesystem::directory_iterator it(folder, ec), end; !ec && it != end; it.increment(ec)) {
				std::error_code fileEc;
				if (it->is_regular_file(fileEc) && it->path().extension() == ".json") {
					names.insert(it->path().stem().string());
				}
			}
		}
		attachmentNames = static_cast<int>(names.size());
		for (const std::string& name : names) {
			const std::vector<std::filesystem::path> definitions = assets::FindAttachmentDefinitions(roots, name);
			if (definitions.size() < 2) {
				continue;
			}
			++duplicateAttachments;
			std::printf("DUPLICATE attachment %s: %zu definitions (using the first with models)\n",
			            name.c_str(), definitions.size());
			for (const std::filesystem::path& path : definitions) {
				std::printf("  %s (%s)\n", path.string().c_str(),
				            assets::AttachmentHasModels(path) ? "models" : "stub, no models");
			}
		}
	}

	std::printf("\nsummary:\n");
	std::printf("  xmodels:   ok %d, failed %d\n", modelOk, modelFailed);
	std::printf("  xsurfaces: ok %d, failed %d\n", surfaceOk, surfaceFailed);
	std::printf("  materials: ok %d, failed %d\n", materialOk, materialFailed);
	std::printf("  missing surface files: %zu, missing materials: %zu\n", missingSurfaces.size(),
	            missingMaterials.size());
	std::printf("  models with missing surface data: %zu\n", modelsMissingSurfaces.size());
	std::printf("  attachments: %d names, %d duplicate definitions\n", attachmentNames,
	            duplicateAttachments);
	return modelFailed + surfaceFailed + materialFailed;
}

int RunRoundTrip(const std::vector<std::filesystem::path>& roots, const std::filesystem::path& outputDir,
                 std::FILE* out) {
	int total = 0;
	int identical = 0;
	int different = 0;
	int errors = 0;
	std::vector<std::filesystem::path> files;
	for (std::size_t rootIndex = 0; rootIndex < roots.size(); ++rootIndex) {
		const std::filesystem::path folder = roots[rootIndex] / "xmodel";
		std::error_code ec;
		for (std::filesystem::directory_iterator it(folder, ec), end; !ec && it != end; it.increment(ec)) {
			std::error_code fileEc;
			if (it->is_regular_file(fileEc) && it->path().extension() == ".xmb") {
				files.push_back(it->path());
			}
		}
	}
	std::sort(files.begin(), files.end());

	for (const std::filesystem::path& source : files) {
		++total;
		std::vector<std::uint8_t> original;
		{
			std::ifstream in(source, std::ios::binary);
			original.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
		}
		if (original.empty()) {
			std::fprintf(out, "FAIL read %s\n", source.string().c_str());
			++errors;
			continue;
		}

		assets::XModelDocument document;
		std::string error;
		if (!assets::LoadXModelDocument(source, document, error)) {
			std::fprintf(out, "FAIL parse %s: %s\n", source.string().c_str(), error.c_str());
			++errors;
			continue;
		}

		std::filesystem::path target = outputDir / source.parent_path().parent_path().filename() /
		                               source.filename();
		std::error_code ec;
		std::filesystem::create_directories(target.parent_path(), ec);
		if (!assets::SaveXModelDocument(document, target, error)) {
			std::fprintf(out, "FAIL write %s: %s\n", source.string().c_str(), error.c_str());
			++errors;
			continue;
		}

		std::vector<std::uint8_t> written;
		{
			std::ifstream in(target, std::ios::binary);
			written.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
		}
		if (written == original) {
			++identical;
		} else {
			++different;
			std::size_t first = 0;
			const std::size_t limit = std::min(original.size(), written.size());
			while (first < limit && original[first] == written[first]) {
				++first;
			}
			std::fprintf(out, "DIFF %s (original %zu B, written %zu B, first difference at %zu)\n",
			             source.string().c_str(), original.size(), written.size(), first);
		}

		// Re-read the written file: it must parse and keep the bone count/name.
		try {
			const assets::XModelData reloaded = assets::LoadXModel(target);
			if (reloaded.numBones != document.model.numBones || reloaded.name != document.model.name) {
				std::fprintf(out, "FAIL reload %s: bone count/name changed\n", target.string().c_str());
				++errors;
			}
		} catch (const std::exception& e) {
			std::fprintf(out, "FAIL reload %s: %s\n", target.string().c_str(), e.what());
			++errors;
		}
	}

	std::fprintf(out, "\nroundtrip summary:\n");
	std::fprintf(out, "  files:     %d\n", total);
	std::fprintf(out, "  identical: %d\n", identical);
	std::fprintf(out, "  different: %d\n", different);
	std::fprintf(out, "  errors:    %d\n", errors);
	std::fprintf(out, "  output:    %s\n", outputDir.string().c_str());
	return different + errors;
}

bool PrintAnimInfo(const std::string& name, const std::vector<std::filesystem::path>& roots) {
	assets::XAnimData anim;
	std::string error;
	if (!commands::LoadAnimForInfo(roots, name, anim, error)) {
		std::fprintf(stderr, "-info %s: %s\n", name.c_str(), error.c_str());
		return false;
	}

	std::printf("anim: %s\n", anim.name.c_str());
	std::printf("file: %s\n", anim.path.string().c_str());
	std::printf("frames: %u (keys 0..%u)  framerate: %.4f  duration: %.4f s\n", anim.frameCount, anim.frameCount,
	            anim.frameRate, anim.Duration());
	std::printf("flags: 0x%02X%s%s  assetType: %u%s\n", anim.flags, anim.looping ? "  loop" : "",
	            anim.delta ? "  delta" : "", anim.assetType, anim.additive ? "  additive" : "");
	std::printf("bones: %zu\n", anim.boneNames.size());
	for (std::size_t i = 0; i < anim.boneNames.size(); ++i) {
		const assets::AnimChannel& channel = anim.channels[i];
		std::printf("  [%3zu] %-28s rot %3zu  trans %3zu\n", i, anim.boneNames[i].c_str(),
		            channel.rotations.size(), channel.translations.size());
	}
	std::printf("notetracks: %zu\n", anim.notifies.size());
	for (const assets::AnimNotify& note : anim.notifies) {
		std::printf("  frame %3u  %8.4f s  %6.4f  %s\n", note.frame, note.time, note.fraction,
		            note.name.c_str());
	}
	if (!anim.deltaRotations.empty() || !anim.deltaTranslations.empty()) {
		std::printf("delta parts: %zu rotations, %zu translations\n", anim.deltaRotations.size(),
		            anim.deltaTranslations.size());
	} else {
		std::printf("delta parts: none\n");
	}
	return true;
}

bool PrintWeaponInfo(const std::string& name, const std::vector<std::filesystem::path>& roots) {
	const std::optional<std::filesystem::path> path = assets::FindWeaponFile(roots, name);
	if (!path.has_value()) {
		std::fprintf(stderr, "-info %s: weapon json not found\n", name.c_str());
		return false;
	}
	assets::WeaponAsset weapon;
	try {
		weapon = assets::LoadWeapon(*path);
	} catch (const std::exception& e) {
		std::fprintf(stderr, "-info %s: %s\n", name.c_str(), e.what());
		return false;
	}
	std::printf("weapon: %s\n", weapon.name.c_str());
	std::printf("internal: %s\n", weapon.internalName.c_str());
	std::printf("gun model: %s\n", weapon.gunModel.c_str());
	std::printf("world model: %s\n", weapon.worldModel.c_str());
	std::printf("hand model: %s\n", weapon.handModel.empty() ? "(none)" : weapon.handModel.c_str());
	std::printf("adsZoomFov: %.3f\n", weapon.adsZoomFov);
	std::printf("camo view material: %s\n",
	            weapon.camoViewModelMaterialOverride.empty() ? "(none)"
	                                                        : weapon.camoViewModelMaterialOverride.c_str());
	std::printf("hideTags (%zu):", weapon.hideTags.size());
	for (const std::string& tag : weapon.hideTags) {
		std::printf(" %s", tag.c_str());
	}
	std::printf("\n");
	std::printf("attachments (%zu):", weapon.attachments.size());
	for (const std::string& attachment : weapon.attachments) {
		std::printf(" %s", attachment.c_str());
	}
	std::printf("\n\n");
	std::printf("anim states: %zu\n", weapon.animStates.size());
	for (const assets::WeaponAnimState& state : weapon.animStates) {
		std::printf("  %-28s %s\n", state.state.c_str(), state.anim.c_str());
	}
	std::printf("\nanimOverrides: %zu\n", weapon.animOverrides.size());
	for (const assets::WeaponAnimOverride& entry : weapon.animOverrides) {
		const std::string state = commands::AnimStateForTreeType(entry.animTreeType);
		std::printf("  att %-3d tree %-4d %-34s %s%s%s\n", entry.attachment1, entry.animTreeType,
		            entry.anim.c_str(), state.empty() ? "" : "-> ", state.c_str(),
		            entry.altmodeAnim.empty() ? "" : "  (alt)");
	}
	return true;
}

} // namespace wb
