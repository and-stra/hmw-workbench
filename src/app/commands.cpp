#include "app/commands.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_set>

namespace wb::commands {
namespace {

constexpr float kMinSpeed = 0.1f;
constexpr float kMaxSpeed = 2.0f;

DirectX::XMMATRIX BoneMatrix(const assets::BoneData& bone) {
	const DirectX::XMVECTOR quaternion = DirectX::XMLoadFloat4(&bone.worldQuat);
	return DirectX::XMMatrixRotationQuaternion(quaternion) *
	       DirectX::XMMatrixTranslation(bone.worldPos.x, bone.worldPos.y, bone.worldPos.z);
}

DirectX::XMMATRIX PoseBoneMatrix(const scene::PoseBone& bone) {
	const DirectX::XMVECTOR quaternion = DirectX::XMLoadFloat4(&bone.worldQuat);
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
	// H1 weapAnimFiles_t values. The AW crossbow port stores 128/129 for ADS_UP/ADS_DOWN
	// (AW's enum is shifted by one); no H1/H2 weapon in the dumps has a 128/129 override.
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
	case 128:
		return "ADS_UP";
	case 129:
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
}

Workbench::~Workbench() = default;

void Workbench::SetRoots(std::vector<std::filesystem::path> roots) {
	roots_ = std::move(roots);
	RefreshAttachmentList();
}

void Workbench::ClearWeapon() {
	ClearAttachments();
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
}

void Workbench::ClearAttachments() {
	attachments_.clear();
	hiddenTags_.clear();
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
	if (mount.attachment.viewModels.empty()) {
		error = "attachment " + mount.name + " has no view model";
		return false;
	}
	mount.viewModel = mount.attachment.viewModels.front();

	std::string loadError;
	if (!assets::LoadModelAsset(mount.viewModel, roots_, mount.model, loadError, true)) {
		error = "attachment " + mount.name + " view model " + mount.viewModel + ": " + loadError;
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

bool Workbench::OverrideAppliesToAttachment(const assets::WeaponAnimOverride& entry) const {
	if (attachments_.empty()) {
		return false;
	}
	const std::vector<std::string>& listed = viewmodel_.weapon.attachments;
	if (!listed.empty()) {
		const auto matches = [this, &listed, &entry](std::size_t index) {
			if (index >= listed.size()) {
				return false;
			}
			const std::string& name = listed[index];
			const std::string lower = ToLower(name);
			for (const AttachmentMount& mount : attachments_) {
				if (ToLower(mount.name) == lower || ToLower(mount.internalName) == lower) {
					return true;
				}
			}
			return false;
		};
		if (matches(entry.attachment1) || (entry.attachment1 > 0 && matches(entry.attachment1 - 1))) {
			return true;
		}
	}
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
                           std::string& error) {
	ClearWeapon();
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
	if (!scene::LoadViewmodel(weapon, hands, roots_, loaded, error)) {
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

	RefreshAnimLists();

	viewMode_ = firstPerson ? ViewMode::FirstPerson : ViewMode::Orbit;
	if (viewMode_ == ViewMode::FirstPerson) {
		showGrid_ = false;
	} else {
		FrameCamera();
	}
	UpdateViewCamera();
	AutoPlayIdle();
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
	std::vector<std::string> attached;
	attached.reserve(attachments_.size());
	for (const AttachmentMount& mount : attachments_) {
		attached.push_back(mount.name);
	}
	if (!LoadWeapon(weaponName_, handsOverride_, firstPerson, error)) {
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
	modelSurfaceVisible_.assign(model_.surfaceSet.surfaces.size(), true);
	if (!hasWeapon_) {
		FrameCamera();
	}
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
	handsOverride_ = hands;
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
	BuildPose();
	return true;
}

void Workbench::StopAnim() {
	primaryAnim_.reset();
	idleAnim_.reset();
	animName_.clear();
	playing_ = false;
	time_ = 0.0;
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
	if (hasWeapon_) {
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
		const int attachTag = FindModelBone(mount.model.model, "tag_reticle_attach");
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

	if (hasWeapon_) {
		pose_ = scene::BuildViewmodelPose(viewmodel_, base, baseTime, additive, additiveTime);
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
		if (AlignAdsToReticle()) {
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
		pose_ = scene::BuildModelPose(model_.model, base, baseTime, additive, additiveTime);
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

	if (gpuGun_) {
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
                    AttachmentFitReport& out, std::string& error) {
	out = AttachmentFitReport{};
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
	out.gunModel = weapon.gunModel;

	assets::ModelAsset gun;
	std::string loadError;
	if (!assets::LoadModelAsset(weapon.gunModel, roots, gun, loadError, false)) {
		error = "gun model " + weapon.gunModel + ": " + loadError;
		return false;
	}

	std::string handsName = handsOverride;
	if (handsName.empty()) {
		handsName = weapon.handModel == "viewmodel_base_viewhands" ? scene::DefaultFactionHands(roots)
		                                                          : weapon.handModel;
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
			if (attachment.viewModels.empty()) {
				entry.note = "no view model";
				out.attachments.push_back(std::move(entry));
				continue;
			}
			entry.viewModel = attachment.viewModels.front();
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
	std::fprintf(out, "gun: %s\n", report.gunModel.c_str());
	std::fprintf(out, "hands: %s\n", report.handsModel.empty() ? "(none)" : report.handsModel.c_str());
	std::fprintf(out, "weapon tags (%zu):", report.weaponTags.size());
	for (const std::string& tag : report.weaponTags) {
		std::fprintf(out, " %s", tag.c_str());
	}
	std::fprintf(out, "\n\n");
	std::fprintf(out, "attachments: %zu (mount tag = attachment view model root bone)\n",
	             report.attachments.size());
	std::fprintf(out, "  %-32s %-40s %-22s %-8s %s\n", "attachment", "view model", "needed tag", "status",
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

std::string AnimStateForTreeType(int type) { return StateForAnimTreeType(type); }

} // namespace wb::commands
