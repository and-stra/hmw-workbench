#include "app/commands.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_set>

#include "assets/camo.h"

#include <fstream>
#include <nlohmann/json.hpp>

namespace wb::commands {
namespace {

constexpr float kMinSpeed = 0.1f;
constexpr float kMaxSpeed = 2.0f;

DirectX::XMMATRIX BoneMatrix(const assets::BoneData& bone) {
	DirectX::XMVECTOR quaternion = DirectX::XMLoadFloat4(&bone.worldQuat);
	if (DirectX::XMVectorGetX(DirectX::XMVector4LengthSq(quaternion)) < 1.0e-8f) {
		quaternion = DirectX::XMQuaternionIdentity();
	}
	quaternion = DirectX::XMQuaternionNormalize(quaternion);
	return DirectX::XMMatrixRotationQuaternion(quaternion) *
	       DirectX::XMMatrixTranslation(bone.worldPos.x, bone.worldPos.y, bone.worldPos.z);
}

DirectX::XMMATRIX PoseBoneMatrix(const scene::PoseBone& bone) {
	DirectX::XMVECTOR quaternion = DirectX::XMLoadFloat4(&bone.worldQuat);
	if (DirectX::XMVectorGetX(DirectX::XMVector4LengthSq(quaternion)) < 1.0e-8f) {
		quaternion = DirectX::XMQuaternionIdentity();
	}
	quaternion = DirectX::XMQuaternionNormalize(quaternion);
	return DirectX::XMMatrixRotationQuaternion(quaternion) *
	       DirectX::XMMatrixTranslation(bone.worldPos.x, bone.worldPos.y, bone.worldPos.z);
}

int FindPoseBone(const scene::SkeletonPose& pose, const std::string& name) {
	for (std::size_t i = 0; i < pose.bones.size(); ++i) {
		if (pose.bones[i].name == name) {
			return static_cast<int>(i);
		}
	}
	return -1;
}

int FindModelBone(const assets::XModelData& model, const std::string& name) {
	for (std::size_t i = 0; i < model.bones.size(); ++i) {
		if (model.bones[i].name == name) {
			return static_cast<int>(i);
		}
	}
	return -1;
}

bool ModelHasBone(const assets::XModelData& model, const std::string& name) {
	return FindModelBone(model, name) >= 0;
}

std::string ToLower(std::string text) {
	std::transform(text.begin(), text.end(), text.begin(),
	               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return text;
}

std::vector<std::string> SplitTokens(const std::string& text) {
	std::vector<std::string> tokens;
	std::string current;
	for (const char character : text) {
		if (character == ' ' || character == '\t') {
			if (!current.empty()) {
				tokens.push_back(current);
				current.clear();
			}
		} else {
			current.push_back(character);
		}
	}
	if (!current.empty()) {
		tokens.push_back(current);
	}
	return tokens;
}

std::string ReplaceToken(const std::string& text, const std::string& from, const std::string& to) {
	if (from.empty()) {
		return text;
	}
	const std::size_t position = text.find(from);
	if (position == std::string::npos) {
		return text;
	}
	std::string out = text;
	out.replace(position, from.size(), to);
	return out;
}

std::string StateForAnimTreeType(int type) {
	// H1 weapAnimFiles_t values (cod-mwx database.hpp enum weapAnimFiles_t):
	// WEAP_ANIM_ADS_UP = 129, WEAP_ANIM_ADS_DOWN = 130.
	switch (type) {
	case 2:
		return "EMPTY_IDLE";
	case 47:
		return "HYBRID_SIGHT_ON";
	case 48:
		return "HYBRID_SIGHT_OFF";
	case 64:
		return "ADS_FIRE";
	case 65:
		return "ADS_LASTSHOT";
	case 129:
		return "ADS_UP";
	case 130:
		return "ADS_DOWN";
	case 133:
		return "ADDITIVE_ADS_ROOT";
	case 135:
		return "ADDITIVE_HYBRID_SIGHT_UP_ROOT";
	default:
		return {};
	}
}

std::optional<std::filesystem::path> FindAttachmentByName(const std::vector<std::filesystem::path>& roots,
                                                          const std::string& name) {
	if (const auto direct = assets::FindAttachmentFile(roots, name); direct.has_value()) {
		return direct;
	}
	const std::string lower = ToLower(name);
	for (const std::string& candidate : assets::ListAttachments(roots)) {
		if (ToLower(candidate) != lower) {
			continue;
		}
		if (const auto path = assets::FindAttachmentFile(roots, candidate); path.has_value()) {
			return path;
		}
	}
	for (const std::string& candidate : assets::ListAttachments(roots)) {
		const auto path = assets::FindAttachmentFile(roots, candidate);
		if (!path.has_value()) {
			continue;
		}
		try {
			const assets::AttachmentAsset attachment = assets::LoadAttachment(*path);
			if (ToLower(attachment.internalName) == lower) {
				return path;
			}
		} catch (const std::exception&) {
			continue;
		}
	}
	return std::nullopt;
}

DirectX::XMFLOAT4X4 ToFloat4x4(const DirectX::XMMATRIX& matrix) {
	DirectX::XMFLOAT4X4 out;
	DirectX::XMStoreFloat4x4(&out, matrix);
	return out;
}

void StoreZero(DirectX::XMFLOAT4X4& matrix) {
	std::memset(&matrix, 0, sizeof(matrix));
}

DirectX::XMFLOAT4 NormalizeQuat(DirectX::XMFLOAT4 q) {
	const float length = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
	if (length > 1.0e-8f) {
		q.x /= length;
		q.y /= length;
		q.z /= length;
		q.w /= length;
	} else {
		q = {0.0f, 0.0f, 0.0f, 1.0f};
	}
	return q;
}

DirectX::XMFLOAT4 InverseQuat(const DirectX::XMFLOAT4& q) {
	DirectX::XMFLOAT4 out;
	DirectX::XMStoreFloat4(&out, DirectX::XMQuaternionInverse(DirectX::XMLoadFloat4(&q)));
	return out;
}

DirectX::XMFLOAT4 MultiplyQuat(const DirectX::XMFLOAT4& a, const DirectX::XMFLOAT4& b) {
	DirectX::XMFLOAT4 out;
	DirectX::XMStoreFloat4(&out, DirectX::XMQuaternionMultiply(DirectX::XMLoadFloat4(&a),
	                                                          DirectX::XMLoadFloat4(&b)));
	return out;
}

DirectX::XMFLOAT3 Rotate(const DirectX::XMFLOAT3& value, const DirectX::XMFLOAT4& q) {
	DirectX::XMFLOAT3 out;
	DirectX::XMStoreFloat3(&out, DirectX::XMVector3Rotate(DirectX::XMLoadFloat3(&value),
	                                                      DirectX::XMLoadFloat4(&q)));
	return out;
}

float BoundsRadius(const DirectX::XMFLOAT3& halfSize) {
	return std::max({halfSize.x, halfSize.y, halfSize.z, 0.25f}) * 1.1f;
}

// H1 cg_fov is the horizontal FOV of a 4:3 view. The engine widens it horizontally
// for wider aspect ratios (Hor+), so the vertical FOV is independent of aspect:
// fovY = 2 * atan(tan(fovX / 2) * 3/4).
float VerticalFovFromCgFov(float cgFov) {
	const float half = DirectX::XMConvertToRadians(std::clamp(cgFov, 1.0f, 179.0f) * 0.5f);
	return DirectX::XMConvertToDegrees(2.0f * std::atan(std::tan(half) * 0.75f));
}

bool FindAnimState(const std::vector<AnimStateEntry>& states, const std::string& state, std::string& anim) {
	for (const AnimStateEntry& entry : states) {
		if (entry.state == state) {
			anim = entry.anim;
			return !anim.empty();
		}
	}
	return false;
}

} // namespace

Workbench::Workbench(Settings& settings, AssetIndex& assets, Renderer& renderer,
                     std::vector<std::filesystem::path> roots)
    : settings_(settings), assets_(assets), renderer_(renderer), roots_(std::move(roots)) {
	camera_.SetYawPitch(settings_.camera.yaw, settings_.camera.pitch);
	camera_.SetDistance(settings_.camera.distance);
	camera_.SetFov(settings_.camera.fov);
	RefreshAttachmentList();
	RefreshRootDirWatch();
}

Workbench::~Workbench() = default;

void Workbench::SetRoots(std::vector<std::filesystem::path> roots) {
	roots_ = std::move(roots);
	RefreshAttachmentList();
	RefreshRootDirWatch();
}

void Workbench::ClearWeapon() {
	ClearAttachments();
	ClearEditableDocument();
	attachmentCatalogValid_ = false;
	attachmentCatalog_.clear();
	hasWeapon_ = false;
	viewmodel_ = scene::ViewmodelAsset{};
	gpuGun_.reset();
	gpuHands_.reset();
	weaponError_.clear();
	gunSurfaceVisible_.clear();
	handsSurfaceVisible_.clear();
	animStates_.clear();
	animOverrides_.clear();
}

void Workbench::ClearModel() {
	hasModel_ = false;
	model_ = assets::ModelAsset{};
	gpuModel_.reset();
	modelError_.clear();
	modelSurfaceVisible_.clear();
	if (!documentIsWeapon_) {
		ClearEditableDocument();
	}
}

void Workbench::ClearAttachments() {
	attachments_.clear();
	hiddenTags_.clear();
}

void Workbench::ClearCharacter() {
	hasCharacter_ = false;
	characterFaction_ = assets::CharacterFaction{};
	characterBody_ = assets::ModelAsset{};
	characterHead_ = assets::ModelAsset{};
	characterWeaponModel_ = assets::ModelAsset{};
	characterHasHead_ = false;
	characterHeadBone_.clear();
	gpuCharacterBody_.reset();
	gpuCharacterHead_.reset();
	gpuCharacterWeapon_.reset();
	characterBodyPalette_.clear();
	characterHeadPalette_.clear();
	DirectX::XMStoreFloat4x4(&characterHeadWorld_, DirectX::XMMatrixIdentity());
	DirectX::XMStoreFloat4x4(&characterWeaponWorld_, DirectX::XMMatrixIdentity());
	characterWeaponName_.clear();
	characterWeaponTag_.clear();
	characterWeaponFound_ = false;
	characterMissing_.clear();
	characterWarnings_.clear();
}

bool Workbench::LoadCharacterInternal(const assets::CharacterFaction& faction, const std::string& weapon,
                                      std::string& error) {
	ClearWeapon();
	ClearModel();
	ClearCharacter();

	assets::ModelAsset body;
	assets::ModelAsset head;
	bool hasHead = false;
	std::vector<std::string> missing;
	std::vector<std::string> warnings;
	if (!assets::LoadCharacter(faction, roots_, body, head, hasHead, missing, warnings, error)) {
		characterMissing_ = missing;
		return false;
	}

	std::string uploadError;
	std::unique_ptr<GpuModel> gpuBody = renderer_.UploadModel(body, uploadError);
	if (!gpuBody) {
		error = "character body upload: " + uploadError;
		return false;
	}
	std::unique_ptr<GpuModel> gpuHead;
	if (hasHead) {
		gpuHead = renderer_.UploadModel(head, uploadError);
		if (!gpuHead) {
			warnings.push_back("head upload failed: " + uploadError);
			hasHead = false;
		}
	}

	characterFaction_ = faction;
	characterBody_ = std::move(body);
	characterHead_ = std::move(head);
	characterHasHead_ = hasHead;
	gpuCharacterBody_ = std::move(gpuBody);
	gpuCharacterHead_ = std::move(gpuHead);
	characterMissing_ = std::move(missing);
	characterWarnings_ = std::move(warnings);
	hasCharacter_ = true;

	primaryAnim_.reset();
	idleAnim_.reset();
	adsAnim_.reset();
	animName_.clear();
	time_ = 0.0;
	playing_ = false;
	additive_ = false;
	ads_ = false;
	viewMode_ = ViewMode::Orbit;
	showGrid_ = true;
	FrameCamera();
	UpdateViewCamera();
	BuildPose();

	if (!weapon.empty()) {
		std::string weaponError;
		if (!SetCharacterWeapon(weapon, weaponError)) {
			characterWarnings_.push_back("weapon " + weapon + ": " + weaponError);
		}
	}
	RefreshWatchedFiles();
	return true;
}

bool Workbench::LoadCharacter(const std::string& factionName, const std::string& weapon, std::string& error) {
	const assets::CharacterFaction* faction = assets::FindCharacterFaction(factionName);
	if (faction == nullptr) {
		error = "unknown character faction: " + factionName;
		return false;
	}
	return LoadCharacterInternal(*faction, weapon, error);
}

bool Workbench::LoadCharacterParts(const std::string& body, const std::string& head, const std::string& weapon,
                                   std::string& error) {
	if (body.empty()) {
		error = "character body model is required";
		return false;
	}
	assets::CharacterFaction faction;
	faction.id = "custom";
	faction.display = "custom (" + body + ")";
	faction.body = body;
	faction.head = head;
	return LoadCharacterInternal(faction, weapon, error);
}

bool Workbench::SetCharacterWeapon(const std::string& weapon, std::string& error) {
	if (!hasCharacter_) {
		error = "no character loaded";
		return false;
	}
	if (weapon.empty() || ToLower(weapon) == "none") {
		characterWeaponModel_ = assets::ModelAsset{};
		gpuCharacterWeapon_.reset();
		characterWeaponName_.clear();
		characterWeaponTag_.clear();
		characterWeaponFound_ = false;
		BuildPose();
		return true;
	}
	const std::optional<std::filesystem::path> weaponPath = assets::FindWeaponFile(roots_, weapon);
	if (!weaponPath.has_value()) {
		error = "weapon json not found: weapons/" + weapon + ".json";
		return false;
	}
	assets::WeaponAsset weaponAsset;
	try {
		weaponAsset = assets::LoadWeapon(*weaponPath);
	} catch (const std::exception& e) {
		error = e.what();
		return false;
	}
	const std::string modelName =
	    weaponAsset.worldModel.empty() ? weaponAsset.gunModel : weaponAsset.worldModel;
	assets::ModelAsset model;
	if (!assets::LoadModelAsset(modelName, roots_, model, error, true)) {
		error = "character weapon " + modelName + ": " + error;
		return false;
	}
	std::string uploadError;
	std::unique_ptr<GpuModel> gpu = renderer_.UploadModel(model, uploadError);
	if (!gpu) {
		error = "character weapon upload: " + uploadError;
		return false;
	}
	characterWeaponModel_ = std::move(model);
	gpuCharacterWeapon_ = std::move(gpu);
	characterWeaponName_ = weaponAsset.name.empty() ? weapon : weaponAsset.name;
	BuildPose();
	return true;
}

const std::vector<std::string>& Workbench::CamoNames() {
	if (camoNames_.empty()) {
		camoNames_ = assets::ListCamos(roots_);
	}
	return camoNames_;
}

bool Workbench::SetCamo(const std::string& name, std::string& error) {
	std::string trimmed = name;
	while (!trimmed.empty() && (trimmed.back() == ' ' || trimmed.back() == '\t')) {
		trimmed.pop_back();
	}
	while (!trimmed.empty() && (trimmed.front() == ' ' || trimmed.front() == '\t')) {
		trimmed.erase(trimmed.begin());
	}
	if (trimmed.empty() || ToLower(trimmed) == "none") {
		camoName_.clear();
		camoTextureName_.clear();
		camoWarnings_.clear();
		camoTextures_ = assets::CamoTextures{};
		camoImageValid_ = false;
		ApplyCamo();
		return true;
	}
	assets::CamoTextures textures;
	std::vector<std::string> warnings;
	if (!assets::LoadCamoTextures(roots_, trimmed, textures, warnings, error)) {
		return false;
	}
	camoName_ = trimmed;
	camoTextureName_ = textures.colorTextureName;
	if (camoTextureName_.empty()) {
		camoTextureName_ = textures.color.name;
	}
	camoWarnings_ = std::move(warnings);
	camoTextures_ = std::move(textures);
	camoImageValid_ = true;
	ApplyCamo();
	return true;
}

void Workbench::ApplyCamo() {
	const auto apply = [this](GpuModel* gpu) {
		if (gpu == nullptr) {
			return;
		}
		if (!camoImageValid_) {
			renderer_.ClearModelCamo(*gpu);
			return;
		}
		std::string error;
		if (camoTextures_.native) {
			std::string materialStem;
			std::string baseStem;
			if (!assets::SplitNativeCamoName(camoName_, materialStem, baseStem)) {
				renderer_.ClearModelCamo(*gpu);
				return;
			}
			// Not every loaded model has this material (e.g. the crossbow); no match is fine.
			(void)renderer_.SetModelNativeCamo(*gpu, materialStem, camoTextures_, camoStrength_,
			                                   camoScale_, error);
		} else if (!renderer_.SetModelCamo(*gpu, camoTextures_, camoStrength_, camoScale_, error)) {
			renderer_.ClearModelCamo(*gpu);
		}
	};
	apply(gpuGun_.get());
	apply(gpuModel_.get());
	for (AttachmentMount& mount : attachments_) {
		apply(mount.gpu.get());
	}
}

void Workbench::SetCamoStrength(float strength) {
	camoStrength_ = std::clamp(strength, 0.0f, 1.0f);
	const auto set = [this](GpuModel* gpu) {
		if (gpu == nullptr) {
			return;
		}
		gpu->camoStrength = camoStrength_;
		for (GpuMaterial& material : gpu->materials) {
			if (material.nativeStrength >= 0.0f) {
				material.nativeStrength = camoStrength_;
			}
		}
	};
	set(gpuGun_.get());
	set(gpuModel_.get());
	for (AttachmentMount& mount : attachments_) {
		set(mount.gpu.get());
	}
}

void Workbench::SetCamoScale(float scale) {
	camoScale_ = std::clamp(scale, 0.01f, 16.0f);
	const auto set = [this](GpuModel* gpu) {
		if (gpu != nullptr) {
			gpu->camoScale = camoScale_;
		}
	};
	set(gpuGun_.get());
	set(gpuModel_.get());
	for (AttachmentMount& mount : attachments_) {
		set(mount.gpu.get());
	}
}

void Workbench::RefreshRootDirWatch() {
	rootDirs_.clear();
	static const char* kDirs[] = {"materials", "materials/m", "images", "streamed_images",
	                              "weapons", "xmodel", "xsurface", "xanim"};
	const auto addDir = [this](const std::filesystem::path& path) {
		std::error_code ec;
		if (!std::filesystem::is_directory(path, ec) || ec) {
			return;
		}
		const auto time = std::filesystem::last_write_time(path, ec);
		if (ec) {
			return;
		}
		rootDirs_.push_back({path, time});
	};
	for (const auto& root : roots_) {
		// The root itself catches newly created asset subfolders.
		addDir(root);
		for (const char* dir : kDirs) {
			addDir(root / dir);
		}
	}
}

void Workbench::RefreshWatchedFiles() {
	watchedFiles_.clear();
	const auto add = [this](const std::filesystem::path& path) {
		if (path.empty()) {
			return;
		}
		std::error_code ec;
		const auto time = std::filesystem::last_write_time(path, ec);
		if (ec) {
			return;
		}
		for (const WatchedFile& existing : watchedFiles_) {
			if (existing.path == path) {
				return;
			}
		}
		watchedFiles_.push_back({path, time});
	};
	const auto addImage = [this, &add](const std::string& imageName) {
		if (imageName.empty()) {
			return;
		}
		const auto imagePath = assets::FindImageFile(roots_, imageName);
		if (!imagePath.has_value()) {
			return;
		}
		add(*imagePath);
		const std::string stem = imagePath->stem().string();
		for (int stream = 0; stream < 4; ++stream) {
			const std::string pixels = stem + "_stream" + std::to_string(stream) + ".pixels";
			add(imagePath->parent_path() / pixels);
			add(imagePath->parent_path().parent_path() / "streamed_images" / pixels);
		}
	};
	const auto addModel = [this, &add, &addImage](const assets::ModelAsset& model) {
		if (const auto path = assets::FindModelFile(roots_, model.model.name); path.has_value()) {
			add(*path);
		}
		if (!model.model.lodSurfaces.empty()) {
			if (const auto path = assets::FindXSurfaceFile(roots_, model.model.lodSurfaces.front());
			    path.has_value()) {
				add(*path);
			}
		}
		// Material jsons and every image/pixel stream the material references. A change to
		// any of these reloads the asset and re-uploads the textures.
		for (const assets::MaterialSlot& slot : model.materials) {
			if (!slot.material.path.empty()) {
				add(slot.material.path);
			}
			for (const assets::MaterialTextureInfo& texture : slot.textures) {
				if (texture.fileFound) {
					addImage(texture.imageName);
				}
			}
		}
	};

	if (hasWeapon_) {
		if (const auto path = assets::FindWeaponFile(roots_, weaponName_); path.has_value()) {
			add(*path);
		}
		addModel(viewmodel_.gun);
		addModel(viewmodel_.hands);
		for (const AttachmentMount& mount : attachments_) {
			addModel(mount.model);
		}
	}
	if (hasModel_) {
		addModel(model_);
	}
	if (hasCharacter_) {
		addModel(characterBody_);
		addModel(characterHead_);
		addModel(characterWeaponModel_);
	}
	if (!animName_.empty()) {
		if (const auto path = assets::FindAnimFile(roots_, animName_); path.has_value()) {
			add(*path);
		}
	}
	if (camoImageValid_ && !camoTextures_.materialName.empty()) {
		if (const auto path = assets::FindMaterialFile(roots_, camoTextures_.materialName);
		    path.has_value()) {
			add(*path);
		}
		for (const assets::ImageData* image : {&camoTextures_.color, &camoTextures_.normal,
		                                       &camoTextures_.specular, &camoTextures_.environment,
		                                       &camoTextures_.emissive}) {
			if (!image->name.empty()) {
				addImage(image->name);
			}
		}
	}
}

std::vector<std::string> Workbench::WatchedFiles() const {
	std::vector<std::string> paths;
	paths.reserve(watchedFiles_.size());
	for (const WatchedFile& file : watchedFiles_) {
		paths.push_back(file.path.string());
	}
	return paths;
}

void Workbench::PollHotReload() {
	if (!hotReloadEnabled_) {
		return;
	}
	if (watchedFiles_.empty()) {
		RefreshWatchedFiles();
	}
	const auto now = std::chrono::steady_clock::now();
	if (now >= nextRootScan_) {
		nextRootScan_ = now + std::chrono::seconds(1);
		bool rootChanged = false;
		for (RootDirWatch& dir : rootDirs_) {
			std::error_code ec;
			const auto time = std::filesystem::last_write_time(dir.path, ec);
			if (ec || time == dir.time) {
				continue;
			}
			dir.time = time;
			rootChanged = true;
		}
		if (rootChanged) {
			// New files can add a material/image/model. The index is rescanned after the
			// debounce, together with the loaded asset reload.
			rootChangePending_ = true;
			hotReloadPending_ = true;
			hotReloadDetected_ = now;
			hotReloadStatus_ = "asset roots changed";
			return;
		}
	}
	std::string changed;
	for (WatchedFile& file : watchedFiles_) {
		std::error_code ec;
		const auto time = std::filesystem::last_write_time(file.path, ec);
		if (ec || time == file.time) {
			continue;
		}
		file.time = time;
		changed = file.path.filename().string();
	}
	if (!changed.empty()) {
		hotReloadPending_ = true;
		hotReloadDetected_ = std::chrono::steady_clock::now();
		hotReloadStatus_ = "changed: " + changed;
		return;
	}
	if (hotReloadPending_ &&
	    std::chrono::steady_clock::now() - hotReloadDetected_ > std::chrono::milliseconds(400)) {
		hotReloadPending_ = false;
		if (rootChangePending_) {
			rootChangePending_ = false;
			assets_.Scan(roots_);
			RefreshAttachmentList();
			RefreshRootDirWatch();
		}
		ReloadWatchedAsset();
	}
}

bool Workbench::LoadRenameMap(const std::filesystem::path& path, std::string& error) {
	std::map<std::string, std::string> table;
	if (!LoadRenameTable(path, table, error)) {
		return false;
	}
	renameMap_ = std::move(table);
	BuildPose();
	return true;
}

void Workbench::ReloadWatchedAsset() {
	const std::string previousAnim = animName_;	const double previousTime = time_;
	const bool previousPlaying = playing_;
	const bool previousAdditive = additive_;
	const Camera& camera = camera_;
	const float yaw = camera.Yaw();
	const float pitch = camera.Pitch();
	const float distance = camera.Distance();
	const DirectX::XMFLOAT3 target = camera.Target();
	const bool firstPerson = viewMode_ == ViewMode::FirstPerson;

	std::string error;
	const std::string name = hasCharacter_ ? characterFaction_.id : (hasWeapon_ ? weaponName_ : modelName_);
	bool ok = false;
	if (hasCharacter_) {
		ok = LoadCharacter(characterFaction_.id, characterWeaponName_, error);
	} else if (hasWeapon_) {
		ok = ReloadWeapon(error);
	} else if (hasModel_) {
		ok = ReloadModel(error);
	}
	if (!ok) {
		hotReloadStatus_ = "reload failed: " + error;
		return;
	}
	if (firstPerson && hasWeapon_) {
		SetViewMode(ViewMode::FirstPerson);
	} else {
		camera_.SetTarget(target);
		SetOrbitView(yaw, pitch, distance > 0.0f ? distance : settings_.camera.distance, 0.0f);
	}
	if (!previousAnim.empty()) {
		std::string animError;
		SetAdditive(previousAdditive);
		if (PlayAnim(previousAnim, animError)) {
			SetAnimTime(previousTime);
			SetAnimPlaying(previousPlaying);
		}
	}
	++hotReloadCount_;
	char message[256];
	std::snprintf(message, sizeof(message), "reloaded %s (%d)", name.c_str(), hotReloadCount_);
	hotReloadStatus_ = message;
	RefreshWatchedFiles();
}

void Workbench::RefreshAttachmentList() {
	attachmentNames_ = assets::ListAttachments(roots_);
	attachmentCatalogValid_ = false;
	attachmentCatalog_.clear();
}

const std::vector<AttachmentFitEntry>& Workbench::AttachmentCatalog() {
	if (!attachmentCatalogValid_) {
		BuildAttachmentCatalog();
	}
	return attachmentCatalog_;
}

void Workbench::BuildAttachmentCatalog() {
	attachmentCatalog_.clear();
	attachmentCatalogValid_ = true;
	std::vector<std::string> tags;
	const auto addTags = [&tags](const assets::XModelData& model) {
		for (const assets::BoneData& bone : model.bones) {
			if (bone.name.rfind("tag_", 0) == 0 &&
			    std::find(tags.begin(), tags.end(), bone.name) == tags.end()) {
				tags.push_back(bone.name);
			}
		}
	};
	if (hasWeapon_) {
		addTags(viewmodel_.gun.model);
		addTags(viewmodel_.hands.model);
	}
	for (const std::string& name : attachmentNames_) {
		AttachmentFitEntry entry;
		entry.name = name;
		const std::optional<std::filesystem::path> path = assets::FindAttachmentFile(roots_, name);
		if (!path.has_value()) {
			entry.note = "attachment json not found";
			attachmentCatalog_.push_back(std::move(entry));
			continue;
		}
		try {
			const assets::AttachmentAsset attachment = assets::LoadAttachment(*path);
			if (attachment.viewModels.empty()) {
				entry.note = "no view model";
				attachmentCatalog_.push_back(std::move(entry));
				continue;
			}
			entry.viewModel = attachment.viewModels.front();
			const std::optional<std::filesystem::path> modelPath =
			    assets::FindModelFile(roots_, entry.viewModel);
			if (!modelPath.has_value()) {
				entry.note = "view model not found";
				attachmentCatalog_.push_back(std::move(entry));
				continue;
			}
			const assets::XModelData model = assets::LoadXModel(*modelPath);
			if (model.bones.empty()) {
				entry.note = "view model has no bones";
				attachmentCatalog_.push_back(std::move(entry));
				continue;
			}
			entry.requiredTag = model.bones.front().name;
			entry.found = std::find(tags.begin(), tags.end(), entry.requiredTag) != tags.end();
			if (!entry.found) {
				entry.nearest = assets::NearestTags(entry.requiredTag, tags, 3);
			}
		} catch (const std::exception& e) {
			entry.note = e.what();
		}
		attachmentCatalog_.push_back(std::move(entry));
	}
}

void Workbench::RebuildHiddenTags() {
	std::vector<std::string> hidden;
	const auto add = [&hidden](const std::string& tag) {
		if (tag.empty()) {
			return;
		}
		if (std::find(hidden.begin(), hidden.end(), tag) == hidden.end()) {
			hidden.push_back(tag);
		}
	};
	const auto remove = [&hidden](const std::string& tag) {
		hidden.erase(std::remove(hidden.begin(), hidden.end(), tag), hidden.end());
	};
	for (const std::string& tag : viewmodel_.weapon.hideTags) {
		add(tag);
	}
	for (const AttachmentMount& mount : attachments_) {
		for (const std::string& tag : mount.attachment.hideTags) {
			add(tag);
		}
	}
	for (const AttachmentMount& mount : attachments_) {
		for (const std::string& tag : mount.attachment.showTags) {
			remove(tag);
		}
	}
	hiddenTags_ = std::move(hidden);
}

bool Workbench::IsAttached(const std::string& attachment) const {
	const std::string lower = ToLower(attachment);
	for (const AttachmentMount& mount : attachments_) {
		if (ToLower(mount.name) == lower || ToLower(mount.internalName) == lower ||
		    ToLower(mount.viewModel) == lower) {
			return true;
		}
	}
	return false;
}

bool Workbench::Attach(const std::string& attachment, std::string& error) {
	if (!hasWeapon_) {
		error = "no weapon loaded";
		return false;
	}
	if (IsAttached(attachment)) {
		return true;
	}
	const std::optional<std::filesystem::path> path = FindAttachmentByName(roots_, attachment);
	if (!path.has_value()) {
		error = "attachment not found: attachments/" + attachment + ".json";
		return false;
	}

	AttachmentMount mount;
	try {
		mount.attachment = assets::LoadAttachment(*path);
	} catch (const std::exception& e) {
		error = e.what();
		return false;
	}
	mount.name = mount.attachment.name;
	mount.internalName = mount.attachment.internalName;
	const std::vector<std::string>& modelNames =
	    worldModel_ && !mount.attachment.worldModels.empty() ? mount.attachment.worldModels
	                                                         : mount.attachment.viewModels;
	if (modelNames.empty()) {
		error = "attachment " + mount.name + " has no " + (worldModel_ ? "world model" : "view model");
		return false;
	}
	mount.viewModel = modelNames.front();

	std::string loadError;
	if (!assets::LoadModelAsset(mount.viewModel, roots_, mount.model, loadError, true)) {
		error = "attachment " + mount.name + (worldModel_ ? " world model " : " view model ") +
		        mount.viewModel + ": " + loadError;
		return false;
	}
	std::string uploadError;
	mount.gpu = renderer_.UploadModel(mount.model, uploadError);
	if (!mount.gpu) {
		error = "attachment " + mount.name + " upload: " + uploadError;
		return false;
	}
	if (!mount.model.model.bones.empty()) {
		mount.requiredTag = mount.model.model.bones.front().name;
		mount.tagFound = ModelHasBone(viewmodel_.gun.model, mount.requiredTag) ||
		                 (viewmodel_.hasHands && ModelHasBone(viewmodel_.hands.model, mount.requiredTag));
	} else {
		mount.error = "attachment view model has no bones";
	}

	for (const std::string& reticleName : mount.attachment.reticleViewModels) {
		if (worldModel_) {
			break; // reticle view models are a first-person overlay
		}
		assets::ModelAsset reticle;
		std::string reticleError;
		if (!assets::LoadModelAsset(reticleName, roots_, reticle, reticleError, true)) {
			mount.error = "reticle " + reticleName + ": " + reticleError;
			continue;
		}
		std::unique_ptr<GpuModel> gpu = renderer_.UploadModel(reticle, uploadError);
		if (!gpu) {
			mount.error = "reticle " + reticleName + " upload: " + uploadError;
			continue;
		}
		mount.reticleWorlds.push_back(DirectX::XMFLOAT4X4{});
		mount.reticleGpus.push_back(std::move(gpu));
		mount.reticles.push_back(std::move(reticle));
	}

	attachments_.push_back(std::move(mount));
	RebuildHiddenTags();
	if (ads_) {
		LoadAdsAnim();
	}
	ApplyCamo();
	BuildPose();
	return true;
}

bool Workbench::Detach(const std::string& attachment) {
	const std::string lower = ToLower(attachment);
	const auto it = std::find_if(attachments_.begin(), attachments_.end(), [&lower](const AttachmentMount& mount) {
		return ToLower(mount.name) == lower || ToLower(mount.internalName) == lower ||
		       ToLower(mount.viewModel) == lower;
	});
	if (it == attachments_.end()) {
		return false;
	}
	attachments_.erase(it);
	RebuildHiddenTags();
	if (ads_) {
		LoadAdsAnim();
	}
	BuildPose();
	return true;
}

void Workbench::DetachAll() {
	if (attachments_.empty()) {
		return;
	}
	attachments_.clear();
	RebuildHiddenTags();
	if (ads_) {
		LoadAdsAnim();
	}
	BuildPose();
}

void Workbench::ClearEditableDocument() {
	document_.reset();
	documentIsWeapon_ = false;
}

void Workbench::LoadEditableDocumentForWeapon(const std::string& modelName) {
	ClearEditableDocument();
	const std::optional<std::filesystem::path> path = assets::FindModelFile(roots_, modelName);
	if (!path.has_value()) {
		return;
	}
	auto document = std::make_unique<assets::XModelDocument>();
	std::string error;
	if (assets::LoadXModelDocument(*path, *document, error)) {
		document_ = std::move(document);
		documentIsWeapon_ = true;
	}
}

void Workbench::LoadEditableDocumentForModel(const std::string& modelName) {
	ClearEditableDocument();
	const std::optional<std::filesystem::path> path = assets::FindModelFile(roots_, modelName);
	if (!path.has_value()) {
		return;
	}
	auto document = std::make_unique<assets::XModelDocument>();
	std::string error;
	if (assets::LoadXModelDocument(*path, *document, error)) {
		document_ = std::move(document);
		documentIsWeapon_ = false;
	}
}

std::string Workbench::EditableModelName() const {
	return document_ ? document_->model.name : std::string();
}

void Workbench::SyncDocumentToScene() {
	if (!document_) {
		return;
	}
	if (documentIsWeapon_ && hasWeapon_) {
		viewmodel_.gun.model = document_->model;
		attachmentCatalogValid_ = false;
	} else if (!documentIsWeapon_ && hasModel_) {
		model_.model = document_->model;
	}
	BuildPose();
}

std::vector<TagEditInfo> Workbench::EditableTags() const {
	std::vector<TagEditInfo> tags;
	if (!document_) {
		return tags;
	}
	for (std::size_t i = 0; i < document_->model.bones.size(); ++i) {
		const assets::BoneData& bone = document_->model.bones[i];
		if (!bone.IsTag()) {
			continue;
		}
		TagEditInfo info;
		info.name = bone.name;
		info.parent = bone.parent >= 0
		                  ? document_->model.bones[static_cast<std::size_t>(bone.parent)].name
		                  : std::string();
		info.localPos = bone.localPos;
		info.localQuat = bone.localQuat;
		info.worldPos = bone.worldPos;
		info.added = i < document_->addedBones.size() && document_->addedBones[i];
		tags.push_back(info);
	}
	return tags;
}

bool Workbench::HasTagChanges() const {
	return document_ != nullptr && assets::HasTagChanges(*document_);
}

bool Workbench::MoveTagLocal(const std::string& tag, const DirectX::XMFLOAT3& localPos,
                             const DirectX::XMFLOAT4& localQuat, std::string& error) {
	if (!document_) {
		error = "no editable model loaded";
		return false;
	}
	if (!assets::MoveTagLocal(*document_, tag, localPos, localQuat, error)) {
		return false;
	}
	SyncDocumentToScene();
	return true;
}

bool Workbench::TagPoseWorld(const std::string& tag, DirectX::XMFLOAT4X4& out) const {
	const int index = FindPoseBone(pose_, tag);
	if (index < 0) {
		return false;
	}
	DirectX::XMStoreFloat4x4(&out, PoseBoneMatrix(pose_.bones[static_cast<std::size_t>(index)]));
	return true;
}

bool Workbench::TagGhostWorld(const std::string& tag, DirectX::XMFLOAT3& out) const {
	if (!document_) {
		return false;
	}
	for (std::size_t i = 0; i < document_->originalModel.bones.size(); ++i) {
		const assets::BoneData& original = document_->originalModel.bones[i];
		if (original.name != tag) {
			continue;
		}
		DirectX::XMFLOAT4 parentQuat{0.0f, 0.0f, 0.0f, 1.0f};
		DirectX::XMFLOAT3 parentPos{0.0f, 0.0f, 0.0f};
		if (original.parent >= 0 &&
		    static_cast<std::size_t>(original.parent) < document_->originalModel.bones.size()) {
			const assets::BoneData& parent =
			    document_->originalModel.bones[static_cast<std::size_t>(original.parent)];
			// Follow the animated parent in the current pose when possible.
			const int poseIndex = FindPoseBone(pose_, parent.name);
			if (poseIndex >= 0) {
				parentQuat = pose_.bones[static_cast<std::size_t>(poseIndex)].worldQuat;
				parentPos = pose_.bones[static_cast<std::size_t>(poseIndex)].worldPos;
			} else {
				parentQuat = parent.worldQuat;
				parentPos = parent.worldPos;
			}
		}
		const DirectX::XMFLOAT3 rotated = Rotate(original.localPos, parentQuat);
		out = {parentPos.x + rotated.x, parentPos.y + rotated.y, parentPos.z + rotated.z};
		return true;
	}
	return false;
}

bool Workbench::MoveTagWorld(const std::string& tag, const DirectX::XMFLOAT3& worldPos,
                             const DirectX::XMFLOAT4& worldQuat, std::string& error) {
	const int index = FindPoseBone(pose_, tag);
	if (index < 0) {
		error = "tag not in the pose: " + tag;
		return false;
	}
	const scene::PoseBone& bone = pose_.bones[static_cast<std::size_t>(index)];
	if (bone.parent < 0) {
		error = "tag has no parent";
		return false;
	}
	const scene::PoseBone& parent = pose_.bones[static_cast<std::size_t>(bone.parent)];
	const DirectX::XMFLOAT4 inverseParent =
	    InverseQuat(parent.worldQuat);
	const DirectX::XMFLOAT3 offset = {worldPos.x - parent.worldPos.x, worldPos.y - parent.worldPos.y,
	                                 worldPos.z - parent.worldPos.z};
	const DirectX::XMFLOAT3 localPos = Rotate(offset, inverseParent);
	const DirectX::XMFLOAT4 localQuat = NormalizeQuat(DirectX::XMFLOAT4{
	    worldQuat.x, worldQuat.y, worldQuat.z, worldQuat.w});
	const DirectX::XMFLOAT4 local = NormalizeQuat(MultiplyQuat(localQuat, inverseParent));
	return MoveTagLocal(tag, localPos, local, error);
}

bool Workbench::AddTag(const std::string& tag, const std::string& parent, const std::string& copyFrom,
                       const DirectX::XMFLOAT3& localPos, const DirectX::XMFLOAT4& localQuat,
                       std::string& error) {
	if (!document_) {
		error = "no editable model loaded";
		return false;
	}
	if (!assets::AddTag(*document_, tag, parent, copyFrom, localPos, localQuat, error)) {
		return false;
	}
	SyncDocumentToScene();
	return true;
}

bool Workbench::RenameTag(const std::string& oldName, const std::string& newName, std::string& error) {
	if (!document_) {
		error = "no editable model loaded";
		return false;
	}
	if (!assets::RenameTag(*document_, oldName, newName, error)) {
		return false;
	}
	SyncDocumentToScene();
	RebuildHiddenTags();
	return true;
}

bool Workbench::RevertTag(const std::string& tag, std::string& error) {
	if (!document_) {
		error = "no editable model loaded";
		return false;
	}
	if (!assets::RevertTag(*document_, tag, error)) {
		return false;
	}
	SyncDocumentToScene();
	return true;
}

void Workbench::RevertAllTags() {
	if (!document_) {
		return;
	}
	auto document = std::make_unique<assets::XModelDocument>();
	std::string error;
	const bool isWeapon = documentIsWeapon_;
	if (assets::LoadXModelDocument(document_->path, *document, error)) {
		document_ = std::move(document);
		documentIsWeapon_ = isWeapon;
		SyncDocumentToScene();
	}
}

bool Workbench::SaveEditedModel(ModelSaveResult& result, std::string& error) {
	if (!document_) {
		error = "no editable model loaded";
		return false;
	}
	const std::filesystem::path path = document_->path;
	const std::filesystem::path backup = std::filesystem::path(path.string() + ".bak");
	std::error_code ec;
	if (!std::filesystem::exists(backup, ec)) {
		std::filesystem::copy_file(path, backup, std::filesystem::copy_options::none, ec);
		if (ec) {
			error = "cannot create " + backup.string() + ": " + ec.message();
			return false;
		}
	}
	if (!assets::SaveXModelDocument(*document_, path, error)) {
		return false;
	}
	const std::filesystem::path tagsPath =
	    path.parent_path() / (path.stem().string() + ".tags.json");
	if (!assets::WriteTagsJson(*document_, path, tagsPath, error)) {
		return false;
	}

	// Re-read the saved model and run the same checks as -validate on it.
	auto reloaded = std::make_unique<assets::XModelDocument>();
	std::string reloadError;
	if (!assets::LoadXModelDocument(path, *reloaded, reloadError)) {
		error = "saved file does not reload: " + reloadError;
		return false;
	}
	const bool isWeapon = documentIsWeapon_;
	document_ = std::move(reloaded);
	documentIsWeapon_ = isWeapon;
	SyncDocumentToScene();

	assets::ModelAsset asset;
	std::string assetError;
	if (!assets::LoadModelAssetFromFile(path, roots_, asset, assetError, false)) {
		result.validation = "FAILED: " + assetError;
	} else {
		int missingMaterials = 0;
		for (const assets::MaterialSlot& slot : asset.materials) {
			if (!slot.jsonFound) {
				++missingMaterials;
			}
		}
		char buffer[256];
		std::snprintf(buffer, sizeof(buffer), "ok: %zu surfaces, %zu materials, %d material json missing",
		              asset.surfaceSet.surfaces.size(), asset.materials.size(), missingMaterials);
		result.validation = buffer;
	}
	result.path = path.string();
	result.backup = backup.string();
	result.tagsJson = tagsPath.string();
	return true;
}

bool Workbench::FitTagToReticle(const std::string& tag, const std::string& attachment,
                                TagFitResult& result, std::string& error) {
	if (!document_) {
		error = "no editable model loaded";
		return false;
	}
	if (!camera_.IsFirstPerson()) {
		error = "fit needs the first-person view";
		return false;
	}
	if (!Attach(attachment, error)) {
		return false;
	}
	const AttachmentMount* mount = nullptr;
	for (const AttachmentMount& candidate : attachments_) {
		if (candidate.requiredTag == tag || candidate.name == attachment) {
			mount = &candidate;
			break;
		}
	}
	if (mount == nullptr || mount->reticleWorlds.empty() || mount->reticles.empty()) {
		error = "attachment has no mounted reticle for " + tag;
		return false;
	}

	const int boneIndex = FindModelBone(document_->model, tag);
	if (boneIndex < 0) {
		error = "tag not found: " + tag;
		return false;
	}
	result = TagFitResult{};
	result.tag = tag;
	result.attachment = mount->name;
	result.sourceLocalPos = document_->model.bones[static_cast<std::size_t>(boneIndex)].localPos;
	result.localDelta = {0.0f, 0.0f, 0.0f};
	const DirectX::XMFLOAT3 startWorldPos =
	    document_->model.bones[static_cast<std::size_t>(boneIndex)].worldPos;

	const bool previousAutoAlign = adsAutoAlign_;
	adsAutoAlign_ = false;
	ads_ = true;
	LoadAdsAnim();
	UpdateViewCamera();
	BuildPose();

	const int iterations = 6;
	for (int iteration = 0; iteration < iterations; ++iteration) {
		const AttachmentMount* current = nullptr;
		for (const AttachmentMount& candidate : attachments_) {
			if (candidate.requiredTag == tag) {
				current = &candidate;
				break;
			}
		}
		if (current == nullptr || current->reticleWorlds.empty()) {
			error = "attachment did not mount on " + tag;
			break;
		}
		const DirectX::XMMATRIX reticleWorld =
		    DirectX::XMLoadFloat4x4(&current->reticleWorlds.front());
		const DirectX::XMVECTOR aim = DirectX::XMVector3Transform(
		    DirectX::XMLoadFloat3(&current->reticles.front().boundsCenter), reticleWorld);
		const DirectX::XMFLOAT3 eye = camera_.Eye();
		const DirectX::XMFLOAT3 look = camera_.LookTarget();
		DirectX::XMFLOAT3 forward = {look.x - eye.x, look.y - eye.y, look.z - eye.z};
		const float length =
		    std::sqrt(forward.x * forward.x + forward.y * forward.y + forward.z * forward.z);
		if (length < 1.0e-5f) {
			error = "no camera direction";
			break;
		}
		forward.x /= length;
		forward.y /= length;
		forward.z /= length;
		const DirectX::XMVECTOR forwardVector =
		    DirectX::XMVectorSet(forward.x, forward.y, forward.z, 0.0f);
		const DirectX::XMVECTOR eyeVector = DirectX::XMLoadFloat3(&eye);
		const float along = DirectX::XMVectorGetX(DirectX::XMVector3Dot(
		    DirectX::XMVectorSubtract(aim, eyeVector), forwardVector));
		const DirectX::XMVECTOR onAxis =
		    DirectX::XMVectorAdd(eyeVector, DirectX::XMVectorScale(forwardVector, along));
		DirectX::XMFLOAT3 delta;
		DirectX::XMStoreFloat3(&delta, DirectX::XMVectorSubtract(onAxis, aim));
		const float distance = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
		if (distance < 5.0e-4f) {
			result.iterations = iteration;
			break;
		}

		const int poseTag = FindPoseBone(pose_, tag);
		if (poseTag < 0 || pose_.bones[static_cast<std::size_t>(poseTag)].parent < 0) {
			error = "tag is not in the pose";
			break;
		}
		const scene::PoseBone& poseBone = pose_.bones[static_cast<std::size_t>(poseTag)];
		const scene::PoseBone& parent = pose_.bones[static_cast<std::size_t>(poseBone.parent)];
		const DirectX::XMFLOAT3 localDelta = Rotate(delta, InverseQuat(parent.worldQuat));
		const assets::BoneData& bone = document_->model.bones[static_cast<std::size_t>(boneIndex)];
		const DirectX::XMFLOAT3 newLocal = {bone.localPos.x + localDelta.x,
		                                    bone.localPos.y + localDelta.y,
		                                    bone.localPos.z + localDelta.z};
		if (!assets::MoveTagLocal(*document_, tag, newLocal, bone.localQuat, error)) {
			break;
		}
		SyncDocumentToScene();
		result.iterations = iteration + 1;
	}

	const assets::BoneData& finalBone = document_->model.bones[static_cast<std::size_t>(boneIndex)];
	result.finalLocalPos = finalBone.localPos;
	result.localDelta = {finalBone.localPos.x - result.sourceLocalPos.x,
	                     finalBone.localPos.y - result.sourceLocalPos.y,
	                     finalBone.localPos.z - result.sourceLocalPos.z};
	result.startWorldPos = startWorldPos;
	result.finalWorldPos = finalBone.worldPos;
	result.worldDelta = {finalBone.worldPos.x - result.startWorldPos.x,
	                     finalBone.worldPos.y - result.startWorldPos.y,
	                     finalBone.worldPos.z - result.startWorldPos.z};
	result.anim = EffectiveAnimForState("ADS_UP");
	result.screenX = 0.0f;
	result.screenY = 0.0f;
	if (mount != nullptr) {
		const DirectX::XMMATRIX viewProjection = camera_.ViewProjection(1920.0f / 1016.0f);
		const AttachmentMount* current = nullptr;
		for (const AttachmentMount& candidate : attachments_) {
			if (candidate.requiredTag == tag) {
				current = &candidate;
				break;
			}
		}
		if (current != nullptr && !current->reticleWorlds.empty()) {
			const DirectX::XMVECTOR aim = DirectX::XMVector3Transform(
			    DirectX::XMLoadFloat3(&current->reticles.front().boundsCenter),
			    DirectX::XMLoadFloat4x4(&current->reticleWorlds.front()));
			const DirectX::XMVECTOR clip = DirectX::XMVector3TransformCoord(aim, viewProjection);
			DirectX::XMFLOAT3 ndc;
			DirectX::XMStoreFloat3(&ndc, clip);
			result.screenX = (ndc.x * 0.5f + 0.5f) * 1920.0f;
			result.screenY = (1.0f - (ndc.y * 0.5f + 0.5f)) * 1016.0f;
		}
	}
	adsAutoAlign_ = previousAutoAlign;
	BuildPose();
	return error.empty();
}

bool Workbench::OverrideAppliesToAttachment(const assets::WeaponAnimOverride& entry) const {
	if (attachments_.empty()) {
		return false;
	}
	const std::vector<std::string>& listed = viewmodel_.weapon.attachments;
	if (!listed.empty()) {
		// The weapon json's attachments array is 1-based: attachment1 = 1 is listed[0],
		// 0 means "no attachment". Both attachment1 and attachment2 are checked.
		const auto matches = [this, &listed](int index) {
			if (index <= 0) {
				return false;
			}
			const std::size_t slot = static_cast<std::size_t>(index - 1);
			if (slot >= listed.size()) {
				return false;
			}
			const std::string lower = ToLower(listed[slot]);
			for (const AttachmentMount& mount : attachments_) {
				if (ToLower(mount.name) == lower || ToLower(mount.internalName) == lower) {
					return true;
				}
			}
			return false;
		};
		return matches(entry.attachment1) || matches(entry.attachment2);
	}
	// No attachment list (older ports): fall back to the AW optic index map.
	for (const AttachmentMount& mount : attachments_) {
		if (entry.attachment1 == assets::OpticAttachmentIndexForTag(mount.requiredTag)) {
			return true;
		}
	}
	return false;
}

std::string Workbench::EffectiveAnimForState(const std::string& state) const {
	std::string base;
	if (!FindAnimState(animStates_, state, base)) {
		return {};
	}
	for (const AttachmentMount& mount : attachments_) {
		for (const assets::AttachmentField& field : mount.attachment.fields) {
			if (field.name != "szXAnims" || field.anim != state) {
				continue;
			}
			const std::vector<std::string> tokens = SplitTokens(field.value);
			if (tokens.size() < 2) {
				continue;
			}
			const std::string candidate = ReplaceToken(base, tokens[0], tokens[1]);
			if (candidate != base && assets::FindAnimFile(roots_, candidate).has_value()) {
				return candidate;
			}
		}
	}
	for (const assets::WeaponAnimOverride& entry : animOverrides_) {
		if (StateForAnimTreeType(entry.animTreeType) != state) {
			continue;
		}
		if (!entry.anim.empty() && OverrideAppliesToAttachment(entry) &&
		    assets::FindAnimFile(roots_, entry.anim).has_value()) {
			return entry.anim;
		}
	}
	return base;
}

bool Workbench::LoadWeapon(const std::string& weapon, const std::string& hands, bool firstPerson,
                           std::string& error, bool world) {
	ClearWeapon();
	ClearCharacter();
	worldModel_ = world;
	primaryAnim_.reset();
	idleAnim_.reset();
	animName_.clear();
	time_ = 0.0;
	playing_ = false;
	additive_ = false;
	ads_ = false;
	adsAnim_.reset();
	adsTime_ = 0.0f;
	weaponName_ = weapon;
	handsOverride_ = hands;

	scene::ViewmodelAsset loaded;
	if (!scene::LoadViewmodel(weapon, hands, roots_, loaded, error, world)) {
		weaponError_ = error;
		return false;
	}

	std::string uploadError;
	std::unique_ptr<GpuModel> gun = renderer_.UploadModel(loaded.gun, uploadError);
	if (!gun) {
		weaponError_ = "gun upload failed: " + uploadError;
		return false;
	}
	std::unique_ptr<GpuModel> handsModel;
	if (loaded.hasHands) {
		handsModel = renderer_.UploadModel(loaded.hands, uploadError);
		if (!handsModel) {
			loaded.warnings.push_back("hands upload failed: " + uploadError);
		}
	}

	viewmodel_ = std::move(loaded);
	gpuGun_ = std::move(gun);
	gpuHands_ = std::move(handsModel);
	hasWeapon_ = true;
	gunSurfaceVisible_.assign(viewmodel_.gun.surfaceSet.surfaces.size(), true);
	handsSurfaceVisible_.assign(viewmodel_.hands.surfaceSet.surfaces.size(), true);
	LoadEditableDocumentForWeapon(viewmodel_.weapon.gunModel);

	RefreshAnimLists();

	viewMode_ = (firstPerson && !world) ? ViewMode::FirstPerson : ViewMode::Orbit;
	if (viewMode_ == ViewMode::FirstPerson) {
		showGrid_ = false;
	} else {
		FrameCamera();
	}
	UpdateViewCamera();
	AutoPlayIdle();
	ApplyCamo();
	RefreshWatchedFiles();
	BuildPose();
	return true;
}

bool Workbench::ReloadWeapon(std::string& error) {
	if (weaponName_.empty()) {
		error = "no weapon loaded";
		return false;
	}
	const std::string previousAnim = animName_;
	const double previousTime = time_;
	const bool previousAdditive = additive_;
	const bool firstPerson = viewMode_ == ViewMode::FirstPerson;
	const bool previousWorld = worldModel_;
	std::vector<std::string> attached;
	attached.reserve(attachments_.size());
	for (const AttachmentMount& mount : attachments_) {
		attached.push_back(mount.name);
	}
	if (!LoadWeapon(weaponName_, handsOverride_, firstPerson, error, previousWorld)) {
		return false;
	}
	for (const std::string& attachment : attached) {
		std::string attachError;
		(void)Attach(attachment, attachError);
	}
	if (!previousAnim.empty()) {
		std::string animError;
		SetAdditive(previousAdditive);
		if (PlayAnim(previousAnim, animError)) {
			SetAnimTime(previousTime);
		}
	}
	return true;
}

bool Workbench::LoadModel(const std::string& model, std::string& error) {
	const std::optional<std::filesystem::path> path = assets::FindModelFile(roots_, model);
	if (!path.has_value()) {
		modelError_ = "model not found: " + model;
		error = modelError_;
		return false;
	}
	ClearCharacter();
	ClearModel();
	modelName_ = model;
	assets::ModelAsset loaded;
	if (!assets::LoadModelAssetFromFile(*path, roots_, loaded, error, true)) {
		modelError_ = error;
		return false;
	}
	std::string uploadError;
	std::unique_ptr<GpuModel> gpu = renderer_.UploadModel(loaded, uploadError);
	if (!gpu) {
		modelError_ = "GPU upload failed: " + uploadError;
		error = modelError_;
		return false;
	}
	model_ = std::move(loaded);
	gpuModel_ = std::move(gpu);
	hasModel_ = true;
	LoadEditableDocumentForModel(model_.model.name);
	modelSurfaceVisible_.assign(model_.surfaceSet.surfaces.size(), true);
	if (!hasWeapon_) {
		FrameCamera();
	}
	ApplyCamo();
	RefreshWatchedFiles();
	BuildPose();
	return true;
}

bool Workbench::ReloadModel(std::string& error) {
	if (modelName_.empty()) {
		error = "no model loaded";
		return false;
	}
	return LoadModel(modelName_, error);
}

bool Workbench::SetHands(const std::string& hands, std::string& error) {
	if (worldModel_) {
		return true;
	}
	handsOverride_ = hands;
	return ReloadWeapon(error);
}

bool Workbench::SetWorldModel(bool world, std::string& error) {
	if (worldModel_ == world) {
		return true;
	}
	worldModel_ = world;
	return ReloadWeapon(error);
}

void Workbench::RefreshAnimLists() {
	animStates_.clear();
	animOverrides_.clear();
	if (!hasWeapon_) {
		return;
	}
	for (const assets::WeaponAnimState& state : viewmodel_.weapon.animStates) {
		animStates_.push_back({state.state, state.anim});
	}
	animOverrides_ = viewmodel_.weapon.animOverrides;
}

void Workbench::AutoPlayIdle() {
	const std::string idle = EffectiveAnimForState("IDLE");
	if (idle.empty()) {
		return;
	}
	std::string error;
	(void)PlayAnim(idle, error);
}

bool Workbench::LoadAnimAsset(const std::string& name, std::optional<assets::XAnimData>& out,
                              std::string& error) const {
	const std::optional<std::filesystem::path> path = assets::FindAnimFile(roots_, name);
	if (!path.has_value()) {
		error = "animation not found: " + name;
		return false;
	}
	try {
		out = assets::LoadXAnim(*path);
	} catch (const std::exception& e) {
		error = "animation " + name + ": " + e.what();
		return false;
	}
	return true;
}

bool Workbench::PlayAnim(const std::string& anim, std::string& error) {
	std::optional<assets::XAnimData> loaded;
	if (!LoadAnimAsset(anim, loaded, error)) {
		return false;
	}
	primaryAnim_ = std::move(loaded);
	animName_ = anim;
	time_ = 0.0;
	playing_ = true;
	looping_ = primaryAnim_->looping;
	speed_ = std::clamp(speed_, kMinSpeed, kMaxSpeed);

	if (additive_) {
		std::string idleName;
		if (FindAnimState(animStates_, "IDLE", idleName)) {
			std::optional<assets::XAnimData> idle;
			std::string idleError;
			if (LoadAnimAsset(idleName, idle, idleError)) {
				idleAnim_ = std::move(idle);
			}
		}
	} else {
		idleAnim_.reset();
	}
	RefreshWatchedFiles();
	BuildPose();
	return true;
}

void Workbench::StopAnim() {
	primaryAnim_.reset();
	idleAnim_.reset();
	animName_.clear();
	playing_ = false;
	time_ = 0.0;
	RefreshWatchedFiles();
	BuildPose();
}

void Workbench::SetAnimTime(double seconds) {
	const double duration = SelectedDuration();
	time_ = std::clamp(seconds, 0.0, std::max(0.0, duration));
	BuildPose();
}

void Workbench::SetAnimFraction(double fraction) {
	const double duration = SelectedDuration();
	time_ = std::clamp(fraction, 0.0, 1.0) * duration;
	BuildPose();
}

void Workbench::SetAnimFrame(int frame) {
	if (!primaryAnim_) {
		return;
	}
	const double duration = SelectedDuration();
	const double count = std::max(1, static_cast<int>(primaryAnim_->frameCount));
	time_ = std::clamp(static_cast<double>(frame) / count, 0.0, 1.0) * duration;
	BuildPose();
}

void Workbench::SetAnimPlaying(bool playing) { playing_ = playing; }
void Workbench::SetAnimLooping(bool looping) { looping_ = looping; }

void Workbench::SetAnimSpeed(float speed) { speed_ = std::clamp(speed, kMinSpeed, kMaxSpeed); }

void Workbench::SetAdditive(bool additive) {
	if (additive_ == additive) {
		return;
	}
	additive_ = additive;
	if (additive_) {
		std::string idleName;
		if (FindAnimState(animStates_, "IDLE", idleName)) {
			std::optional<assets::XAnimData> idle;
			std::string error;
			if (LoadAnimAsset(idleName, idle, error)) {
				idleAnim_ = std::move(idle);
			}
		}
	} else {
		idleAnim_.reset();
	}
	BuildPose();
}

void Workbench::Update(double deltaSeconds) {
	PollHotReload();
	if (!primaryAnim_ || !playing_) {
		return;
	}
	const double duration = SelectedDuration();
	if (duration <= 0.0) {
		time_ = 0.0;
		return;
	}
	time_ += deltaSeconds * static_cast<double>(speed_);
	if (time_ > duration) {
		if (looping_) {
			time_ = std::fmod(time_, duration);
		} else {
			time_ = duration;
			playing_ = false;
		}
	} else if (time_ < 0.0) {
		time_ = 0.0;
	}
}

double Workbench::SelectedDuration() const {
	return primaryAnim_ ? static_cast<double>(primaryAnim_->Duration()) : 0.0;
}

double Workbench::AnimDuration() const { return SelectedDuration(); }

float Workbench::AnimFrame() const {
	if (!primaryAnim_ || primaryAnim_->frameRate <= 0.0f) {
		return 0.0f;
	}
	return static_cast<float>(time_) * primaryAnim_->frameRate;
}

float Workbench::AnimProgress() const {
	const double duration = SelectedDuration();
	return duration > 0.0 ? static_cast<float>(time_ / duration) : 0.0f;
}

const assets::XAnimData* Workbench::BaseAnim() const {
	if (!additive_) {
		return primaryAnim_ ? &*primaryAnim_ : nullptr;
	}
	return idleAnim_ ? &*idleAnim_ : nullptr;
}

void Workbench::FrameCamera() {
	camera_.SetOrbitMode();
	if (hasCharacter_) {
		camera_.Frame(characterBody_.boundsCenter, BoundsRadius(characterBody_.boundsHalf));
	} else if (hasWeapon_) {
		camera_.Frame(viewmodel_.boundsCenter, BoundsRadius(viewmodel_.boundsHalf));
	} else if (hasModel_) {
		camera_.Frame(model_.boundsCenter, BoundsRadius(model_.boundsHalf));
	} else {
		camera_.Frame(DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f), 7.0f);
	}
}

void Workbench::UpdateViewCamera() {
	if (viewMode_ == ViewMode::FirstPerson && hasWeapon_) {
		DirectX::XMFLOAT3 eye = viewmodel_.viewOrigin;
		if (!viewmodel_.viewOriginValid) {
			eye = {0.0f, 0.0f, 0.0f};
		}
		const float yaw = DirectX::XMConvertToRadians(fpYaw_);
		const float pitch = DirectX::XMConvertToRadians(fpPitch_);
		const DirectX::XMFLOAT3 target = {eye.x + std::cos(pitch) * std::cos(yaw),
		                                  eye.y + std::cos(pitch) * std::sin(yaw),
		                                  eye.z + std::sin(pitch)};
		camera_.SetFirstPerson(eye, target);
		camera_.SetFov(fpFovOverride_ > 0.0f ? fpFovOverride_ : ViewmodelFov());
		return;
	}
	camera_.SetOrbitMode();
	camera_.SetFov(orbitFovOverride_ > 0.0f ? orbitFovOverride_ : settings_.camera.fov);
}

float Workbench::CgFov() const {
	if (ads_ && hasWeapon_ && viewmodel_.weapon.adsZoomFov > 1.0f) {
		return viewmodel_.weapon.adsZoomFov;
	}
	return settings_.firstPersonFov;
}

float Workbench::ViewmodelFov() const {
	// The viewmodel is rendered with the cg_fov-derived vertical FOV scaled by
	// cg_fovScale. The default is 1.0, i.e. the same FOV as the world.
	return std::clamp(VerticalFovFromCgFov(CgFov()) * settings_.viewmodelFovScale, 10.0f, 140.0f);
}

void Workbench::SetViewMode(ViewMode mode) {
	viewMode_ = mode;
	showGrid_ = mode == ViewMode::Orbit;
	UpdateViewCamera();
}

void Workbench::LoadAdsAnim() {
	adsAnim_.reset();
	adsTime_ = 0.0f;
	if (!hasWeapon_) {
		return;
	}
	const std::string anim = EffectiveAnimForState("ADS_UP");
	if (anim.empty()) {
		return;
	}
	std::optional<assets::XAnimData> loaded;
	std::string error;
	if (LoadAnimAsset(anim, loaded, error)) {
		adsAnim_ = std::move(loaded);
		adsTime_ = adsAnim_->Duration();
	}
}

void Workbench::SetAds(bool ads) {
	if (ads_ == ads) {
		return;
	}
	ads_ = ads;
	if (ads_ && hasWeapon_ && !adsAnim_) {
		LoadAdsAnim();
	}
	UpdateViewCamera();
	BuildPose();
}

void Workbench::SetOrbitView(float yawDegrees, float pitchDegrees, float distance, float fovDegrees) {
	viewMode_ = ViewMode::Orbit;
	showGrid_ = true;
	camera_.SetOrbitMode();
	camera_.SetYawPitch(yawDegrees, pitchDegrees);
	if (distance > 0.0f) {
		camera_.SetDistance(distance);
	}
	orbitFovOverride_ = fovDegrees > 0.0f ? fovDegrees : 0.0f;
	UpdateViewCamera();
}

void Workbench::SetFirstPersonView(float yawDegrees, float pitchDegrees, float fovDegrees) {
	viewMode_ = ViewMode::FirstPerson;
	showGrid_ = false;
	fpYaw_ = yawDegrees;
	fpPitch_ = pitchDegrees;
	fpFovOverride_ = fovDegrees > 0.0f ? fovDegrees : 0.0f;
	UpdateViewCamera();
}

void Workbench::UpdateAttachmentWorlds() {
	if (attachments_.empty()) {
		return;
	}
	DirectX::XMMATRIX weaponOrigin = DirectX::XMMatrixIdentity();
	const int gunRoot = FindPoseBone(pose_, "j_gun") >= 0 ? FindPoseBone(pose_, "j_gun")
	                                                     : FindPoseBone(pose_, "tag_weapon");
	if (gunRoot >= 0) {
		weaponOrigin = PoseBoneMatrix(pose_.bones[static_cast<std::size_t>(gunRoot)]);
	}

	for (AttachmentMount& mount : attachments_) {
		DirectX::XMMATRIX rootBind = DirectX::XMMatrixIdentity();
		if (!mount.model.model.bones.empty()) {
			rootBind = BoneMatrix(mount.model.model.bones.front());
		}
		const DirectX::XMMATRIX inverseRoot = DirectX::XMMatrixInverse(nullptr, rootBind);

		DirectX::XMMATRIX target = weaponOrigin;
		mount.tagFound = !mount.requiredTag.empty();
		if (mount.tagFound) {
			const int index = FindPoseBone(pose_, mount.requiredTag);
			if (index >= 0) {
				target = PoseBoneMatrix(pose_.bones[static_cast<std::size_t>(index)]);
			} else {
				mount.tagFound = false;
				target = weaponOrigin;
			}
		}
		mount.hidden = false;
		if (mount.tagFound) {
			mount.hidden = std::find(hiddenTags_.begin(), hiddenTags_.end(), mount.requiredTag) !=
			               hiddenTags_.end();
		}
		const DirectX::XMMATRIX world = inverseRoot * target;
		mount.world = ToFloat4x4(world);

		DirectX::XMMATRIX reticleAttach = DirectX::XMMatrixIdentity();
		int attachTag = FindModelBone(mount.model.model, "tag_reticle_attach");
		if (attachTag < 0) {
			attachTag = FindModelBone(mount.model.model, "tag_reticle_acog");
		}
		if (attachTag < 0) {
			for (std::size_t boneIndex = 0; boneIndex < mount.model.model.bones.size(); ++boneIndex) {
				if (mount.model.model.bones[boneIndex].name.rfind("tag_reticle", 0) == 0) {
					attachTag = static_cast<int>(boneIndex);
					break;
				}
			}
		}
		if (attachTag >= 0) {
			reticleAttach = BoneMatrix(mount.model.model.bones[static_cast<std::size_t>(attachTag)]);
		}
		for (std::size_t i = 0; i < mount.reticles.size(); ++i) {
			DirectX::XMMATRIX reticleRoot = DirectX::XMMatrixIdentity();
			if (!mount.reticles[i].model.bones.empty()) {
				reticleRoot = BoneMatrix(mount.reticles[i].model.bones.front());
			}
			const DirectX::XMMATRIX inverseReticleRoot = DirectX::XMMatrixInverse(nullptr, reticleRoot);
			const DirectX::XMMATRIX reticleWorld = inverseReticleRoot * reticleAttach * world;
			if (i < mount.reticleWorlds.size()) {
				mount.reticleWorlds[i] = ToFloat4x4(reticleWorld);
			}
		}
	}
}

bool Workbench::AlignAdsToReticle() {
	if (!ads_ || !hasWeapon_ || !camera_.IsFirstPerson()) {
		return false;
	}
	const AttachmentMount* sight = nullptr;
	for (const AttachmentMount& mount : attachments_) {
		if (mount.tagFound && !mount.hidden && !mount.reticleWorlds.empty() && !mount.reticles.empty()) {
			sight = &mount;
			break;
		}
	}
	if (sight == nullptr) {
		return false;
	}

	const DirectX::XMMATRIX world = DirectX::XMLoadFloat4x4(&sight->reticleWorlds.front());
	const DirectX::XMVECTOR aim = DirectX::XMVector3Transform(
	    DirectX::XMLoadFloat3(&sight->reticles.front().boundsCenter), world);
	const DirectX::XMFLOAT3 eye = camera_.Eye();
	const DirectX::XMFLOAT3 target = camera_.LookTarget();
	DirectX::XMFLOAT3 forward = {target.x - eye.x, target.y - eye.y, target.z - eye.z};
	const float length = std::sqrt(forward.x * forward.x + forward.y * forward.y + forward.z * forward.z);
	if (length < 1.0e-5f) {
		return false;
	}
	forward.x /= length;
	forward.y /= length;
	forward.z /= length;
	const DirectX::XMVECTOR forwardVector =
	    DirectX::XMVectorSet(forward.x, forward.y, forward.z, 0.0f);
	const DirectX::XMVECTOR eyeVector = DirectX::XMLoadFloat3(&eye);
	const float along = DirectX::XMVectorGetX(DirectX::XMVector3Dot(
	    DirectX::XMVectorSubtract(aim, eyeVector), forwardVector));
	const DirectX::XMVECTOR onAxis =
	    DirectX::XMVectorAdd(eyeVector, DirectX::XMVectorScale(forwardVector, along));
	DirectX::XMFLOAT3 delta;
	DirectX::XMStoreFloat3(&delta, DirectX::XMVectorSubtract(onAxis, aim));
	const float deltaLength =
	    std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
	if (deltaLength < 1.0e-4f || deltaLength > 5.0f) {
		return false;
	}

	for (scene::PoseBone& bone : pose_.bones) {
		if (bone.name != "tag_ads") {
			continue;
		}
		// tag_ads' parent (tag_view) has an identity rotation in the hands bind pose.
		bone.localPos.x += delta.x;
		bone.localPos.y += delta.y;
		bone.localPos.z += delta.z;
		scene::RecomputeWorld(pose_);
		return true;
	}
	return false;
}

void Workbench::BuildPose() {
	pose_ = scene::SkeletonPose{};
	gunPalette_.clear();
	handsPalette_.clear();
	modelPalette_.clear();

	const assets::XAnimData* base = BaseAnim();
	const assets::XAnimData* additive = AdditiveAnim();
	float baseTime = static_cast<float>(time_);
	if (additive_ && idleAnim_ && idleAnim_->Duration() > 0.0f) {
		baseTime = static_cast<float>(std::fmod(time_, static_cast<double>(idleAnim_->Duration())));
	}
	const float additiveTime = static_cast<float>(time_);

	if (hasCharacter_) {
		pose_ = scene::BuildModelPose(characterBody_.model, base, baseTime, additive, additiveTime,
		                              renameMap_.empty() ? nullptr : &renameMap_);
		characterBodyPalette_ = scene::BuildBonePalette(characterBody_.model, pose_, pose_.modelBoneToPose);
		characterHeadPalette_.clear();
		DirectX::XMStoreFloat4x4(&characterHeadWorld_, DirectX::XMMatrixIdentity());
		if (characterHasHead_ && !characterHead_.model.bones.empty()) {
			const std::vector<int> headMap = scene::MapBonesByName(characterHead_.model, pose_);
			characterHeadPalette_ = scene::BuildBonePalette(characterHead_.model, pose_, headMap);
			// The H2/H1 head models are authored on the body skeleton: the root is j_spine4 and
			// the palette above already maps every head bone onto the body pose. That places the
			// head directly on the neck, so the world matrix must stay identity. (Attaching the
			// root at j_head, or multiplying by the spine pose, moved the head off the body.)
			const std::string& headRoot = characterHead_.model.bones.front().name;
			const int headRootIndex = headRoot.empty() ? -1 : FindPoseBone(pose_, headRoot);
			DirectX::XMStoreFloat4x4(&characterHeadWorld_, DirectX::XMMatrixIdentity());
			characterHeadBone_.clear();
			if (headRootIndex >= 0) {
				characterHeadBone_ = headRoot;
			} else {
				// Fallback for a head that is not on the body skeleton: attach it at the neck/
				// head tag.
				int headBone = FindPoseBone(pose_, "j_head");
				if (headBone < 0) {
					headBone = FindPoseBone(pose_, "tag_head");
				}
				if (headBone < 0) {
					headBone = FindPoseBone(pose_, "j_neck");
				}
				if (headBone < 0) {
					headBone = FindPoseBone(pose_, "tag_neck");
				}
				if (headBone >= 0) {
					const DirectX::XMMATRIX headRootBind = BoneMatrix(characterHead_.model.bones.front());
					const DirectX::XMMATRIX inverseRoot = DirectX::XMMatrixInverse(nullptr, headRootBind);
					const DirectX::XMMATRIX headWorld =
					    inverseRoot * PoseBoneMatrix(pose_.bones[static_cast<std::size_t>(headBone)]);
					DirectX::XMStoreFloat4x4(&characterHeadWorld_, headWorld);
					characterHeadBone_ = pose_.bones[static_cast<std::size_t>(headBone)].name;
				}
			}
		}

		characterWeaponFound_ = false;
		characterWeaponTag_.clear();
		characterWeaponTagWorld_ = {0.0f, 0.0f, 0.0f};
		DirectX::XMStoreFloat4x4(&characterWeaponWorld_, DirectX::XMMatrixIdentity());
		if (gpuCharacterWeapon_ && !characterWeaponModel_.model.bones.empty()) {
			int weaponBone = -1;
			for (const char* tag : {"tag_weapon_right", "tag_inhand", "tag_weapon"}) {
				weaponBone = FindPoseBone(pose_, tag);
				if (weaponBone >= 0) {
					characterWeaponTag_ = tag;
					break;
				}
			}
			if (weaponBone >= 0) {
				characterWeaponFound_ = true;
				characterWeaponTagWorld_ = pose_.bones[static_cast<std::size_t>(weaponBone)].worldPos;
				const DirectX::XMMATRIX rootBind = BoneMatrix(characterWeaponModel_.model.bones.front());
				const DirectX::XMMATRIX inverseRoot = DirectX::XMMatrixInverse(nullptr, rootBind);
				const DirectX::XMMATRIX weaponWorld =
				    inverseRoot * PoseBoneMatrix(pose_.bones[static_cast<std::size_t>(weaponBone)]);
				DirectX::XMStoreFloat4x4(&characterWeaponWorld_, weaponWorld);
			}
		}
		return;
	}

	if (hasWeapon_) {
		pose_ = scene::BuildViewmodelPose(viewmodel_, base, baseTime, additive, additiveTime,
		                                  renameMap_.empty() ? nullptr : &renameMap_);
		if (ads_ && adsAnim_) {
			for (scene::PoseBone& bone : pose_.bones) {
				if (bone.name != "tag_ads") {
					continue;
				}
				DirectX::XMFLOAT4 rotation;
				if (adsAnim_->SampleRotation("tag_ads", adsTime_, rotation)) {
					bone.localQuat = rotation;
				}
				DirectX::XMFLOAT3 translation;
				if (adsAnim_->SampleTranslation("tag_ads", adsTime_, translation)) {
					bone.localPos = translation;
				}
				scene::RecomputeWorld(pose_);
				break;
			}
		}
		UpdateAttachmentWorlds();
		if (adsAutoAlign_ && AlignAdsToReticle()) {
			UpdateAttachmentWorlds();
		}
		handsPalette_ = scene::BuildBonePalette(viewmodel_.hands.model, pose_, pose_.handsBoneToPose);
		gunPalette_ = scene::BuildBonePalette(viewmodel_.gun.model, pose_, pose_.gunBoneToPose);
		if (!hiddenTags_.empty()) {
			const auto hideBones = [this](std::vector<DirectX::XMFLOAT4X4>& palette,
			                             const assets::XModelData& model) {
				if (palette.size() != model.bones.size()) {
					return;
				}
				for (std::size_t i = 0; i < model.bones.size(); ++i) {
					if (std::find(hiddenTags_.begin(), hiddenTags_.end(), model.bones[i].name) !=
					    hiddenTags_.end()) {
						StoreZero(palette[i]);
					}
				}
			};
			hideBones(handsPalette_, viewmodel_.hands.model);
			hideBones(gunPalette_, viewmodel_.gun.model);
		}
	} else if (hasModel_) {
		pose_ = scene::BuildModelPose(model_.model, base, baseTime, additive, additiveTime,
		                              renameMap_.empty() ? nullptr : &renameMap_);
		modelPalette_ = scene::BuildBonePalette(model_.model, pose_, pose_.modelBoneToPose);
	}
}

bool Workbench::RenderScene(int width, int height) {
	if (!renderer_.BeginScene(width, height)) {
		return false;
	}
	if (showGrid_) {
		renderer_.RenderGrid(camera_);
	}

	BuildPose();

	if (hasCharacter_) {
		if (gpuCharacterBody_) {
			renderer_.RenderModel(*gpuCharacterBody_, camera_, wireframe_, nullptr, settings_.exposure, nullptr,
			                      &characterBodyPalette_);
		}
		if (gpuCharacterHead_) {
			renderer_.RenderModel(*gpuCharacterHead_, camera_, wireframe_, nullptr, settings_.exposure,
			                      &characterHeadWorld_, &characterHeadPalette_);
		}
		if (gpuCharacterWeapon_) {
			DirectX::XMFLOAT4 tint = {1.0f, 1.0f, 1.0f, 1.0f};
			const DirectX::XMFLOAT4* tintPtr = nullptr;
			if (!characterWeaponFound_) {
				tint = {0.95f, 0.12f, 0.12f, 1.0f};
				tintPtr = &tint;
			}
			renderer_.RenderModel(*gpuCharacterWeapon_, camera_, wireframe_, nullptr, settings_.exposure,
			                      &characterWeaponWorld_, nullptr, tintPtr, !characterWeaponFound_);
		}
	} else if (gpuGun_) {
		if (gpuHands_) {
			renderer_.RenderModel(*gpuHands_, camera_, wireframe_, &handsSurfaceVisible_, settings_.exposure,
			                      nullptr, &handsPalette_);
		}
		renderer_.RenderModel(*gpuGun_, camera_, wireframe_, &gunSurfaceVisible_, settings_.exposure, nullptr,
		                      &gunPalette_);
		for (const AttachmentMount& mount : attachments_) {
			if (mount.hidden) {
				continue;
			}
			DirectX::XMFLOAT4 tint = {1.0f, 1.0f, 1.0f, 1.0f};
			const DirectX::XMFLOAT4* tintPtr = nullptr;
			if (!mount.tagFound) {
				tint = {0.95f, 0.12f, 0.12f, 1.0f};
				tintPtr = &tint;
			}
			if (mount.gpu) {
				renderer_.RenderModel(*mount.gpu, camera_, wireframe_, nullptr, settings_.exposure,
				                      &mount.world, nullptr, tintPtr, !mount.tagFound);
			}
			for (std::size_t i = 0; i < mount.reticleGpus.size() && i < mount.reticleWorlds.size(); ++i) {
				if (mount.reticleGpus[i]) {
					renderer_.RenderModel(*mount.reticleGpus[i], camera_, wireframe_, nullptr,
					                      settings_.exposure, &mount.reticleWorlds[i], nullptr, tintPtr,
					                      !mount.tagFound);
				}
			}
		}
	} else if (gpuModel_) {
		if (modelVisible_) {
			renderer_.RenderModel(*gpuModel_, camera_, wireframe_, &modelSurfaceVisible_, settings_.exposure,
			                      nullptr, &modelPalette_);
		}
	}
	return true;
}

bool HasAnim(const std::vector<std::filesystem::path>& roots, const std::string& name) {
	return assets::FindAnimFile(roots, name).has_value();
}

bool LoadAnimForInfo(const std::vector<std::filesystem::path>& roots, const std::string& name,
                     assets::XAnimData& out, std::string& error) {
	const std::optional<std::filesystem::path> path = assets::FindAnimFile(roots, name);
	if (!path.has_value()) {
		error = "animation not found: " + name;
		return false;
	}
	try {
		out = assets::LoadXAnim(*path);
	} catch (const std::exception& e) {
		error = e.what();
		return false;
	}
	return true;
}

bool LoadModelForInfo(const std::vector<std::filesystem::path>& roots, const std::string& name,
                      assets::ModelAsset& out, std::string& error) {
	const std::optional<std::filesystem::path> path = assets::FindModelFile(roots, name);
	if (!path.has_value()) {
		error = "model not found: " + name;
		return false;
	}
	return assets::LoadModelAssetFromFile(*path, roots, out, error, false);
}

bool BuildFitReport(const std::vector<std::filesystem::path>& roots, const std::string& weaponName,
                    const std::string& handsOverride, const std::vector<std::string>& attachmentFilter,
                    AttachmentFitReport& out, std::string& error, bool world) {
	out = AttachmentFitReport{};
	out.world = world;
	const std::optional<std::filesystem::path> weaponPath = assets::FindWeaponFile(roots, weaponName);
	if (!weaponPath.has_value()) {
		error = "weapon json not found: weapons/" + weaponName + ".json";
		return false;
	}
	assets::WeaponAsset weapon;
	try {
		weapon = assets::LoadWeapon(*weaponPath);
	} catch (const std::exception& e) {
		error = e.what();
		return false;
	}
	out.weapon = weapon.name;
	out.gunModel = world && !weapon.worldModel.empty() ? weapon.worldModel : weapon.gunModel;

	assets::ModelAsset gun;
	std::string loadError;
	if (!assets::LoadModelAsset(out.gunModel, roots, gun, loadError, false)) {
		error = (world ? "world model " : "gun model ") + out.gunModel + ": " + loadError;
		return false;
	}

	std::string handsName = handsOverride;
	if (!world) {
		if (handsName.empty()) {
			handsName = weapon.handModel == "viewmodel_base_viewhands" ? scene::DefaultFactionHands(roots)
			                                                          : weapon.handModel;
		}
	}
	out.handsModel = handsName;
	assets::ModelAsset hands;
	const bool haveHands =
	    !handsName.empty() && assets::LoadModelAsset(handsName, roots, hands, loadError, false);

	const auto addTags = [&out](const assets::XModelData& model) {
		for (const assets::BoneData& bone : model.bones) {
			if (bone.name.rfind("tag_", 0) != 0) {
				continue;
			}
			if (std::find(out.weaponTags.begin(), out.weaponTags.end(), bone.name) ==
			    out.weaponTags.end()) {
				out.weaponTags.push_back(bone.name);
			}
		}
	};
	addTags(gun.model);
	if (haveHands) {
		addTags(hands.model);
	}

	std::vector<std::string> names = attachmentFilter;
	if (names.empty()) {
		names = assets::ListAttachments(roots);
	}

	for (const std::string& name : names) {
		AttachmentFitEntry entry;
		entry.name = name;
		const std::optional<std::filesystem::path> path = FindAttachmentByName(roots, name);
		if (!path.has_value()) {
			entry.note = "attachment json not found";
			out.attachments.push_back(std::move(entry));
			continue;
		}
		try {
			const assets::AttachmentAsset attachment = assets::LoadAttachment(*path);
			entry.name = attachment.name;
			const std::vector<std::string>& modelNames =
			    world && !attachment.worldModels.empty() ? attachment.worldModels : attachment.viewModels;
			if (modelNames.empty()) {
				entry.note = world ? "no world model" : "no view model";
				out.attachments.push_back(std::move(entry));
				continue;
			}
			entry.viewModel = modelNames.front();
			assets::ModelAsset model;
			std::string modelError;
			if (!assets::LoadModelAsset(entry.viewModel, roots, model, modelError, false)) {
				entry.note = "view model: " + modelError;
				out.attachments.push_back(std::move(entry));
				continue;
			}
			if (model.model.bones.empty()) {
				entry.note = "view model has no bones";
				out.attachments.push_back(std::move(entry));
				continue;
			}
			entry.requiredTag = model.model.bones.front().name;
			entry.found = std::find(out.weaponTags.begin(), out.weaponTags.end(), entry.requiredTag) !=
			              out.weaponTags.end();
			if (!entry.found) {
				entry.nearest = assets::NearestTags(entry.requiredTag, out.weaponTags, 3);
			}
			out.attachments.push_back(std::move(entry));
		} catch (const std::exception& e) {
			entry.note = e.what();
			out.attachments.push_back(std::move(entry));
		}
	}
	return true;
}

void PrintFitReport(const AttachmentFitReport& report, std::FILE* out) {
	std::fprintf(out, "weapon: %s\n", report.weapon.c_str());
	std::fprintf(out, "model: %s%s\n", report.gunModel.c_str(), report.world ? " (world)" : "");
	std::fprintf(out, "hands: %s\n", report.handsModel.empty() ? "(none)" : report.handsModel.c_str());
	std::fprintf(out, "weapon tags (%zu):", report.weaponTags.size());
	for (const std::string& tag : report.weaponTags) {
		std::fprintf(out, " %s", tag.c_str());
	}
	std::fprintf(out, "\n\n");
	std::fprintf(out, "attachments: %zu (mount tag = attachment %s root bone)\n", report.attachments.size(),
	             report.world ? "world model" : "view model");
	std::fprintf(out, "  %-32s %-40s %-22s %-8s %s\n", "attachment",
	             report.world ? "world model" : "view model", "needed tag", "status",
	             "nearest weapon tags");
	for (const AttachmentFitEntry& entry : report.attachments) {
		std::string nearest;
		for (std::size_t i = 0; i < entry.nearest.size(); ++i) {
			if (i > 0) {
				nearest += ", ";
			}
			nearest += entry.nearest[i];
		}
		std::string status = entry.found ? "found" : "MISSING";
		if (!entry.note.empty()) {
			status = "-";
		}
		std::fprintf(out, "  %-32s %-40s %-22s %-8s %s%s%s\n", entry.name.c_str(),
		             entry.viewModel.empty() ? "-" : entry.viewModel.c_str(),
		             entry.requiredTag.empty() ? "-" : entry.requiredTag.c_str(), status.c_str(),
		             nearest.c_str(), entry.note.empty() ? "" : "  ", entry.note.c_str());
	}
}

bool LoadRenameTable(const std::filesystem::path& path, std::map<std::string, std::string>& table,
                     std::string& error) {
	table.clear();
	nlohmann::json json;
	try {
		std::ifstream in(path);
		if (!in) {
			error = "rename map: cannot open " + path.string();
			return false;
		}
		in >> json;
	} catch (const std::exception& e) {
		error = std::string("rename map: ") + e.what();
		return false;
	}
	const nlohmann::json* source = &json;
	if (json.is_object() && json.contains("rename") && json["rename"].is_object()) {
		source = &json["rename"];
	}
	if (!source->is_object()) {
		error = "rename map: expected a JSON object {\"old\":\"new\"}";
		return false;
	}
	for (auto it = source->begin(); it != source->end(); ++it) {
		if (!it.value().is_string()) {
			error = "rename map: value for " + it.key() + " is not a string";
			return false;
		}
		table[it.key()] = it.value().get<std::string>();
	}
	return true;
}

bool CompareBones(const std::vector<std::filesystem::path>& roots, const std::string& animName,
                  const std::string& modelName, const std::map<std::string, std::string>& rename,
                  scene::BoneCompareResult& result, std::string& error) {
	const std::optional<std::filesystem::path> animPath = assets::FindAnimFile(roots, animName);
	if (!animPath.has_value()) {
		error = "animation not found: " + animName;
		return false;
	}
	assets::XAnimData anim;
	try {
		anim = assets::LoadXAnim(*animPath);
	} catch (const std::exception& e) {
		error = std::string("anim: ") + e.what();
		return false;
	}
	assets::ModelAsset model;
	if (!assets::LoadModelAsset(modelName, roots, model, error, false)) {
		error = "model " + modelName + ": " + error;
		return false;
	}
	result = scene::CompareAnimToModel(anim, model.model, rename);
	return true;
}

std::string AnimStateForTreeType(int type) { return StateForAnimTreeType(type); }

} // namespace wb::commands
