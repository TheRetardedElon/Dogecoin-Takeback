#pragma once

#include <string>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

/**
 * In-client GPE arcade cabinet (Windows WebView2).
 * Fills the Arcade content rect — no external browser popup.
 * Sends Core Pro unlock identity (UA + headers + JS inject).
 */
class ArcadeWebView {
public:
    ArcadeWebView();
    ~ArcadeWebView();

#if defined(_WIN32)
    /** parent = main GLFW HWND. Creates child host + WebView2. */
    bool Init(HWND parent);
#else
    bool Init(void* parent);
#endif

    void Shutdown();
    void Show(bool visible);
    /** Bounds in parent client coordinates. */
    void SetBounds(int x, int y, int w, int h);
    void Navigate(const std::string& url);
    void Resize();

    bool ready() const { return ready_; }
    bool visible() const { return visible_; }
    const std::string& status() const { return status_; }

private:
#if defined(_WIN32)
    HWND parent_ = nullptr;
    HWND host_ = nullptr;
    void* controller_ = nullptr; // ICoreWebView2Controller*
    void* webview_ = nullptr;    // ICoreWebView2*
    void* env_ = nullptr;        // ICoreWebView2Environment*
#endif
    bool ready_ = false;
    bool visible_ = false;
    bool navigating_ = false;
    std::string status_ = "Arcade WebView idle";
    std::string pendingUrl_;
    int bx_ = 0, by_ = 0, bw_ = 0, bh_ = 0;

#if defined(_WIN32)
    bool CreateHostWindow();
    bool StartEnvironment();
    void OnEnvironmentCreated(void* env);
    void OnControllerCreated(void* controller);
    void ApplyUnlockIdentity();
    void InjectCoreProScript();
#endif
};
