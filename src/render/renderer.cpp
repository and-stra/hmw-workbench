#include "render/renderer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <vector>

#include <d3dcompiler.h>
#include <DirectXTex.h>

namespace wb {
namespace {

constexpr char kGridShader[] = R"(
cbuffer SceneConstants : register(b0)
{
    row_major float4x4 gViewProjection;
};

struct VSInput
{
    float4 position : POSITION;
    float4 color : COLOR;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

PSInput VSMain(VSInput input)
{
    PSInput output;
    output.position = mul(input.position, gViewProjection);
    output.color = input.color;
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return input.color;
}
)";

struct GridVertex {
	DirectX::XMFLOAT4 position; // xyz used, w must be 1
	DirectX::XMFLOAT4 color;
};

bool CompileShader(const char* entryPoint, const char* target, ID3DBlob** blob) {
	UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
	flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
	const HRESULT hr = D3DCompile(kGridShader, sizeof(kGridShader) - 1, "grid.hlsl", nullptr, nullptr,
	                              entryPoint, target, flags, 0, blob, &errorBlob);
	if (FAILED(hr)) {
		const char* message = errorBlob ? static_cast<const char*>(errorBlob->GetBufferPointer()) : "unknown error";
		std::fprintf(stderr, "grid shader (%s) failed: %s\n", entryPoint, message);
		return false;
	}
	if (errorBlob && errorBlob->GetBufferSize() > 0) {
		std::fprintf(stderr, "grid shader (%s) warnings: %s\n", entryPoint,
		             static_cast<const char*>(errorBlob->GetBufferPointer()));
	}
	return true;
}

void AddLine(std::vector<GridVertex>& vertices, const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b,
             const DirectX::XMFLOAT4& color) {
	vertices.push_back({DirectX::XMFLOAT4(a.x, a.y, a.z, 1.0f), color});
	vertices.push_back({DirectX::XMFLOAT4(b.x, b.y, b.z, 1.0f), color});
}

} // namespace

bool Renderer::Initialize(const RendererDesc& desc) {
	if (!CreateDeviceAndSwapChain(desc.hwnd, desc.width, desc.height)) {
		return false;
	}
	if (!CreateGridResources()) {
		return false;
	}
	if (!swapChain_ && !EnsureSceneTarget(desc.width, desc.height)) {
		return false;
	}
	return true;
}

void Renderer::Shutdown() {
	ReleaseSceneTarget();
	gridConstantBuffer_.Reset();
	gridVertexBuffer_.Reset();
	gridInputLayout_.Reset();
	gridPs_.Reset();
	gridVs_.Reset();
	backBufferRtv_.Reset();
	swapChain_.Reset();
	if (context_) {
		context_->ClearState();
		context_->Flush();
	}
	context_.Reset();
	device_.Reset();
}

bool Renderer::CreateDeviceAndSwapChain(HWND hwnd, int width, int height) {
	const D3D_FEATURE_LEVEL requestedLevels[] = {
	    D3D_FEATURE_LEVEL_11_0,
	    D3D_FEATURE_LEVEL_10_1,
	    D3D_FEATURE_LEVEL_10_0,
	};
	const UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

	HRESULT hr = S_OK;
	if (hwnd != nullptr) {
		DXGI_SWAP_CHAIN_DESC desc = {};
		desc.BufferCount = 2;
		desc.BufferDesc.Width = static_cast<UINT>(std::max(width, 1));
		desc.BufferDesc.Height = static_cast<UINT>(std::max(height, 1));
		desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		desc.OutputWindow = hwnd;
		desc.SampleDesc.Count = 1;
		desc.Windowed = TRUE;
		desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

		hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, requestedLevels,
		                                   static_cast<UINT>(std::size(requestedLevels)), D3D11_SDK_VERSION,
		                                   &desc, &swapChain_, &device_, nullptr, &context_);
		if (FAILED(hr)) {
			std::fprintf(stderr, "D3D11CreateDeviceAndSwapChain failed: 0x%08lX\n",
			             static_cast<unsigned long>(hr));
			return false;
		}
		if (!CreateSwapChainTargets()) {
			return false;
		}
	} else {
		hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, requestedLevels,
		                       static_cast<UINT>(std::size(requestedLevels)), D3D11_SDK_VERSION, &device_, nullptr,
		                       &context_);
		if (FAILED(hr)) {
			std::fprintf(stderr, "D3D11CreateDevice failed: 0x%08lX\n", static_cast<unsigned long>(hr));
			return false;
		}
	}

	return true;
}

bool Renderer::CreateSwapChainTargets() {
	if (!swapChain_) {
		return true;
	}
	Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
	HRESULT hr = swapChain_->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
	if (FAILED(hr)) {
		std::fprintf(stderr, "swap chain GetBuffer failed: 0x%08lX\n", static_cast<unsigned long>(hr));
		return false;
	}
	hr = device_->CreateRenderTargetView(backBuffer.Get(), nullptr, &backBufferRtv_);
	if (FAILED(hr)) {
		std::fprintf(stderr, "CreateRenderTargetView (back buffer) failed: 0x%08lX\n",
		             static_cast<unsigned long>(hr));
		return false;
	}
	return true;
}

bool Renderer::ResizeSwapChain(int width, int height) {
	if (!swapChain_) {
		return true;
	}
	if (width <= 0 || height <= 0) {
		return true;
	}

	context_->OMSetRenderTargets(0, nullptr, nullptr);
	backBufferRtv_.Reset();

	const HRESULT hr = swapChain_->ResizeBuffers(0, static_cast<UINT>(width), static_cast<UINT>(height),
	                                             DXGI_FORMAT_UNKNOWN, 0);
	if (FAILED(hr)) {
		std::fprintf(stderr, "ResizeBuffers failed: 0x%08lX\n", static_cast<unsigned long>(hr));
		return false;
	}
	return CreateSwapChainTargets();
}

bool Renderer::CreateGridResources() {
	Microsoft::WRL::ComPtr<ID3DBlob> vsBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> psBlob;
	if (!CompileShader("VSMain", "vs_5_0", &vsBlob) || !CompileShader("PSMain", "ps_5_0", &psBlob)) {
		return false;
	}

	HRESULT hr = device_->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr,
	                                         &gridVs_);
	if (FAILED(hr)) {
		return false;
	}
	hr = device_->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &gridPs_);
	if (FAILED(hr)) {
		return false;
	}

	const D3D11_INPUT_ELEMENT_DESC layout[] = {
	    {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
	    {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0},
	};
	hr = device_->CreateInputLayout(layout, static_cast<UINT>(std::size(layout)), vsBlob->GetBufferPointer(),
	                                vsBlob->GetBufferSize(), &gridInputLayout_);
	if (FAILED(hr)) {
		return false;
	}

	// Grid on the XZ plane plus axes: red = X, green = Y, blue = Z.
	const DirectX::XMFLOAT4 minorColor(0.20f, 0.21f, 0.245f, 1.0f);
	const DirectX::XMFLOAT4 majorColor(0.27f, 0.29f, 0.33f, 1.0f);
	const DirectX::XMFLOAT4 xAxisColor(0.72f, 0.24f, 0.24f, 1.0f);
	const DirectX::XMFLOAT4 yAxisColor(0.28f, 0.70f, 0.30f, 1.0f);
	const DirectX::XMFLOAT4 zAxisColor(0.24f, 0.42f, 0.78f, 1.0f);

	constexpr int halfExtent = 50;
	std::vector<GridVertex> vertices;
	vertices.reserve(static_cast<size_t>(halfExtent * 8 + 16));

	for (int i = -halfExtent; i <= halfExtent; ++i) {
		const float offset = static_cast<float>(i);
		const bool isAxis = i == 0;
		const bool isMajor = i % 5 == 0;
		const DirectX::XMFLOAT4 color = isMajor ? majorColor : minorColor;

		if (!isAxis) {
			AddLine(vertices, DirectX::XMFLOAT3(offset, 0.0f, -static_cast<float>(halfExtent)),
			        DirectX::XMFLOAT3(offset, 0.0f, static_cast<float>(halfExtent)), color);
			AddLine(vertices, DirectX::XMFLOAT3(-static_cast<float>(halfExtent), 0.0f, offset),
			        DirectX::XMFLOAT3(static_cast<float>(halfExtent), 0.0f, offset), color);
		}
	}

	AddLine(vertices, DirectX::XMFLOAT3(-static_cast<float>(halfExtent), 0.0f, 0.0f),
	        DirectX::XMFLOAT3(static_cast<float>(halfExtent), 0.0f, 0.0f), xAxisColor);
	AddLine(vertices, DirectX::XMFLOAT3(0.0f, 0.0f, -static_cast<float>(halfExtent)),
	        DirectX::XMFLOAT3(0.0f, 0.0f, static_cast<float>(halfExtent)), zAxisColor);
	AddLine(vertices, DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f), DirectX::XMFLOAT3(0.0f, 5.0f, 0.0f), yAxisColor);

	gridVertexCount_ = static_cast<UINT>(vertices.size());

	D3D11_BUFFER_DESC vertexDesc = {};
	vertexDesc.ByteWidth = static_cast<UINT>(vertices.size() * sizeof(GridVertex));
	vertexDesc.Usage = D3D11_USAGE_IMMUTABLE;
	vertexDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	D3D11_SUBRESOURCE_DATA vertexData = {};
	vertexData.pSysMem = vertices.data();
	hr = device_->CreateBuffer(&vertexDesc, &vertexData, &gridVertexBuffer_);
	if (FAILED(hr)) {
		return false;
	}

	D3D11_BUFFER_DESC constantDesc = {};
	constantDesc.ByteWidth = sizeof(DirectX::XMFLOAT4X4);
	constantDesc.Usage = D3D11_USAGE_DYNAMIC;
	constantDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	constantDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	hr = device_->CreateBuffer(&constantDesc, nullptr, &gridConstantBuffer_);
	if (FAILED(hr)) {
		return false;
	}

	return true;
}

bool Renderer::EnsureSceneTarget(int width, int height) {
	width = std::max(width, 1);
	height = std::max(height, 1);
	if (sceneTexture_ && sceneWidth_ == width && sceneHeight_ == height) {
		return true;
	}
	ReleaseSceneTarget();

	D3D11_TEXTURE2D_DESC colorDesc = {};
	colorDesc.Width = static_cast<UINT>(width);
	colorDesc.Height = static_cast<UINT>(height);
	colorDesc.MipLevels = 1;
	colorDesc.ArraySize = 1;
	colorDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	colorDesc.SampleDesc.Count = 1;
	colorDesc.Usage = D3D11_USAGE_DEFAULT;
	colorDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	HRESULT hr = device_->CreateTexture2D(&colorDesc, nullptr, &sceneTexture_);
	if (FAILED(hr)) {
		std::fprintf(stderr, "CreateTexture2D (scene) failed: 0x%08lX\n", static_cast<unsigned long>(hr));
		return false;
	}
	hr = device_->CreateRenderTargetView(sceneTexture_.Get(), nullptr, &sceneRtv_);
	if (FAILED(hr)) {
		return false;
	}
	hr = device_->CreateShaderResourceView(sceneTexture_.Get(), nullptr, &sceneSrv_);
	if (FAILED(hr)) {
		return false;
	}

	D3D11_TEXTURE2D_DESC depthDesc = {};
	depthDesc.Width = static_cast<UINT>(width);
	depthDesc.Height = static_cast<UINT>(height);
	depthDesc.MipLevels = 1;
	depthDesc.ArraySize = 1;
	depthDesc.Format = DXGI_FORMAT_D32_FLOAT;
	depthDesc.SampleDesc.Count = 1;
	depthDesc.Usage = D3D11_USAGE_DEFAULT;
	depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	hr = device_->CreateTexture2D(&depthDesc, nullptr, &depthTexture_);
	if (FAILED(hr)) {
		return false;
	}
	hr = device_->CreateDepthStencilView(depthTexture_.Get(), nullptr, &depthDsv_);
	if (FAILED(hr)) {
		return false;
	}

	sceneWidth_ = width;
	sceneHeight_ = height;
	return true;
}

void Renderer::ReleaseSceneTarget() {
	depthDsv_.Reset();
	depthTexture_.Reset();
	sceneSrv_.Reset();
	sceneRtv_.Reset();
	sceneTexture_.Reset();
	sceneWidth_ = 0;
	sceneHeight_ = 0;
}

bool Renderer::RenderGrid(const DirectX::XMFLOAT4X4& viewProjection, int width, int height) {
	if (!EnsureSceneTarget(width, height)) {
		return false;
	}

	const float clearColor[4] = {0.105f, 0.115f, 0.135f, 1.0f};
	context_->ClearRenderTargetView(sceneRtv_.Get(), clearColor);
	context_->ClearDepthStencilView(depthDsv_.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

	ID3D11RenderTargetView* renderTargets[] = {sceneRtv_.Get()};
	context_->OMSetRenderTargets(1, renderTargets, depthDsv_.Get());
	context_->OMSetDepthStencilState(nullptr, 0);
	context_->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);

	D3D11_VIEWPORT viewport = {};
	viewport.Width = static_cast<float>(sceneWidth_);
	viewport.Height = static_cast<float>(sceneHeight_);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	context_->RSSetViewports(1, &viewport);

	D3D11_MAPPED_SUBRESOURCE mapped = {};
	if (FAILED(context_->Map(gridConstantBuffer_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
		return false;
	}
	std::memcpy(mapped.pData, &viewProjection, sizeof(DirectX::XMFLOAT4X4));
	context_->Unmap(gridConstantBuffer_.Get(), 0);

	const UINT stride = sizeof(GridVertex);
	const UINT offset = 0;
	ID3D11Buffer* vertexBuffer = gridVertexBuffer_.Get();
	ID3D11Buffer* constantBuffer = gridConstantBuffer_.Get();

	context_->IASetInputLayout(gridInputLayout_.Get());
	context_->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
	context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
	context_->VSSetShader(gridVs_.Get(), nullptr, 0);
	context_->VSSetConstantBuffers(0, 1, &constantBuffer);
	context_->PSSetShader(gridPs_.Get(), nullptr, 0);
	context_->Draw(gridVertexCount_, 0);
	return true;
}

bool Renderer::SaveSceneToPng(const std::filesystem::path& path) {
	if (!sceneTexture_) {
		std::fprintf(stderr, "screenshot: no rendered scene\n");
		return false;
	}

	D3D11_TEXTURE2D_DESC desc = {};
	sceneTexture_->GetDesc(&desc);
	desc.Usage = D3D11_USAGE_STAGING;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	desc.BindFlags = 0;
	desc.MiscFlags = 0;

	Microsoft::WRL::ComPtr<ID3D11Texture2D> staging;
	HRESULT hr = device_->CreateTexture2D(&desc, nullptr, &staging);
	if (FAILED(hr)) {
		std::fprintf(stderr, "screenshot: staging texture failed: 0x%08lX\n", static_cast<unsigned long>(hr));
		return false;
	}

	context_->CopyResource(staging.Get(), sceneTexture_.Get());

	D3D11_MAPPED_SUBRESOURCE mapped = {};
	hr = context_->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped);
	if (FAILED(hr)) {
		std::fprintf(stderr, "screenshot: map failed: 0x%08lX\n", static_cast<unsigned long>(hr));
		return false;
	}

	DirectX::Image image = {};
	image.width = desc.Width;
	image.height = desc.Height;
	image.format = desc.Format;
	image.rowPitch = mapped.RowPitch;
	image.slicePitch = static_cast<size_t>(mapped.RowPitch) * desc.Height;
	image.pixels = static_cast<uint8_t*>(mapped.pData);

	hr = DirectX::SaveToWICFile(image, DirectX::WIC_FLAGS_NONE, DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG),
	                            path.c_str());
	context_->Unmap(staging.Get(), 0);

	if (FAILED(hr)) {
		std::fprintf(stderr, "screenshot: SaveToWICFile failed: 0x%08lX\n", static_cast<unsigned long>(hr));
		return false;
	}
	return true;
}

void Renderer::ClearBackBuffer(const float color[4]) {
	if (!backBufferRtv_) {
		return;
	}
	ID3D11RenderTargetView* renderTargets[] = {backBufferRtv_.Get()};
	context_->OMSetRenderTargets(1, renderTargets, nullptr);
	context_->ClearRenderTargetView(backBufferRtv_.Get(), color);
}

void Renderer::Present() {
	if (swapChain_) {
		swapChain_->Present(1, 0);
	}
}

} // namespace wb
