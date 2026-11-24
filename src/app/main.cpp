#include <Windows.h>
#include <objbase.h>
#include <shellapi.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <filesystem>
#include <iterator>
#include <string>
#include <vector>

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include "app/application.h"
#include "app/settings.h"
#include "assets/asset_index.h"
#include "render/camera.h"
#include "render/renderer.h"

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

struct Options {
	bool headless = false;
	std::filesystem::path screenshotPath;
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
	    "  hmw-workbench.exe -screenshot <file.png> [-width <n>] [-height <n>]\n"
	    "                    [-yaw <deg>] [-pitch <deg>] [-dist <units>] [-fov <deg>]\n"
	    "      Render the viewport offscreen (no window) and exit.\n"
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

int RunHeadless(const std::filesystem::path& exeDir, const wb::Settings& settings, const Options& options) {
	std::printf("hmw-workbench M0 (headless)\n");

	const std::vector<std::filesystem::path> roots = wb::ResolveAssetRoots(settings, exeDir);
	wb::AssetIndex assets;
	assets.Scan(roots);

	std::printf("asset roots:\n");
	for (size_t i = 0; i < assets.Roots().size(); ++i) {
		const wb::AssetRoot& root = assets.Roots()[i];
		if (root.exists) {
			std::printf("  [%zu] %s\n        %d weapons, %d models, %d anims\n", i + 1, root.path.string().c_str(),
			            root.weaponCount, root.modelCount, root.animCount);
		} else {
			std::printf("  [%zu] %s (missing)\n", i + 1, root.path.string().c_str());
		}
	}
	std::printf("assets: %d weapons, %d models, %d anims (%d total, duplicates shadowed by earlier roots)\n",
	            assets.Count(wb::AssetKind::Weapon), assets.Count(wb::AssetKind::Model),
	            assets.Count(wb::AssetKind::Anim), assets.TotalCount());

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
	ApplyCameraOptions(camera, options, settings);

	const float aspect = static_cast<float>(options.width) / static_cast<float>(options.height);
	DirectX::XMFLOAT4X4 viewProjection = {};
	DirectX::XMStoreFloat4x4(&viewProjection, camera.ViewProjection(aspect));
	if (!renderer.RenderGrid(viewProjection, options.width, options.height)) {
		std::fprintf(stderr, "failed to render the grid\n");
		return 1;
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

int RunWindowed(const std::filesystem::path& exeDir, wb::Settings& settings, HINSTANCE instance) {
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
				options.headless = true;
				options.screenshotPath = std::filesystem::path(value);
			}
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

	const std::filesystem::path exeDir = ExeDirectory();
	wb::Settings settings = wb::LoadSettings(exeDir);

	int result = 0;
	if (options.headless) {
		EnsureHeadlessConsole();
		result = RunHeadless(exeDir, settings, options);
	} else {
		result = RunWindowed(exeDir, settings, instance);
	}

	CoUninitialize();
	return result;
}
