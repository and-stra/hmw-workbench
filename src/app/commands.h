#pragma once

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <DirectXMath.h>

#include "app/settings.h"
#include "assets/asset_index.h"
#include "assets/attachment.h"
#include "assets/model_asset.h"
#include "assets/xanim.h"
#include "render/camera.h"
#include "render/renderer.h"
#include "scene/animation.h"
#include "scene/viewmodel.h"

namespace wb::commands {

// A mounted viewmodel attachment. The attachment model's root bone is the weapon tag
// it mounts on; `world` maps the attachment model space into viewmodel space.
struct AttachmentMount {
	std::string name; // attachment asset name (file stem)
	std::string internalName;
	std::string viewModel;
	std::string requiredTag;
	bool tagFound = false;
	bool hidden = false;
	std::string error;
	assets::AttachmentAsset attachment;
	assets::ModelAsset model;
	std::unique_ptr<GpuModel> gpu;
	std::vector<assets::ModelAsset> reticles;
	std::vector<std::unique_ptr<GpuModel>> reticleGpus;
	std::vector<DirectX::XMFLOAT4X4> reticleWorlds;
	DirectX::XMFLOAT4X4 world{};
};

struct AttachmentFitEntry {
	std::string name;
	std::string viewModel;
	std::string requiredTag;
	bool found = false;
	std::vector<std::string> nearest;
	std::string note;
};

struct AttachmentFitReport {
	std::string weapon;
	std::string gunModel;
	std::string handsModel;
	std::vector<std::string> weaponTags;
	std::vector<AttachmentFitEntry> attachments;
};

// Plain scene/command layer shared by the UI, the CLI and (later) the MCP server.
// No ImGui types and no direct loader calls live above this layer: every asset is
// loaded, posed and rendered through these commands.
enum class ViewMode { Orbit, FirstPerson };

struct AnimStateEntry {
	std::string state;
	std::string anim;
};

class Workbench {
public:
	Workbench(Settings& settings, AssetIndex& assets, Renderer& renderer,
	          std::vector<std::filesystem::path> roots);
	~Workbench();

	Workbench(const Workbench&) = delete;
	Workbench& operator=(const Workbench&) = delete;

	void SetRoots(std::vector<std::filesystem::path> roots);

	// ---- loading ----
	bool LoadWeapon(const std::string& weapon, const std::string& hands, bool firstPerson, std::string& error);
	void UnloadWeapon() { ClearWeapon(); }
	bool ReloadWeapon(std::string& error);
	bool LoadModel(const std::string& model, std::string& error);
	bool ReloadModel(std::string& error);
	bool SetHands(const std::string& hands, std::string& error);

	// ---- attachments ----
	bool Attach(const std::string& attachment, std::string& error);
	bool Detach(const std::string& attachment);
	void DetachAll();
	bool IsAttached(const std::string& attachment) const;
	const std::vector<AttachmentMount>& Attachments() const { return attachments_; }
	const std::vector<std::string>& AvailableAttachments() const { return attachmentNames_; }
	const std::vector<std::string>& HiddenTags() const { return hiddenTags_; }
	void RefreshAttachmentList();
	const std::vector<AttachmentFitEntry>& AttachmentCatalog();
	// The effective anim for a weapon state after attachment `szXAnims` fields and the
	// weapon's animOverrides (the crossbow's AW optic overrides) have been applied.
	std::string EffectiveAnimForState(const std::string& state) const;
	bool IsAnimOverrideActive(const assets::WeaponAnimOverride& entry) const {
		return OverrideAppliesToAttachment(entry);
	}

	// ---- animation ----
	bool PlayAnim(const std::string& anim, std::string& error);
	void StopAnim();
	void SetAnimTime(double seconds);
	void SetAnimFraction(double fraction);
	void SetAnimPlaying(bool playing);
	void SetAnimLooping(bool looping);
	void SetAnimSpeed(float speed);
	void SetAdditive(bool additive);
	void SetAnimFrame(int frame);
	void Update(double deltaSeconds);

	bool HasAnim() const { return primaryAnim_.has_value(); }
	const assets::XAnimData* Anim() const { return primaryAnim_ ? &*primaryAnim_ : nullptr; }
	const std::string& AnimName() const { return animName_; }
	double AnimTime() const { return time_; }
	double AnimDuration() const;
	float AnimFrame() const;
	float AnimProgress() const;
	bool AnimPlaying() const { return playing_; }
	bool AnimLooping() const { return looping_; }
	float AnimSpeed() const { return speed_; }
	bool AdditiveEnabled() const { return additive_; }
	const assets::XAnimData* AdditiveAnim() const {
		return additive_ && primaryAnim_ ? &*primaryAnim_ : nullptr;
	}
	const assets::XAnimData* BaseAnim() const;

	// ---- view / camera ----
	ViewMode GetViewMode() const { return viewMode_; }
	void SetViewMode(ViewMode mode);
	bool ShowGrid() const { return showGrid_; }
	void SetShowGrid(bool show) { showGrid_ = show; }
	bool Wireframe() const { return wireframe_; }
	void SetWireframe(bool wire) { wireframe_ = wire; }
	bool ModelVisible() const { return modelVisible_; }
	void SetModelVisible(bool visible) { modelVisible_ = visible; }
	Camera& GetCamera() { return camera_; }
	const Camera& GetCamera() const { return camera_; }
	void FrameCamera();
	void RefreshCamera() { UpdateViewCamera(); }
	void SetAds(bool ads);
	bool Aiming() const { return ads_; }
	float CgFov() const;
	float ViewmodelFov() const;
	void SetOrbitView(float yawDegrees, float pitchDegrees, float distance, float fovDegrees);
	void SetFirstPersonView(float yawDegrees, float pitchDegrees, float fovDegrees);

	// ---- render ----
	// Draws the scene (grid optional, models, GPU skinning) into the renderer's scene
	// target. The caller draws overlays and presents/saves.
	bool RenderScene(int width, int height);

	// ---- state accessors (plain data only) ----
	const scene::ViewmodelAsset* Viewmodel() const { return hasWeapon_ ? &viewmodel_ : nullptr; }
	const assets::ModelAsset* Model() const { return hasModel_ ? &model_ : nullptr; }
	GpuModel* GpuWeaponGun() { return gpuGun_.get(); }
	GpuModel* GpuWeaponHands() { return gpuHands_.get(); }
	GpuModel* GpuStandaloneModel() { return gpuModel_.get(); }
	const std::vector<AnimStateEntry>& AnimStates() const { return animStates_; }
	const std::vector<assets::WeaponAnimOverride>& AnimOverrides() const { return animOverrides_; }
	const std::string& WeaponName() const { return weaponName_; }
	const std::string& WeaponError() const { return weaponError_; }
	const std::string& ModelName() const { return modelName_; }
	const std::string& ModelError() const { return modelError_; }
	const std::string& HandsName() const { return handsOverride_; }
	bool IsFirstPerson() const { return viewMode_ == ViewMode::FirstPerson; }

	std::vector<bool>& GunSurfaceVisible() { return gunSurfaceVisible_; }
	std::vector<bool>& HandsSurfaceVisible() { return handsSurfaceVisible_; }
	std::vector<bool>& ModelSurfaceVisible() { return modelSurfaceVisible_; }

	const scene::SkeletonPose& Pose() const { return pose_; }
	const std::vector<DirectX::XMFLOAT4X4>& GunPalette() const { return gunPalette_; }
	const std::vector<DirectX::XMFLOAT4X4>& HandsPalette() const { return handsPalette_; }
	const std::vector<DirectX::XMFLOAT4X4>& ModelPalette() const { return modelPalette_; }

private:
	void ClearWeapon();
	void ClearModel();
	void ClearAttachments();
	void RebuildHiddenTags();
	void UpdateAttachmentWorlds();
	bool AlignAdsToReticle();
	void BuildAttachmentCatalog();
	bool OverrideAppliesToAttachment(const assets::WeaponAnimOverride& entry) const;
	void LoadAdsAnim();
	void RefreshAnimLists();
	void AutoPlayIdle();
	void UpdateViewCamera();
	void BuildPose();
	double SelectedDuration() const;
	bool LoadAnimAsset(const std::string& name, std::optional<assets::XAnimData>& out, std::string& error) const;

	Settings& settings_;
	AssetIndex& assets_;
	Renderer& renderer_;
	std::vector<std::filesystem::path> roots_;

	Camera camera_;

	bool hasWeapon_ = false;
	scene::ViewmodelAsset viewmodel_;
	std::unique_ptr<GpuModel> gpuGun_;
	std::unique_ptr<GpuModel> gpuHands_;
	std::string weaponName_;
	std::string weaponError_;
	std::string handsOverride_;
	std::vector<bool> gunSurfaceVisible_;
	std::vector<bool> handsSurfaceVisible_;

	bool hasModel_ = false;
	assets::ModelAsset model_;
	std::unique_ptr<GpuModel> gpuModel_;
	std::string modelName_;
	std::string modelError_;
	std::vector<bool> modelSurfaceVisible_;

	std::vector<AnimStateEntry> animStates_;
	std::vector<assets::WeaponAnimOverride> animOverrides_;

	std::vector<std::string> attachmentNames_;
	std::vector<AttachmentMount> attachments_;
	std::vector<std::string> hiddenTags_;
	std::vector<AttachmentFitEntry> attachmentCatalog_;
	bool attachmentCatalogValid_ = false;

	std::string animName_;
	std::optional<assets::XAnimData> primaryAnim_;
	std::optional<assets::XAnimData> idleAnim_;
	std::optional<assets::XAnimData> adsAnim_;
	bool additive_ = false;
	bool playing_ = true;
	bool looping_ = true;
	float speed_ = 1.0f;
	double time_ = 0.0;
	float adsTime_ = 0.0f;

	ViewMode viewMode_ = ViewMode::Orbit;
	bool showGrid_ = true;
	bool wireframe_ = false;
	bool modelVisible_ = true;
	bool ads_ = false;
	float fpYaw_ = 0.0f;
	float fpPitch_ = 0.0f;
	float fpFovOverride_ = 0.0f;
	float orbitFovOverride_ = 0.0f;

	scene::SkeletonPose pose_;
	std::vector<DirectX::XMFLOAT4X4> gunPalette_;
	std::vector<DirectX::XMFLOAT4X4> handsPalette_;
	std::vector<DirectX::XMFLOAT4X4> modelPalette_;
};

// Read-only inspection commands used by the CLI `-info` dump. They live in the command
// layer so the CLI never calls an asset loader directly; they need no renderer.
bool HasAnim(const std::vector<std::filesystem::path>& roots, const std::string& name);
bool LoadAnimForInfo(const std::vector<std::filesystem::path>& roots, const std::string& name,
                     assets::XAnimData& out, std::string& error);
bool LoadModelForInfo(const std::vector<std::filesystem::path>& roots, const std::string& name,
                      assets::ModelAsset& out, std::string& error);

// Attachment fit report as plain data (used by `-fit-report` and the M4b MCP). Loads the
// weapon view model and every attachment view model in the roots, reads the attachment
// root bone (= required weapon tag), and reports found/missing plus the nearest weapon tags.
bool BuildFitReport(const std::vector<std::filesystem::path>& roots, const std::string& weapon,
                    const std::string& hands, const std::vector<std::string>& attachmentFilter,
                    AttachmentFitReport& out, std::string& error);
void PrintFitReport(const AttachmentFitReport& report, std::FILE* out);

// Direct mapping used by the Inspector for animOverrides[].animTreeType.
std::string AnimStateForTreeType(int type);

} // namespace wb::commands
