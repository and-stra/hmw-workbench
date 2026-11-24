#include "app/commands.h"

#include <algorithm>
#include <cmath>

namespace wb::commands {
namespace {

constexpr float kMinSpeed = 0.1f;
constexpr float kMaxSpeed = 2.0f;

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
}

Workbench::~Workbench() = default;

void Workbench::SetRoots(std::vector<std::filesystem::path> roots) { roots_ = std::move(roots); }

void Workbench::ClearWeapon() {
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
	if (!LoadWeapon(weaponName_, handsOverride_, firstPerson, error)) {
		return false;
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
	std::string idle;
	if (!FindAnimState(animStates_, "IDLE", idle)) {
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

void Workbench::SetAds(bool ads) {
	if (ads_ == ads) {
		return;
	}
	ads_ = ads;
	if (ads_ && hasWeapon_ && !adsAnim_) {
		std::string anim;
		if (FindAnimState(animStates_, "ADS_UP", anim)) {
			std::optional<assets::XAnimData> loaded;
			std::string error;
			if (LoadAnimAsset(anim, loaded, error)) {
				adsAnim_ = std::move(loaded);
				adsTime_ = adsAnim_->Duration();
			}
		}
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
		handsPalette_ = scene::BuildBonePalette(viewmodel_.hands.model, pose_, pose_.handsBoneToPose);
		gunPalette_ = scene::BuildBonePalette(viewmodel_.gun.model, pose_, pose_.gunBoneToPose);
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

} // namespace wb::commands
