#include <Windows.h>
#include <objbase.h>
#include <shellapi.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <filesystem>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include "app/application.h"
#include "app/info.h"
#include "app/model_overlay.h"
#include "app/settings.h"
#include "assets/asset_index.h"
#include "assets/model_asset.h"
#include "render/camera.h"
#include "render/renderer.h"
#include "scene/viewmodel.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT message, WPARAM wParam,
                                                             LPARAM lParam);

namespace {

constexpr wchar_t kWindowClass[] = L"hmw-workbench";
constexpr wchar_t kWindowTitle[] = L"hmw-workbench";

bool g_imguiReady = false;
bool g_rendererReady = false;
bool g_resizePending = false;
bool g_minimized = false;
int g_clientWidth = 0;
int g_clientHeight = 0;

bool g_consoleAttached = false;

enum class Mode { Window, Screenshot, Info, Validate };

struct Options {
	Mode mode = Mode::Window;
	std::filesystem::path screenshotPath;
	std::string modelName;
	std::string weaponName;
	std::string handsName;
	bool viewFirstPerson = false;
	bool hasView = false;
	std::string infoName;
	bool bones = false;
	bool wire = false;
	int width = 1600;
	int height = 900;
	bool hasYaw = false;
	bool hasPitch = false;
	bool hasDistance = false;
	bool hasFov = false;
	float yaw = 0.0f;
	float pitch = 0.0f;
	float distance = 0.0f;
	float fov = 0.0f;
	bool hasExposure = false;
	float exposure = 1.0f;
};

void OpenConsoleStreams() {
	FILE* dummy = nullptr;
	freopen_s(&dummy, "CONOUT$", "w", stdout);
	freopen_s(&dummy, "CONOUT$", "w", stderr);
}

void AttachParentConsole() {
	if (AttachConsole(ATTACH_PARENT_PROCESS)) {
		g_consoleAttached = true;
		const HANDLE stdoutHandle = GetStdHandle(STD_OUTPUT_HANDLE);
		if (stdoutHandle == nullptr || stdoutHandle == INVALID_HANDLE_VALUE) {
			OpenConsoleStreams();
		}
	}
}

void EnsureHeadlessConsole() {
	if (g_consoleAttached) {
		return;
	}
	if (AllocConsole()) {
		g_consoleAttached = true;
		OpenConsoleStreams();
	}
}

void PrintUsage() {
	std::fputs(
	    "hmw-workbench - HMW / MWR content workbench\n"
	    "\n"
	    "usage:\n"
	    "  hmw-workbench.exe\n"
	    "      Open the window.\n"
	    "  hmw-workbench.exe -screenshot <file.png> [-model <xmodel> | -weapon <name>]\n"
	    "                    [-hands <viewhands>] [-view firstperson|orbit] [-bones] [-wire]\n"
	    "                    [-width <n>] [-height <n>] [-yaw <deg>] [-pitch <deg>]\n"
	    "                    [-dist <units>] [-fov <deg>] [-exposure <n>]\n"
	    "      Render the viewport offscreen (no window) and exit.\n"
	    "  hmw-workbench.exe -info <xmodel>\n"
	    "      Text dump: bones, tags, surfaces, materials, bounds.\n"
	    "  hmw-workbench.exe -validate\n"
	    "      Parse every .xmb in all roots plus its .xsb surface sets and material json.\n"
	    "\n"
	    "workbench.json next to the exe configures asset roots, window size and camera.\n",
	    stdout);
}

std::filesystem::path ExeDirectory() {
	wchar_t buffer[MAX_PATH * 4] = {};
	const DWORD length = GetModuleFileNameW(nullptr, buffer, static_cast<DWORD>(std::size(buffer)));
	if (length == 0) {
		return std::filesystem::current_path();
	}
	return std::filesystem::path(std::wstring(buffer, length)).parent_path();
}

bool ParseFloatArg(const wchar_t* text, float& value) {
	if (text == nullptr || *text == L'\0') {
		return false;
	}
	wchar_t* end = nullptr;
	const double parsed = std::wcstod(text, &end);
	if (end == text || *end != L'\0') {
		return false;
	}
	value = static_cast<float>(parsed);
	return true;
}

bool ParseIntArg(const wchar_t* text, int& value) {
	if (text == nullptr || *text == L'\0') {
		return false;
	}
	wchar_t* end = nullptr;
	const long parsed = std::wcstol(text, &end, 10);
	if (end == text || *end != L'\0') {
		return false;
	}
	value = static_cast<int>(parsed);
	return true;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
	if (g_imguiReady && ImGui_ImplWin32_WndProcHandler(hwnd, message, wParam, lParam)) {
		return 1;
	}

	switch (message) {
	case WM_SIZE:
		g_clientWidth = static_cast<int>(LOWORD(lParam));
		g_clientHeight = static_cast<int>(HIWORD(lParam));
		g_minimized = wParam == SIZE_MINIMIZED;
		if (!g_minimized && g_rendererReady) {
			g_resizePending = true;
		}
		return 0;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	default:
		break;
	}

	return DefWindowProcW(hwnd, message, wParam, lParam);
}

void ApplyCameraOptions(wb::Camera& camera, const Options& options, const wb::Settings& settings) {
	camera.SetYawPitch(options.hasYaw ? options.yaw : settings.camera.yaw,
	                   options.hasPitch ? options.pitch : settings.camera.pitch);
	camera.SetDistance(options.hasDistance ? options.distance : settings.camera.distance);
	camera.SetFov(options.hasFov ? options.fov : settings.camera.fov);
}

float CameraRadius(const DirectX::XMFLOAT3& halfSize) {
	return std::max({halfSize.x, halfSize.y, halfSize.z, 0.25f}) * 1.1f;
}

void DrawHeadlessBoneOverlay(wb::Renderer& renderer, const wb::assets::ModelAsset& model,
                             const wb::Camera& camera, int width, int height, bool showAllNames) {
	const ImVec2 displaySize(static_cast<float>(width), static_cast<float>(height));
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.IniFilename = nullptr;
	io.DisplaySize = displaySize;
	io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);

	if (!ImGui_ImplDX11_Init(renderer.Device(), renderer.Context())) {
		std::fprintf(stderr, "overlay: ImGui_ImplDX11_Init failed\n");
		ImGui::DestroyContext();
		return;
	}

	ImGui_ImplDX11_NewFrame();
	ImGui::NewFrame();

	const float aspect = displaySize.x / displaySize.y;
	DirectX::XMFLOAT4X4 viewProjection = {};
	DirectX::XMStoreFloat4x4(&viewProjection, camera.ViewProjection(aspect));
	wb::DrawModelOverlay(ImGui::GetBackgroundDrawList(), ImVec2(0.0f, 0.0f), displaySize, viewProjection,
	                     model.model, -1, showAllNames);

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	ImGui_ImplDX11_Shutdown();
	ImGui::DestroyContext();
}

int RunHeadless(const std::filesystem::path& exeDir, const wb::Settings& settings, const Options& options) {
	std::printf("hmw-workbench M2 (headless)\n");

	const std::vector<std::filesystem::path> roots = wb::ResolveAssetRoots(settings, exeDir);
	wb::AssetIndex assets;
	assets.Scan(roots);
	std::printf("assets: %d weapons, %d models, %d anims (%d total)\n", assets.Count(wb::AssetKind::Weapon),
	            assets.Count(wb::AssetKind::Model), assets.Count(wb::AssetKind::Anim), assets.TotalCount());

	wb::Renderer renderer;
	wb::RendererDesc desc;
	desc.hwnd = nullptr;
	desc.width = options.width;
	desc.height = options.height;
	if (!renderer.Initialize(desc)) {
		std::fprintf(stderr, "failed to initialize D3D11\n");
		return 1;
	}

	wb::Camera camera;
	wb::assets::ModelAsset model;
	std::unique_ptr<wb::GpuModel> gpuModel;
	wb::scene::ViewmodelAsset viewmodel;
	std::unique_ptr<wb::GpuModel> gpuHands;
	std::unique_ptr<wb::GpuModel> gpuGun;
	const float exposure = options.hasExposure ? options.exposure : settings.exposure;

	if (!options.weaponName.empty()) {
		std::string error;
		if (!wb::scene::LoadViewmodel(options.weaponName, options.handsName, roots, viewmodel, error)) {
			std::fprintf(stderr, "cannot load weapon %s: %s\n", options.weaponName.c_str(), error.c_str());
			return 1;
		}
		std::string uploadError;
		gpuGun = renderer.UploadModel(viewmodel.gun, uploadError);
		if (!gpuGun) {
			std::fprintf(stderr, "cannot upload gun %s: %s\n", viewmodel.weapon.gunModel.c_str(),
			             uploadError.c_str());
			return 1;
		}
		if (viewmodel.hasHands) {
			gpuHands = renderer.UploadModel(viewmodel.hands, uploadError);
			if (!gpuHands) {
				std::fprintf(stderr, "cannot upload hands %s: %s\n", viewmodel.handsName.c_str(),
				             uploadError.c_str());
			}
		}
		std::printf("weapon: %s | gun: %s | hands: %s (%s)\n", viewmodel.weapon.name.c_str(),
		            viewmodel.weapon.gunModel.c_str(),
		            viewmodel.handsName.empty() ? "(none)" : viewmodel.handsName.c_str(),
		            viewmodel.handsSource.c_str());
		for (const std::string& warning : viewmodel.warnings) {
			std::printf("warning: %s\n", warning.c_str());
		}

		if (options.viewFirstPerson) {
			const DirectX::XMFLOAT3 eye = viewmodel.viewOriginValid
			                                  ? viewmodel.viewOrigin
			                                  : DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
			const float yaw = DirectX::XMConvertToRadians(options.hasYaw ? options.yaw : 0.0f);
			const float pitch = DirectX::XMConvertToRadians(options.hasPitch ? options.pitch : 0.0f);
			const DirectX::XMFLOAT3 target(eye.x + std::cos(pitch) * std::cos(yaw),
			                               eye.y + std::cos(pitch) * std::sin(yaw),
			                               eye.z + std::sin(pitch));
			camera.SetFirstPerson(eye, target);
			camera.SetFov(options.hasFov
			                  ? options.fov
			                  : std::clamp(settings.firstPersonFov * settings.viewmodelFovScale, 20.0f, 140.0f));
			if (!viewmodel.viewOriginValid) {
				std::printf("warning: no tag_view on the hands model, first-person origin at (0,0,0)\n");
			}
		} else {
			camera.SetFov(options.hasFov ? options.fov : settings.camera.fov);
			camera.Frame(viewmodel.boundsCenter, CameraRadius(viewmodel.boundsHalf));
			if (options.hasYaw || options.hasPitch) {
				camera.SetYawPitch(options.hasYaw ? options.yaw : camera.Yaw(),
				                   options.hasPitch ? options.pitch : camera.Pitch());
			}
			if (options.hasDistance) {
				camera.SetDistance(options.distance);
			}
		}
	} else if (!options.modelName.empty()) {
		std::string error;
		if (!wb::assets::LoadModelAsset(options.modelName, roots, model, error, true)) {
			std::fprintf(stderr, "cannot load model %s: %s\n", options.modelName.c_str(), error.c_str());
			return 1;
		}
		std::string uploadError;
		gpuModel = renderer.UploadModel(model, uploadError);
		if (!gpuModel) {
			std::fprintf(stderr, "cannot upload model %s: %s\n", options.modelName.c_str(),
			             uploadError.c_str());
			return 1;
		}
		std::printf("model: %s (%zu surfaces, %u bones, %zu materials, %zu images)\n",
		            model.model.name.c_str(), model.surfaceSet.surfaces.size(), model.model.numBones,
		            model.materials.size(), model.images.size());
		for (const std::string& warning : model.warnings) {
			std::printf("warning: %s\n", warning.c_str());
		}
		camera.SetFov(options.hasFov ? options.fov : settings.camera.fov);
		camera.Frame(model.boundsCenter, CameraRadius(model.boundsHalf));
		if (options.hasYaw || options.hasPitch) {
			camera.SetYawPitch(options.hasYaw ? options.yaw : camera.Yaw(),
			                   options.hasPitch ? options.pitch : camera.Pitch());
		}
		if (options.hasDistance) {
			camera.SetDistance(options.distance);
		}
	} else {
		ApplyCameraOptions(camera, options, settings);
	}

	if (!renderer.BeginScene(options.width, options.height)) {
		std::fprintf(stderr, "failed to begin the scene\n");
		return 1;
	}
	renderer.RenderGrid(camera);
	if (gpuGun) {
		if (gpuHands) {
			renderer.RenderModel(*gpuHands, camera, options.wire, nullptr, exposure);
		}
		renderer.RenderModel(*gpuGun, camera, options.wire, nullptr, exposure, &viewmodel.gunTransform);
	} else if (gpuModel) {
		renderer.RenderModel(*gpuModel, camera, options.wire, nullptr, exposure);
		if (options.bones) {
			DrawHeadlessBoneOverlay(renderer, model, camera, options.width, options.height, true);
		}
	}

	std::error_code ec;
	const std::filesystem::path parent = options.screenshotPath.parent_path();
	if (!parent.empty()) {
		std::filesystem::create_directories(parent, ec);
	}
	if (!renderer.SaveSceneToPng(options.screenshotPath)) {
		return 1;
	}

	std::printf("wrote %s (%dx%d, yaw %.1f pitch %.1f dist %.2f)\n", options.screenshotPath.string().c_str(),
	            options.width, options.height, camera.Yaw(), camera.Pitch(), camera.Distance());
	return 0;
}

int RunWindowed(const std::filesystem::path& exeDir, wb::Settings& settings, HINSTANCE instance,
                const Options& options) {
	const std::vector<std::filesystem::path> roots = wb::ResolveAssetRoots(settings, exeDir);
	wb::AssetIndex assets;
	assets.Scan(roots);

	ImGui_ImplWin32_EnableDpiAwareness();

	WNDCLASSEXW windowClass = {};
	windowClass.cbSize = sizeof(windowClass);
	windowClass.style = CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc = WndProc;
	windowClass.hInstance = instance;
	windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
	windowClass.lpszClassName = kWindowClass;
	if (RegisterClassExW(&windowClass) == 0) {
		std::fprintf(stderr, "RegisterClassExW failed\n");
		return 1;
	}

	RECT workArea = {};
	SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
	const int workWidth = workArea.right - workArea.left;
	const int workHeight = workArea.bottom - workArea.top;
	const int clientWidth = std::min(settings.windowWidth, workWidth > 320 ? workWidth - 80 : settings.windowWidth);
	const int clientHeight = std::min(settings.windowHeight, workHeight > 240 ? workHeight - 80 : settings.windowHeight);

	const DWORD windowStyle = WS_OVERLAPPEDWINDOW;
	RECT windowRect = {0, 0, clientWidth, clientHeight};
	AdjustWindowRectEx(&windowRect, windowStyle, FALSE, 0);

	HWND hwnd = CreateWindowExW(0, kWindowClass, kWindowTitle, windowStyle, CW_USEDEFAULT, CW_USEDEFAULT,
	                            windowRect.right - windowRect.left, windowRect.bottom - windowRect.top, nullptr,
	                            nullptr, instance, nullptr);
	if (hwnd == nullptr) {
		std::fprintf(stderr, "CreateWindowExW failed\n");
		return 1;
	}

	RECT clientRect = {};
	GetClientRect(hwnd, &clientRect);
	g_clientWidth = clientRect.right - clientRect.left;
	g_clientHeight = clientRect.bottom - clientRect.top;

	ShowWindow(hwnd, SW_SHOW);
	UpdateWindow(hwnd);

	wb::Renderer renderer;
	wb::RendererDesc desc;
	desc.hwnd = hwnd;
	desc.width = g_clientWidth;
	desc.height = g_clientHeight;
	if (!renderer.Initialize(desc)) {
		std::fprintf(stderr, "failed to initialize D3D11\n");
		DestroyWindow(hwnd);
		return 1;
	}
	g_rendererReady = true;

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigWindowsMoveFromTitleBarOnly = true;
	const std::string iniPath = (exeDir / "imgui.ini").string();
	io.IniFilename = iniPath.c_str();

	ImGui::StyleColorsDark();
	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowRounding = 4.0f;
	style.FrameRounding = 3.0f;
	style.GrabRounding = 3.0f;
	style.TabRounding = 3.0f;
	style.Colors[ImGuiCol_WindowBg] = ImVec4(0.086f, 0.090f, 0.106f, 1.0f);
	style.Colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.060f, 0.064f, 0.078f, 1.0f);

	if (!ImGui_ImplWin32_Init(hwnd)) {
		std::fprintf(stderr, "ImGui_ImplWin32_Init failed\n");
		renderer.Shutdown();
		DestroyWindow(hwnd);
		return 1;
	}
	if (!ImGui_ImplDX11_Init(renderer.Device(), renderer.Context())) {
		std::fprintf(stderr, "ImGui_ImplDX11_Init failed\n");
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
		renderer.Shutdown();
		DestroyWindow(hwnd);
		return 1;
	}
	g_imguiReady = true;

	const bool restoreLayout = std::filesystem::exists(exeDir / "imgui.ini");
	wb::Application app(settings, assets, renderer, exeDir, restoreLayout);
	if (!options.weaponName.empty()) {
		app.LoadWeapon(options.weaponName, options.handsName, options.viewFirstPerson);
	} else if (!options.modelName.empty()) {
		app.LoadModelByName(options.modelName);
	}

	bool quit = false;
	while (!quit) {
		MSG message;
		while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
			if (message.message == WM_QUIT) {
				quit = true;
				break;
			}
			TranslateMessage(&message);
			DispatchMessageW(&message);
		}
		if (quit || app.WantsClose()) {
			break;
		}

		if (g_resizePending) {
			renderer.ResizeSwapChain(g_clientWidth, g_clientHeight);
			g_resizePending = false;
		}

		if (g_minimized || g_clientWidth <= 0 || g_clientHeight <= 0) {
			Sleep(10);
			continue;
		}

		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		app.Frame();

		ImGui::Render();
		const float clearColor[4] = {0.06f, 0.064f, 0.078f, 1.0f};
		renderer.ClearBackBuffer(clearColor);
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
		renderer.Present();
	}

	wb::SaveSettings(exeDir, settings);

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
	g_imguiReady = false;

	renderer.Shutdown();
	g_rendererReady = false;

	if (IsWindow(hwnd)) {
		DestroyWindow(hwnd);
	}

	return 0;
}

} // namespace

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int) {
	CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	AttachParentConsole();

	int argc = 0;
	LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	if (argv == nullptr) {
		return 1;
	}

	Options options;
	bool valid = true;
	for (int i = 1; i < argc && valid; ++i) {
		const std::wstring arg = argv[i];
		auto nextValue = [&]() -> const wchar_t* {
			if (i + 1 >= argc) {
				return nullptr;
			}
			return argv[++i];
		};

		if (arg == L"-screenshot") {
			const wchar_t* value = nextValue();
			if (value == nullptr) {
				std::fputs("-screenshot needs a file name\n", stderr);
				valid = false;
			} else {
				options.mode = Mode::Screenshot;
				options.screenshotPath = std::filesystem::path(value);
			}
		} else if (arg == L"-info") {
			const wchar_t* value = nextValue();
			if (value == nullptr) {
				std::fputs("-info needs an xmodel name\n", stderr);
				valid = false;
			} else {
				options.mode = Mode::Info;
				options.infoName = std::filesystem::path(value).string();
			}
		} else if (arg == L"-validate") {
			options.mode = Mode::Validate;
		} else if (arg == L"-model") {
			const wchar_t* value = nextValue();
			if (value == nullptr) {
				std::fputs("-model needs an xmodel name\n", stderr);
				valid = false;
			} else {
				options.modelName = std::filesystem::path(value).string();
			}
		} else if (arg == L"-weapon") {
			const wchar_t* value = nextValue();
			if (value == nullptr) {
				std::fputs("-weapon needs a weapon name\n", stderr);
				valid = false;
			} else {
				options.weaponName = std::filesystem::path(value).string();
			}
		} else if (arg == L"-hands") {
			const wchar_t* value = nextValue();
			if (value == nullptr) {
				std::fputs("-hands needs a viewhands model name\n", stderr);
				valid = false;
			} else {
				options.handsName = std::filesystem::path(value).string();
			}
		} else if (arg == L"-view") {
			const wchar_t* value = nextValue();
			const std::wstring view = value != nullptr ? value : L"";
			if (view == L"firstperson" || view == L"fp") {
				options.viewFirstPerson = true;
				options.hasView = true;
			} else if (view == L"orbit") {
				options.viewFirstPerson = false;
				options.hasView = true;
			} else {
				std::fputs("-view needs 'firstperson' or 'orbit'\n", stderr);
				valid = false;
			}
		} else if (arg == L"-bones") {
			options.bones = true;
		} else if (arg == L"-wire") {
			options.wire = true;
		} else if (arg == L"-width") {
			valid = ParseIntArg(nextValue(), options.width) && options.width > 0;
		} else if (arg == L"-height") {
			valid = ParseIntArg(nextValue(), options.height) && options.height > 0;
		} else if (arg == L"-yaw") {
			valid = ParseFloatArg(nextValue(), options.yaw);
			options.hasYaw = valid;
		} else if (arg == L"-pitch") {
			valid = ParseFloatArg(nextValue(), options.pitch);
			options.hasPitch = valid;
		} else if (arg == L"-dist" || arg == L"-distance") {
			valid = ParseFloatArg(nextValue(), options.distance);
			options.hasDistance = valid;
		} else if (arg == L"-fov") {
			valid = ParseFloatArg(nextValue(), options.fov);
			options.hasFov = valid;
		} else if (arg == L"-exposure") {
			valid = ParseFloatArg(nextValue(), options.exposure) && options.exposure > 0.0f;
			options.hasExposure = valid;
		} else if (arg == L"-help" || arg == L"--help" || arg == L"-h" || arg == L"/?") {
			EnsureHeadlessConsole();
			PrintUsage();
			LocalFree(argv);
			CoUninitialize();
			return 0;
		} else {
			std::fprintf(stderr, "unknown argument: %ls\n", arg.c_str());
			valid = false;
		}
	}

	LocalFree(argv);

	if (!valid) {
		EnsureHeadlessConsole();
		PrintUsage();
		CoUninitialize();
		return 2;
	}
	if (!options.modelName.empty() && !options.weaponName.empty()) {
		EnsureHeadlessConsole();
		std::fputs("-model and -weapon cannot be combined\n", stderr);
		CoUninitialize();
		return 2;
	}

	const std::filesystem::path exeDir = ExeDirectory();
	wb::Settings settings = wb::LoadSettings(exeDir);
	if (options.hasExposure) {
		settings.exposure = options.exposure;
	}
	const std::vector<std::filesystem::path> roots = wb::ResolveAssetRoots(settings, exeDir);

	int result = 0;
	if (options.mode == Mode::Info) {
		EnsureHeadlessConsole();
		result = wb::PrintModelInfo(options.infoName, roots) ? 0 : 1;
	} else if (options.mode == Mode::Validate) {
		EnsureHeadlessConsole();
		result = wb::RunValidate(roots) == 0 ? 0 : 1;
	} else if (options.mode == Mode::Screenshot) {
		EnsureHeadlessConsole();
		result = RunHeadless(exeDir, settings, options);
	} else {
		result = RunWindowed(exeDir, settings, instance, options);
	}

	CoUninitialize();
	return result;
}
