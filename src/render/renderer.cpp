#include "render/renderer.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <vector>

#include <d3dcompiler.h>
#include <DirectXTex.h>

#include "render/camera.h"

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

constexpr char kModelShader[] = R"(
cbuffer ModelConstants : register(b0)
{
    row_major float4x4 gViewProjection;
    row_major float4x4 gWorld;
    float4 gCameraPosition;
    float4 gLightDirection;
    float4 gParams;
    float4 gTint;
};

cbuffer CamoConstants : register(b2)
{
    // x strength (0 = off), y/z decalParams1 UV scale, w envMapParms scale
    float4 gSurfaceCamo;
};

#define MAX_BONES 256
cbuffer BoneConstants : register(b1)
{
    row_major float4x4 gBones[MAX_BONES];
};

Texture2D gAlbedo : register(t0);
Texture2D gNormalMap : register(t1);
Texture2D gSpecMap : register(t2);
Texture2D gCamo : register(t3);
Texture2D gCamoMask : register(t4);
Texture2D gCamoNormal : register(t5);
Texture2D gCamoSpec : register(t6);
Texture2D gCamoEnv : register(t7);
Texture2D gCamoEmissive : register(t8);
SamplerState gLinear : register(s0);

struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float binormalSign : BINORMAL;
    float2 uv : TEXCOORD0;
    uint4 boneIndices : BLENDINDICES;
    float4 boneWeights : BLENDWEIGHT;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float3 worldPosition : TEXCOORD0;
    float3 normal : TEXCOORD1;
    float3 tangent : TEXCOORD2;
    float3 bitangent : TEXCOORD3;
    float2 uv : TEXCOORD4;
};

PSInput VSMain(VSInput input)
{
    PSInput output;

    float weightSum = dot(input.boneWeights, float4(1.0f, 1.0f, 1.0f, 1.0f));
    if (weightSum < 1.0e-5f)
    {
        input.boneWeights = float4(1.0f, 0.0f, 0.0f, 0.0f);
        input.boneIndices = uint4(0u, 0u, 0u, 0u);
        weightSum = 1.0f;
    }

    float4 skinnedPosition = float4(0.0f, 0.0f, 0.0f, 0.0f);
    float3 skinnedNormal = float3(0.0f, 0.0f, 0.0f);
    float3 skinnedTangent = float3(0.0f, 0.0f, 0.0f);
    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        if (input.boneWeights[i] <= 0.0f)
        {
            continue;
        }
        const uint bone = min(input.boneIndices[i], MAX_BONES - 1);
        const float4x4 boneMatrix = gBones[bone];
        const float weight = input.boneWeights[i] / weightSum;
        skinnedPosition += mul(float4(input.position, 1.0f), boneMatrix) * weight;
        skinnedNormal += mul(input.normal, (float3x3)boneMatrix) * weight;
        skinnedTangent += mul(input.tangent, (float3x3)boneMatrix) * weight;
    }
    skinnedPosition.w = 1.0f;

    output.position = mul(mul(skinnedPosition, gWorld), gViewProjection);
    output.worldPosition = mul(skinnedPosition, gWorld).xyz;

    float3 n = normalize(mul(skinnedNormal, (float3x3)gWorld));
    float3 t = mul(skinnedTangent, (float3x3)gWorld);
    if (length(t) < 1.0e-5f)
    {
        t = (abs(n.z) < 0.9f) ? cross(n, float3(0.0f, 0.0f, 1.0f)) : cross(n, float3(0.0f, 1.0f, 0.0f));
    }
    t = normalize(t);
    float signValue = input.binormalSign < 0.0f ? -1.0f : 1.0f;

    output.normal = n;
    output.tangent = t;
    output.bitangent = cross(n, t) * signValue;
    output.uv = input.uv;
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    float3 albedo = gAlbedo.Sample(gLinear, input.uv).rgb;
    float2 normalXY = gNormalMap.Sample(gLinear, input.uv).xy * 2.0f - 1.0f;
    float specularMask = gSpecMap.Sample(gLinear, input.uv).r;
    float camoMask = 0.0f;

    // Full camo technique: the design colour, its normal and specular replace the base
    // maps where the material mask is white. The design is sampled with the material's
    // decalParams1 scale. gSurfaceCamo.x is 0 when no camo is selected, so the output is
    // then unchanged.
    if (gSurfaceCamo.x > 0.0f)
    {
        const float2 camoUv = input.uv * gSurfaceCamo.yz;
        camoMask = saturate(gCamoMask.Sample(gLinear, input.uv).r * gSurfaceCamo.x);
        const float3 camoColor = gCamo.Sample(gLinear, camoUv).rgb;
        const float2 camoNormalXY = gCamoNormal.Sample(gLinear, camoUv).xy * 2.0f - 1.0f;
        const float camoSpecularMask = gCamoSpec.Sample(gLinear, camoUv).r;
        albedo = lerp(albedo, camoColor, camoMask);
        normalXY = lerp(normalXY, camoNormalXY, camoMask);
        specularMask = lerp(specularMask, camoSpecularMask, camoMask);
    }

    float normalZ = sqrt(saturate(1.0f - dot(normalXY, normalXY)));
    float3 n = normalize(input.tangent * normalXY.x + input.bitangent * normalXY.y + input.normal * normalZ);

    float3 lightDirection = normalize(gLightDirection.xyz);
    float3 viewDirection = normalize(gCameraPosition.xyz - input.worldPosition);
    float3 halfVector = normalize(lightDirection + viewDirection);

    float ndl = saturate(dot(n, lightDirection));
    float specular = pow(saturate(dot(n, halfVector)), gParams.x) * (0.06f + specularMask * 0.94f);

    // Environment term: the material's env LUT (BRDF) scaled by envMapParms.x. There is no
    // cubemap in the material dumps, so use a neutral sky gradient as the environment colour.
    float3 environment = float3(0.0f, 0.0f, 0.0f);
    float3 emissive = float3(0.0f, 0.0f, 0.0f);
    if (gSurfaceCamo.x > 0.0f)
    {
        const float NdotV = saturate(dot(n, viewDirection));
        const float3 envBrdf = gCamoEnv.Sample(gLinear, float2(NdotV, 0.5f)).rgb;
        const float3 envColor = lerp(float3(0.22f, 0.26f, 0.34f), float3(0.05f, 0.06f, 0.08f),
                                     saturate(n.z * 0.5f + 0.5f));
        environment = envColor * envBrdf.x * gSurfaceCamo.w * 4.0f * camoMask;
        // H2 native variants (fire/doomsday) put the animated design in the emissive map.
        emissive = gCamoEmissive.Sample(gLinear, input.uv * gSurfaceCamo.yz).rgb * camoMask * 2.0f;
    }

    // Linear lighting, then exposure and gamma encoding into the UNORM scene target.
    float3 linearColor = albedo * (0.65f + 1.15f * ndl) + specular * float3(0.9f, 0.93f, 0.98f) +
                         environment + emissive;
    linearColor *= gParams.y;
    linearColor = pow(saturate(linearColor), 1.0f / 2.2f);
    return float4(linearColor * gTint.rgb, 1.0f);
}
)";

struct GridVertex {
	DirectX::XMFLOAT4 position;
	DirectX::XMFLOAT4 color;
};

struct alignas(16) ModelConstants {
	DirectX::XMFLOAT4X4 viewProjection;
	DirectX::XMFLOAT4X4 world;
	DirectX::XMFLOAT4 cameraPosition;
	DirectX::XMFLOAT4 lightDirection;
	DirectX::XMFLOAT4 params;
	DirectX::XMFLOAT4 tint;
};

struct alignas(16) CamoConstants {
	DirectX::XMFLOAT4 surfaceCamo;
};

static_assert(sizeof(assets::SurfaceVertex) == 72);
static_assert(sizeof(ModelConstants) == 192);
static_assert(sizeof(CamoConstants) == 16);
constexpr UINT kMaxBones = 256;

bool CompileShader(const char* source, const char* entryPoint, const char* target, ID3DBlob** blob) {
	UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
	flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
	const HRESULT hr = D3DCompile(source, std::strlen(source), "workbench.hlsl", nullptr, nullptr, entryPoint,
	                              target, flags, 0, blob, &errorBlob);
	if (FAILED(hr)) {
		const char* message = errorBlob ? static_cast<const char*>(errorBlob->GetBufferPointer()) : "unknown error";
		std::fprintf(stderr, "shader (%s) failed: %s\n", entryPoint, message);
		return false;
	}
	if (errorBlob && errorBlob->GetBufferSize() > 0) {
		std::fprintf(stderr, "shader (%s) warnings: %s\n", entryPoint,
		             static_cast<const char*>(errorBlob->GetBufferPointer()));
	}
	return true;
}

void AddLine(std::vector<GridVertex>& vertices, const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b,
             const DirectX::XMFLOAT4& color) {
	vertices.push_back({DirectX::XMFLOAT4(a.x, a.y, a.z, 1.0f), color});
	vertices.push_back({DirectX::XMFLOAT4(b.x, b.y, b.z, 1.0f), color});
}

void AddGridLevel(std::vector<GridVertex>& vertices, int step, int halfExtent,
                  const DirectX::XMFLOAT4& color) {
	const float extent = static_cast<float>(halfExtent);
	for (int i = -halfExtent; i <= halfExtent; i += step) {
		if (i == 0) {
			continue; // axes are drawn separately
		}
		const float offset = static_cast<float>(i);
		AddLine(vertices, DirectX::XMFLOAT3(offset, -extent, 0.0f), DirectX::XMFLOAT3(offset, extent, 0.0f), color);
		AddLine(vertices, DirectX::XMFLOAT3(-extent, offset, 0.0f), DirectX::XMFLOAT3(extent, offset, 0.0f), color);
	}
}

} // namespace

namespace {

DXGI_FORMAT ToSrgbFormat(DXGI_FORMAT format) {
	switch (format) {
	case DXGI_FORMAT_BC1_UNORM:
		return DXGI_FORMAT_BC1_UNORM_SRGB;
	case DXGI_FORMAT_BC2_UNORM:
		return DXGI_FORMAT_BC2_UNORM_SRGB;
	case DXGI_FORMAT_BC3_UNORM:
		return DXGI_FORMAT_BC3_UNORM_SRGB;
	case DXGI_FORMAT_BC7_UNORM:
		return DXGI_FORMAT_BC7_UNORM_SRGB;
	case DXGI_FORMAT_R8G8B8A8_UNORM:
		return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	case DXGI_FORMAT_B8G8R8A8_UNORM:
		return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
	default:
		return format;
	}
}

} // namespace

bool Renderer::Initialize(const RendererDesc& desc) {
	if (!CreateDeviceAndSwapChain(desc.hwnd, desc.width, desc.height)) {
		return false;
	}
	if (!CreateGridResources() || !CreateModelResources()) {
		return false;
	}
	if (!swapChain_ && !EnsureSceneTarget(desc.width, desc.height)) {
		return false;
	}
	return true;
}

void Renderer::Shutdown() {
	ReleaseSceneTarget();
	blackTexture_.Reset();
	whiteTexture_.Reset();
	flatNormalTexture_.Reset();
	checkerTexture_.Reset();
	samplerState_.Reset();
	wireframeState_.Reset();
	solidState_.Reset();
	overlayDepthState_.Reset();
	modelConstantBuffer_.Reset();
	modelInputLayout_.Reset();
	modelPs_.Reset();
	modelVs_.Reset();
	gridDepthState_.Reset();
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
	if (!CompileShader(kGridShader, "VSMain", "vs_5_0", &vsBlob) ||
	    !CompileShader(kGridShader, "PSMain", "ps_5_0", &psBlob)) {
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

	// Grid on the XY plane (game space is Z-up) with 1/10/100 unit steps.
	const DirectX::XMFLOAT4 color1(0.285f, 0.305f, 0.35f, 1.0f);
	const DirectX::XMFLOAT4 color10(0.215f, 0.23f, 0.27f, 1.0f);
	const DirectX::XMFLOAT4 color100(0.165f, 0.175f, 0.205f, 1.0f);
	const DirectX::XMFLOAT4 xAxisColor(0.72f, 0.24f, 0.24f, 1.0f);
	const DirectX::XMFLOAT4 yAxisColor(0.28f, 0.70f, 0.30f, 1.0f);
	const DirectX::XMFLOAT4 zAxisColor(0.24f, 0.42f, 0.90f, 1.0f);

	std::vector<GridVertex> vertices;
	vertices.reserve(3 * 101 * 4 + 16);
	AddGridLevel(vertices, 100, 5000, color100);
	AddGridLevel(vertices, 10, 500, color10);
	AddGridLevel(vertices, 1, 50, color1);

	constexpr float axisExtent = 50.0f;
	AddLine(vertices, DirectX::XMFLOAT3(-axisExtent, 0.0f, 0.0f), DirectX::XMFLOAT3(axisExtent, 0.0f, 0.0f),
	        xAxisColor);
	AddLine(vertices, DirectX::XMFLOAT3(0.0f, -axisExtent, 0.0f), DirectX::XMFLOAT3(0.0f, axisExtent, 0.0f),
	        yAxisColor);
	AddLine(vertices, DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f), DirectX::XMFLOAT3(0.0f, 0.0f, 5.0f), zAxisColor);

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

	D3D11_DEPTH_STENCIL_DESC depthDesc = {};
	depthDesc.DepthEnable = FALSE;
	depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	depthDesc.DepthFunc = D3D11_COMPARISON_NEVER;
	hr = device_->CreateDepthStencilState(&depthDesc, &gridDepthState_);
	if (FAILED(hr)) {
		return false;
	}

	return true;
}

bool Renderer::CreateModelResources() {
	Microsoft::WRL::ComPtr<ID3DBlob> vsBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> psBlob;
	if (!CompileShader(kModelShader, "VSMain", "vs_5_0", &vsBlob) ||
	    !CompileShader(kModelShader, "PSMain", "ps_5_0", &psBlob)) {
		return false;
	}

	HRESULT hr = device_->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr,
	                                         &modelVs_);
	if (FAILED(hr)) {
		return false;
	}
	hr = device_->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &modelPs_);
	if (FAILED(hr)) {
		return false;
	}

	const D3D11_INPUT_ELEMENT_DESC layout[] = {
	    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(assets::SurfaceVertex, position),
	     D3D11_INPUT_PER_VERTEX_DATA, 0},
	    {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(assets::SurfaceVertex, normal),
	     D3D11_INPUT_PER_VERTEX_DATA, 0},
	    {"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(assets::SurfaceVertex, tangent),
	     D3D11_INPUT_PER_VERTEX_DATA, 0},
	    {"BINORMAL", 0, DXGI_FORMAT_R32_FLOAT, 0, offsetof(assets::SurfaceVertex, binormalSign),
	     D3D11_INPUT_PER_VERTEX_DATA, 0},
	    {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(assets::SurfaceVertex, uv),
	     D3D11_INPUT_PER_VERTEX_DATA, 0},
	    {"BLENDINDICES", 0, DXGI_FORMAT_R16G16B16A16_UINT, 0,
	     offsetof(assets::SurfaceVertex, boneIndices), D3D11_INPUT_PER_VERTEX_DATA, 0},
	    {"BLENDWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
	     offsetof(assets::SurfaceVertex, boneWeights), D3D11_INPUT_PER_VERTEX_DATA, 0},
	};
	hr = device_->CreateInputLayout(layout, static_cast<UINT>(std::size(layout)), vsBlob->GetBufferPointer(),
	                                vsBlob->GetBufferSize(), &modelInputLayout_);
	if (FAILED(hr)) {
		return false;
	}

	D3D11_BUFFER_DESC constantDesc = {};
	constantDesc.ByteWidth = sizeof(ModelConstants);
	constantDesc.Usage = D3D11_USAGE_DYNAMIC;
	constantDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	constantDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	hr = device_->CreateBuffer(&constantDesc, nullptr, &modelConstantBuffer_);
	if (FAILED(hr)) {
		return false;
	}

	D3D11_BUFFER_DESC boneDesc = {};
	boneDesc.ByteWidth = static_cast<UINT>(sizeof(DirectX::XMFLOAT4X4) * kMaxBones);
	boneDesc.Usage = D3D11_USAGE_DYNAMIC;
	boneDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	boneDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	hr = device_->CreateBuffer(&boneDesc, nullptr, &boneConstantBuffer_);
	if (FAILED(hr)) {
		return false;
	}

	D3D11_BUFFER_DESC camoDesc = {};
	camoDesc.ByteWidth = sizeof(CamoConstants);
	camoDesc.Usage = D3D11_USAGE_DYNAMIC;
	camoDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	camoDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	hr = device_->CreateBuffer(&camoDesc, nullptr, &camoConstantBuffer_);
	if (FAILED(hr)) {
		return false;
	}

	D3D11_RASTERIZER_DESC rasterDesc = {};
	rasterDesc.FillMode = D3D11_FILL_SOLID;
	rasterDesc.CullMode = D3D11_CULL_NONE;
	rasterDesc.FrontCounterClockwise = FALSE;
	rasterDesc.DepthClipEnable = TRUE;
	hr = device_->CreateRasterizerState(&rasterDesc, &solidState_);
	if (FAILED(hr)) {
		return false;
	}
	rasterDesc.FillMode = D3D11_FILL_WIREFRAME;
	hr = device_->CreateRasterizerState(&rasterDesc, &wireframeState_);
	if (FAILED(hr)) {
		return false;
	}

	D3D11_DEPTH_STENCIL_DESC overlayDepthDesc = {};
	overlayDepthDesc.DepthEnable = FALSE;
	overlayDepthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	overlayDepthDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
	overlayDepthDesc.StencilEnable = FALSE;
	hr = device_->CreateDepthStencilState(&overlayDepthDesc, &overlayDepthState_);
	if (FAILED(hr)) {
		return false;
	}

	D3D11_SAMPLER_DESC samplerDesc = {};
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
	hr = device_->CreateSamplerState(&samplerDesc, &samplerState_);
	if (FAILED(hr)) {
		return false;
	}

	return CreateDefaultTextures();
}

bool Renderer::CreateDefaultTextures() {
	// Checker for missing color maps.
	constexpr int checkerSize = 64;
	constexpr int checkerCell = 8;
	std::vector<std::uint8_t> checker(checkerSize * checkerSize * 4);
	for (int y = 0; y < checkerSize; ++y) {
		for (int x = 0; x < checkerSize; ++x) {
			const bool light = ((x / checkerCell) + (y / checkerCell)) % 2 == 0;
			const std::uint8_t value = light ? 150 : 70;
			std::uint8_t* pixel = &checker[(static_cast<std::size_t>(y) * checkerSize + x) * 4];
			pixel[0] = light ? 160 : 80;
			pixel[1] = value;
			pixel[2] = light ? 180 : 90;
			pixel[3] = 255;
		}
	}

	D3D11_TEXTURE2D_DESC desc = {};
	desc.Width = checkerSize;
	desc.Height = checkerSize;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_IMMUTABLE;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	D3D11_SUBRESOURCE_DATA data = {};
	data.pSysMem = checker.data();
	data.SysMemPitch = checkerSize * 4;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
	HRESULT hr = device_->CreateTexture2D(&desc, &data, &texture);
	if (FAILED(hr)) {
		return false;
	}
	hr = device_->CreateShaderResourceView(texture.Get(), nullptr, &checkerTexture_);
	if (FAILED(hr)) {
		return false;
	}

	const std::uint8_t flatNormal[4] = {128, 128, 255, 255};
	const std::uint8_t black[4] = {0, 0, 0, 255};
	const std::uint8_t white[4] = {255, 255, 255, 255};
	auto make1x1 = [this](const std::uint8_t pixel[4], ComPtr<ID3D11ShaderResourceView>& out) {
		D3D11_TEXTURE2D_DESC desc1 = {};
		desc1.Width = 1;
		desc1.Height = 1;
		desc1.MipLevels = 1;
		desc1.ArraySize = 1;
		desc1.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc1.SampleDesc.Count = 1;
		desc1.Usage = D3D11_USAGE_IMMUTABLE;
		desc1.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		D3D11_SUBRESOURCE_DATA data1 = {};
		data1.pSysMem = pixel;
		data1.SysMemPitch = 4;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> texture1;
		if (FAILED(device_->CreateTexture2D(&desc1, &data1, &texture1))) {
			return false;
		}
		return SUCCEEDED(device_->CreateShaderResourceView(texture1.Get(), nullptr, &out));
	};
	if (!make1x1(flatNormal, flatNormalTexture_) || !make1x1(black, blackTexture_) ||
	    !make1x1(white, whiteTexture_)) {
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

bool Renderer::BeginScene(int width, int height) {
	if (!EnsureSceneTarget(width, height)) {
		return false;
	}

	const float clearColor[4] = {0.105f, 0.115f, 0.135f, 1.0f};
	context_->ClearRenderTargetView(sceneRtv_.Get(), clearColor);
	context_->ClearDepthStencilView(depthDsv_.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

	ID3D11RenderTargetView* renderTargets[] = {sceneRtv_.Get()};
	context_->OMSetRenderTargets(1, renderTargets, depthDsv_.Get());

	D3D11_VIEWPORT viewport = {};
	viewport.Width = static_cast<float>(sceneWidth_);
	viewport.Height = static_cast<float>(sceneHeight_);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	context_->RSSetViewports(1, &viewport);

	context_->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
	context_->RSSetState(solidState_.Get());
	return true;
}

void Renderer::RenderGrid(const Camera& camera) {
	if (!gridVertexBuffer_ || sceneWidth_ <= 0 || sceneHeight_ <= 0) {
		return;
	}

	const float aspect = static_cast<float>(sceneWidth_) / static_cast<float>(sceneHeight_);
	DirectX::XMFLOAT4X4 viewProjection = {};
	DirectX::XMStoreFloat4x4(&viewProjection, camera.ViewProjection(aspect));

	D3D11_MAPPED_SUBRESOURCE mapped = {};
	if (FAILED(context_->Map(gridConstantBuffer_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
		return;
	}
	std::memcpy(mapped.pData, &viewProjection, sizeof(viewProjection));
	context_->Unmap(gridConstantBuffer_.Get(), 0);

	const UINT stride = sizeof(GridVertex);
	const UINT offset = 0;
	ID3D11Buffer* vertexBuffer = gridVertexBuffer_.Get();
	ID3D11Buffer* constantBuffer = gridConstantBuffer_.Get();

	context_->OMSetDepthStencilState(gridDepthState_.Get(), 0);
	context_->IASetInputLayout(gridInputLayout_.Get());
	context_->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
	context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
	context_->VSSetShader(gridVs_.Get(), nullptr, 0);
	context_->VSSetConstantBuffers(0, 1, &constantBuffer);
	context_->PSSetShader(gridPs_.Get(), nullptr, 0);
	context_->Draw(gridVertexCount_, 0);

	context_->OMSetDepthStencilState(nullptr, 0);
}

void Renderer::RenderModel(const GpuModel& model, const Camera& camera, bool wireframe,
                           const std::vector<bool>* visibleSurfaces, float exposure,
                           const DirectX::XMFLOAT4X4* world,
                           const std::vector<DirectX::XMFLOAT4X4>* bonePalette,
                           const DirectX::XMFLOAT4* tint, bool onTop) {
	if (model.surfaces.empty() || sceneWidth_ <= 0 || sceneHeight_ <= 0) {
		return;
	}

	const float aspect = static_cast<float>(sceneWidth_) / static_cast<float>(sceneHeight_);
	ModelConstants constants = {};
	DirectX::XMStoreFloat4x4(&constants.viewProjection, camera.ViewProjection(aspect));
	if (world != nullptr) {
		constants.world = *world;
	} else {
		DirectX::XMStoreFloat4x4(&constants.world, DirectX::XMMatrixIdentity());
	}
	const DirectX::XMFLOAT3 eye = camera.Eye();
	constants.cameraPosition = DirectX::XMFLOAT4(eye.x, eye.y, eye.z, 1.0f);
	const DirectX::XMFLOAT3 target = camera.Target();
	DirectX::XMFLOAT3 forward = {target.x - eye.x, target.y - eye.y, target.z - eye.z};
	const float forwardLength = std::sqrt(forward.x * forward.x + forward.y * forward.y + forward.z * forward.z);
	if (forwardLength > 1.0e-5f) {
		forward.x /= forwardLength;
		forward.y /= forwardLength;
		forward.z /= forwardLength;
	} else {
		forward = {1.0f, 0.0f, 0.0f};
	}
	// Key light in front of the weapon (roughly from the camera side), slightly above
	// and to the right, so the receiver/handguard faces are visible in first person.
	const DirectX::XMFLOAT3 up = {0.0f, 0.0f, 1.0f};
	DirectX::XMFLOAT3 right = {forward.y * up.z - forward.z * up.y, forward.z * up.x - forward.x * up.z,
	                           forward.x * up.y - forward.y * up.x};
	const float rightLength = std::sqrt(right.x * right.x + right.y * right.y + right.z * right.z);
	if (rightLength > 1.0e-5f) {
		right.x /= rightLength;
		right.y /= rightLength;
		right.z /= rightLength;
	} else {
		right = {0.0f, 1.0f, 0.0f};
	}
	DirectX::XMFLOAT3 light = {-forward.x * 0.85f + up.x * 0.55f + right.x * 0.22f,
	                           -forward.y * 0.85f + up.y * 0.55f + right.y * 0.22f,
	                           -forward.z * 0.85f + up.z * 0.55f + right.z * 0.22f};
	const float lightLength = std::sqrt(light.x * light.x + light.y * light.y + light.z * light.z);
	if (lightLength > 1.0e-5f) {
		light.x /= lightLength;
		light.y /= lightLength;
		light.z /= lightLength;
	}
	constants.lightDirection = DirectX::XMFLOAT4(light.x, light.y, light.z, 0.0f);
	constants.params = DirectX::XMFLOAT4(48.0f, exposure, 0.0f, 0.0f);
	constants.tint = tint != nullptr ? *tint : DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

	D3D11_MAPPED_SUBRESOURCE mapped = {};
	if (FAILED(context_->Map(modelConstantBuffer_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
		return;
	}
	std::memcpy(mapped.pData, &constants, sizeof(constants));
	context_->Unmap(modelConstantBuffer_.Get(), 0);

	// Upload the skinning palette. Bones beyond the palette (or a missing palette) use
	// identity so the bind pose still renders.
	std::vector<DirectX::XMFLOAT4X4> identityPalette;
	const std::vector<DirectX::XMFLOAT4X4>* palette = bonePalette;
	if (palette == nullptr) {
		DirectX::XMFLOAT4X4 identity;
		DirectX::XMStoreFloat4x4(&identity, DirectX::XMMatrixIdentity());
		identityPalette.assign(1, identity);
		palette = &identityPalette;
	}
	if (FAILED(context_->Map(boneConstantBuffer_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
		return;
	}
	auto* bones = static_cast<DirectX::XMFLOAT4X4*>(mapped.pData);
	DirectX::XMFLOAT4X4 identity;
	DirectX::XMStoreFloat4x4(&identity, DirectX::XMMatrixIdentity());
	const std::size_t paletteCount = std::min<std::size_t>(palette->size(), kMaxBones);
	for (std::size_t i = 0; i < kMaxBones; ++i) {
		bones[i] = i < paletteCount ? (*palette)[i] : identity;
	}
	context_->Unmap(boneConstantBuffer_.Get(), 0);

	ID3D11Buffer* constantBuffer = modelConstantBuffer_.Get();
	ID3D11Buffer* boneBuffer = boneConstantBuffer_.Get();
	ID3D11Buffer* camoBuffer = camoConstantBuffer_.Get();
	ID3D11ShaderResourceView* textures[9] = {checkerTexture_.Get(), flatNormalTexture_.Get(),
	                                         blackTexture_.Get(), blackTexture_.Get(),
	                                         whiteTexture_.Get(), flatNormalTexture_.Get(),
	                                         whiteTexture_.Get(), blackTexture_.Get(),
	                                         blackTexture_.Get()};
	ID3D11SamplerState* sampler = samplerState_.Get();

	context_->OMSetDepthStencilState(onTop ? overlayDepthState_.Get() : nullptr, 0);
	context_->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
	context_->RSSetState(wireframe ? wireframeState_.Get() : solidState_.Get());
	context_->IASetInputLayout(modelInputLayout_.Get());
	context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context_->VSSetShader(modelVs_.Get(), nullptr, 0);
	context_->VSSetConstantBuffers(0, 1, &constantBuffer);
	context_->VSSetConstantBuffers(1, 1, &boneBuffer);
	context_->PSSetShader(modelPs_.Get(), nullptr, 0);
	context_->PSSetConstantBuffers(0, 1, &constantBuffer);
	context_->PSSetConstantBuffers(2, 1, &camoBuffer);
	context_->PSSetSamplers(0, 1, &sampler);

	for (std::size_t i = 0; i < model.surfaces.size(); ++i) {
		const GpuSurface& surface = model.surfaces[i];
		if (surface.indexCount == 0 || !surface.vertexBuffer || !surface.indexBuffer) {
			continue;
		}
		if (visibleSurfaces != nullptr && i < visibleSurfaces->size() && !(*visibleSurfaces)[i]) {
			continue;
		}

		const GpuMaterial* material = nullptr;
		if (surface.materialIndex >= 0 &&
		    surface.materialIndex < static_cast<int>(model.materials.size())) {
			material = &model.materials[static_cast<std::size_t>(surface.materialIndex)];
		}
		const bool nativeCamo = material != nullptr && material->nativeStrength >= 0.0f;
		const float strength = nativeCamo ? material->nativeStrength : model.camoStrength;
		const float materialScaleU =
		    material == nullptr ? 1.0f : (nativeCamo ? material->nativeUvScaleU : material->camoUvScaleU);
		const float materialScaleV =
		    material == nullptr ? 1.0f : (nativeCamo ? material->nativeUvScaleV : material->camoUvScaleV);
		const float scaleU = (materialScaleU > 0.0f ? materialScaleU : 1.0f) *
		                     (model.camoScale > 0.0f ? model.camoScale : 1.0f);
		const float scaleV = (materialScaleV > 0.0f ? materialScaleV : 1.0f) *
		                     (model.camoScale > 0.0f ? model.camoScale : 1.0f);
		const float envScale = nativeCamo && material != nullptr ? material->envMapScale : model.camoEnvScale;

		textures[0] = checkerTexture_.Get();
		textures[1] = flatNormalTexture_.Get();
		textures[2] = blackTexture_.Get();
		textures[3] = nativeCamo && material->nativeCamoColor ? material->nativeCamoColor.Get()
		                                                      : model.camo.Get();
		textures[4] = whiteTexture_.Get();
		textures[5] = nativeCamo && material->nativeCamoNormal ? material->nativeCamoNormal.Get()
		                                                       : model.camoNormal.Get();
		textures[6] = nativeCamo && material->nativeCamoSpecular ? material->nativeCamoSpecular.Get()
		                                                         : model.camoSpecular.Get();
		textures[7] = nativeCamo && material->nativeCamoEnvironment ? material->nativeCamoEnvironment.Get()
		                                                            : model.camoEnvironment.Get();
		textures[8] = nativeCamo && material->nativeCamoEmissive ? material->nativeCamoEmissive.Get()
		                                                          : blackTexture_.Get();
		if (textures[3] == nullptr) {
			textures[3] = blackTexture_.Get();
		}
		if (textures[5] == nullptr) {
			textures[5] = flatNormalTexture_.Get();
		}
		if (textures[6] == nullptr) {
			textures[6] = whiteTexture_.Get();
		}
		if (textures[7] == nullptr) {
			textures[7] = blackTexture_.Get();
		}
		if (material != nullptr) {
			if (material->albedo) {
				textures[0] = material->albedo.Get();
			}
			if (material->normal) {
				textures[1] = material->normal.Get();
			}
			if (material->specular) {
				textures[2] = material->specular.Get();
			}
			if (material->camoMask) {
				textures[4] = material->camoMask.Get();
			}
		}
		context_->PSSetShaderResources(0, 9, textures);

		CamoConstants camoConstants = {};
		camoConstants.surfaceCamo = DirectX::XMFLOAT4(strength, scaleU, scaleV, envScale);
		if (FAILED(context_->Map(camoConstantBuffer_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
			break;
		}
		std::memcpy(mapped.pData, &camoConstants, sizeof(camoConstants));
		context_->Unmap(camoConstantBuffer_.Get(), 0);

		const UINT stride = sizeof(assets::SurfaceVertex);
		const UINT offset = 0;
		ID3D11Buffer* vertexBuffer = surface.vertexBuffer.Get();
		ID3D11Buffer* indexBuffer = surface.indexBuffer.Get();
		context_->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
		context_->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R16_UINT, 0);
		context_->DrawIndexed(surface.indexCount, 0, 0);
	}

	context_->OMSetDepthStencilState(nullptr, 0);
	context_->RSSetState(solidState_.Get());
}

ComPtr<ID3D11ShaderResourceView> Renderer::CreateTexture(const assets::ImageData& image, bool srgb,
                                                          std::string& error) {
	if (!image.available || image.pixels.empty()) {
		error = image.note.empty() ? "no pixel data" : image.note;
		return nullptr;
	}

	std::size_t rowPitch = 0;
	std::size_t slicePitch = 0;
	DXGI_FORMAT baseFormat = static_cast<DXGI_FORMAT>(image.format);
	// The H1/HMW env BRDF LUT is stored as R16G16_TYPELESS; the pixel data is half float.
	if (image.format == 34) {
		baseFormat = DXGI_FORMAT_R16G16_FLOAT;
	}
	const DXGI_FORMAT format = srgb ? ToSrgbFormat(baseFormat) : baseFormat;
	const HRESULT pitchResult = DirectX::ComputePitch(format, image.width, image.height, rowPitch, slicePitch);
	if (FAILED(pitchResult) || slicePitch == 0) {
		error = "unsupported texture format " + std::to_string(image.format);
		return nullptr;
	}
	if (image.pixels.size() < slicePitch) {
		error = "texture data is smaller than one mip level";
		return nullptr;
	}

	D3D11_TEXTURE2D_DESC desc = {};
	desc.Width = image.width;
	desc.Height = image.height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = format;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_IMMUTABLE;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	D3D11_SUBRESOURCE_DATA data = {};
	data.pSysMem = image.pixels.data();
	data.SysMemPitch = static_cast<UINT>(rowPitch);
	data.SysMemSlicePitch = static_cast<UINT>(slicePitch);

	Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
	HRESULT hr = device_->CreateTexture2D(&desc, &data, &texture);
	if (FAILED(hr)) {
		error = "CreateTexture2D failed (format " + std::to_string(image.format) + ")";
		return nullptr;
	}
	ComPtr<ID3D11ShaderResourceView> view;
	hr = device_->CreateShaderResourceView(texture.Get(), nullptr, &view);
	if (FAILED(hr)) {
		error = "CreateShaderResourceView failed";
		return nullptr;
	}
	return view;
}

std::unique_ptr<GpuModel> Renderer::UploadModel(const assets::ModelAsset& model, std::string& error) {
	auto gpu = std::make_unique<GpuModel>();
	gpu->boundsCenter = model.boundsCenter;
	gpu->boundsHalf = model.boundsHalf;

	gpu->materials.resize(model.materials.size());
	for (std::size_t i = 0; i < model.materials.size(); ++i) {
		const assets::MaterialSlot& slot = model.materials[i];
		GpuMaterial& material = gpu->materials[i];
		material.name = slot.material.name;
		material.camoUvScaleU = slot.material.decalScaleU > 0.0f ? slot.material.decalScaleU : 1.0f;
		material.camoUvScaleV = slot.material.decalScaleV > 0.0f ? slot.material.decalScaleV : 1.0f;
		material.envMapScale = slot.material.envMapScale;
		std::string textureError;
		const auto applyTexture = [&](int imageIndex, ComPtr<ID3D11ShaderResourceView>& target, bool srgb) {
			if (imageIndex < 0 || imageIndex >= static_cast<int>(model.images.size())) {
				return;
			}
			ComPtr<ID3D11ShaderResourceView> view =
			    CreateTexture(model.images[static_cast<std::size_t>(imageIndex)], srgb, textureError);
			if (view) {
				target = std::move(view);
			}
		};
		applyTexture(slot.colorImage, material.albedo, true);
		applyTexture(slot.normalImage, material.normal, false);
		applyTexture(slot.specularImage, material.specular, false);
		applyTexture(slot.camoMaskImage, material.camoMask, false);
	}

	gpu->surfaces.resize(model.surfaceSet.surfaces.size());
	for (std::size_t i = 0; i < model.surfaceSet.surfaces.size(); ++i) {
		const assets::SurfaceData& source = model.surfaceSet.surfaces[i];
		GpuSurface& surface = gpu->surfaces[i];
		surface.materialIndex = source.materialIndex;
		surface.indexCount = static_cast<UINT>(source.indices.size());

		if (source.vertices.empty() || source.indices.empty()) {
			continue;
		}
		if (source.vertices.size() > 65535) {
			error = "surface " + std::to_string(i) + " has more than 65535 vertices";
			return nullptr;
		}

		D3D11_BUFFER_DESC vertexDesc = {};
		vertexDesc.ByteWidth = static_cast<UINT>(source.vertices.size() * sizeof(assets::SurfaceVertex));
		vertexDesc.Usage = D3D11_USAGE_IMMUTABLE;
		vertexDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		D3D11_SUBRESOURCE_DATA vertexData = {};
		vertexData.pSysMem = source.vertices.data();
		if (FAILED(device_->CreateBuffer(&vertexDesc, &vertexData, &surface.vertexBuffer))) {
			error = "failed to create vertex buffer for surface " + std::to_string(i);
			return nullptr;
		}

		D3D11_BUFFER_DESC indexDesc = {};
		indexDesc.ByteWidth = static_cast<UINT>(source.indices.size() * sizeof(std::uint16_t));
		indexDesc.Usage = D3D11_USAGE_IMMUTABLE;
		indexDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		D3D11_SUBRESOURCE_DATA indexData = {};
		indexData.pSysMem = source.indices.data();
		if (FAILED(device_->CreateBuffer(&indexDesc, &indexData, &surface.indexBuffer))) {
			error = "failed to create index buffer for surface " + std::to_string(i);
			return nullptr;
		}
	}

	return gpu;
}

bool Renderer::SetModelCamo(GpuModel& model, const assets::CamoTextures& textures, float strength,
                            float scale, std::string& error) {
	ClearModelCamo(model);
	ComPtr<ID3D11ShaderResourceView> color = CreateTexture(textures.color, true, error);
	if (!color) {
		error = "camo colour texture: " + error;
		return false;
	}
	model.camo = std::move(color);
	const auto uploadOptional = [&](const assets::ImageData& image, bool srgb,
	                                ComPtr<ID3D11ShaderResourceView>& target) {
		if (!image.available || image.pixels.empty()) {
			return;
		}
		std::string ignored;
		ComPtr<ID3D11ShaderResourceView> view = CreateTexture(image, srgb, ignored);
		if (view) {
			target = std::move(view);
		}
	};
	uploadOptional(textures.normal, false, model.camoNormal);
	uploadOptional(textures.specular, false, model.camoSpecular);
	uploadOptional(textures.environment, false, model.camoEnvironment);
	model.camoStrength = std::clamp(strength, 0.0f, 1.0f);
	model.camoScale = scale > 0.0f ? scale : 1.0f;
	model.camoEnvScale = textures.envMapScale;
	return true;
}

bool Renderer::SetModelNativeCamo(GpuModel& model, const std::string& variantStem,
                                  const assets::CamoTextures& textures, float strength, float scale,
                                  std::string& error) {
	ClearModelCamo(model);
	ComPtr<ID3D11ShaderResourceView> color = CreateTexture(textures.color, true, error);
	if (!color) {
		error = "native camo colour texture: " + error;
		return false;
	}
	ComPtr<ID3D11ShaderResourceView> normal;
	ComPtr<ID3D11ShaderResourceView> specular;
	ComPtr<ID3D11ShaderResourceView> environment;
	ComPtr<ID3D11ShaderResourceView> emissive;
	const auto uploadOptional = [&](const assets::ImageData& image, bool srgb,
	                                ComPtr<ID3D11ShaderResourceView>& target) {
		if (!image.available || image.pixels.empty()) {
			return;
		}
		std::string ignored;
		ComPtr<ID3D11ShaderResourceView> view = CreateTexture(image, srgb, ignored);
		if (view) {
			target = std::move(view);
		}
	};
	uploadOptional(textures.normal, false, normal);
	uploadOptional(textures.specular, false, specular);
	uploadOptional(textures.environment, false, environment);
	uploadOptional(textures.emissive, false, emissive);

	std::string baseStem = variantStem;
	for (const char* suffix : {"_fire", "_doomsday"}) {
		const std::string s = suffix;
		if (baseStem.size() > s.size() && baseStem.ends_with(s)) {
			baseStem.resize(baseStem.size() - s.size());
			break;
		}
	}
	const auto stemOf = [](const std::string& name) {
		const std::size_t slash = name.find_last_of('/');
		return slash == std::string::npos ? name : name.substr(slash + 1);
	};
	const auto stripCamo = [](std::string stem) {
		if (stem.size() > 5 && stem.ends_with("_camo")) {
			stem.resize(stem.size() - 5);
		}
		return stem;
	};

	bool matched = false;
	for (GpuMaterial& material : model.materials) {
		const std::string stem = stemOf(material.name);
		if (stem != baseStem && stripCamo(stem) != baseStem) {
			continue;
		}
		material.nativeCamoColor = color;
		material.nativeCamoNormal = normal;
		material.nativeCamoSpecular = specular;
		material.nativeCamoEnvironment = environment;
		material.nativeCamoEmissive = emissive;
		material.nativeStrength = std::clamp(strength, 0.0f, 1.0f);
		material.nativeUvScaleU = textures.decalScaleU > 0.0f ? textures.decalScaleU : 1.0f;
		material.nativeUvScaleV = textures.decalScaleV > 0.0f ? textures.decalScaleV : 1.0f;
		material.envMapScale = textures.envMapScale;
		matched = true;
	}
	if (!matched) {
		error = "native camo material " + variantStem + " does not match any loaded material";
		return false;
	}
	model.camoScale = scale > 0.0f ? scale : 1.0f;
	return true;
}

void Renderer::ClearModelCamo(GpuModel& model) {
	model.camo.Reset();
	model.camoNormal.Reset();
	model.camoSpecular.Reset();
	model.camoEnvironment.Reset();
	model.camoStrength = 0.0f;
	model.camoScale = 1.0f;
	model.camoEnvScale = 0.04f;
	for (GpuMaterial& material : model.materials) {
		material.nativeCamoColor.Reset();
		material.nativeCamoNormal.Reset();
		material.nativeCamoSpecular.Reset();
		material.nativeCamoEnvironment.Reset();
		material.nativeCamoEmissive.Reset();
		material.nativeStrength = -1.0f;
	}
}

bool Renderer::CaptureScene(std::vector<std::uint8_t>& pixels, int& width, int& height,
                            DXGI_FORMAT& format) {
	if (!sceneTexture_) {
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
		return false;
	}
	context_->CopyResource(staging.Get(), sceneTexture_.Get());

	D3D11_MAPPED_SUBRESOURCE mapped = {};
	hr = context_->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped);
	if (FAILED(hr)) {
		return false;
	}

	const UINT bytesPerPixel = static_cast<UINT>(DirectX::BitsPerPixel(desc.Format) / 8);
	const std::size_t rowSize = static_cast<std::size_t>(desc.Width) * bytesPerPixel;
	width = static_cast<int>(desc.Width);
	height = static_cast<int>(desc.Height);
	format = desc.Format;
	pixels.resize(rowSize * desc.Height);
	for (UINT y = 0; y < desc.Height; ++y) {
		std::memcpy(pixels.data() + rowSize * y, static_cast<const std::uint8_t*>(mapped.pData) + mapped.RowPitch * y,
		            rowSize);
	}
	context_->Unmap(staging.Get(), 0);
	return true;
}

bool Renderer::SavePixelsToPng(const std::filesystem::path& path, const std::vector<std::uint8_t>& pixels,
                               int width, int height, DXGI_FORMAT format) {
	if (width <= 0 || height <= 0) {
		return false;
	}
	const UINT bytesPerPixel = static_cast<UINT>(DirectX::BitsPerPixel(format) / 8);
	DirectX::Image image = {};
	image.width = static_cast<std::size_t>(width);
	image.height = static_cast<std::size_t>(height);
	image.format = format;
	image.rowPitch = static_cast<std::size_t>(width) * bytesPerPixel;
	image.slicePitch = image.rowPitch * image.height;
	image.pixels = const_cast<std::uint8_t*>(pixels.data());
	const HRESULT hr = DirectX::SaveToWICFile(image, DirectX::WIC_FLAGS_NONE,
	                                          DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG), path.c_str());
	return SUCCEEDED(hr);
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

