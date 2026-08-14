// Dogecoin Core Pro — Dear ImGui shell (spike)
// GLFW + OpenGL3 + docking. Headless node talks RPC later.
//
// Potato / bg-friendly loop:
//   - V-Sync (swap interval 1) — no 144 FPS wallet tax
//   - glfwWaitEventsTimeout — sleep when idle; wake for input or ~1 Hz status

#include "app.h"
#include "theme.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>
#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <dwmapi.h>
#include <windows.h>
#include <windowsx.h>
#include "win_tray.h"
#endif

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION_MAIN_ICON
// Use existing stb from texture unit — declare load for icon only via local include of stb with no impl
// (implementation already in texture.cpp). Just declare stbi_load.
extern "C" {
unsigned char* stbi_load(char const* filename, int* x, int* y, int* channels_in_file, int desired_channels);
void stbi_image_free(void* retval_from_stbi_load);
}

// Wake at least this often so RPC status / IBD bars can tick without spinning.
static const double kIdleWakeSec = 0.35;

static void GlfwError(int code, const char* desc)
{
    std::fprintf(stderr, "GLFW error %d: %s\n", code, desc);
}

static std::string FindAssets()
{
    // Prefer next to binary, then ../assets, then source-tree relative
    const char* candidates[] = {
        "assets",
        "./assets",
        "../assets",
        "../../pro-gui/assets",
        "pro-gui/assets",
    };
    for (const char* c : candidates) {
        std::string p = std::string(c) + "/catalog.json";
        FILE* f = std::fopen(p.c_str(), "rb");
        if (f) {
            std::fclose(f);
            return c;
        }
    }
    return "assets";
}

#if defined(_WIN32)
static WNDPROC g_prevWndProc = nullptr;
static RECT g_captionDragClient = {0, 0, 0, 0};
static int g_captionBtnLeft = 0;
static int g_captionMenuRight = 140;
static bool g_windowMaximized = false;

void ProGuiSetCaptionDragRect(int x0, int y0, int x1, int y1)
{
    g_captionDragClient.left = x0;
    g_captionDragClient.top = y0;
    g_captionDragClient.right = x1;
    g_captionDragClient.bottom = y1;
}

void ProGuiSetCaptionButtonLeft(int x)
{
    g_captionBtnLeft = x;
}

void ProGuiSetCaptionMenuRight(int x)
{
    g_captionMenuRight = x;
}

static LRESULT CALLBACK BorderlessWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_NCCALCSIZE && wParam == TRUE)
        return 0;
    if (msg == WM_NCHITTEST) {
        POINT p{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        RECT wr;
        GetWindowRect(hWnd, &wr);
        const int x = p.x - wr.left;
        const int y = p.y - wr.top;
        const int w = wr.right - wr.left;
        const int h = wr.bottom - wr.top;
        const int btnLeft = (g_captionBtnLeft > 0) ? g_captionBtnLeft : (w - 80);
        const int menuRight = (g_captionMenuRight > 8) ? g_captionMenuRight : 140;
        const int captionH = 42;
        // File / Help / min / close must stay client clicks. Never HTTOP over them —
        // the old 8px top-resize strip ate most of the menu buttons.
        if (y < captionH) {
            if (x >= btnLeft)
                return HTCLIENT;
            if (x < menuRight)
                return HTCLIENT;
            if (x >= g_captionDragClient.left && x < g_captionDragClient.right &&
                y >= g_captionDragClient.top && y < g_captionDragClient.bottom)
                return HTCAPTION;
            return HTCAPTION;
        }
        const int B = g_windowMaximized ? 0 : 8;
        if (B > 0) {
            const bool left = x < B;
            const bool bottom = y >= h - B;
            if (bottom && left) return HTBOTTOMLEFT;
            if (bottom && x >= w - B) return HTBOTTOMRIGHT;
            if (left) return HTLEFT;
            if (x >= w - B) return HTRIGHT;
            if (bottom) return HTBOTTOM;
        }
        return HTCLIENT;
    }
    if (msg == WM_GETMINMAXINFO) {
        auto* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
        mmi->ptMinTrackSize.x = 880;
        mmi->ptMinTrackSize.y = 560;
        return 0;
    }
    if (msg == WM_SIZE)
        g_windowMaximized = (wParam == SIZE_MAXIMIZED);
    if (WinTrayHandleMessage(msg, wParam, lParam))
        return 0;
    return CallWindowProc(g_prevWndProc, hWnd, msg, wParam, lParam);
}
#endif

static bool LaunchWantsTestnet(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i] ? argv[i] : "";
        if (std::strcmp(a, "--testnet") == 0 || std::strcmp(a, "-testnet") == 0)
            return true;
    }
    return false;
}

/** Window / taskbar icon. Testnet uses the green coin so the taskbar matches Start Menu. */
static void SetWindowDogecoinIcon(GLFWwindow* window, const std::string& assets, bool testnet)
{
    const char* namesGold[] = {"/brand/dogecoin256.png", "/brand/dogecoin32.png"};
    const char* namesTest[] = {"/brand/dogecoin-testnet256.png", "/brand/dogecoin-testnet32.png"};
    const char** names = testnet ? namesTest : namesGold;
    GLFWimage images[2];
    int count = 0;
    unsigned char* pixels[2] = {nullptr, nullptr};
    for (int i = 0; i < 2; ++i) {
        std::string path = assets + names[i];
        int w = 0, h = 0, n = 0;
        pixels[i] = stbi_load(path.c_str(), &w, &h, &n, 4);
        if (pixels[i] && w > 0 && h > 0) {
            images[count].width = w;
            images[count].height = h;
            images[count].pixels = pixels[i];
            count++;
        }
    }
    if (count > 0)
        glfwSetWindowIcon(window, count, images);
    for (int i = 0; i < 2; ++i) {
        if (pixels[i])
            stbi_image_free(pixels[i]);
    }
}

int main(int argc, char** argv)
{
#if defined(_WIN32)
    char exePath[MAX_PATH] = {};
    if (GetModuleFileNameA(nullptr, exePath, MAX_PATH) > 0) {
        std::string inst = exePath;
        auto slash = inst.find_last_of("/\\");
        if (slash != std::string::npos)
            inst = inst.substr(0, slash);
        if (!HybridHasExplicitUiFlag(argc, argv) && HybridShouldPrompt(inst)) {
            if (RelaunchHybridPicker(inst))
                return 0;
        }
    }
#endif
    glfwSetErrorCallback(GlfwError);
    if (!glfwInit())
        return 1;

#if defined(__APPLE__)
    const char* glsl = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
    const char* glsl = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    const bool testnetLaunch = LaunchWantsTestnet(argc, argv);
    GLFWwindow* window = glfwCreateWindow(1280, 800,
                                          testnetLaunch ? "Dogecoin Core Pro  —  TESTNET" : "Dogecoin Core Pro",
                                          nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return 1;
    }
#if defined(_WIN32)
    if (HWND hwnd = glfwGetWin32Window(window)) {
        LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
        style |= WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU;
        style &= ~(WS_CAPTION);
        SetWindowLongPtr(hwnd, GWL_STYLE, style);
        MARGINS shadow = {1, 1, 1, 1};
        DwmExtendFrameIntoClientArea(hwnd, &shadow);
        g_prevWndProc = (WNDPROC)SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)BorderlessWndProc);
        SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                     SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
    }
#endif
    glfwMakeContextCurrent(window);
    // Cap to display refresh — wallet UI does not need uncapped FPS.
    glfwSwapInterval(1);

    const std::string assetsEarly = FindAssets();
    SetWindowDogecoinIcon(window, assetsEarly, testnetLaunch);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // optional multi-viewport

    ApplyProTheme(ProTheme::GoldDark);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl);

    App app;
    std::fprintf(stderr, "pro-gui assets: %s\n", assetsEarly.c_str());
    if (!app.Init(assetsEarly, argc, argv)) {
        std::fprintf(stderr, "App::Init failed\n");
        return 1;
    }
    app.SetHostWindow(window); // WebView2 arcade embed parent HWND
    // Re-apply icon after assets are confirmed next to the binary (POST_BUILD copy)
    SetWindowDogecoinIcon(window, assetsEarly, testnetLaunch || app.IsTestnet());

    // Caption / OS close → tray. File→Exit / tray Quit stop the node then exit.
    while (!app.IsShutdownComplete()) {
        if (glfwWindowShouldClose(window) && !app.IsShuttingDown()) {
            glfwSetWindowShouldClose(window, GLFW_FALSE);
            app.RequestHideToTray();
        }
        if (app.wantsClose && app.IsShutdownComplete())
            break;

        // Sleep until input OR timeout (IBD/status refresh). Avoids hot-loop GPU tax
        // that would steal cycles from dogecoind on low-end hosts.
        const double wake = app.IsShuttingDown() ? 0.25 : kIdleWakeSec;
        glfwWaitEventsTimeout(wake);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        app.Frame();

        ImGui::Render();
        int dw, dh;
        glfwGetFramebufferSize(window, &dw, &dh);
        glViewport(0, 0, dw, dh);
        glClearColor(0.04f, 0.04f, 0.06f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    app.Shutdown(); // settings only — node already stopped by RequestExit path
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
