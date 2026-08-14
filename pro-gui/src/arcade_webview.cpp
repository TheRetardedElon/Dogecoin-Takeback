#include "arcade_webview.h"
#include "arcade_host.h"

#include <atomic>
#include <cstdio>
#include <functional>
#include <string>
#include <utility>

#if defined(_WIN32)

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <objbase.h>
#include <shlwapi.h>

// WebView2 SDK headers only (no WRL — MinGW lacks wrl/event.h)
#include "WebView2.h"

#if defined(__MINGW32__)
// MinGW does not emit __uuidof() IIDs for WebView2 interfaces. Bind them
// to the SDK's IID_* constants so QueryInterface / Settings2 link.
template <>
const GUID& __mingw_uuidof<ICoreWebView2Settings2>()
{
    return IID_ICoreWebView2Settings2;
}
template <>
const GUID& __mingw_uuidof<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>()
{
    return IID_ICoreWebView2CreateCoreWebView2ControllerCompletedHandler;
}
template <>
const GUID& __mingw_uuidof<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>()
{
    return IID_ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler;
}
template <>
const GUID& __mingw_uuidof<ICoreWebView2NavigationCompletedEventHandler>()
{
    return IID_ICoreWebView2NavigationCompletedEventHandler;
}
template <>
const GUID& __mingw_uuidof<ICoreWebView2WebResourceRequestedEventHandler>()
{
    return IID_ICoreWebView2WebResourceRequestedEventHandler;
}
#endif

// ---------------------------------------------------------------------------
// Dynamic load of WebView2Loader.dll
// ---------------------------------------------------------------------------
typedef HRESULT(STDMETHODCALLTYPE* FnCreateEnv)(
    PCWSTR browserExecutableFolder,
    PCWSTR userDataFolder,
    ICoreWebView2EnvironmentOptions* environmentOptions,
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler* environmentCreatedHandler);

static HMODULE g_loader = nullptr;
static FnCreateEnv g_createEnv = nullptr;

static bool LoadWebView2Loader()
{
    if (g_createEnv)
        return true;
    wchar_t path[MAX_PATH];
    if (GetModuleFileNameW(nullptr, path, MAX_PATH)) {
        std::wstring p(path);
        auto slash = p.find_last_of(L"\\/");
        if (slash != std::wstring::npos)
            p = p.substr(0, slash + 1);
        p += L"WebView2Loader.dll";
        g_loader = LoadLibraryW(p.c_str());
    }
    if (!g_loader)
        g_loader = LoadLibraryW(L"WebView2Loader.dll");
    if (!g_loader)
        return false;
    g_createEnv = (FnCreateEnv)GetProcAddress(g_loader, "CreateCoreWebView2EnvironmentWithOptions");
    return g_createEnv != nullptr;
}

static std::wstring Widen(const std::string& s)
{
    if (s.empty())
        return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w((size_t)n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}

// ---------------------------------------------------------------------------
// Minimal COM handlers (no WRL)
// ---------------------------------------------------------------------------
template <typename TInterface, typename TLambda>
class ComHandler : public TInterface {
public:
    explicit ComHandler(TLambda fn) : fn_(std::move(fn)), refs_(1) {}

    // IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override
    {
        if (!ppv)
            return E_POINTER;
        if (riid == IID_IUnknown || riid == __uuidof(TInterface)) {
            *ppv = static_cast<TInterface*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs_; }
    ULONG STDMETHODCALLTYPE Release() override
    {
        ULONG r = --refs_;
        if (r == 0)
            delete this;
        return r;
    }

protected:
    TLambda fn_;
    std::atomic<ULONG> refs_;
};

class EnvDoneHandler
    : public ComHandler<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler,
                        std::function<void(HRESULT, ICoreWebView2Environment*)>> {
public:
    using Base = ComHandler<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler,
                            std::function<void(HRESULT, ICoreWebView2Environment*)>>;
    using Base::Base;
    HRESULT STDMETHODCALLTYPE Invoke(HRESULT result, ICoreWebView2Environment* env) override
    {
        fn_(result, env);
        return S_OK;
    }
};

class CtrlDoneHandler
    : public ComHandler<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler,
                        std::function<void(HRESULT, ICoreWebView2Controller*)>> {
public:
    using Base = ComHandler<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler,
                            std::function<void(HRESULT, ICoreWebView2Controller*)>>;
    using Base::Base;
    HRESULT STDMETHODCALLTYPE Invoke(HRESULT result, ICoreWebView2Controller* ctrl) override
    {
        fn_(result, ctrl);
        return S_OK;
    }
};

class ResourceHandler
    : public ComHandler<ICoreWebView2WebResourceRequestedEventHandler,
                        std::function<void(ICoreWebView2*, ICoreWebView2WebResourceRequestedEventArgs*)>> {
public:
    using Base = ComHandler<ICoreWebView2WebResourceRequestedEventHandler,
                            std::function<void(ICoreWebView2*, ICoreWebView2WebResourceRequestedEventArgs*)>>;
    using Base::Base;
    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2* s, ICoreWebView2WebResourceRequestedEventArgs* a) override
    {
        fn_(s, a);
        return S_OK;
    }
};

class NavDoneHandler
    : public ComHandler<ICoreWebView2NavigationCompletedEventHandler,
                        std::function<void(ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs*)>> {
public:
    using Base = ComHandler<ICoreWebView2NavigationCompletedEventHandler,
                            std::function<void(ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs*)>>;
    using Base::Base;
    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2* s, ICoreWebView2NavigationCompletedEventArgs* a) override
    {
        fn_(s, a);
        return S_OK;
    }
};

static LRESULT CALLBACK HostWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_ERASEBKGND)
        return 1;
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

ArcadeWebView::ArcadeWebView() = default;

ArcadeWebView::~ArcadeWebView()
{
    Shutdown();
}

bool ArcadeWebView::CreateHostWindow()
{
    static bool registered = false;
    if (!registered) {
        WNDCLASSW wc {};
        wc.lpfnWndProc = HostWndProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"DogecoinProArcadeHost";
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
        RegisterClassW(&wc);
        registered = true;
    }
    host_ = CreateWindowExW(
        0, L"DogecoinProArcadeHost", L"",
        WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        0, 0, 100, 100,
        parent_, nullptr, GetModuleHandleW(nullptr), nullptr);
    return host_ != nullptr;
}

bool ArcadeWebView::Init(HWND parent)
{
    if (ready_)
        return true;
    parent_ = parent;
    if (!parent_) {
        status_ = "No parent HWND";
        return false;
    }

    HRESULT co = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    (void)co;

    if (!LoadWebView2Loader()) {
        status_ = "WebView2Loader.dll missing next to exe";
        return false;
    }
    if (!host_ && !CreateHostWindow()) {
        status_ = "Failed to create arcade host window";
        return false;
    }
    ShowWindow(host_, SW_HIDE);
    visible_ = false;
    status_ = "Starting WebView2...";
    return StartEnvironment();
}

bool ArcadeWebView::StartEnvironment()
{
    wchar_t localApp[MAX_PATH];
    std::wstring userData;
    if (GetEnvironmentVariableW(L"LOCALAPPDATA", localApp, MAX_PATH))
        userData = std::wstring(localApp) + L"\\DogecoinCorePro\\WebView2";
    else
        userData = L".\\webview2-data";

    auto* self = this;
    // Create handler with refcount 1; CreateEnv will AddRef; we Release our copy after call
    auto* handler = new EnvDoneHandler([self](HRESULT result, ICoreWebView2Environment* env) {
        if (FAILED(result) || !env) {
            char buf[128];
            std::snprintf(buf, sizeof(buf),
                          "WebView2 env failed (0x%08lx) — install Edge WebView2 Runtime",
                          (unsigned long)result);
            self->status_ = buf;
            return;
        }
        self->OnEnvironmentCreated(env);
    });

    // nullptr options — set UA/headers after controller is ready
    HRESULT hr = g_createEnv(nullptr, userData.c_str(), nullptr, handler);
    handler->Release();

    if (FAILED(hr)) {
        char buf[96];
        std::snprintf(buf, sizeof(buf), "CreateEnv call failed 0x%08lx", (unsigned long)hr);
        status_ = buf;
        return false;
    }
    return true;
}

void ArcadeWebView::OnEnvironmentCreated(void* envPtr)
{
    auto* env = static_cast<ICoreWebView2Environment*>(envPtr);
    if (env_) {
        static_cast<ICoreWebView2Environment*>(env_)->Release();
        env_ = nullptr;
    }
    env_ = env;
    env->AddRef();

    auto* self = this;
    auto* handler = new CtrlDoneHandler([self](HRESULT result, ICoreWebView2Controller* controller) {
        if (FAILED(result) || !controller) {
            self->status_ = "WebView2 controller failed";
            return;
        }
        self->OnControllerCreated(controller);
    });
    env->CreateCoreWebView2Controller(host_, handler);
    handler->Release();
}

void ArcadeWebView::OnControllerCreated(void* controllerPtr)
{
    auto* controller = static_cast<ICoreWebView2Controller*>(controllerPtr);
    if (controller_) {
        static_cast<ICoreWebView2Controller*>(controller_)->Close();
        static_cast<ICoreWebView2Controller*>(controller_)->Release();
        controller_ = nullptr;
    }
    controller_ = controller;
    controller->AddRef();

    ICoreWebView2* webview = nullptr;
    if (FAILED(controller->get_CoreWebView2(&webview)) || !webview) {
        status_ = "get_CoreWebView2 failed";
        return;
    }
    if (webview_) {
        static_cast<ICoreWebView2*>(webview_)->Release();
        webview_ = nullptr;
    }
    webview_ = webview;

    ApplyUnlockIdentity();

    if (bw_ > 0 && bh_ > 0) {
        MoveWindow(host_, bx_, by_, bw_, bh_, TRUE);
        RECT r {0, 0, bw_, bh_};
        controller->put_Bounds(r);
    }

    controller->put_IsVisible(visible_ ? TRUE : FALSE);
    ready_ = true;
    status_ = "Cabinet ready";

    std::string url = pendingUrl_.empty() ? std::string(ArcadeHost::kHubUrl) : pendingUrl_;
    pendingUrl_.clear();
    Navigate(url);
}

void ArcadeWebView::ApplyUnlockIdentity()
{
    auto* webview = static_cast<ICoreWebView2*>(webview_);
    if (!webview)
        return;

    // User-Agent (ICoreWebView2Settings2)
    ICoreWebView2Settings* settings = nullptr;
    if (SUCCEEDED(webview->get_Settings(&settings)) && settings) {
        ICoreWebView2Settings2* s2 = nullptr;
        if (SUCCEEDED(settings->QueryInterface(__uuidof(ICoreWebView2Settings2), (void**)&s2)) && s2) {
            auto ua = Widen(ArcadeHost::CoreProUserAgent());
            s2->put_UserAgent(ua.c_str());
            s2->Release();
        }
        settings->Release();
    }

    webview->AddWebResourceRequestedFilter(L"*", COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);

    EventRegistrationToken token {};
    auto* resHandler = new ResourceHandler(
        [](ICoreWebView2*, ICoreWebView2WebResourceRequestedEventArgs* args) {
            ICoreWebView2WebResourceRequest* req = nullptr;
            if (FAILED(args->get_Request(&req)) || !req)
                return;
            ICoreWebView2HttpRequestHeaders* headers = nullptr;
            if (SUCCEEDED(req->get_Headers(&headers)) && headers) {
                headers->SetHeader(Widen(ArcadeHost::kHeaderName).c_str(),
                                   Widen(ArcadeHost::kHeaderValue).c_str());
                headers->SetHeader(Widen(ArcadeHost::kHeaderBuildName).c_str(),
                                   Widen(ArcadeHost::kBuildTag).c_str());
                headers->Release();
            }
            req->Release();
        });
    webview->add_WebResourceRequested(resHandler, &token);
    resHandler->Release();

    auto* self = this;
    auto* navHandler = new NavDoneHandler(
        [self](ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs* args) {
            BOOL ok = FALSE;
            args->get_IsSuccess(&ok);
            if (ok) {
                self->status_ = "IN CORE — cabinet unlocked";
                self->InjectCoreProScript();
            } else {
                self->status_ = "Navigation failed";
            }
            self->navigating_ = false;
        });
    webview->add_NavigationCompleted(navHandler, &token);
    navHandler->Release();
}

void ArcadeWebView::InjectCoreProScript()
{
    auto* webview = static_cast<ICoreWebView2*>(webview_);
    if (!webview)
        return;
    std::wstring js =
        L"window.__DOGECOIN_CORE_PRO__=true;"
        L"window.__DOGECOIN_CORE_PRO_BUILD__='" +
        Widen(ArcadeHost::kBuildTag) +
        L"';"
        L"try{window.dispatchEvent(new Event('dogecoin-core-pro-ready'));}catch(e){}";
    webview->ExecuteScript(js.c_str(), nullptr);
}

void ArcadeWebView::Shutdown()
{
    ready_ = false;
    visible_ = false;
    if (controller_) {
        auto* c = static_cast<ICoreWebView2Controller*>(controller_);
        c->Close();
        c->Release();
        controller_ = nullptr;
    }
    if (webview_) {
        static_cast<ICoreWebView2*>(webview_)->Release();
        webview_ = nullptr;
    }
    if (env_) {
        static_cast<ICoreWebView2Environment*>(env_)->Release();
        env_ = nullptr;
    }
    if (host_) {
        DestroyWindow(host_);
        host_ = nullptr;
    }
    status_ = "Arcade WebView closed";
}

void ArcadeWebView::Show(bool visible)
{
    visible_ = visible;
    if (host_)
        ShowWindow(host_, visible ? SW_SHOW : SW_HIDE);
    if (controller_)
        static_cast<ICoreWebView2Controller*>(controller_)->put_IsVisible(visible ? TRUE : FALSE);
}

void ArcadeWebView::SetBounds(int x, int y, int w, int h)
{
    bx_ = x;
    by_ = y;
    bw_ = w > 1 ? w : 1;
    bh_ = h > 1 ? h : 1;
    if (host_)
        MoveWindow(host_, bx_, by_, bw_, bh_, TRUE);
    if (controller_) {
        RECT r {0, 0, bw_, bh_};
        static_cast<ICoreWebView2Controller*>(controller_)->put_Bounds(r);
    }
}

void ArcadeWebView::Resize()
{
    SetBounds(bx_, by_, bw_, bh_);
}

void ArcadeWebView::Navigate(const std::string& url)
{
    std::string u = url.empty() ? std::string(ArcadeHost::kHubUrl) : url;
    if (!ready_ || !webview_) {
        pendingUrl_ = u;
        status_ = "Waiting for WebView2...";
        return;
    }
    navigating_ = true;
    status_ = "Loading cabinet...";
    static_cast<ICoreWebView2*>(webview_)->Navigate(Widen(u).c_str());
}

#else

ArcadeWebView::ArcadeWebView() = default;
ArcadeWebView::~ArcadeWebView() = default;
bool ArcadeWebView::Init(void*)
{
    status_ = "Arcade embed is Windows/WebView2 only";
    return false;
}
void ArcadeWebView::Shutdown() {}
void ArcadeWebView::Show(bool) {}
void ArcadeWebView::SetBounds(int, int, int, int) {}
void ArcadeWebView::Navigate(const std::string&) {}
void ArcadeWebView::Resize() {}

#endif
