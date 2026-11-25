#include <Windows.h>
#include <objbase.h>
#include <shellapi.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <filesystem>
#include <iterator>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <DirectXTex.h>

#include "app/application.h"
#include "app/commands.h"
#include "app/diag.h"
#include "app/info.h"
#include "app/mcp.h"
#include "app/model_overlay.h"
#include "app/review.h"
#include "app/settings.h"
#include "assets/asset_index.h"
#include "assets/model_asset.h"
#include "assets/xanim.h"
#include "assets/xmodel_document.h"
#include "render/camera.h"
#include "render/renderer.h"
#include "scene/viewmodel.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT message, WPARAM wParam,
                                                             LPARAM lParam);

namespace {

constexpr wchar_t kWindowClass[] = L"hmw-workbench";
constexpr wchar_t kWindowTitle[] = L"hmw-workbench";

bool g_imguiReady = false;
bool g_rendererReady = false;
bool g_resizePending = false;
bool g_minimized = false;
int g_clientWidth = 0;
int g_clientHeight = 0;

bool g_consoleAttached = false;

enum class Mode { Window, Screenshot, Info, Validate, Mcp, RoundTrip, EditTest };

struct Options {
	Mode mode = Mode::Window;
	std::filesystem::path screenshotPath;
	std::string modelName;
	std::string weaponName;
	std::string handsName;
	std::string animName;
	bool viewFirstPerson = false;
	bool hasView = false;
	std::string infoName;
	bool bones = false;
	bool wire = false;
	int width = 1600;
	int height = 900;
	bool hasWidth = false;
	bool hasHeight = false;
	bool hasYaw = false;
	bool hasPitch = false;
	bool hasDistance = false;
	bool hasFov = false;
	float yaw = 0.0f;
	float pitch = 0.0f;
	float distance = 0.0f;
	float fov = 0.0f;
	bool hasExposure = false;
	float exposure = 1.0f;
	bool hasAnimTime = false;
	double animTime = 0.0;
	bool animFraction = false;
	int sheet = 0;
	bool hasGrid = false;
	bool gridEnabled = true;
	bool ads = false;
	bool poseDump = false;
	std::filesystem::path poseDumpPath;
	bool crosshair = false;
	bool markTags = false;
	std::vector<int> hideSurfaces;
	std::vector<std::string> attachNames;
	bool fitReport = false;
	std::filesystem::path fitReportPath;
	bool roundTrip = false;
	std::filesystem::path roundTripPath;
	bool fitCrossbow = false;
	bool noAdsAlign = false;
	bool adsAlign = false;
	std::string camoName;
	std::string characterName;
	std::string characterWeapon;
	std::string compareA;
	std::string compareB;
	std::string compareMode = "side";
	float compareAlpha = 0.45f;
	std::filesystem::path renameMapPath;
	std::string compareBonesAnim;
	std::string compareBonesModel;
	bool hasCamoStrength = false;
	float camoStrength = 1.0f;
	bool hasCamoScale = false;
	float camoScale = 1.0f;
	bool windowTest = false;
	std::filesystem::path windowTestReport;
	bool worldModel = false;
	std::filesystem::path editTestSource;
	std::filesystem::path editTestTarget;
	std::string caption;
};

void OpenConsoleStreams() {
	FILE* dummy = nullptr;
	freopen_s(&dummy, "CONOUT$", "w", stdout);
	freopen_s(&dummy, "CONOUT$", "w", stderr);
}

void AttachParentConsole() {
	if (AttachConsole(ATTACH_PARENT_PROCESS)) {
		g_consoleAttached = true;
		const HANDLE stdoutHandle = GetStdHandle(STD_OUTPUT_HANDLE);
		if (stdoutHandle == nullptr || stdoutHandle == INVALID_HANDLE_VALUE) {
			OpenConsoleStreams();
		}
	}
}

void EnsureHeadlessConsole() {
	if (g_consoleAttached) {
		return;
	}
	if (AllocConsole()) {
		g_consoleAttached = true;
		OpenConsoleStreams();
	}
}

void PrintUsage() {
	std::fputs(
	    "hmw-workbench - HMW / MWR content workbench\n"
	    "\n"
	    "usage:\n"
	    "  hmw-workbench.exe\n"
	    "      Open the window.\n"
	    "  hmw-workbench.exe -screenshot <file.png> [-model <xmodel> | -weapon <name>]\n"
	    "                    [-hands <viewhands>] [-view firstperson|orbit] [-ads]\n"
	    "                    [-attach <attachment>]... [-hidesurface <i,i,...>]\n"
	    "                    [-anim <xanim>] [-time <seconds|fraction>] [-frac]\n"
	    "                    [-sheet <n>] [-bones] [-wire] [-grid|-nogrid]\n"
	    "                    [-width <n>] [-height <n>] [-yaw <deg>] [-pitch <deg>]\n"
	    "                    [-dist <units>] [-fov <vertical deg>] [-exposure <n>]\n"
	    "                    [-crosshair] [-marktags]\n"
	    "                    [-camo <camo_h1_name|name>] [-camo-strength <0..1>] [-camo-scale <n>]\n"
	    "                    [-world] [-caption <text>] [-ads-align]\n"
	    "      Render the viewport offscreen (no window) and exit.\n"
	    "  hmw-workbench.exe -character <faction> [-character-weapon <weapon>]\n"
	    "                    [-anim <xanim>] [-time <s>] [-view orbit] [-screenshot <file.png>]\n"
	    "      Load an HMW player character (body + optional head) and mount a world weapon on\n"
	    "      tag_weapon_right/tag_inhand; third-person anims play on the player skeleton.\n"
	    "  hmw-workbench.exe -compare <a> <b> -screenshot <file.png> [-compare-mode side|ghost]\n"
	    "                    [-compare-alpha <0..1>] [-view orbit|firstperson] [-world] [-camo <name>]\n"
	    "      Render two weapons/models with the same camera side by side or ghosted (M10).\n"
	    "  hmw-workbench.exe -compare-bones <xanim> <xmodel> [-rename-map <file.json>]\n"
	    "      List anim channels that do not match the model bones; an optional JSON map\n"
	    "      {\"old_anim_bone\":\"new_model_bone\"} is applied while playing anims too.\n"
	    "  hmw-workbench.exe -fit-report -weapon <name> [-hands <viewhands>] [-world]\n"
	    "      Text report: every attachment's view model, needed weapon tag, found/missing,\n"
	    "      and the nearest weapon tags for missing ones. -world uses the world models.\n"
	    "  hmw-workbench.exe -fit-crossbow -weapon iw5_exocrossbow_mp\n"
	    "      Add/fit tag_reflex, tag_acog and tag_silencer on the crossbow and save the xmb\n"
	    "      (with .bak and .tags.json).\n"
	    "  hmw-workbench.exe -info <xmodel|xanim>\n"
	    "      Text dump: model bones/tags/surfaces/materials, or anim frames/notetracks.\n"
	    "  hmw-workbench.exe -posedump [-weapon <name>| -model <xmodel>] [-anim <xanim>] [-time <s>] [file]\n"
	    "      Text dump of the posed hands+gun skeleton (world positions, anim channels, bounds).\n"
	    "  hmw-workbench.exe -validate\n"
	    "      Parse every .xmb in all roots plus its .xsb surface sets and material json.\n"
	    "  hmw-workbench.exe -roundtrip [report.txt]\n"
	    "      Read and write every .xmb into build\\roundtrip and byte-compare (must be 100%).\n"
	    "  hmw-workbench.exe -mcp\n"
	    "      Stdio MCP server: load weapons/models/attachments, pose anims and render offscreen PNGs.\n"
	    "  hmw-workbench.exe -window-test [report.txt]\n"
	    "      Scripted window run (load crossbow + M4, anims, attachment, tag edit on the copy\n"
	    "      model, close after 30 frames) for M7 regression testing.\n"
	    "\n"
	    "workbench.json next to the exe configures asset roots, window size and camera.\n",
	    stdout);
}

std::filesystem::path ExeDirectory() {
	wchar_t buffer[MAX_PATH * 4] = {};
	const DWORD length = GetModuleFileNameW(nullptr, buffer, static_cast<DWORD>(std::size(buffer)));
	if (length == 0) {
		return std::filesystem::current_path();
	}
	return std::filesystem::path(std::wstring(buffer, length)).parent_path();
}

bool ParseFloatArg(const wchar_t* text, float& value) {
	if (text == nullptr || *text == L'\0') {
		return false;
	}
	wchar_t* end = nullptr;
	const double parsed = std::wcstod(text, &end);
	if (end == text || *end != L'\0') {
		return false;
	}
	value = static_cast<float>(parsed);
	return true;
}

bool ParseDoubleArg(const wchar_t* text, double& value) {
	if (text == nullptr || *text == L'\0') {
		return false;
	}
	wchar_t* end = nullptr;
	const double parsed = std::wcstod(text, &end);
	if (end == text || *end != L'\0') {
		return false;
	}
	value = parsed;
	return true;
}

bool ParseIntArg(const wchar_t* text, int& value) {
	if (text == nullptr || *text == L'\0') {
		return false;
	}
	wchar_t* end = nullptr;
	const long parsed = std::wcstol(text, &end, 10);
	if (end == text || *end != L'\0') {
		return false;
	}
	value = static_cast<int>(parsed);
	return true;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
	if (g_imguiReady && ImGui_ImplWin32_WndProcHandler(hwnd, message, wParam, lParam)) {
		return 1;
	}

	switch (message) {
	case WM_SIZE:
		g_clientWidth = static_cast<int>(LOWORD(lParam));
		g_clientHeight = static_cast<int>(HIWORD(lParam));
		g_minimized = wParam == SIZE_MINIMIZED;
		if (!g_minimized && g_rendererReady) {
			g_resizePending = true;
		}
		return 0;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	default:
		break;
	}

	return DefWindowProcW(hwnd, message, wParam, lParam);
}

void RunCrossbowFit(wb::commands::Workbench& workbench) {
	const std::vector<wb::commands::TagEditInfo> tags = workbench.EditableTags();
	const auto has = [&tags](const std::string& name) {
		for (const auto& tag : tags) {
			if (tag.name == name) {
				return true;
			}
		}
		return false;
	};
	std::string error;
	const DirectX::XMFLOAT3 zero{0.0f, 0.0f, 0.0f};
	const DirectX::XMFLOAT4 identity{0.0f, 0.0f, 0.0f, 1.0f};
	if (!has("tag_reflex") &&
	    !workbench.AddTag("tag_reflex", "j_gun", "tag_red_dot", zero, identity, error)) {
		std::fprintf(stderr, "add tag_reflex: %s\n", error.c_str());
		return;
	}
	if (!has("tag_acog") &&
	    !workbench.AddTag("tag_acog", "j_gun", "tag_acog_2", zero, identity, error)) {
		std::fprintf(stderr, "add tag_acog: %s\n", error.c_str());
		return;
	}
	if (!has("tag_silencer") &&
	    !workbench.AddTag("tag_silencer", "j_gun", "tag_flash", zero, identity, error)) {
		std::fprintf(stderr, "add tag_silencer: %s\n", error.c_str());
		return;
	}

	workbench.SetAdsAutoAlign(false);
	wb::commands::TagFitResult reflex;
	if (workbench.Attach("reflexmwr", error) &&
	    workbench.FitTagToReticle("tag_reflex", "reflexmwr", reflex, error)) {
		std::printf("fit tag_reflex: final local (%.4f %.4f %.4f) screen (%.1f %.1f) iterations %d anim %s\n",
		            reflex.finalLocalPos.x, reflex.finalLocalPos.y, reflex.finalLocalPos.z, reflex.screenX,
		            reflex.screenY, reflex.iterations, reflex.anim.c_str());
	} else {
		std::fprintf(stderr, "fit tag_reflex: %s\n", error.c_str());
	}
	workbench.Detach("reflexmwr");

	wb::commands::TagFitResult acog;
	if (workbench.Attach("acogmwr", error) &&
	    workbench.FitTagToReticle("tag_acog", "acogmwr", acog, error)) {
		std::printf("fit tag_acog: final local (%.4f %.4f %.4f) screen (%.1f %.1f) iterations %d anim %s\n",
		            acog.finalLocalPos.x, acog.finalLocalPos.y, acog.finalLocalPos.z, acog.screenX,
		            acog.screenY, acog.iterations, acog.anim.c_str());
	} else {
		std::fprintf(stderr, "fit tag_acog: %s\n", error.c_str());
	}
	workbench.Detach("acogmwr");
	workbench.SetAds(false);

	const std::vector<wb::commands::TagEditInfo> finalTags = workbench.EditableTags();
	for (const char* name : {"tag_red_dot", "tag_acog_2", "tag_flash", "tag_reflex", "tag_acog",
	                         "tag_silencer"}) {
		for (const auto& tag : finalTags) {
			if (tag.name == name) {
				std::printf("tag %-14s parent %-10s local (%.4f %.4f %.4f)\n", tag.name.c_str(),
				            tag.parent.c_str(), tag.localPos.x, tag.localPos.y, tag.localPos.z);
			}
		}
	}

	wb::commands::ModelSaveResult saved;
	if (!workbench.SaveEditedModel(saved, error)) {
		std::fprintf(stderr, "save: %s\n", error.c_str());
		return;
	}
	std::printf("saved: %s\nbackup: %s\ntags: %s\nvalidation: %s\n", saved.path.c_str(),
	            saved.backup.c_str(), saved.tagsJson.c_str(), saved.validation.c_str());
}

void ApplyWorkbenchView(wb::commands::Workbench& workbench, const Options& options,
                        const wb::Settings& settings, bool hasAsset) {
	if (!hasAsset) {
		workbench.SetOrbitView(options.hasYaw ? options.yaw : settings.camera.yaw,
		                       options.hasPitch ? options.pitch : settings.camera.pitch,
		                       options.hasDistance ? options.distance : settings.camera.distance,
		                       options.hasFov ? options.fov : settings.camera.fov);
	} else if (options.viewFirstPerson && !options.worldModel) {
		workbench.SetFirstPersonView(options.hasYaw ? options.yaw : 0.0f,
		                             options.hasPitch ? options.pitch : 0.0f,
		                             options.hasFov ? options.fov : 0.0f);
	} else {
		const wb::Camera& camera = workbench.GetCamera();
		if (options.hasYaw || options.hasPitch || options.hasDistance || options.hasFov) {
			workbench.SetOrbitView(options.hasYaw ? options.yaw : camera.Yaw(),
			                       options.hasPitch ? options.pitch : camera.Pitch(),
			                       options.hasDistance ? options.distance : camera.Distance(),
			                       options.hasFov ? options.fov : 0.0f);
		} else {
			workbench.FrameCamera();
		}
	}
	if (options.hasGrid) {
		workbench.SetShowGrid(options.gridEnabled);
	}
}

int RunHeadless(const std::filesystem::path& exeDir, wb::Settings& settings, const Options& options) {
	std::printf("hmw-workbench M3 (headless)\n");

	const std::vector<std::filesystem::path> roots = wb::ResolveAssetRoots(settings, exeDir);
	wb::AssetIndex assets;
	assets.Scan(roots);
	std::printf("assets: %d weapons, %d models, %d anims (%d total)\n", assets.Count(wb::AssetKind::Weapon),
	            assets.Count(wb::AssetKind::Model), assets.Count(wb::AssetKind::Anim), assets.TotalCount());

	wb::Renderer renderer;
	wb::RendererDesc desc;
	desc.hwnd = nullptr;
	desc.width = options.width;
	desc.height = options.height;
	if (!renderer.Initialize(desc)) {
		std::fprintf(stderr, "failed to initialize D3D11\n");
		return 1;
	}

	wb::commands::Workbench workbench(settings, assets, renderer, roots);
	if (!options.renameMapPath.empty()) {
		std::string error;
		if (!workbench.LoadRenameMap(options.renameMapPath, error)) {
			std::fprintf(stderr, "rename map: %s\n", error.c_str());
			return 1;
		}
	}
	if (!options.compareA.empty()) {
		if (options.screenshotPath.empty()) {
			std::fprintf(stderr, "-compare needs -screenshot <file.png>\n");
			return 2;
		}
		wb::commands::CompareOptions compare;
		compare.width = options.width;
		compare.height = options.height;
		compare.mode = options.compareMode == "ghost" ? wb::commands::CompareMode::Ghost
		                                              : wb::commands::CompareMode::SideBySide;
		compare.ghostAlpha = options.compareAlpha;
		compare.firstPerson = options.viewFirstPerson;
		compare.hasView = options.hasView;
		compare.yaw = options.yaw;
		compare.pitch = options.pitch;
		compare.distance = options.distance;
		compare.fov = options.fov;
		compare.world = options.worldModel;
		compare.camo = options.camoName;
		std::string error;
		if (!wb::commands::RenderComparison(workbench, renderer, options.compareA, options.compareB, compare,
		                                   options.screenshotPath, error)) {
			std::fprintf(stderr, "compare: %s\n", error.c_str());
			return 1;
		}
		std::printf("compared %s + %s (%s) -> %s\n", options.compareA.c_str(), options.compareB.c_str(),
		            compare.mode == wb::commands::CompareMode::Ghost ? "ghost" : "side by side",
		            options.screenshotPath.string().c_str());
		return 0;
	}
	bool hasAsset = false;
	if (!options.characterName.empty()) {
		std::string error;
		if (!workbench.LoadCharacter(options.characterName, options.characterWeapon, error)) {
			std::fprintf(stderr, "cannot load character %s: %s\n", options.characterName.c_str(),
			             error.c_str());
			return 1;
		}
		const wb::assets::CharacterFaction* faction = workbench.CharacterFaction();
		const wb::assets::ModelAsset* body = workbench.CharacterBody();
		const wb::assets::ModelAsset* head = workbench.CharacterHead();
		const std::string bodyText =
		    body->model.name + (head != nullptr ? " + head " + head->model.name : std::string());
		std::printf("character: %s | body: %s | weapon: %s (tag %s, %s)\n", faction->display.c_str(),
		            bodyText.c_str(),
		            workbench.CharacterWeaponName().empty() ? "(none)" : workbench.CharacterWeaponName().c_str(),
		            workbench.CharacterWeaponTag().empty() ? "-" : workbench.CharacterWeaponTag().c_str(),
		            workbench.CharacterWeaponFound() ? "found" : "MISSING");
		const DirectX::XMFLOAT3 tagWorld = workbench.CharacterWeaponTagWorld();
		std::printf("weapon tag world: (%.3f %.3f %.3f)\n", tagWorld.x, tagWorld.y, tagWorld.z);
		for (const std::string& warning : workbench.CharacterWarnings()) {
			std::printf("warning: %s\n", warning.c_str());
		}
		for (const std::string& missing : workbench.CharacterMissing()) {
			std::printf("missing: %s\n", missing.c_str());
		}
		hasAsset = true;
	} else if (!options.weaponName.empty()) {
		std::string error;
		if (!workbench.LoadWeapon(options.weaponName, options.handsName, options.viewFirstPerson, error,
		                          options.worldModel)) {
			std::fprintf(stderr, "cannot load weapon %s: %s\n", options.weaponName.c_str(), error.c_str());
			return 1;
		}
		const wb::scene::ViewmodelAsset* viewmodel = workbench.Viewmodel();
		std::printf("weapon: %s | model: %s%s | hands: %s (%s)\n", viewmodel->weapon.name.c_str(),
		            viewmodel->gun.model.name.c_str(), viewmodel->isWorldModel ? " (world)" : "",
		            viewmodel->handsName.empty() ? "(none)" : viewmodel->handsName.c_str(),
		            viewmodel->handsSource.c_str());
		for (const std::string& warning : viewmodel->warnings) {
			std::printf("warning: %s\n", warning.c_str());
		}
		for (const int surface : options.hideSurfaces) {
			std::vector<bool>& visible = workbench.GunSurfaceVisible();
			if (surface >= 0 && static_cast<std::size_t>(surface) < visible.size()) {
				visible[static_cast<std::size_t>(surface)] = false;
			}
		}
		for (const std::string& attachment : options.attachNames) {
			std::string attachError;
			if (workbench.Attach(attachment, attachError)) {
				const wb::commands::AttachmentMount* mount = nullptr;
				for (const auto& candidate : workbench.Attachments()) {
					if (candidate.name == attachment || candidate.internalName == attachment ||
					    candidate.viewModel == attachment) {
						mount = &candidate;
						break;
					}
				}
				if (mount == nullptr && !workbench.Attachments().empty()) {
					mount = &workbench.Attachments().back();
				}
				if (mount != nullptr) {
					std::printf("attached: %s | vm %s | tag %s (%s)\n", mount->name.c_str(),
					            mount->viewModel.c_str(), mount->requiredTag.c_str(),
					            mount->tagFound ? "found" : "MISSING");
				} else {
					std::printf("attached: %s\n", attachment.c_str());
				}
			} else {
				std::fprintf(stderr, "attach %s: %s\n", attachment.c_str(), attachError.c_str());
			}
		}
		if (options.noAdsAlign) {
			workbench.SetAdsAutoAlign(false);
		}
		if (options.adsAlign) {
			workbench.SetAdsAutoAlign(true);
		}
		if (options.fitCrossbow) {
			RunCrossbowFit(workbench);
			return 0;
		}
		hasAsset = true;
	} else if (!options.modelName.empty()) {
		std::string error;
		if (!workbench.LoadModel(options.modelName, error)) {
			std::fprintf(stderr, "cannot load model %s: %s\n", options.modelName.c_str(), error.c_str());
			return 1;
		}
		const wb::assets::ModelAsset* model = workbench.Model();
		std::printf("model: %s (%zu surfaces, %u bones, %zu materials, %zu images)\n",
		            model->model.name.c_str(), model->surfaceSet.surfaces.size(), model->model.numBones,
		            model->materials.size(), model->images.size());
		for (const std::string& warning : model->warnings) {
			std::printf("warning: %s\n", warning.c_str());
		}
		hasAsset = true;
	}

	ApplyWorkbenchView(workbench, options, settings, hasAsset);
	if (options.ads && workbench.Viewmodel() != nullptr) {
		workbench.SetAds(true);
	}
	if (!options.camoName.empty()) {
		std::string error;
		if (!workbench.SetCamo(options.camoName, error)) {
			std::fprintf(stderr, "camo %s: %s\n", options.camoName.c_str(), error.c_str());
			return 1;
		}
		if (options.hasCamoStrength) {
			workbench.SetCamoStrength(options.camoStrength);
		}
		if (options.hasCamoScale) {
			workbench.SetCamoScale(options.camoScale);
		}
		std::printf("camo: %s (texture %s, strength %.2f, uv scale %.2f)\n", workbench.CamoName().c_str(),
		            workbench.CamoTexture().c_str(), workbench.CamoStrength(), workbench.CamoScale());
	}	if (workbench.IsFirstPerson() && workbench.Viewmodel() != nullptr) {
		std::printf("fov: cg %.2f (4:3 horizontal) -> vertical %.3f (viewmodel scale %.2f)\n",
		            workbench.CgFov(), workbench.GetCamera().Fov(), settings.viewmodelFovScale);
	}

	if (!options.animName.empty()) {
		std::string error;
		if (!workbench.PlayAnim(options.animName, error)) {
			std::fprintf(stderr, "cannot play anim %s: %s\n", options.animName.c_str(), error.c_str());
			return 1;
		}
		std::printf("anim: %s (%u frames, %.1f fps, %.3f s, %zu notetracks)\n",
		            workbench.Anim()->name.c_str(), workbench.Anim()->frameCount,
		            workbench.Anim()->frameRate, workbench.Anim()->Duration(),
		            workbench.Anim()->notifies.size());
	}
	if (options.hasAnimTime) {
		if (options.animFraction) {
			workbench.SetAnimFraction(options.animTime);
		} else {
			workbench.SetAnimTime(options.animTime);
		}
	}
	if (workbench.Anim() != nullptr && options.hasAnimTime) {
		std::printf("time: %.4f s  frame %.2f\n", workbench.AnimTime(), workbench.AnimFrame());
	}

	if (options.poseDump) {
		std::FILE* out = stdout;
		std::FILE* file = nullptr;
		if (!options.poseDumpPath.empty()) {
			if (_wfopen_s(&file, options.poseDumpPath.c_str(), L"w") == 0 && file != nullptr) {
				out = file;
			} else {
				std::fprintf(stderr, "posedump: cannot write %s\n",
				             options.poseDumpPath.string().c_str());
			}
		}
		wb::PrintPoseDump(workbench, wb::DefaultPoseDumpBones(), options.width, options.height, out);
		if (file != nullptr) {
			std::fclose(file);
		}
		if (options.screenshotPath.empty()) {
			return 0;
		}
	}

	const std::filesystem::path& output = options.screenshotPath;
	wb::ShotOptions shot;
	shot.width = options.width;
	shot.height = options.height;
	shot.bones = options.bones;
	shot.wire = options.wire;
	shot.crosshair = options.crosshair;
	shot.markTags = options.markTags;
	shot.caption = options.caption;
	shot.sheet = options.sheet;
	std::string shotError;
	if (!wb::RenderShot(workbench, renderer, shot, output, shotError)) {
		std::fprintf(stderr, "screenshot: %s\n", shotError.c_str());
		return 1;
	}

	const wb::Camera& camera = workbench.GetCamera();
	if (options.sheet > 0) {
		std::printf("wrote %s (sheet, %d cells of %dx%d)\n", output.string().c_str(), options.sheet,
		            options.width, options.height);
	} else {
		std::printf("wrote %s (%dx%d, yaw %.1f pitch %.1f dist %.2f)\n", output.string().c_str(),
		            options.width, options.height, camera.Yaw(), camera.Pitch(), camera.Distance());
	}
	return 0;
}

int RunWindowTest(wb::commands::Workbench& workbench, const std::filesystem::path& reportPath) {
	std::FILE* out = stdout;
	std::FILE* file = nullptr;
	if (!reportPath.empty()) {
		std::error_code ec;
		std::filesystem::create_directories(reportPath.parent_path(), ec);
		if (_wfopen_s(&file, reportPath.c_str(), L"w") == 0 && file != nullptr) {
			out = file;
		} else {
			std::fprintf(stderr, "window-test: cannot write %s\n", reportPath.string().c_str());
		}
	}
	int failures = 0;
	const auto check = [&out, &failures](bool ok, const std::string& what) {
		std::fprintf(out, "%s %s\n", ok ? "PASS" : "FAIL", what.c_str());
		if (!ok) {
			++failures;
		}
	};

	std::fprintf(out, "hmw-workbench M7 scripted window test\n");
	std::string error;

	// 1. Crossbow weapon in first person, idle animation, HMW reflex attachment.
	const bool crossbow = workbench.LoadWeapon("iw5_exocrossbow_mp", "viewhands_tf141", true, error);
	check(crossbow, "load crossbow weapon" + (crossbow ? std::string() : ": " + error));
	if (crossbow) {
		const bool idle = workbench.PlayAnim("vm_crossbow_idle", error);
		check(idle, "play vm_crossbow_idle" + (idle ? std::string() : ": " + error));
		workbench.SetAnimTime(0.3);
		workbench.SetAnimPlaying(true);
		check(workbench.Anim() != nullptr && workbench.AnimFrame() > 0.0f,
		      "crossbow animation advanced to a posed frame");
		const bool attached = workbench.Attach("reflexmwr", error);
		bool found = false;
		for (const auto& mount : workbench.Attachments()) {
			if (mount.name == "reflexmwr" && mount.tagFound) {
				found = true;
			}
		}
		check(attached && found, "mount reflexmwr on tag_reflex" + (attached ? std::string() : ": " + error));
	}

	// 2. H2 M4 weapon in first person, idle animation, its reflex attachment.
	const bool m4 = workbench.LoadWeapon("h2_m4_mp", "", true, error);
	check(m4, "load h2_m4_mp weapon" + (m4 ? std::string() : ": " + error));
	if (m4) {
		const std::string stateAnim = workbench.EffectiveAnimForState("IDLE");
		const bool idle = !stateAnim.empty() && workbench.PlayAnim(stateAnim, error);
		check(idle, "play M4 idle (" + stateAnim + ")" + (idle ? std::string() : ": " + error));
		workbench.SetAnimTime(0.2);
		const bool attached = workbench.Attach("reflexmwrm4", error);
		check(attached, "mount reflexmwrm4" + (attached ? std::string() : ": " + error));
	}

	// 3. Standalone crossbow from the copy root (build\edit_test\m5root): add/move/save a tag.
	const bool model = workbench.LoadModel("vm_crossbow_base", error);
	check(model, "load copy model vm_crossbow_base" + (model ? std::string() : ": " + error));
	if (model) {
		const DirectX::XMFLOAT3 zero{0.0f, 0.0f, 0.0f};
		const DirectX::XMFLOAT4 identity{0.0f, 0.0f, 0.0f, 1.0f};
		bool added = workbench.AddTag("tag_window_test", "j_gun", "tag_red_dot", zero, identity, error);
		check(added, "add tag_window_test on the copy" + (added ? std::string() : ": " + error));
		DirectX::XMFLOAT3 pos{0.0f, 0.0f, 0.0f};
		for (const auto& info : workbench.EditableTags()) {
			if (info.name == "tag_window_test") {
				pos = info.localPos;
			}
		}
		pos.z += 0.02f;
		const bool moved = workbench.MoveTagLocal("tag_window_test", pos, identity, error);
		check(moved, "move tag_window_test" + (moved ? std::string() : ": " + error));
		wb::commands::ModelSaveResult saved;
		const bool savedOk = workbench.SaveEditedModel(saved, error);
		check(savedOk, "save the copy model" + (savedOk ? "" : ": " + error));
		if (savedOk) {
			std::fprintf(out, "  saved: %s\n  backup: %s\n  tags: %s\n  validation: %s\n",
			             saved.path.c_str(), saved.backup.c_str(), saved.tagsJson.c_str(),
			             saved.validation.c_str());
			check(std::filesystem::exists(saved.backup), "copy .bak exists");
			check(std::filesystem::exists(saved.tagsJson), "copy .tags.json exists");
		}
		const bool reloaded = workbench.LoadModel("vm_crossbow_base", error);
		bool found = false;
		for (const auto& info : workbench.EditableTags()) {
			if (info.name == "tag_window_test") {
				found = true;
			}
		}
		check(reloaded && found, "reload the copy and find tag_window_test");
	}

	std::fprintf(out, "\nresult: %s (%d failed)\n", failures == 0 ? "PASS" : "FAIL", failures);
	if (file != nullptr) {
		std::fclose(file);
	}
	std::printf("window-test: %s (%d failed)\n", reportPath.string().c_str(), failures);
	return failures;
}

int RunWindowed(const std::filesystem::path& exeDir, wb::Settings& settings, HINSTANCE instance,
                const Options& options) {
	const std::vector<std::filesystem::path> roots = wb::ResolveAssetRoots(settings, exeDir);
	wb::AssetIndex assets;
	assets.Scan(roots);

	ImGui_ImplWin32_EnableDpiAwareness();

	WNDCLASSEXW windowClass = {};
	windowClass.cbSize = sizeof(windowClass);
	windowClass.style = CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc = WndProc;
	windowClass.hInstance = instance;
	windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
	windowClass.lpszClassName = kWindowClass;
	if (RegisterClassExW(&windowClass) == 0) {
		std::fprintf(stderr, "RegisterClassExW failed\n");
		return 1;
	}

	RECT workArea = {};
	SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
	const int workWidth = workArea.right - workArea.left;
	const int workHeight = workArea.bottom - workArea.top;
	const int clientWidth = std::min(settings.windowWidth, workWidth > 320 ? workWidth - 80 : settings.windowWidth);
	const int clientHeight = std::min(settings.windowHeight, workHeight > 240 ? workHeight - 80 : settings.windowHeight);

	const DWORD windowStyle = WS_OVERLAPPEDWINDOW;
	RECT windowRect = {0, 0, clientWidth, clientHeight};
	AdjustWindowRectEx(&windowRect, windowStyle, FALSE, 0);

	HWND hwnd = CreateWindowExW(0, kWindowClass, kWindowTitle, windowStyle, CW_USEDEFAULT, CW_USEDEFAULT,
	                            windowRect.right - windowRect.left, windowRect.bottom - windowRect.top, nullptr,
	                            nullptr, instance, nullptr);
	if (hwnd == nullptr) {
		std::fprintf(stderr, "CreateWindowExW failed\n");
		return 1;
	}

	RECT clientRect = {};
	GetClientRect(hwnd, &clientRect);
	g_clientWidth = clientRect.right - clientRect.left;
	g_clientHeight = clientRect.bottom - clientRect.top;

	ShowWindow(hwnd, SW_SHOW);
	UpdateWindow(hwnd);

	wb::Renderer renderer;
	wb::RendererDesc desc;
	desc.hwnd = hwnd;
	desc.width = g_clientWidth;
	desc.height = g_clientHeight;
	if (!renderer.Initialize(desc)) {
		std::fprintf(stderr, "failed to initialize D3D11\n");
		DestroyWindow(hwnd);
		return 1;
	}
	g_rendererReady = true;

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigWindowsMoveFromTitleBarOnly = true;
	const std::string iniPath = (exeDir / "imgui.ini").string();
	io.IniFilename = iniPath.c_str();

	ImGui::StyleColorsDark();
	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowRounding = 4.0f;
	style.FrameRounding = 3.0f;
	style.GrabRounding = 3.0f;
	style.TabRounding = 3.0f;
	style.Colors[ImGuiCol_WindowBg] = ImVec4(0.086f, 0.090f, 0.106f, 1.0f);
	style.Colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.060f, 0.064f, 0.078f, 1.0f);

	if (!ImGui_ImplWin32_Init(hwnd)) {
		std::fprintf(stderr, "ImGui_ImplWin32_Init failed\n");
		renderer.Shutdown();
		DestroyWindow(hwnd);
		return 1;
	}
	if (!ImGui_ImplDX11_Init(renderer.Device(), renderer.Context())) {
		std::fprintf(stderr, "ImGui_ImplDX11_Init failed\n");
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
		renderer.Shutdown();
		DestroyWindow(hwnd);
		return 1;
	}
	g_imguiReady = true;

	wb::commands::Workbench workbench(settings, assets, renderer, roots);
	const bool restoreLayout = std::filesystem::exists(exeDir / "imgui.ini");
	wb::Application app(settings, assets, renderer, workbench, exeDir, restoreLayout);
	if (!options.renameMapPath.empty()) {
		std::string error;
		if (!workbench.LoadRenameMap(options.renameMapPath, error)) {
			std::fprintf(stderr, "rename map: %s\n", error.c_str());
		}
	}
	if (!options.characterName.empty()) {
		app.LoadCharacter(options.characterName, options.characterWeapon);
	} else if (!options.weaponName.empty()) {
		app.LoadWeapon(options.weaponName, options.handsName, options.viewFirstPerson);
	} else if (!options.modelName.empty()) {
		app.LoadModelByName(options.modelName);
	}
	if (options.worldModel && !options.weaponName.empty()) {
		std::string error;
		if (!workbench.SetWorldModel(true, error)) {
			std::fprintf(stderr, "world model: %s\n", error.c_str());
		}
	}
	if (!options.camoName.empty()) {
		std::string error;
		if (!workbench.SetCamo(options.camoName, error)) {
			std::fprintf(stderr, "camo %s: %s\n", options.camoName.c_str(), error.c_str());
		} else {
			if (options.hasCamoStrength) {
				workbench.SetCamoStrength(options.camoStrength);
			}
			if (options.hasCamoScale) {
				workbench.SetCamoScale(options.camoScale);
			}
		}
	}

	int windowTestFrames = -1;
	if (options.windowTest) {
		const std::filesystem::path report =
		    options.windowTestReport.empty() ? exeDir / "window-test.txt"
		                                     : std::filesystem::absolute(options.windowTestReport);
		RunWindowTest(workbench, report);
		windowTestFrames = 30;
	}

	bool quit = false;
	while (!quit) {
		MSG message;
		while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
			if (message.message == WM_QUIT) {
				quit = true;
				break;
			}
			TranslateMessage(&message);
			DispatchMessageW(&message);
		}
		if (quit || app.WantsClose()) {
			break;
		}
		if (windowTestFrames >= 0) {
			if (windowTestFrames == 0) {
				break;
			}
			--windowTestFrames;
		}

		if (g_resizePending) {
			renderer.ResizeSwapChain(g_clientWidth, g_clientHeight);
			g_resizePending = false;
		}

		if (g_minimized || g_clientWidth <= 0 || g_clientHeight <= 0) {
			Sleep(10);
			continue;
		}

		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		app.Frame();

		ImGui::Render();
		const float clearColor[4] = {0.06f, 0.064f, 0.078f, 1.0f};
		renderer.ClearBackBuffer(clearColor);
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
		renderer.Present();
	}

	wb::SaveSettings(exeDir, settings);

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
	g_imguiReady = false;

	renderer.Shutdown();
	g_rendererReady = false;

	if (IsWindow(hwnd)) {
		DestroyWindow(hwnd);
	}

	return 0;
}

} // namespace

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int) {
	CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	AttachParentConsole();

	int argc = 0;
	LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	if (argv == nullptr) {
		return 1;
	}

	Options options;
	bool valid = true;
	for (int i = 1; i < argc && valid; ++i) {
		const std::wstring arg = argv[i];
		auto nextValue = [&]() -> const wchar_t* {
			if (i + 1 >= argc) {
				return nullptr;
			}
			return argv[++i];
		};

		if (arg == L"-screenshot") {
			const wchar_t* value = nextValue();
			if (value == nullptr) {
				std::fputs("-screenshot needs a file name\n", stderr);
				valid = false;
			} else {
				options.mode = Mode::Screenshot;
				options.screenshotPath = std::filesystem::path(value);
			}
		} else if (arg == L"-info") {
			const wchar_t* value = nextValue();
			if (value == nullptr) {
				std::fputs("-info needs an xmodel or xanim name\n", stderr);
				valid = false;
			} else {
				options.mode = Mode::Info;
				options.infoName = std::filesystem::path(value).string();
			}
		} else if (arg == L"-validate") {
			options.mode = Mode::Validate;
		} else if (arg == L"-mcp") {
			options.mode = Mode::Mcp;
		} else if (arg == L"-window-test") {
			options.windowTest = true;
			if (i + 1 < argc && argv[i + 1][0] != L'-') {
				options.windowTestReport = std::filesystem::path(argv[++i]);
			}
		} else if (arg == L"-roundtrip") {
			options.mode = Mode::RoundTrip;
			options.roundTrip = true;
			if (i + 1 < argc && argv[i + 1][0] != L'-') {
				options.roundTripPath = std::filesystem::path(argv[++i]);
			}
		} else if (arg == L"-edit-test") {
			options.mode = Mode::EditTest;
			if (i + 2 < argc) {
				options.editTestSource = std::filesystem::path(argv[++i]);
				options.editTestTarget = std::filesystem::path(argv[++i]);
			} else {
				std::fputs("-edit-test needs <source.xmb> <target.xmb>\n", stderr);
				valid = false;
			}
		} else if (arg == L"-model") {
			const wchar_t* value = nextValue();
			if (value == nullptr) {
				std::fputs("-model needs an xmodel name\n", stderr);
				valid = false;
			} else {
				options.modelName = std::filesystem::path(value).string();
			}
		} else if (arg == L"-weapon") {
			const wchar_t* value = nextValue();
			if (value == nullptr) {
				std::fputs("-weapon needs a weapon name\n", stderr);
				valid = false;
			} else {
				options.weaponName = std::filesystem::path(value).string();
			}
		} else if (arg == L"-hands") {
			const wchar_t* value = nextValue();
			if (value == nullptr) {
				std::fputs("-hands needs a viewhands model name\n", stderr);
				valid = false;
			} else {
				options.handsName = std::filesystem::path(value).string();
			}
		} else if (arg == L"-anim") {
			const wchar_t* value = nextValue();
			if (value == nullptr) {
				std::fputs("-anim needs an xanim name\n", stderr);
				valid = false;
			} else {
				options.animName = std::filesystem::path(value).string();
			}
		} else if (arg == L"-time") {
			valid = ParseDoubleArg(nextValue(), options.animTime);
			options.hasAnimTime = valid;
		} else if (arg == L"-frac") {
			options.animFraction = true;
		} else if (arg == L"-sheet") {
			valid = ParseIntArg(nextValue(), options.sheet) && options.sheet > 0;
		} else if (arg == L"-view") {
			const wchar_t* value = nextValue();
			const std::wstring view = value != nullptr ? value : L"";
			if (view == L"firstperson" || view == L"fp") {
				options.viewFirstPerson = true;
				options.hasView = true;
			} else if (view == L"orbit") {
				options.viewFirstPerson = false;
				options.hasView = true;
			} else {
				std::fputs("-view needs 'firstperson' or 'orbit'\n", stderr);
				valid = false;
			}
		} else if (arg == L"-bones") {
			options.bones = true;
		} else if (arg == L"-ads") {
			options.ads = true;
		} else if (arg == L"-posedump") {
			options.poseDump = true;
			if (i + 1 < argc && argv[i + 1][0] != L'-') {
				options.poseDumpPath = std::filesystem::path(argv[++i]);
			}
		} else if (arg == L"-crosshair") {
			options.crosshair = true;
		} else if (arg == L"-fit-crossbow") {
			options.fitCrossbow = true;
		} else if (arg == L"-no-ads-align") {
			options.noAdsAlign = true;
		} else if (arg == L"-ads-align") {
			options.adsAlign = true;
		} else if (arg == L"-camo") {
			const wchar_t* value = nextValue();
			if (value == nullptr) {
				std::fputs("-camo needs a camo material or name\n", stderr);
				valid = false;
			} else {
				options.camoName = std::filesystem::path(value).string();
			}
		} else if (arg == L"-camo-strength") {
			valid = ParseFloatArg(nextValue(), options.camoStrength) && options.camoStrength >= 0.0f &&
			        options.camoStrength <= 1.0f;
			options.hasCamoStrength = valid;
		} else if (arg == L"-camo-scale") {
			valid = ParseFloatArg(nextValue(), options.camoScale) && options.camoScale > 0.0f;
			options.hasCamoScale = valid;
		} else if (arg == L"-world") {
			options.worldModel = true;
		} else if (arg == L"-character") {
			const wchar_t* value = nextValue();
			if (value == nullptr) {
				std::fputs("-character needs a faction id\n", stderr);
				valid = false;
			} else {
				options.characterName = std::filesystem::path(value).string();
			}
		} else if (arg == L"-character-weapon") {
			const wchar_t* value = nextValue();
			if (value == nullptr) {
				std::fputs("-character-weapon needs a weapon name\n", stderr);
				valid = false;
			} else {
				options.characterWeapon = std::filesystem::path(value).string();
			}
		} else if (arg == L"-compare") {
			if (i + 2 < argc) {
				options.compareA = std::filesystem::path(argv[++i]).string();
				options.compareB = std::filesystem::path(argv[++i]).string();
			} else {
				std::fputs("-compare needs two weapon/model names\n", stderr);
				valid = false;
			}
		} else if (arg == L"-compare-mode") {
			const wchar_t* value = nextValue();
			const std::wstring mode = value != nullptr ? value : L"";
			if (mode == L"side" || mode == L"sidebyside") {
				options.compareMode = "side";
			} else if (mode == L"ghost") {
				options.compareMode = "ghost";
			} else {
				std::fputs("-compare-mode needs 'side' or 'ghost'\n", stderr);
				valid = false;
			}
		} else if (arg == L"-compare-alpha") {
			valid = ParseFloatArg(nextValue(), options.compareAlpha) && options.compareAlpha > 0.0f &&
			        options.compareAlpha < 1.0f;
		} else if (arg == L"-rename-map") {
			const wchar_t* value = nextValue();
			if (value == nullptr) {
				std::fputs("-rename-map needs a json file\n", stderr);
				valid = false;
			} else {
				options.renameMapPath = std::filesystem::path(value);
			}
		} else if (arg == L"-compare-bones") {
			if (i + 2 < argc) {
				options.compareBonesAnim = std::filesystem::path(argv[++i]).string();
				options.compareBonesModel = std::filesystem::path(argv[++i]).string();
			} else {
				std::fputs("-compare-bones needs an xanim and an xmodel\n", stderr);
				valid = false;
			}
		} else if (arg == L"-fit-report") {
			options.fitReport = true;
			if (i + 1 < argc && argv[i + 1][0] != L'-') {
				options.fitReportPath = std::filesystem::path(argv[++i]);
			}
		} else if (arg == L"-caption") {
			const wchar_t* value = nextValue();
			if (value == nullptr) {
				std::fputs("-caption needs text\n", stderr);
				valid = false;
			} else {
				options.caption = std::filesystem::path(value).string();
			}
		} else if (arg == L"-attach") {
			const wchar_t* value = nextValue();
			if (value == nullptr) {
				std::fputs("-attach needs an attachment name\n", stderr);
				valid = false;
			} else {
				options.attachNames.emplace_back(std::filesystem::path(value).string());
			}
		} else if (arg == L"-marktags") {
			options.markTags = true;
		} else if (arg == L"-hidesurface") {
			const wchar_t* value = nextValue();
			if (value == nullptr) {
				std::fputs("-hidesurface needs a comma separated surface index list\n", stderr);
				valid = false;
			} else {
				std::wstring list = value;
				std::size_t start = 0;
				while (start <= list.size()) {
					const std::size_t comma = list.find(L',', start);
					const std::wstring token =
					    list.substr(start, comma == std::wstring::npos ? std::wstring::npos : comma - start);
					int index = 0;
					valid = valid && ParseIntArg(token.c_str(), index) && index >= 0;
					if (valid) {
						options.hideSurfaces.push_back(index);
					}
					if (comma == std::wstring::npos) {
						break;
					}
					start = comma + 1;
				}
			}
		} else if (arg == L"-wire") {
			options.wire = true;
		} else if (arg == L"-grid") {
			options.hasGrid = true;
			options.gridEnabled = true;
		} else if (arg == L"-nogrid") {
			options.hasGrid = true;
			options.gridEnabled = false;
		} else if (arg == L"-width") {
			valid = ParseIntArg(nextValue(), options.width) && options.width > 0;
			options.hasWidth = valid;
		} else if (arg == L"-height") {
			valid = ParseIntArg(nextValue(), options.height) && options.height > 0;
			options.hasHeight = valid;
		} else if (arg == L"-yaw") {
			valid = ParseFloatArg(nextValue(), options.yaw);
			options.hasYaw = valid;
		} else if (arg == L"-pitch") {
			valid = ParseFloatArg(nextValue(), options.pitch);
			options.hasPitch = valid;
		} else if (arg == L"-dist" || arg == L"-distance") {
			valid = ParseFloatArg(nextValue(), options.distance);
			options.hasDistance = valid;
		} else if (arg == L"-fov") {
			valid = ParseFloatArg(nextValue(), options.fov);
			options.hasFov = valid;
		} else if (arg == L"-exposure") {
			valid = ParseFloatArg(nextValue(), options.exposure) && options.exposure > 0.0f;
			options.hasExposure = valid;
		} else if (arg == L"-help" || arg == L"--help" || arg == L"-h" || arg == L"/?") {
			EnsureHeadlessConsole();
			PrintUsage();
			LocalFree(argv);
			CoUninitialize();
			return 0;
		} else {
			std::fprintf(stderr, "unknown argument: %ls\n", arg.c_str());
			valid = false;
		}
	}

	LocalFree(argv);

	if (!valid) {
		EnsureHeadlessConsole();
		PrintUsage();
		CoUninitialize();
		return 2;
	}
	if (options.poseDump && options.mode == Mode::Window) {
		options.mode = Mode::Screenshot;
	}
	if (options.fitCrossbow && options.mode == Mode::Window) {
		options.mode = Mode::Screenshot;
	}
	if (options.fitCrossbow) {
		options.viewFirstPerson = true;
		options.hasView = true;
	}
	if (!options.modelName.empty() && !options.weaponName.empty()) {
		EnsureHeadlessConsole();
		std::fputs("-model and -weapon cannot be combined\n", stderr);
		CoUninitialize();
		return 2;
	}
	if (!options.compareA.empty() && options.mode != Mode::Screenshot) {
		EnsureHeadlessConsole();
		std::fputs("-compare needs -screenshot <file.png>\n", stderr);
		CoUninitialize();
		return 2;
	}
	if (options.mode == Mode::Screenshot && options.screenshotPath.empty() && !options.poseDump &&
	    !options.fitCrossbow) {
		EnsureHeadlessConsole();
		std::fputs("-screenshot needs a file name\n", stderr);
		CoUninitialize();
		return 2;
	}

	const std::filesystem::path exeDir = ExeDirectory();
	wb::Settings settings = wb::LoadSettings(exeDir);
	if (options.hasExposure) {
		settings.exposure = options.exposure;
	}
	const std::vector<std::filesystem::path> roots = wb::ResolveAssetRoots(settings, exeDir);

	if (!options.compareBonesAnim.empty()) {
		EnsureHeadlessConsole();
		std::map<std::string, std::string> rename;
		std::string error;
		if (!options.renameMapPath.empty() && !wb::commands::LoadRenameTable(options.renameMapPath, rename, error)) {
			std::fprintf(stderr, "%s\n", error.c_str());
			CoUninitialize();
			return 1;
		}
		wb::scene::BoneCompareResult result;
		if (!wb::commands::CompareBones(roots, options.compareBonesAnim, options.compareBonesModel, rename, result, error)) {
			std::fprintf(stderr, "compare-bones: %s\n", error.c_str());
			CoUninitialize();
			return 1;
		}
		std::printf("anim: %s\nmodel: %s\nchannels: %d  model bones: %d  matched: %d  renamed: %d  missing: %d\n",
		            options.compareBonesAnim.c_str(), options.compareBonesModel.c_str(), result.animChannels,
		            result.modelBones, result.matched, result.renamed, result.missing);
		if (!result.renamedChannels.empty()) {
			std::printf("renamed channels:\n");
			for (const std::string& name : result.renamedChannels) {
				std::printf("  %s\n", name.c_str());
			}
		}
		if (!result.missingChannels.empty()) {
			std::printf("missing channels:");
			for (const std::string& name : result.missingChannels) {
				std::printf(" %s", name.c_str());
			}
			std::printf("\n");
		}
		CoUninitialize();
		return 0;
	}

	if (options.fitReport) {
		EnsureHeadlessConsole();
		if (options.weaponName.empty()) {			std::fputs("-fit-report needs -weapon <name>\n", stderr);
			CoUninitialize();
			return 2;
		}
		wb::commands::AttachmentFitReport report;
		std::string error;
		if (!wb::commands::BuildFitReport(roots, options.weaponName, options.handsName, {}, report, error,
		                                  options.worldModel)) {
			std::fprintf(stderr, "fit report: %s\n", error.c_str());
			CoUninitialize();
			return 1;
		}
		std::FILE* out = stdout;
		std::FILE* file = nullptr;
		if (!options.fitReportPath.empty()) {
			if (_wfopen_s(&file, options.fitReportPath.c_str(), L"w") == 0 && file != nullptr) {
				out = file;
			} else {
				std::fprintf(stderr, "fit report: cannot write %s\n",
				             options.fitReportPath.string().c_str());
			}
		}
		wb::commands::PrintFitReport(report, out);
		if (file != nullptr) {
			std::fclose(file);
		}
		CoUninitialize();
		return 0;
	}

	if (options.roundTrip) {
		EnsureHeadlessConsole();
		const std::filesystem::path repoRoot = exeDir.parent_path().parent_path().parent_path();
		const std::filesystem::path outputDir = repoRoot / "build" / "roundtrip";
		std::FILE* out = stdout;
		std::FILE* file = nullptr;
		if (!options.roundTripPath.empty()) {
			if (_wfopen_s(&file, options.roundTripPath.c_str(), L"w") == 0 && file != nullptr) {
				out = file;
			} else {
				std::fprintf(stderr, "roundtrip: cannot write %s\n",
				             options.roundTripPath.string().c_str());
			}
		}
		const int failures = wb::RunRoundTrip(roots, outputDir, out);
		if (file != nullptr) {
			std::fclose(file);
		}
		CoUninitialize();
		return failures == 0 ? 0 : 1;
	}

	if (options.mode == Mode::EditTest) {
		EnsureHeadlessConsole();
		wb::assets::XModelDocument document;
		std::string error;
		if (!wb::assets::LoadXModelDocument(options.editTestSource, document, error)) {
			std::fprintf(stderr, "edit-test load: %s\n", error.c_str());
			CoUninitialize();
			return 1;
		}
		const std::string testTag = "tag_test_workbench";
		if (!wb::assets::AddTag(document, testTag, "j_gun", "tag_red_dot", {0.0f, 0.0f, 0.0f},
		                        {0.0f, 0.0f, 0.0f, 1.0f}, error)) {
			std::fprintf(stderr, "edit-test add: %s\n", error.c_str());
			CoUninitialize();
			return 1;
		}
		int testIndex = -1;
		for (std::size_t i = 0; i < document.model.bones.size(); ++i) {
			if (document.model.bones[i].name == testTag) {
				testIndex = static_cast<int>(i);
			}
		}
		const auto original = document.model.bones[static_cast<std::size_t>(testIndex)];
		if (!wb::assets::MoveTagLocal(document, testTag,
		                              {original.localPos.x + 1.0f, original.localPos.y + 2.0f,
		                               original.localPos.z + 3.0f},
		                              original.localQuat, error)) {
			std::fprintf(stderr, "edit-test move: %s\n", error.c_str());
			CoUninitialize();
			return 1;
		}
		if (!wb::assets::SaveXModelDocument(document, options.editTestTarget, error)) {
			std::fprintf(stderr, "edit-test save: %s\n", error.c_str());
			CoUninitialize();
			return 1;
		}
		wb::assets::XModelDocument reloaded;
		if (!wb::assets::LoadXModelDocument(options.editTestTarget, reloaded, error)) {
			std::fprintf(stderr, "edit-test reload: %s\n", error.c_str());
			CoUninitialize();
			return 1;
		}
		std::printf("edit-test: %zu -> %zu bones, tag_test present %s, saved %s\n",
		            document.originalModel.bones.size(), reloaded.model.bones.size(),
		            (reloaded.model.bones.size() == document.model.bones.size() ? "yes" : "no"),
		            options.editTestTarget.string().c_str());
		wb::assets::ModelAsset asset;
		if (!wb::assets::LoadModelAssetFromFile(options.editTestTarget, roots, asset, error, false)) {
			std::fprintf(stderr, "edit-test validate: %s\n", error.c_str());
			CoUninitialize();
			return 1;
		}
		std::printf("edit-test validate: ok %zu surfaces %zu materials\n", asset.surfaceSet.surfaces.size(),
		            asset.materials.size());
		CoUninitialize();
		return 0;
	}

	int result = 0;
	if (options.mode == Mode::Info) {
		EnsureHeadlessConsole();
		const bool animFound = wb::commands::HasAnim(roots, options.infoName);
		result = (animFound ? wb::PrintAnimInfo(options.infoName, roots)
		                    : wb::PrintModelInfo(options.infoName, roots))
		             ? 0
		             : 1;
	} else if (options.mode == Mode::Validate) {
		EnsureHeadlessConsole();
		result = wb::RunValidate(roots) == 0 ? 0 : 1;
	} else if (options.mode == Mode::Mcp) {
		result = wb::RunMcp(exeDir, settings);
	} else if (options.mode == Mode::Screenshot) {
		EnsureHeadlessConsole();
		result = RunHeadless(exeDir, settings, options);
	} else {
		result = RunWindowed(exeDir, settings, instance, options);
	}

	CoUninitialize();
	return result;
}
