#pragma once

#include <cstdint>
#include <cstdio>
#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <DirectXMath.h>

#include "app/settings.h"
#include "assets/asset_index.h"
#include "assets/attachment.h"
#include "assets/character.h"
#include "assets/model_asset.h"
#include "assets/weapon.h"
#include "assets/xanim.h"
#include "assets/xmodel_document.h"
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
	bool world = false;
	std::vector<std::string> weaponTags;
	std::vector<AttachmentFitEntry> attachments;
};

struct TagEditInfo {
	std::string name;
	std::string parent;
	DirectX::XMFLOAT3 localPos{};
	DirectX::XMFLOAT4 localQuat{0.0f, 0.0f, 0.0f, 1.0f};
	DirectX::XMFLOAT3 worldPos{};
	bool added = false;
};

struct ModelSaveResult {
	std::string path;
	std::string backup;
	std::string tagsJson;
	std::string validation;
};

struct TagFitResult {
	std::string tag;
	std::string attachment;
	std::string anim;
	DirectX::XMFLOAT3 sourceLocalPos{};
	DirectX::XMFLOAT3 finalLocalPos{};
	DirectX::XMFLOAT3 localDelta{};
	DirectX::XMFLOAT3 startWorldPos{};
	DirectX::XMFLOAT3 finalWorldPos{};
	DirectX::XMFLOAT3 worldDelta{};
	float screenX = 0.0f;
	float screenY = 0.0f;
	int iterations = 0;
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
	const std::vector<std::filesystem::path>& Roots() const { return roots_; }

	// ---- loading ----
	bool LoadWeapon(const std::string& weapon, const std::string& hands, bool firstPerson, std::string& error,
	                bool world = false);
	void UnloadWeapon() { ClearWeapon(); }
	bool ReloadWeapon(std::string& error);
	bool LoadModel(const std::string& model, std::string& error);
	bool ReloadModel(std::string& error);
	bool SetHands(const std::string& hands, std::string& error);
	bool IsWorldModel() const { return worldModel_; }
	bool SetWorldModel(bool world, std::string& error);

	// ---- player characters (M9) ----
	bool LoadCharacter(const std::string& faction, const std::string& weapon, std::string& error);
	bool LoadCharacterParts(const std::string& body, const std::string& head, const std::string& weapon,
	                        std::string& error);
	bool SetCharacterWeapon(const std::string& weapon, std::string& error);
	void ClearCharacter();
	bool HasCharacter() const { return hasCharacter_; }
	const assets::CharacterFaction* CharacterFaction() const {
		return hasCharacter_ ? &characterFaction_ : nullptr;
	}
	const assets::ModelAsset* CharacterBody() const { return hasCharacter_ ? &characterBody_ : nullptr; }
	const assets::ModelAsset* CharacterHead() const { return characterHasHead_ ? &characterHead_ : nullptr; }
	GpuModel* GpuCharacterBody() { return gpuCharacterBody_.get(); }
	GpuModel* GpuCharacterHead() { return gpuCharacterHead_.get(); }
	GpuModel* GpuCharacterWeapon() { return gpuCharacterWeapon_.get(); }
	const std::vector<std::string>& CharacterMissing() const { return characterMissing_; }
	const std::vector<std::string>& CharacterWarnings() const { return characterWarnings_; }
	const std::string& CharacterWeaponName() const { return characterWeaponName_; }
	const std::string& CharacterWeaponTag() const { return characterWeaponTag_; }
	DirectX::XMFLOAT3 CharacterWeaponTagWorld() const { return characterWeaponTagWorld_; }
	bool CharacterWeaponFound() const { return characterWeaponFound_; }
	const std::vector<DirectX::XMFLOAT4X4>& CharacterBodyPalette() const { return characterBodyPalette_; }

	// ---- hot reload (M10) ----
	bool HotReloadEnabled() const { return hotReloadEnabled_; }
	void SetHotReloadEnabled(bool enabled) { hotReloadEnabled_ = enabled; }
	int HotReloadCount() const { return hotReloadCount_; }
	bool HotReloadPending() const { return hotReloadPending_; }
	const std::string& HotReloadStatus() const { return hotReloadStatus_; }
	void PollHotReload();
	std::vector<std::string> WatchedFiles() const;

	// ---- bone rename map (M10) ----
	bool LoadRenameMap(const std::filesystem::path& path, std::string& error);
	void ClearRenameMap() { renameMap_.clear(); BuildPose(); }
	bool HasRenameMap() const { return !renameMap_.empty(); }
	const std::map<std::string, std::string>& RenameMap() const { return renameMap_; }

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

	// ---- camos ----
	const std::vector<std::string>& CamoNames();
	const std::string& CamoName() const { return camoName_; }
	const std::string& CamoTexture() const { return camoTextureName_; }
	bool SetCamo(const std::string& name, std::string& error);
	void ClearCamo() { camoName_.clear(); camoTextureName_.clear(); camoImageValid_ = false; ApplyCamo(); }
	float CamoStrength() const { return camoStrength_; }
	float CamoScale() const { return camoScale_; }
	void SetCamoStrength(float strength);
	void SetCamoScale(float scale);

	// ---- tag editing (weapon gun model or standalone model) ----
	bool HasEditableModel() const { return document_ != nullptr; }
	const assets::XModelDocument* Document() const { return document_.get(); }
	std::string EditableModelName() const;
	std::vector<TagEditInfo> EditableTags() const;
	bool HasTagChanges() const;
	bool MoveTagLocal(const std::string& tag, const DirectX::XMFLOAT3& localPos,
	                  const DirectX::XMFLOAT4& localQuat, std::string& error);
	bool MoveTagWorld(const std::string& tag, const DirectX::XMFLOAT3& worldPos,
	                  const DirectX::XMFLOAT4& worldQuat, std::string& error);
	bool AddTag(const std::string& tag, const std::string& parent, const std::string& copyFrom,
	            const DirectX::XMFLOAT3& localPos, const DirectX::XMFLOAT4& localQuat,
	            std::string& error);
	bool RenameTag(const std::string& oldName, const std::string& newName, std::string& error);
	bool RevertTag(const std::string& tag, std::string& error);
	void RevertAllTags();
	bool TagPoseWorld(const std::string& tag, DirectX::XMFLOAT4X4& out) const;
	bool TagGhostWorld(const std::string& tag, DirectX::XMFLOAT3& out) const;
	bool SaveEditedModel(ModelSaveResult& result, std::string& error);
	void SetAdsAutoAlign(bool enabled) { adsAutoAlign_ = enabled; }
	bool AdsAutoAlign() const { return adsAutoAlign_; }
	bool FitTagToReticle(const std::string& tag, const std::string& attachment,
	                     TagFitResult& result, std::string& error);

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
	void ClearEditableDocument();
	void LoadEditableDocumentForWeapon(const std::string& modelName);
	void LoadEditableDocumentForModel(const std::string& modelName);
	void SyncDocumentToScene();
	void ApplyCamo();
	void RebuildHiddenTags();	void UpdateAttachmentWorlds();
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
	bool LoadCharacterInternal(const assets::CharacterFaction& faction, const std::string& weapon,
	                           std::string& error);
	void RefreshWatchedFiles();
	void ReloadWatchedAsset();

	Settings& settings_;
	AssetIndex& assets_;
	Renderer& renderer_;
	std::vector<std::filesystem::path> roots_;

	Camera camera_;

	bool hasWeapon_ = false;
	bool worldModel_ = false;
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
	std::unique_ptr<assets::XModelDocument> document_;
	bool documentIsWeapon_ = false;
	bool adsAutoAlign_ = true;

	// character (body + optional head + world weapon on a hand tag)
	bool hasCharacter_ = false;
	assets::CharacterFaction characterFaction_;
	assets::ModelAsset characterBody_;
	assets::ModelAsset characterHead_;
	assets::ModelAsset characterWeaponModel_;
	bool characterHasHead_ = false;
	std::unique_ptr<GpuModel> gpuCharacterBody_;
	std::unique_ptr<GpuModel> gpuCharacterHead_;
	std::unique_ptr<GpuModel> gpuCharacterWeapon_;
	std::vector<DirectX::XMFLOAT4X4> characterBodyPalette_;
	std::vector<DirectX::XMFLOAT4X4> characterHeadPalette_;
	DirectX::XMFLOAT4X4 characterHeadWorld_{};
	DirectX::XMFLOAT4X4 characterWeaponWorld_{};
	std::string characterWeaponName_;
	std::string characterWeaponTag_;
	DirectX::XMFLOAT3 characterWeaponTagWorld_{};
	bool characterWeaponFound_ = false;
	std::vector<std::string> characterMissing_;
	std::vector<std::string> characterWarnings_;

	struct WatchedFile {
		std::filesystem::path path;
		std::filesystem::file_time_type time;
	};
	std::vector<WatchedFile> watchedFiles_;
	bool hotReloadEnabled_ = true;
	bool hotReloadPending_ = false;
	int hotReloadCount_ = 0;
	std::string hotReloadStatus_;
	std::chrono::steady_clock::time_point hotReloadDetected_{};
	std::map<std::string, std::string> renameMap_;

	std::vector<std::string> camoNames_;
	std::string camoName_;
	std::string camoTextureName_;
	assets::ImageData camoImage_;
	bool camoImageValid_ = false;
	float camoStrength_ = 1.0f;
	float camoScale_ = 1.0f;

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
                    AttachmentFitReport& out, std::string& error, bool world = false);
void PrintFitReport(const AttachmentFitReport& report, std::FILE* out);

// Direct mapping used by the Inspector for animOverrides[].animTreeType.
std::string AnimStateForTreeType(int type);

enum class CompareMode { SideBySide, Ghost };

struct CompareOptions {
	int width = 1600;
	int height = 900;
	CompareMode mode = CompareMode::SideBySide;
	float ghostAlpha = 0.45f;
	bool firstPerson = false;
	bool hasView = false;
	float yaw = 0.0f;
	float pitch = 0.0f;
	float distance = 0.0f;
	float fov = 0.0f;
	float exposure = 1.0f;
	bool world = false;
	std::string camo;
};

// Renders asset `a` and asset `b` with the same camera: side by side (one output image with a
// divider) or `b` ghosted over `a`. Weapon names load the weapon (gun/world model), other names
// load a standalone model. The workbench is left with `b` loaded.
bool RenderComparison(Workbench& workbench, Renderer& renderer, const std::string& a, const std::string& b,
                      const CompareOptions& options, const std::filesystem::path& output,
                      std::string& error);

// Loads a rename table JSON ({"old_anim_bone": "new_model_bone"}) and compares an anim's
// channels against a model's bones with that table applied.
bool LoadRenameTable(const std::filesystem::path& path, std::map<std::string, std::string>& table,
                     std::string& error);
bool CompareBones(const std::vector<std::filesystem::path>& roots, const std::string& animName,
                  const std::string& modelName, const std::map<std::string, std::string>& rename,
                  scene::BoneCompareResult& result, std::string& error);

} // namespace wb::commands
