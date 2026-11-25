#pragma once

#include <d3d11.h>
#include <DirectXMath.h>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include <wrl/client.h>

#include "assets/camo.h"
#include "assets/model_asset.h"
#include "render/camera.h"

namespace wb {

template <typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

struct RendererDesc {
	HWND hwnd = nullptr; // nullptr = headless: no swap chain, no window
	int width = 1600;
	int height = 900;
};

struct GpuSurface {
	ComPtr<ID3D11Buffer> vertexBuffer;
	ComPtr<ID3D11Buffer> indexBuffer;
	UINT indexCount = 0;
	int materialIndex = -1;
};

struct GpuMaterial {
	std::string name;
	ComPtr<ID3D11ShaderResourceView> albedo;
	ComPtr<ID3D11ShaderResourceView> normal;
	ComPtr<ID3D11ShaderResourceView> specular;
	ComPtr<ID3D11ShaderResourceView> camoMask;
	// Per-material native camo variant (H2 *_fire/*_doomsday materials). When
	// nativeStrength >= 0 this overrides the model-level camo for this material.
	ComPtr<ID3D11ShaderResourceView> nativeCamoColor;
	ComPtr<ID3D11ShaderResourceView> nativeCamoNormal;
	ComPtr<ID3D11ShaderResourceView> nativeCamoSpecular;
	ComPtr<ID3D11ShaderResourceView> nativeCamoEnvironment;
	ComPtr<ID3D11ShaderResourceView> nativeCamoEmissive;
	float nativeStrength = -1.0f;
	float camoUvScaleU = 1.0f;
	float camoUvScaleV = 1.0f;
	float nativeUvScaleU = 1.0f;
	float nativeUvScaleV = 1.0f;
	float envMapScale = 0.04f;
};

struct GpuModel {
	std::vector<GpuSurface> surfaces;
	std::vector<GpuMaterial> materials;
	// Optional camo layer: the selected camo design sampled with the model UVs and blended
	// over the base colour by the material's "m" mask (white when the material has none).
	ComPtr<ID3D11ShaderResourceView> camo;
	ComPtr<ID3D11ShaderResourceView> camoNormal;
	ComPtr<ID3D11ShaderResourceView> camoSpecular;
	ComPtr<ID3D11ShaderResourceView> camoEnvironment;
	float camoStrength = 0.0f;
	float camoScale = 1.0f; // user multiplier on the material's decalParams1 scale
	float camoEnvScale = 0.04f;
	DirectX::XMFLOAT3 boundsCenter{0.0f, 0.0f, 0.0f};
	DirectX::XMFLOAT3 boundsHalf{1.0f, 1.0f, 1.0f};
};

// Owns the D3D11 device, the grid pipeline and the model pipeline. The 3D scene is always
// rendered into an offscreen colour target, which is shown in the ImGui viewport or saved
// by the headless screenshot path.
class Renderer {
public:
	bool Initialize(const RendererDesc& desc);
	void Shutdown();

	bool ResizeSwapChain(int width, int height);

	// Scene pass (clears the target, then grid/model draws).
	bool BeginScene(int width, int height);
	void RenderGrid(const Camera& camera);
	void RenderModel(const GpuModel& model, const Camera& camera, bool wireframe,
	                 const std::vector<bool>* visibleSurfaces, float exposure = 1.0f,
	                 const DirectX::XMFLOAT4X4* world = nullptr,
	                 const std::vector<DirectX::XMFLOAT4X4>* bonePalette = nullptr,
	                 const DirectX::XMFLOAT4* tint = nullptr, bool onTop = false);
	bool SaveSceneToPng(const std::filesystem::path& path);

	// Copies the current scene target into CPU memory (BGRA/RGBA bytes as stored by D3D).
	bool CaptureScene(std::vector<std::uint8_t>& pixels, int& width, int& height, DXGI_FORMAT& format);
	// Copies the current scene target into a persistent SRV for the live split viewport.
	bool CaptureSceneTo(int slot);
	ID3D11ShaderResourceView* CompareTexture(int slot) const;
	bool SavePixelsToPng(const std::filesystem::path& path, const std::vector<std::uint8_t>& pixels,
	                     int width, int height, DXGI_FORMAT format);

	std::unique_ptr<GpuModel> UploadModel(const assets::ModelAsset& model, std::string& error);

	// Sets/clears the camo layer on an uploaded model (used by the weapon and the
	// standalone model; the same camo design is shared by all surfaces of a model).
	bool SetModelCamo(GpuModel& model, const assets::CamoTextures& textures, float strength, float scale,
	                  std::string& error);
	// Applies a native material variant only to the materials that match its base stem
	// (e.g. native:mtl_h2_m4a1_receiver_fire -> all receiver_camo surfaces).
	bool SetModelNativeCamo(GpuModel& model, const std::string& variantStem,
	                        const assets::CamoTextures& textures, float strength, float scale,
	                        std::string& error);
	void ClearModelCamo(GpuModel& model);

	void Present();
	void ClearBackBuffer(const float color[4]);

	ID3D11Device* Device() const { return device_.Get(); }
	ID3D11DeviceContext* Context() const { return context_.Get(); }
	ID3D11ShaderResourceView* SceneTexture() const { return sceneSrv_.Get(); }
	ID3D11RenderTargetView* BackBufferRtv() const { return backBufferRtv_.Get(); }
	int SceneWidth() const { return sceneWidth_; }
	int SceneHeight() const { return sceneHeight_; }
	bool IsOffscreen() const { return swapChain_ == nullptr; }

private:
	bool CreateDeviceAndSwapChain(HWND hwnd, int width, int height);
	bool CreateSwapChainTargets();
	bool CreateGridResources();
	bool CreateModelResources();
	bool CreateDefaultTextures();
	bool EnsureSceneTarget(int width, int height);
	void ReleaseSceneTarget();
	ComPtr<ID3D11ShaderResourceView> CreateTexture(const assets::ImageData& image, bool srgb, std::string& error);

	ComPtr<ID3D11Device> device_;
	ComPtr<ID3D11DeviceContext> context_;
	ComPtr<IDXGISwapChain> swapChain_;
	ComPtr<ID3D11RenderTargetView> backBufferRtv_;

	ComPtr<ID3D11Texture2D> sceneTexture_;
	ComPtr<ID3D11RenderTargetView> sceneRtv_;
	ComPtr<ID3D11ShaderResourceView> sceneSrv_;
	ComPtr<ID3D11Texture2D> depthTexture_;
	ComPtr<ID3D11DepthStencilView> depthDsv_;
	int sceneWidth_ = 0;
	int sceneHeight_ = 0;
	ComPtr<ID3D11Texture2D> compareTexture_[2];
	ComPtr<ID3D11ShaderResourceView> compareSrv_[2];
	int compareWidth_[2] = {0, 0};
	int compareHeight_[2] = {0, 0};
	DXGI_FORMAT compareFormat_[2] = {DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN};

	ComPtr<ID3D11VertexShader> gridVs_;
	ComPtr<ID3D11PixelShader> gridPs_;
	ComPtr<ID3D11InputLayout> gridInputLayout_;
	ComPtr<ID3D11Buffer> gridVertexBuffer_;
	ComPtr<ID3D11Buffer> gridConstantBuffer_;
	UINT gridVertexCount_ = 0;
	ComPtr<ID3D11DepthStencilState> gridDepthState_;

	ComPtr<ID3D11VertexShader> modelVs_;
	ComPtr<ID3D11PixelShader> modelPs_;
	ComPtr<ID3D11InputLayout> modelInputLayout_;
	ComPtr<ID3D11Buffer> modelConstantBuffer_;
	ComPtr<ID3D11Buffer> boneConstantBuffer_;
	ComPtr<ID3D11Buffer> camoConstantBuffer_;
	ComPtr<ID3D11RasterizerState> solidState_;
	ComPtr<ID3D11RasterizerState> wireframeState_;
	ComPtr<ID3D11DepthStencilState> overlayDepthState_;
	ComPtr<ID3D11SamplerState> samplerState_;
	ComPtr<ID3D11ShaderResourceView> checkerTexture_;
	ComPtr<ID3D11ShaderResourceView> flatNormalTexture_;
	ComPtr<ID3D11ShaderResourceView> blackTexture_;
	ComPtr<ID3D11ShaderResourceView> whiteTexture_;
};

} // namespace wb
