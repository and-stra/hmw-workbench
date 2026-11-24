#pragma once

#include <d3d11.h>
#include <DirectXMath.h>
#include <filesystem>
#include <wrl/client.h>

namespace wb {

template <typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

struct RendererDesc {
	HWND hwnd = nullptr; // nullptr = headless: no swap chain, no window
	int width = 1600;
	int height = 900;
};

// Owns the D3D11 device and the grid pipeline. The 3D scene is always rendered into an
// offscreen colour target (scene texture), which is either shown in the ImGui viewport
// or saved by the headless screenshot path.
class Renderer {
public:
	bool Initialize(const RendererDesc& desc);
	void Shutdown();

	bool ResizeSwapChain(int width, int height);
	bool RenderGrid(const DirectX::XMFLOAT4X4& viewProjection, int width, int height);
	bool SaveSceneToPng(const std::filesystem::path& path);

	void Present();
	void ClearBackBuffer(const float color[4]);

	ID3D11Device* Device() const { return device_.Get(); }
	ID3D11DeviceContext* Context() const { return context_.Get(); }
	ID3D11ShaderResourceView* SceneTexture() const { return sceneSrv_.Get(); }
	ID3D11RenderTargetView* BackBufferRtv() const { return backBufferRtv_.Get(); }
	bool IsOffscreen() const { return swapChain_ == nullptr; }

private:
	bool CreateDeviceAndSwapChain(HWND hwnd, int width, int height);
	bool CreateSwapChainTargets();
	bool CreateGridResources();
	bool EnsureSceneTarget(int width, int height);
	void ReleaseSceneTarget();

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

	ComPtr<ID3D11VertexShader> gridVs_;
	ComPtr<ID3D11PixelShader> gridPs_;
	ComPtr<ID3D11InputLayout> gridInputLayout_;
	ComPtr<ID3D11Buffer> gridVertexBuffer_;
	ComPtr<ID3D11Buffer> gridConstantBuffer_;
	UINT gridVertexCount_ = 0;
};

} // namespace wb
