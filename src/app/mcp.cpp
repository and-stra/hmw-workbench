#include "app/mcp.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#include <windows.h>

#include <nlohmann/json.hpp>

#include "app/commands.h"
#include "app/diag.h"
#include "app/info.h"
#include "app/review.h"
#include "app/settings.h"
#include "assets/asset_index.h"
#include "assets/attachment.h"
#include "assets/model_asset.h"
#include "assets/weapon.h"
#include "assets/xanim.h"
#include "render/renderer.h"

namespace wb {
namespace {

using json = nlohmann::json;
constexpr const char* kProtocolVersion = "2025-06-18";

json Vec3(const DirectX::XMFLOAT3& value) { return json::array({value.x, value.y, value.z}); }
json Vec4(const DirectX::XMFLOAT4& value) {
	return json::array({value.x, value.y, value.z, value.w});
}

std::string ToLower(std::string text) {
	std::transform(text.begin(), text.end(), text.begin(),
	               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return text;
}

std::string Base64(const std::vector<std::uint8_t>& data) {
	static const char table[] =
	    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	std::string out;
	out.reserve((data.size() + 2) / 3 * 4);
	std::size_t i = 0;
	while (i + 2 < data.size()) {
		const std::uint32_t value = (static_cast<std::uint32_t>(data[i]) << 16) |
		                            (static_cast<std::uint32_t>(data[i + 1]) << 8) | data[i + 2];
		out.push_back(table[(value >> 18) & 63]);
		out.push_back(table[(value >> 12) & 63]);
		out.push_back(table[(value >> 6) & 63]);
		out.push_back(table[value & 63]);
		i += 3;
	}
	if (i + 1 == data.size()) {
		const std::uint32_t value = static_cast<std::uint32_t>(data[i]) << 16;
		out.push_back(table[(value >> 18) & 63]);
		out.push_back(table[(value >> 12) & 63]);
		out.push_back('=');
		out.push_back('=');
	} else if (i + 2 == data.size()) {
		const std::uint32_t value =
		    (static_cast<std::uint32_t>(data[i]) << 16) | (static_cast<std::uint32_t>(data[i + 1]) << 8);
		out.push_back(table[(value >> 18) & 63]);
		out.push_back(table[(value >> 12) & 63]);
		out.push_back(table[(value >> 6) & 63]);
		out.push_back('=');
	}
	return out;
}

void PngSize(const std::vector<std::uint8_t>& data, int& width, int& height) {
	width = 0;
	height = 0;
	if (data.size() >= 24) {
		width = (static_cast<int>(data[16]) << 24) | (static_cast<int>(data[17]) << 16) |
		        (static_cast<int>(data[18]) << 8) | static_cast<int>(data[19]);
		height = (static_cast<int>(data[20]) << 24) | (static_cast<int>(data[21]) << 16) |
		         (static_cast<int>(data[22]) << 8) | static_cast<int>(data[23]);
	}
}

std::filesystem::path RepoRoot(const std::filesystem::path& exeDir) {
	std::filesystem::path path = exeDir;
	if (ToLower(path.filename().string()) == "release" || ToLower(path.filename().string()) == "debug") {
		path = path.parent_path();
	}
	if (ToLower(path.filename().string()) == "bin") {
		path = path.parent_path();
	}
	if (ToLower(path.filename().string()) == "build") {
		path = path.parent_path();
	}
	return path;
}

// Captures the stdout of a printf-based routine (info/validate) so the JSON-RPC stream stays clean.
std::string CaptureStdout(const std::function<void()>& routine) {
	std::fflush(stdout);
	const int saved = _dup(1);
	char tempPath[MAX_PATH] = {};
	char tempFile[MAX_PATH] = {};
	GetTempPathA(MAX_PATH, tempPath);
	GetTempFileNameA(tempPath, "hmw", 0, tempFile);
	const int file = _open(tempFile, _O_CREAT | _O_TRUNC | _O_WRONLY | _O_BINARY, _S_IREAD | _S_IWRITE);
	if (file < 0 || saved < 0) {
		if (saved >= 0) {
			_close(saved);
		}
		return "<stdout capture failed>";
	}
	_dup2(file, 1);
	routine();
	std::fflush(stdout);
	_dup2(saved, 1);
	_close(saved);
	_close(file);
	std::ifstream in(tempFile, std::ios::binary);
	std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	in.close();
	std::remove(tempFile);
	return text;
}

class McpServer {
public:
	McpServer(std::filesystem::path exeDir, Settings& settings, std::vector<std::filesystem::path> roots)
	    : exeDir_(std::move(exeDir)), settings_(settings), roots_(std::move(roots)),
	      repoRoot_(RepoRoot(exeDir_)), workbench_(settings_, assets_, renderer_, roots_) {
		assets_.Scan(roots_);
		shotCounter_ = 0;
	}

	bool Initialize() {
		RendererDesc desc;
		desc.hwnd = nullptr;
		desc.width = 1920;
		desc.height = 1016;
		if (!renderer_.Initialize(desc)) {
			return false;
		}
		return true;
	}

	void Shutdown() { renderer_.Shutdown(); }

	int Run() {
		_setmode(_fileno(stdin), _O_BINARY);
		_setmode(_fileno(stdout), _O_BINARY);
		std::string line;
		while (std::getline(std::cin, line)) {
			if (!line.empty() && line.back() == '\r') {
				line.pop_back();
			}
			if (line.empty()) {
				continue;
			}
			try {
				Handle(json::parse(line));
			} catch (const std::exception& e) {
				Send({{"jsonrpc", "2.0"}, {"id", nullptr}, {"error", {{"code", -32700}, {"message", e.what()}}}});
			}
		}
		return 0;
	}

private:
	void Send(const json& message) {
		const std::string line = message.dump(-1, ' ', false, json::error_handler_t::replace) + "\n";
		std::fwrite(line.data(), 1, line.size(), stdout);
		std::fflush(stdout);
	}

	json ToolSchemas() {
		const auto str = [](const char* description) { return json{{"type", "string"}, {"description", description}}; };
		const auto integer = [](const char* description) { return json{{"type", "integer"}, {"description", description}}; };
		const auto number = [](const char* description) { return json{{"type", "number"}, {"description", description}}; };
		const auto boolean = [](const char* description) { return json{{"type", "boolean"}, {"description", description}}; };
		const json screenshotProps = {
		    {"path", str("Output PNG path (relative paths are resolved against the repo root).")},
		    {"view", str("'firstperson' or 'orbit'.")},
		    {"width", integer("Image width (default 1920; sheet cell width).")},
		    {"height", integer("Image height (default 1016; sheet cell height).")},
		    {"yaw", number("Camera yaw in degrees.")},
		    {"pitch", number("Camera pitch in degrees.")},
		    {"dist", number("Orbit distance.")},
		    {"fov", number("Vertical FOV override in degrees.")},
		    {"bones", boolean("Draw standalone-model bones.")},
		    {"wire", boolean("Wireframe rendering.")},
		    {"crosshair", boolean("Draw a crosshair at the screen centre.")},
		    {"marktags", boolean("Label every weapon tag.")},
		    {"caption", str("Caption drawn at the top left.")},
		    {"sheet", integer("Contact sheet: N cells of the current animation instead of one frame.")}};

		return json::array({
		    {{"name", "list_assets"},
		     {"description", "List scanned assets (weapons, xmodels, xanims). Optional kind and name substring."},
		     {"inputSchema",
		      {{"type", "object"},
		       {"properties", {{"kind", str("weapon, model, anim or all (default all).")},
		                       {"name", str("Case-insensitive name substring.")},
		                       {"limit", integer("Maximum entries (default 200, max 1000).")}}}}}},
		    {{"name", "load_weapon"},
		     {"description", "Load a weapon with its hands and attachments, first person by default."},
		     {"inputSchema",
		      {{"type", "object"},
		       {"properties", {{"weapon", str("Weapon json name, e.g. iw5_exocrossbow_mp.")},
		                       {"hands", str("Viewhands override (optional).")},
		                       {"attachments", {{"type", "array"}, {"items", {{"type", "string"}}}}},
		                       {"first_person", boolean("First-person view (default true).")}}},
		       {"required", {"weapon"}}}}},
		    {{"name", "load_model"},
		     {"description", "Load a standalone xmodel (orbit view; unloads the weapon)."},
		     {"inputSchema",
		      {{"type", "object"},
		       {"properties", {{"model", str("Xmodel name.")}}},
		       {"required", {"model"}}}}},
		    {{"name", "set_anim"},
		     {"description", "Play an animation (xanim name or weapon state such as IDLE/ADS_UP), set time/fraction and ADS."},
		     {"inputSchema",
		      {{"type", "object"},
		       {"properties", {{"name", str("Xanim name or weapon state.")},
		                       {"time", number("Seconds.")},
		                       {"frac", number("Normalized 0..1.")},
		                       {"ads", boolean("Aim down sights.")}}},
		       {"required", {"name"}}}}},
		    {{"name", "screenshot"},
		     {"description",
		      "Render offscreen and return the PNG as image content plus its path; supports camera, overlays and contact sheets."},
		     {"inputSchema", {{"type", "object"}, {"properties", screenshotProps}}}},
		    {{"name", "get_bones"},
		     {"description", "Posed bones in viewmodel space (name, parent, source, local/world position and rotation)."},
		     {"inputSchema",
		      {{"type", "object"},
		       {"properties", {{"name", str("Name substring filter.")},
		                       {"tags_only", boolean("Only tag_* bones.")},
		                       {"limit", integer("Maximum entries (default 1000).")}}}}}},
		    {{"name", "get_tags"},
		     {"description", "Posed tag bones with world (viewmodel-space) positions."},
		     {"inputSchema",
		      {{"type", "object"},
		       {"properties", {{"name", str("Name substring filter.")}}}}}},
		    {{"name", "posedump"},
		     {"description", "Text dump of the posed skeleton, animation channels, gun bounds and tag screen positions."},
		     {"inputSchema",
		      {{"type", "object"},
		       {"properties", {{"width", integer("Screen width for projection (default 1920).")},
		                       {"height", integer("Screen height for projection (default 1016).")}}}}}},
		    {{"name", "attachment_fit_report"},
		     {"description",
		      "Fit report as data: every attachment's view model, needed weapon tag, found/missing and nearest tags."},
		     {"inputSchema",
		      {{"type", "object"},
		       {"properties", {{"weapon", str("Weapon (default: the loaded weapon).")},
		                       {"hands", str("Viewhands override.")},
		                       {"attachments", {{"type", "array"}, {"items", {{"type", "string"}}}}}}}}}},
		    {{"name", "compare_tags"},
		     {"description", "Compare the bind-pose tags of two weapons/models: shared tags with deltas, and tags only on one side."},
		     {"inputSchema",
		      {{"type", "object"},
		       {"properties", {{"a", str("Weapon or model name.")}, {"b", str("Weapon or model name.")}}},
		       {"required", {"a", "b"}}}}},
		    {{"name", "validate"},
		     {"description", "Parse every xmodel + xsurface + material in all roots; returns the text summary and failure count (~10 s)."},
		     {"inputSchema", {{"type", "object"}, {"properties", json::object()}}}},
		    {{"name", "info"},
		     {"description", "Text info for a weapon, xmodel or xanim (the same dump as the -info CLI)."},
		     {"inputSchema",
		      {{"type", "object"},
		       {"properties", {{"asset", str("Asset name.")},
		                       {"kind", str("weapon, model or anim (optional; detected otherwise).")}}},
		       {"required", {"asset"}}}}},
		});
	}

	json CallTool(const std::string& name, const json& args) {
		if (name == "list_assets") {
			return ListAssets(args);
		}
		if (name == "load_weapon") {
			return LoadWeapon(args);
		}
		if (name == "load_model") {
			return LoadModel(args);
		}
		if (name == "set_anim") {
			return SetAnim(args);
		}
		if (name == "screenshot") {
			return Screenshot(args);
		}
		if (name == "get_bones") {
			return GetBones(args);
		}
		if (name == "get_tags") {
			return GetTags(args);
		}
		if (name == "posedump") {
			return PoseDump(args);
		}
		if (name == "attachment_fit_report") {
			return AttachmentFitReport(args);
		}
		if (name == "compare_tags") {
			return CompareTags(args);
		}
		if (name == "validate") {
			return Validate(args);
		}
		if (name == "info") {
			return Info(args);
		}
		return json{{"error", "unknown tool: " + name}};
	}

	json ListAssets(const json& args) {
		const std::string kindText = ToLower(args.value("kind", "all"));
		const std::string needle = ToLower(args.value("name", ""));
		const int limit = std::clamp(args.value("limit", 200), 1, 1000);
		int kind = -1;
		if (kindText == "weapon") {
			kind = static_cast<int>(AssetKind::Weapon);
		} else if (kindText == "model") {
			kind = static_cast<int>(AssetKind::Model);
		} else if (kindText == "anim") {
			kind = static_cast<int>(AssetKind::Anim);
		} else if (kindText != "all") {
			return json{{"error", "kind must be weapon, model, anim or all"}};
		}

		json entries = json::array();
		int matched = 0;
		bool truncated = false;
		for (const AssetEntry& entry : assets_.Entries()) {
			if (kind >= 0 && static_cast<int>(entry.kind) != kind) {
				continue;
			}
			if (!needle.empty() && ToLower(entry.name).find(needle) == std::string::npos) {
				continue;
			}
			++matched;
			if (static_cast<int>(entries.size()) >= limit) {
				truncated = true;
				continue;
			}
			entries.emplace_back(json{{"name", entry.name}, {"kind", AssetKindLabel(entry.kind)},
			                          {"root", entry.rootIndex}});
		}
		return json{{"count", matched}, {"truncated", truncated}, {"assets", entries}};
	}

	json LoadWeapon(const json& args) {
		const std::string weapon = args.value("weapon", "");
		if (weapon.empty()) {
			return json{{"error", "weapon is required"}};
		}
		const std::string hands = args.value("hands", "");
		const bool firstPerson = args.value("first_person", true);
		std::string error;
		if (!workbench_.LoadWeapon(weapon, hands, firstPerson, error)) {
			return json{{"error", error}};
		}
		json attachments = json::array();
		if (args.contains("attachments") && args["attachments"].is_array()) {
			for (const auto& value : args["attachments"]) {
				if (!value.is_string()) {
					continue;
				}
				const std::string attachment = value.get<std::string>();
				std::string attachError;
				if (workbench_.Attach(attachment, attachError)) {
					const commands::AttachmentMount* mount = nullptr;
					for (const auto& candidate : workbench_.Attachments()) {
						if (candidate.name == attachment || candidate.internalName == attachment) {
							mount = &candidate;
						}
					}
					if (mount == nullptr && !workbench_.Attachments().empty()) {
						mount = &workbench_.Attachments().back();
					}
					if (mount != nullptr) {
						attachments.emplace_back(json{{"name", mount->name},
						                              {"view_model", mount->viewModel},
						                              {"required_tag", mount->requiredTag},
						                              {"found", mount->tagFound}});
					}
				} else {
					attachments.emplace_back(json{{"name", attachment}, {"error", attachError}});
				}
			}
		}
		const scene::ViewmodelAsset* viewmodel = workbench_.Viewmodel();
		json warnings = json::array();
		for (const std::string& warning : viewmodel->warnings) {
			warnings.push_back(warning);
		}
		return json{{"weapon", viewmodel->weapon.name},
		            {"gun_model", viewmodel->weapon.gunModel},
		            {"hands", viewmodel->handsName},
		            {"hands_source", viewmodel->handsSource},
		            {"first_person", workbench_.IsFirstPerson()},
		            {"attachments", attachments},
		            {"warnings", warnings}};
	}

	json LoadModel(const json& args) {
		const std::string model = args.value("model", "");
		if (model.empty()) {
			return json{{"error", "model is required"}};
		}
		workbench_.UnloadWeapon();
		workbench_.StopAnim();
		workbench_.SetViewMode(commands::ViewMode::Orbit);
		std::string error;
		if (!workbench_.LoadModel(model, error)) {
			return json{{"error", error}};
		}
		const assets::ModelAsset* asset = workbench_.Model();
		return json{{"model", asset->name},
		            {"bones", asset->model.bones.size()},
		            {"surfaces", asset->surfaceSet.surfaces.size()},
		            {"materials", asset->materials.size()},
		            {"bounds_mid", Vec3(asset->boundsCenter)},
		            {"bounds_half", Vec3(asset->boundsHalf)}};
	}

	json SetAnim(const json& args) {
		const std::string requested = args.value("name", "");
		if (requested.empty()) {
			return json{{"error", "name is required"}};
		}
		std::string anim = requested;
		for (const commands::AnimStateEntry& state : workbench_.AnimStates()) {
			if (state.state == requested) {
				const std::string effective = workbench_.EffectiveAnimForState(requested);
				anim = effective.empty() ? state.anim : effective;
				break;
			}
		}
		std::string error;
		if (!workbench_.PlayAnim(anim, error)) {
			return json{{"error", error}};
		}
		if (args.contains("time") && args["time"].is_number()) {
			workbench_.SetAnimTime(args["time"].get<double>());
		} else if (args.contains("frac") && args["frac"].is_number()) {
			workbench_.SetAnimFraction(args["frac"].get<double>());
		}
		if (args.contains("ads") && args["ads"].is_boolean()) {
			workbench_.SetAds(args["ads"].get<bool>());
		}
		const assets::XAnimData* loaded = workbench_.Anim();
		return json{{"requested", requested},
		            {"anim", loaded->name},
		            {"frames", loaded->frameCount},
		            {"fps", loaded->frameRate},
		            {"duration", loaded->Duration()},
		            {"time", workbench_.AnimTime()},
		            {"frame", workbench_.AnimFrame()},
		            {"ads", workbench_.Aiming()},
		            {"looping", loaded->looping}};
	}

	json Screenshot(const json& args) {
		if (workbench_.Viewmodel() == nullptr && workbench_.Model() == nullptr) {
			return json{{"error", "no weapon or model loaded"}};
		}
		ShotOptions shot;
		shot.width = std::clamp(args.value("width", 1920), 1, 7680);
		shot.height = std::clamp(args.value("height", 1016), 1, 4320);
		shot.bones = args.value("bones", false);
		shot.wire = args.value("wire", false);
		shot.crosshair = args.value("crosshair", false);
		shot.markTags = args.value("marktags", false);
		shot.caption = args.value("caption", "");
		shot.sheet = std::clamp(args.value("sheet", 0), 0, 64);

		const bool hasYaw = args.contains("yaw") && args["yaw"].is_number();
		const bool hasPitch = args.contains("pitch") && args["pitch"].is_number();
		const bool hasDist = args.contains("dist") && args["dist"].is_number();
		const bool hasFov = args.contains("fov") && args["fov"].is_number();
		const float yaw = hasYaw ? args["yaw"].get<float>() : 0.0f;
		const float pitch = hasPitch ? args["pitch"].get<float>() : 0.0f;
		const float dist = hasDist ? args["dist"].get<float>() : 0.0f;
		const float fov = hasFov ? args["fov"].get<float>() : 0.0f;
		const std::string view = ToLower(args.value("view", ""));
		if (view == "firstperson" || view == "fp") {
			workbench_.SetFirstPersonView(yaw, pitch, fov);
		} else if (view == "orbit") {
			workbench_.SetOrbitView(yaw, pitch, dist, fov);
		} else if (hasYaw || hasPitch || hasDist || hasFov) {
			if (workbench_.IsFirstPerson()) {
				workbench_.SetFirstPersonView(yaw, pitch, fov);
			} else {
				workbench_.SetOrbitView(yaw, pitch, dist, fov);
			}
		}
		if (args.contains("ads") && args["ads"].is_boolean()) {
			workbench_.SetAds(args["ads"].get<bool>());
		}

		std::filesystem::path output;
		const std::string requestedPath = args.value("path", "");
		if (!requestedPath.empty()) {
			output = std::filesystem::path(requestedPath);
			if (output.is_relative()) {
				output = repoRoot_ / output;
			}
		} else {
			char name[64];
			std::snprintf(name, sizeof(name), "shot_%03d.png", ++shotCounter_);
			output = repoRoot_ / "review" / "mcp" / name;
		}

		std::string error;
		if (!RenderShot(workbench_, renderer_, shot, output, error)) {
			return json{{"error", error}};
		}

		std::ifstream file(output, std::ios::binary);
		std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)),
		                                 std::istreambuf_iterator<char>());
		if (bytes.empty()) {
			return json{{"error", "screenshot file is empty: " + output.string()}};
		}
		int width = 0;
		int height = 0;
		PngSize(bytes, width, height);
		return json{{"path", output.string()},
		            {"width", width},
		            {"height", height},
		            {"bytes", bytes.size()},
		            {"png_base64", Base64(bytes)}};
	}

	json GetBones(const json& args) {
		if (workbench_.Viewmodel() == nullptr && workbench_.Model() == nullptr) {
			return json{{"error", "no weapon or model loaded"}};
		}
		const std::string needle = ToLower(args.value("name", ""));
		const bool tagsOnly = args.value("tags_only", false);
		const int limit = std::clamp(args.value("limit", 1000), 1, 5000);
		const bool standalone = workbench_.Viewmodel() == nullptr;
		const scene::SkeletonPose& pose = workbench_.Pose();
		json bones = json::array();
		int total = 0;
		for (const scene::PoseBone& bone : pose.bones) {
			if (tagsOnly && bone.name.rfind("tag_", 0) != 0) {
				continue;
			}
			if (!needle.empty() && ToLower(bone.name).find(needle) == std::string::npos) {
				continue;
			}
			++total;
			if (static_cast<int>(bones.size()) >= limit) {
				continue;
			}
			const std::string parent =
			    bone.parent >= 0 && static_cast<std::size_t>(bone.parent) < pose.bones.size()
			        ? pose.bones[static_cast<std::size_t>(bone.parent)].name
			        : std::string();
			bones.emplace_back(json{{"name", bone.name},
			                        {"parent", parent},
			                        {"source", standalone ? "model" : (bone.fromHands ? "hands" : "gun")},
			                        {"local_pos", Vec3(bone.localPos)},
			                        {"world_pos", Vec3(bone.worldPos)},
			                        {"local_quat", Vec4(bone.localQuat)},
			                        {"world_quat", Vec4(bone.worldQuat)}});
		}
		return json{{"count", total}, {"bones", bones}};
	}

	json GetTags(const json& args) {
		if (workbench_.Viewmodel() == nullptr && workbench_.Model() == nullptr) {
			return json{{"error", "no weapon or model loaded"}};
		}
		const std::string needle = ToLower(args.value("name", ""));
		const scene::SkeletonPose& pose = workbench_.Pose();
		json tags = json::array();
		for (const scene::PoseBone& bone : pose.bones) {
			if (bone.name.rfind("tag_", 0) != 0) {
				continue;
			}
			if (!needle.empty() && ToLower(bone.name).find(needle) == std::string::npos) {
				continue;
			}
			tags.emplace_back(json{{"name", bone.name},
			                       {"local_pos", Vec3(bone.localPos)},
			                       {"world_pos", Vec3(bone.worldPos)}});
		}
		return json{{"count", tags.size()}, {"tags", tags}};
	}

	json PoseDump(const json& args) {
		if (workbench_.Viewmodel() == nullptr && workbench_.Model() == nullptr) {
			return json{{"error", "no weapon or model loaded"}};
		}
		const int width = args.value("width", 1920);
		const int height = args.value("height", 1016);
		char tempPath[MAX_PATH] = {};
		char tempFile[MAX_PATH] = {};
		GetTempPathA(MAX_PATH, tempPath);
		GetTempFileNameA(tempPath, "hmw", 0, tempFile);
		std::FILE* file = nullptr;
		if (_wfopen_s(&file, std::filesystem::path(tempFile).c_str(), L"w") != 0 || file == nullptr) {
			return json{{"error", "cannot create a temp file"}};
		}
		PrintPoseDump(workbench_, DefaultPoseDumpBones(), width, height, file);
		std::fclose(file);
		std::ifstream in(tempFile, std::ios::binary);
		std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
		in.close();
		std::remove(tempFile);
		return json{{"text", text}};
	}

	json AttachmentFitReport(const json& args) {
		std::string weapon = args.value("weapon", "");
		if (weapon.empty()) {
			weapon = workbench_.WeaponName();
		}
		if (weapon.empty()) {
			return json{{"error", "weapon is required (or load a weapon first)"}};
		}
		std::string hands = args.value("hands", "");
		std::vector<std::string> filter;
		if (args.contains("attachments") && args["attachments"].is_array()) {
			for (const auto& value : args["attachments"]) {
				if (value.is_string()) {
					filter.push_back(value.get<std::string>());
				}
			}
		}
		commands::AttachmentFitReport report;
		std::string error;
		if (!commands::BuildFitReport(roots_, weapon, hands, filter, report, error)) {
			return json{{"error", error}};
		}
		json entries = json::array();
		for (const commands::AttachmentFitEntry& entry : report.attachments) {
			entries.emplace_back(json{{"name", entry.name},
			                          {"view_model", entry.viewModel},
			                          {"required_tag", entry.requiredTag},
			                          {"found", entry.found},
			                          {"nearest", entry.nearest},
			                          {"note", entry.note}});
		}
		return json{{"weapon", report.weapon},
		            {"gun_model", report.gunModel},
		            {"hands", report.handsModel},
		            {"weapon_tags", report.weaponTags},
		            {"attachments", entries}};
	}

	json CompareTags(const json& args) {
		const std::string a = args.value("a", "");
		const std::string b = args.value("b", "");
		if (a.empty() || b.empty()) {
			return json{{"error", "a and b are required"}};
		}
		const auto resolve = [this](const std::string& name) {
			std::string model = name;
			if (const auto path = assets::FindWeaponFile(roots_, name); path.has_value()) {
				try {
					model = assets::LoadWeapon(*path).gunModel;
				} catch (const std::exception&) {
				}
			}
			return model;
		};
		const auto tagsOf = [this](const std::string& name, std::map<std::string, DirectX::XMFLOAT3>& tags,
		                           std::string& error) {
			assets::ModelAsset model;
			if (!commands::LoadModelForInfo(roots_, name, model, error)) {
				return false;
			}
			for (const assets::BoneData& bone : model.model.bones) {
				if (bone.name.rfind("tag_", 0) == 0) {
					tags[bone.name] = bone.worldPos;
				}
			}
			return true;
		};
		const std::string modelA = resolve(a);
		const std::string modelB = resolve(b);
		std::map<std::string, DirectX::XMFLOAT3> tagsA;
		std::map<std::string, DirectX::XMFLOAT3> tagsB;
		std::string error;
		if (!tagsOf(modelA, tagsA, error)) {
			return json{{"error", "a: " + error}};
		}
		if (!tagsOf(modelB, tagsB, error)) {
			return json{{"error", "b: " + error}};
		}
		json shared = json::array();
		json onlyA = json::array();
		json onlyB = json::array();
		for (const auto& [name, position] : tagsA) {
			const auto other = tagsB.find(name);
			if (other == tagsB.end()) {
				onlyA.push_back(name);
				continue;
			}
			const DirectX::XMFLOAT3 delta = {other->second.x - position.x, other->second.y - position.y,
			                                 other->second.z - position.z};
			shared.emplace_back(json{{"name", name},
			                         {"a", Vec3(position)},
			                         {"b", Vec3(other->second)},
			                         {"delta", Vec3(delta)},
			                         {"distance", std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z)}});
		}
		for (const auto& [name, position] : tagsB) {
			(void)position;
			if (tagsA.find(name) == tagsA.end()) {
				onlyB.push_back(name);
			}
		}
		return json{{"a", a}, {"a_model", modelA}, {"b", b}, {"b_model", modelB},
		            {"shared", shared}, {"only_a", onlyA}, {"only_b", onlyB}};
	}

	json Validate(const json&) {
		int failures = 0;
		const std::string output = CaptureStdout([this, &failures] { failures = RunValidate(roots_); });
		json lines = json::array();
		std::string line;
		std::istringstream stream(output);
		while (std::getline(stream, line)) {
			if (line.rfind("  ", 0) == 0 || line.rfind("summary:", 0) == 0) {
				lines.push_back(line);
			}
		}
		return json{{"failures", failures}, {"summary", lines}, {"output", output}};
	}

	json Info(const json& args) {
		const std::string name = args.value("asset", "");
		if (name.empty()) {
			return json{{"error", "asset is required"}};
		}
		const std::string kind = ToLower(args.value("kind", ""));
		bool ok = false;
		const std::string output = CaptureStdout([this, &name, &kind, &ok] {
			if (kind == "weapon" || (kind.empty() && assets::FindWeaponFile(roots_, name).has_value() &&
			                         !assets::FindAnimFile(roots_, name).has_value())) {
				ok = PrintWeaponInfo(name, roots_);
			} else if (kind == "anim" || (kind.empty() && assets::FindAnimFile(roots_, name).has_value())) {
				ok = PrintAnimInfo(name, roots_);
			} else {
				ok = PrintModelInfo(name, roots_);
			}
		});
		if (!ok) {
			return json{{"error", output.empty() ? "info failed: " + name : output}};
		}
		return json{{"asset", name}, {"text", output}};
	}

	void Handle(const json& message) {
		const std::string method = message.value("method", "");
		const bool hasId = message.contains("id");
		const json id = hasId ? message["id"] : json{};
		const auto reply = [&](const json& result) {
			if (hasId) {
				Send({{"jsonrpc", "2.0"}, {"id", id}, {"result", result}});
			}
		};

		if (method == "initialize") {
			const auto requested =
			    message.contains("params") ? message["params"].value("protocolVersion", kProtocolVersion)
			                               : kProtocolVersion;
			reply({{"protocolVersion", requested},
			       {"capabilities", {{"tools", json::object()}}},
			       {"serverInfo", {{"name", "hmw-workbench"}, {"version", "0.1"}}},
			       {"instructions",
			        "hmw-workbench: load HMW/MWR weapons, models and attachments, pose animations, answer "
			        "tag/fit questions and render offscreen PNG screenshots. All positions are game/viewmodel "
			        "space (X forward, Y left, Z up)."}});
			return;
		}
		if (method == "ping") {
			reply(json::object());
			return;
		}
		if (method == "tools/list") {
			reply({{"tools", ToolSchemas()}});
			return;
		}
		if (method == "tools/call") {
			const auto& params = message.contains("params") ? message["params"] : json::object();
			const std::string name = params.value("name", "");
			const json args = params.contains("arguments") && params["arguments"].is_object()
			                      ? params["arguments"]
			                      : json::object();
			json result;
			try {
				result = CallTool(name, args);
			} catch (const std::exception& e) {
				result = json{{"error", e.what()}};
			}
			const bool isError = result.contains("error");
			if (!isError && name == "screenshot" && result.contains("png_base64")) {
				json content = json::array();
				content.emplace_back(
				    json{{"type", "image"}, {"data", result["png_base64"]}, {"mimeType", "image/png"}});
				content.emplace_back(json{
				    {"type", "text"},
				    {"text", "saved to " + result.value("path", std::string()) + " (" +
				                 std::to_string(result.value("width", 0)) + "x" +
				                 std::to_string(result.value("height", 0)) + ", " +
				                 std::to_string(result.value("bytes", 0)) + " bytes)"}});
				reply({{"content", content}, {"isError", false}});
				return;
			}
			const json payload = isError ? result : (result.contains("result") ? result["result"] : result);
			const std::string text =
			    payload.is_string() ? payload.get<std::string>()
			                        : payload.dump(2, ' ', false, json::error_handler_t::replace);
			reply({{"content", json::array({{{"type", "text"}, {"text", text}}})}, {"isError", isError}});
			return;
		}
		if (method.rfind("notifications/", 0) == 0) {
			return;
		}
		if (hasId) {
			Send({{"jsonrpc", "2.0"},
			      {"id", id},
			      {"error", {{"code", -32601}, {"message", "method not found: " + method}}}});
		}
	}

	std::filesystem::path exeDir_;
	Settings& settings_;
	std::vector<std::filesystem::path> roots_;
	std::filesystem::path repoRoot_;
	AssetIndex assets_;
	Renderer renderer_;
	commands::Workbench workbench_;
	int shotCounter_ = 0;
};

} // namespace

int RunMcp(const std::filesystem::path& exeDir, Settings& settings) {
	const std::vector<std::filesystem::path> roots = ResolveAssetRoots(settings, exeDir);
	McpServer server(exeDir, settings, roots);
	if (!server.Initialize()) {
		std::fprintf(stderr, "hmw-workbench -mcp: failed to initialize D3D11\n");
		return 1;
	}
	const int result = server.Run();
	server.Shutdown();
	return result;
}

} // namespace wb
