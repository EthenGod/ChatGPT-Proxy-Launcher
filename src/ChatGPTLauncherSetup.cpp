// SPDX-License-Identifier: GPL-3.0-only
#include "SetupCore.h"
#include "SetupResources.h"
#include "StartupIntegration.h"
#include "ProxyDetection.h"
#include <commctrl.h>
#include <shlobj.h>
#include <shellapi.h>
#include <uxtheme.h>
#include <dwmapi.h>
#include <gdiplus.h>
#include <windowsx.h>
#include <algorithm>
#include <memory>
#include <thread>
#include <vector>

namespace {
constexpr UINT SavedMessage = WM_APP + 1;
constexpr UINT DetectPortMessage = WM_APP + 2;
constexpr UINT PortDetectedMessage = WM_APP + 3;
constexpr int FooterHeight = 112;
enum ControlId {
    Title = 1000, Subtitle, State, StartTitle, Startup, Silent, SilentHint,
    ProxyTitle, Proxy, ProtocolLabel, Protocol, ProtocolHttps, ProtocolSocks, HostLabel, Host, PortLabel, Port,
    PreviewTitle, Preview, PreviewHint, Advanced, DirectoryLabel, Directory, Browse, Reset,
    DirectoryHint, BypassLabel, Bypass, ExtraLabel, Extra, Error, Footer, ApplyButton, CloseButton
};
struct Palette {
    COLORREF background, surface, text, secondary, border, accent, error;
};
struct Control { int id; HWND window; int font; };
struct Completion {
    setup::Result result;
    std::wstring target, previous;
    bool checkStartup = false;
    setup::StartupReport startupReport;
};
bool IsSwitch(int id) { return id == Startup || id == Silent || id == Proxy; }
bool IsSegment(int id) { return id == Protocol || id == ProtocolHttps || id == ProtocolSocks; }
bool IsField(int id) { return id == Host || id == Port || id == Directory || id == Bypass || id == Extra || id == Preview; }
bool IsButton(int id) {
    return IsSwitch(id) || IsSegment(id) || id == Advanced || id == Browse || id == Reset || id == CloseButton || id == ApplyButton;
}
bool IsActionButton(int id) { return IsButton(id) && !IsSwitch(id) && !IsSegment(id); }
void Rounded(HDC dc, const RECT& rect, int radius, COLORREF fill, COLORREF border, float thickness = 1.0f) {
    if (rect.right <= rect.left || rect.bottom <= rect.top) return;
    Gdiplus::Graphics g(dc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    const float inset = thickness / 2;
    const float x = static_cast<float>(rect.left) + inset, y = static_cast<float>(rect.top) + inset;
    const float w = static_cast<float>(rect.right - rect.left) - thickness;
    const float h = static_cast<float>(rect.bottom - rect.top) - thickness;
    const float diameter = std::min(static_cast<float>(radius * 2), std::min(w, h));
    if (diameter <= 0) return;
    Gdiplus::GraphicsPath path;
    path.AddArc(x, y, diameter, diameter, 180, 90);
    path.AddArc(x + w - diameter, y, diameter, diameter, 270, 90);
    path.AddArc(x + w - diameter, y + h - diameter, diameter, diameter, 0, 90);
    path.AddArc(x, y + h - diameter, diameter, diameter, 90, 90); path.CloseFigure();
    Gdiplus::SolidBrush brush(Gdiplus::Color(255, GetRValue(fill), GetGValue(fill), GetBValue(fill)));
    Gdiplus::Pen pen(Gdiplus::Color(255, GetRValue(border), GetGValue(border), GetBValue(border)), thickness);
    g.FillPath(&brush, &path); g.DrawPath(&pen, &path);
}
LRESULT CALLBACK StyledControl(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
struct App {
    HINSTANCE instance = nullptr;
    HWND window = nullptr, body = nullptr;
    UINT dpi = 96;
    bool advanced = false, loading = true, dirty = false, busy = false, fatal = false;
    bool savedThisSession = false, startupReminderShown = false;
    bool dark = false, contrast = false, draggingScroll = false;
    int scrollGrab = 0, wheelRemainder = 0;
    unsigned layoutPasses = 0, paintPasses = 0;
    int scroll = 0, contentHeight = 0;
    std::wstring savedTarget;
    setup::Paths paths;
    setup::Payload payload{};
    Palette colors{};
    HBRUSH background = nullptr, surface = nullptr, field = nullptr;
    HFONT fonts[5]{};
    std::vector<Control> controls;
    std::thread worker;
    std::thread portWorker;
    bool detectingPort = false;
    unsigned editRevision = 0, detectionRevision = 0;
    setup::ProxyDetection detectedPort;
    ~App() {
        if (worker.joinable()) worker.join();
        if (portWorker.joinable()) portWorker.join();
        for (HFONT font : fonts) if (font) DeleteObject(font);
        if (background) DeleteObject(background);
        if (surface) DeleteObject(surface);
        if (field) DeleteObject(field);
    }
    int S(int value) const { return MulDiv(value, static_cast<int>(dpi), 96); }
    HWND Get(int id) const {
        for (const auto& item : controls) if (item.id == id) return item.window;
        return nullptr;
    }
    std::wstring Text(int id) const {
        const HWND h = Get(id);
        const int n = GetWindowTextLengthW(h);
        std::wstring text(static_cast<size_t>(n) + 1, L'\0');
        GetWindowTextW(h, &text[0], n + 1); text.resize(static_cast<size_t>(n));
        return text;
    }
    void Set(int id, const std::wstring& text) {
        if (Text(id) != text) SetWindowTextW(Get(id), text.c_str());
    }
    bool Checked(int id) const { return SendMessageW(Get(id), BM_GETCHECK, 0, 0) == BST_CHECKED; }
    void Check(int id, bool value) { SendMessageW(Get(id), BM_SETCHECK, value ? BST_CHECKED : BST_UNCHECKED, 0); }
    void Theme(int mode = -1);
    void CreateControls();
    void Layout();
    void Refresh(bool edited);
    void DetectPort();
    void FinishPortDetection();
    void RestoreOptions(const setup::Loaded& loaded);
    void Command(int id, int notification);
    void Paint(HWND target);
    void DrawSurface(HWND target, HDC dc);
    void DrawControl(HWND control, HDC dc);
    void PaintControl(HWND control, HDC output);
    void ScrollTo(int position);
    RECT ScrollThumb() const;
    COLORREF FieldColor() const { return contrast ? colors.surface : dark ? RGB(28,30,36) : RGB(247,248,251); }
    setup::Options Options() const;
};
App* StateOf(HWND window) { return reinterpret_cast<App*>(GetWindowLongPtrW(window, GWLP_USERDATA)); }
void ShowSetupError(HWND window, const setup::Result& result, PCWSTR title) {
    std::wstring message = result.message;
    if (!result.conflictDirectory.empty())
        message += L"\n\n关闭此提示后，将打开冲突文件所在的文件夹：\n" + result.conflictDirectory;
    MessageBoxW(window, message.c_str(), title, MB_OK | MB_ICONERROR);
    if (result.conflictDirectory.empty()) return;
    SHELLEXECUTEINFOW info{};
    info.cbSize = sizeof(info); info.fMask = SEE_MASK_FLAG_NO_UI;
    info.hwnd = window; info.lpVerb = L"open";
    info.lpFile = result.conflictDirectory.c_str(); info.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&info)) {
        const DWORD error = GetLastError();
        const std::wstring failure = L"无法打开文件夹，请手动查看：\n" + result.conflictDirectory + L"\n\n" + setup::ErrorText(error);
        MessageBoxW(window, failure.c_str(), L"无法打开文件夹", MB_OK | MB_ICONERROR);
    }
}

void App::Theme(int mode) {
    HIGHCONTRASTW hc{ sizeof(hc), 0, nullptr };
    SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(hc), &hc, 0);
    contrast = (hc.dwFlags & HCF_HIGHCONTRASTON) != 0;
    DWORD light = 1, bytes = sizeof(light);
    RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &light, &bytes);
    dark = !light && !contrast;
    if (mode >= 0) { dark = mode == 1; contrast = mode == 2; }
    if (contrast) colors = { GetSysColor(COLOR_WINDOW), GetSysColor(COLOR_WINDOW), GetSysColor(COLOR_WINDOWTEXT),
        GetSysColor(COLOR_WINDOWTEXT), GetSysColor(COLOR_WINDOWTEXT), GetSysColor(COLOR_HIGHLIGHT), GetSysColor(COLOR_WINDOWTEXT) };
    else if (dark) colors = { RGB(24,25,29), RGB(35,37,43), RGB(244,245,248), RGB(171,178,192),
        RGB(63,67,77), RGB(77,146,255), RGB(255,155,149) };
    else colors = { RGB(245,246,249), RGB(255,255,255), RGB(30,37,49), RGB(94,106,124),
        RGB(222,227,235), RGB(0,99,225), RGB(172,40,37) };
    if (background) DeleteObject(background);
    if (surface) DeleteObject(surface);
    if (field) DeleteObject(field);
    background = CreateSolidBrush(colors.background); surface = CreateSolidBrush(colors.surface);
    field = CreateSolidBrush(FieldColor());
    const int sizes[] = { 15, 15, 28, 12, 13 };
    for (int i = 0; i < 5; ++i) {
        if (fonts[i]) DeleteObject(fonts[i]);
        fonts[i] = CreateFontW(-S(sizes[i]), 0, 0, 0, i == 1 || i == 2 ? FW_SEMIBOLD : FW_NORMAL,
            FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");
    }
    for (const auto& item : controls) {
        SendMessageW(item.window, WM_SETFONT, reinterpret_cast<WPARAM>(fonts[item.font]), TRUE);
        SetWindowTheme(item.window, L"", L"");
    }
    const BOOL useDark = dark;
    DwmSetWindowAttribute(window, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &useDark, sizeof(useDark));
    if (body) InvalidateRect(body, nullptr, TRUE);
    InvalidateRect(window, nullptr, TRUE);
}

void App::CreateControls() {
    auto add = [&](int id, PCWSTR type, PCWSTR text, DWORD style, int font = 0, bool footer = false) {
        if (IsActionButton(id)) style = (style & ~BS_TYPEMASK) | BS_OWNERDRAW;
        HWND h = CreateWindowExW(0, type, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 0, 0,
            footer ? window : body, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance, nullptr);
        controls.push_back({ id, h, font });
        SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(fonts[font]), FALSE);
        if (IsButton(id) || IsField(id) || id == Footer) SetWindowSubclass(h, StyledControl, 1, reinterpret_cast<DWORD_PTR>(this));
        return h;
    };
    const DWORD label = SS_LEFT | SS_NOPREFIX;
    const DWORD edit = WS_TABSTOP | ES_AUTOHSCROLL;
    add(Title, L"STATIC", L"设置你的启动方式", label, 2);
    add(Subtitle, L"STATIC", L"ChatGPT 轻量启动器  /  部署与配置", label, 0);
    add(State, L"STATIC", L"尚未部署 · 仅对当前 Windows 用户生效", label, 3);
    add(StartTitle, L"STATIC", L"启动设置", label, 1);
    add(Startup, L"BUTTON", L"登录 Windows 后自动启动", WS_TABSTOP | BS_AUTOCHECKBOX);
    add(Silent, L"BUTTON", L"启动时不显示主窗口", WS_TABSTOP | BS_AUTOCHECKBOX);
    add(SilentHint, L"STATIC", L"保存后尝试关闭应用自带自启，并打开 Windows 设置供你确认。", label, 3);
    add(ProxyTitle, L"STATIC", L"代理连接", label, 1);
    add(Proxy, L"BUTTON", L"使用指定代理", WS_TABSTOP | BS_AUTOCHECKBOX);
    add(ProtocolLabel, L"STATIC", L"协议", label, 3);
    add(Protocol, L"BUTTON", L"HTTP", WS_TABSTOP | WS_GROUP | BS_AUTORADIOBUTTON, 3);
    add(ProtocolHttps, L"BUTTON", L"HTTPS", WS_TABSTOP | BS_AUTORADIOBUTTON, 3);
    add(ProtocolSocks, L"BUTTON", L"SOCKS5", WS_TABSTOP | BS_AUTORADIOBUTTON, 3);
    Check(Protocol, true);
    add(HostLabel, L"STATIC", L"地址", label, 3);
    add(Host, L"EDIT", L"127.0.0.1", edit | WS_GROUP);
    add(PortLabel, L"STATIC", L"端口", label, 3);
    add(Port, L"EDIT", L"", edit);
    SendMessageW(Get(Port), EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"点击检测"));
    SendMessageW(Get(Port), EM_SETLIMITTEXT, 5, 0);
    add(PreviewTitle, L"STATIC", L"启动参数预览", label, 1);
    add(Preview, L"EDIT", L"", WS_TABSTOP | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL);
    SendMessageW(Get(Preview), EM_SETLIMITTEXT, 32767, 0);
    add(PreviewHint, L"STATIC", L"实时显示将写入快捷方式的参数，可选中复制。空白表示不附加参数。", label, 3);
    add(Advanced, L"BUTTON", L"高级", WS_TABSTOP | BS_OWNERDRAW, 4, true);
    add(DirectoryLabel, L"STATIC", L"部署目录", label, 1);
    add(Directory, L"EDIT", L"", edit);
    add(Browse, L"BUTTON", L"浏览…", WS_TABSTOP | BS_PUSHBUTTON);
    add(Reset, L"BUTTON", L"恢复默认", WS_TABSTOP | BS_PUSHBUTTON);
    add(DirectoryHint, L"STATIC", L"更改目录后会重新部署，并更新快捷方式；旧目录中的文件会保留。", label, 3);
    add(BypassLabel, L"STATIC", L"不走代理的地址（选填，用分号分隔）", label, 0);
    add(Bypass, L"EDIT", L"", edit);
    SendMessageW(Get(Bypass), EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"例如 localhost;*.example.com"));
    add(ExtraLabel, L"STATIC", L"额外启动参数（选填）", label, 0);
    add(Extra, L"EDIT", L"", edit);
    SendMessageW(Get(Extra), EM_SETLIMITTEXT, static_cast<WPARAM>(setup::MaxArguments), 0);
    add(Error, L"STATIC", L"", label, 3);
    add(Footer, L"STATIC", L"保存后即可使用", SS_OWNERDRAW, 4, true);
    add(CloseButton, L"BUTTON", L"关闭", WS_TABSTOP | BS_PUSHBUTTON, 0, true);
    add(ApplyButton, L"BUTTON", L"部署并保存", WS_TABSTOP | BS_PUSHBUTTON, 1, true);
}

void App::Layout() {
    if (!body) return;
    ++layoutPasses;
    RECT root{}; GetClientRect(window, &root);
    const int footerHeight = S(FooterHeight);
    SetWindowPos(body, nullptr, 0, 0, root.right, std::max(1L, root.bottom - footerHeight), SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW);
    RECT area{}; GetClientRect(body, &area);
    contentHeight = S(674);
    scroll = std::clamp(scroll, 0, std::max(0, contentHeight - static_cast<int>(area.bottom)));
    const int width = MulDiv(area.right, 96, static_cast<int>(dpi));
    const int inner = width - 96;
    HDWP batch = BeginDeferWindowPos(static_cast<int>(controls.size()));
    auto place = [&](int id, int x, int y, int w, int h, bool visible = true) {
        HWND control = Get(id);
        const UINT flags = SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW | (visible ? SWP_SHOWWINDOW : SWP_HIDEWINDOW);
        if (batch) batch = DeferWindowPos(batch, control, nullptr, S(x), S(y) - scroll, S(w), S(h), flags);
        else SetWindowPos(control, nullptr, S(x), S(y) - scroll, S(w), S(h), flags);
    };
    place(Title, 32, 26, width - 64, 42);
    place(Subtitle, 34, 76, width - 68, 24);
    place(State, 34, 108, width - 68, 20);
    place(StartTitle, 48, 154, inner, 24, !advanced);
    place(Startup, 48, 184, inner, 34, !advanced);
    place(SilentHint, 48, 224, inner, 22, !advanced);
    place(Silent, 48, 254, inner, 34, !advanced);
    place(ProxyTitle, 48, 338, inner, 24, !advanced);
    place(Proxy, 48, 367, inner, 34, !advanced);
    place(ProtocolLabel, 48, 414, 208, 20, !advanced);
    place(Protocol, 48, 440, 66, 40, !advanced);
    place(ProtocolHttps, 119, 440, 66, 40, !advanced);
    place(ProtocolSocks, 190, 440, 66, 40, !advanced);
    place(HostLabel, 272, 414, inner - 344, 20, !advanced);
    place(Host, 284, 450, inner - 368, 22, !advanced);
    place(PortLabel, width - 152, 414, 104, 20, !advanced);
    place(Port, width - 140, 450, 80, 22, !advanced);
    place(PreviewTitle, 34, 526, width - 68, 24);
    place(Preview, 46, 569, width - 92, 50);
    place(PreviewHint, 34, 639, width - 68, 30);
    place(DirectoryLabel, 48, 157, inner, 24, advanced);
    place(Directory, 60, 202, inner - 208, 22, advanced);
    place(Browse, width - 216, 192, 72, 42, advanced);
    place(Reset, width - 136, 192, 88, 42, advanced);
    place(DirectoryHint, 48, 247, inner, 40, advanced);
    place(BypassLabel, 48, 301, inner, 24, advanced);
    place(Bypass, 60, 341, inner - 24, 22, advanced);
    place(ExtraLabel, 48, 400, inner, 24, advanced);
    place(Extra, 60, 440, inner - 24, 22, advanced);
    if (batch) EndDeferWindowPos(batch);
    const int footerTop = root.bottom - footerHeight;
    HDWP footerBatch = BeginDeferWindowPos(4);
    auto footer = [&](int id, int x, int y, int w, int h) {
        const UINT flags = SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW;
        if (footerBatch) footerBatch = DeferWindowPos(footerBatch, Get(id), nullptr, x, y, w, h, flags);
        else SetWindowPos(Get(id), nullptr, x, y, w, h, flags);
    };
    footer(Footer, S(32), footerTop + S(8), root.right - S(64), S(40));
    footer(Advanced, S(32), footerTop + S(56), S(92), S(40));
    footer(CloseButton, root.right - S(240), footerTop + S(56), S(80), S(40));
    footer(ApplyButton, root.right - S(148), footerTop + S(56), S(116), S(40));
    if (footerBatch) EndDeferWindowPos(footerBatch);
    ShowWindow(Get(Error), SW_HIDE);
    RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
}
void App::ScrollTo(int position) {
    RECT viewport{}; GetClientRect(body, &viewport);
    position = std::clamp(position, 0, std::max(0, contentHeight - static_cast<int>(viewport.bottom)));
    const int delta = scroll - position;
    if (!delta) return;
    scroll = position;
    // Translate every body child, including those currently outside the clipping area.
    // No layout, font changes or control-by-control immediate painting during a wheel event.
    HDWP batch = BeginDeferWindowPos(static_cast<int>(controls.size()));
    for (const auto& item : controls) {
        if (GetParent(item.window) != body) continue;
        RECT r{}; GetWindowRect(item.window, &r);
        MapWindowPoints(nullptr, body, reinterpret_cast<POINT*>(&r), 2);
        if (batch) batch = DeferWindowPos(batch, item.window, nullptr, r.left, r.top + delta, 0, 0,
            SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOREDRAW);
        else SetWindowPos(item.window, nullptr, r.left, r.top + delta, 0, 0,
            SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOREDRAW);
    }
    if (batch) EndDeferWindowPos(batch);
    RedrawWindow(body, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
}
RECT App::ScrollThumb() const {
    RECT area{}; GetClientRect(body, &area);
    if (contentHeight <= area.bottom || area.bottom < S(60)) return {};
    const int track = area.bottom - S(16);
    const int height = std::max(S(36), MulDiv(track, area.bottom, contentHeight));
    const int top = S(8) + MulDiv(scroll, track - height, contentHeight - area.bottom);
    return { area.right - S(10), top, area.right - S(5), top + height };
}
setup::Options App::Options() const {
    setup::Options options;
    options.startup = Checked(Startup); options.silent = Checked(Silent); options.proxy = Checked(Proxy);
    options.protocol = Checked(ProtocolSocks) ? L"socks5" : Checked(ProtocolHttps) ? L"https" : L"http";
    options.host = Text(Host); options.port = Text(Port); options.directory = Text(Directory);
    options.bypass = Text(Bypass); options.extra = Text(Extra);
    return options;
}
void App::Refresh(bool edited) {
    if (loading) return;
    if (edited) { dirty = true; ++editRevision; }
    const setup::Options options = Options();
    EnableWindow(Get(Protocol), options.proxy && !busy);
    EnableWindow(Get(ProtocolHttps), options.proxy && !busy);
    EnableWindow(Get(ProtocolSocks), options.proxy && !busy);
    EnableWindow(Get(Host), options.proxy && !busy);
    EnableWindow(Get(Port), options.proxy && !busy);
    std::wstring arguments, directory;
    const setup::Result args = setup::BuildArguments(options, arguments);
    const setup::Result path = setup::NormalizeDirectory(options.directory, directory);
    // Never present a partial command as the command that will actually be saved.
    Set(Preview, args ? arguments : L"（请修正输入后查看启动参数）");
    const bool valid = args && path && !fatal;
    EnableWindow(Get(ApplyButton), valid && !busy && (!savedThisSession || dirty));
    if (!fatal) {
        if (!args) Set(Footer, args.message.substr(0, args.message.find(L'\n')));
        else if (!path) Set(Footer, path.message.substr(0, path.message.find(L'\n')));
        else Set(Footer, busy ? L"正在保存…" : dirty ? L"有更改待保存" : savedThisSession ? L"已保存，可以关闭" : L"保存后即可使用");
    }
    Set(ApplyButton, busy ? L"正在保存…" : savedThisSession && !dirty ? L"已保存" : savedTarget.empty() ? L"部署并保存" : L"保存更改");
    Set(CloseButton, savedThisSession && !dirty ? L"完成" : L"关闭");
}
void App::DetectPort() {
    if (loading || busy || fatal || detectingPort || !Checked(Proxy) || !Text(Port).empty()) return;
    const auto options = Options();
    detectingPort = true; detectionRevision = editRevision;
    Set(Footer, L"正在读取本机代理端口…也可以直接手动填写。");
    try {
        portWorker = std::thread([this, protocol = options.protocol, host = options.host] {
            try { detectedPort = setup::DetectProxyPort(protocol, host); }
            catch (...) { detectedPort = {}; }
            PostMessageW(window, PortDetectedMessage, 0, 0);
        });
    } catch (...) {
        detectingPort = false;
        Set(Footer, setup::ProxyDetection{}.message);
    }
}
void App::FinishPortDetection() {
    if (!detectingPort) return;
    if (portWorker.joinable()) portWorker.join();
    detectingPort = false;
    // Editing, loading or saving while discovery runs invalidates that result.
    if (busy || loading || editRevision != detectionRevision || !Checked(Proxy) || !Text(Port).empty()) return;
    if (!detectedPort.port.empty()) {
        Set(Port, detectedPort.port);
        SendMessageW(Get(Port), EM_SETSEL, 0, -1);
    }
    Set(Footer, detectedPort.message);
}
void App::RestoreOptions(const setup::Loaded& loaded) {
    loading = true;
    ++editRevision;
    savedTarget = loaded.managedTarget;
    Check(Startup, loaded.options.startup); Check(Silent, loaded.options.silent); Check(Proxy, loaded.options.proxy);
    Check(Protocol, loaded.options.protocol == L"http");
    Check(ProtocolHttps, loaded.options.protocol == L"https");
    Check(ProtocolSocks, loaded.options.protocol == L"socks5");
    Set(Host, loaded.options.host); Set(Port, loaded.options.port);
    Set(Directory, loaded.options.directory); Set(Bypass, loaded.options.bypass); Set(Extra, loaded.options.extra);
    Set(State, (loaded.deployed ? L"已部署" : loaded.hasSettings ? L"启动器缺失 · 保存可重新部署" : L"尚未部署")
        + std::wstring(!loaded.hasSettings ? L"  ·  选项将在保存后生效" : loaded.options.startup ? L"  ·  登录自启已配置" : L"  ·  登录自启未配置"));
    if (!loaded.notice.empty()) {
        Set(State, loaded.notice);
    }
    loading = false; dirty = false;
    Refresh(false);
}
void App::Command(int id, int notification) {
    if (loading || busy) return;
    if (id == Port && notification == EN_SETFOCUS) { DetectPort(); return; }
    if (id == Advanced && notification == BN_CLICKED) {
        advanced = !advanced; scroll = 0;
        Set(Advanced, advanced ? L"返回" : L"高级");
        Set(Title, advanced ? L"高级设置" : L"设置你的启动方式");
        Set(Subtitle, advanced ? L"部署位置与额外参数  /  返回后继续设置启动方式" : L"ChatGPT 轻量启动器  /  部署与配置");
        Layout(); SetFocus(Get(Advanced)); return;
    }
    if (id == Reset && notification == BN_CLICKED) { Set(Directory, paths.defaultDirectory); return; }
    if (id == Browse && notification == BN_CLICKED) {
        IFileOpenDialog* dialog = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
        if (SUCCEEDED(hr)) {
            DWORD flags = 0;
            hr = dialog->GetOptions(&flags);
            if (SUCCEEDED(hr)) hr = dialog->SetOptions(flags | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
            dialog->SetTitle(L"选择启动器部署目录");
            IShellItem* initial = nullptr;
            if (SUCCEEDED(SHCreateItemFromParsingName(Text(Directory).c_str(), nullptr, IID_PPV_ARGS(&initial)))) {
                dialog->SetFolder(initial); initial->Release();
            }
            if (SUCCEEDED(hr)) hr = dialog->Show(window);
            if (SUCCEEDED(hr)) {
                IShellItem* item = nullptr;
                hr = dialog->GetResult(&item);
                if (SUCCEEDED(hr)) {
                    PWSTR path = nullptr; hr = item->GetDisplayName(SIGDN_FILESYSPATH, &path);
                    if (SUCCEEDED(hr)) { Set(Directory, path); CoTaskMemFree(path); }
                    item->Release();
                }
            }
            dialog->Release();
        }
        if (FAILED(hr) && hr != HRESULT_FROM_WIN32(ERROR_CANCELLED))
            MessageBoxW(window, setup::ErrorText(static_cast<DWORD>(hr)).c_str(), L"无法选择目录", MB_OK | MB_ICONERROR);
        return;
    }
    if (id == CloseButton && notification == BN_CLICKED) { SendMessageW(window, WM_CLOSE, 0, 0); return; }
    if (id == ApplyButton && notification == BN_CLICKED && !fatal) {
        if (savedThisSession && !dirty) {
            Set(Footer, L"配置已保存，可以点击「完成」关闭本工具，无需再次部署。"); return;
        }
        const setup::Options options = Options();
        std::wstring unused;
        if (!setup::BuildArguments(options, unused) || !setup::NormalizeDirectory(options.directory, unused)) { Refresh(false); return; }
        busy = true;
        for (const auto& control : controls) EnableWindow(control.window, FALSE);
        Refresh(false); UpdateWindow(window);
        worker = std::thread([this, options]() {
            auto completion = std::make_unique<Completion>();
            completion->previous = savedTarget;
            completion->checkStartup = options.startup && !startupReminderShown;
            const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
            if (FAILED(com)) completion->result = { static_cast<DWORD>(com), L"无法初始化 Windows 快捷方式组件。" };
            else {
                try {
                    completion->result = setup::Apply(paths, options, payload, completion->target);
                }
                catch (...) { completion->result = { ERROR_NOT_ENOUGH_MEMORY, L"保存未完成。请重新打开工具检查配置后重试。" }; }
                if (completion->result && completion->checkStartup) {
                    try { completion->startupReport = setup::TryDisableOfficialStartup(); }
                    catch (...) { completion->startupReport.error = ERROR_NOT_ENOUGH_MEMORY; }
                }
                CoUninitialize();
            }
            if (PostMessageW(window, SavedMessage, 0, reinterpret_cast<LPARAM>(completion.get()))) completion.release();
        });
        return;
    }
    if (notification == EN_CHANGE || notification == CBN_SELCHANGE || notification == BN_CLICKED) {
        if (id != Preview) Refresh(true);
    }
}

void App::DrawSurface(HWND target, HDC dc) {
    RECT rect{}; GetClientRect(target, &rect);
    FillRect(dc, &rect, target == body ? background : surface);
    if (target == body) {
        auto card = [&](int top, int bottom) {
            const RECT r{ S(32), S(top) - scroll, rect.right - S(32), S(bottom) - scroll };
            Rounded(dc, r, S(16), colors.surface, colors.border);
        };
        if (advanced) card(138, 494);
        else { card(138, 304); card(322, 504); }
        for (const auto& item : controls) {
            if (!IsField(item.id) || !(GetWindowLongPtrW(item.window, GWL_STYLE) & WS_VISIBLE)) continue;
            RECT r{}; GetWindowRect(item.window, &r); MapWindowPoints(nullptr, body, reinterpret_cast<POINT*>(&r), 2);
            InflateRect(&r, S(12), S(10));
            const bool focused = GetFocus() == item.window;
            Rounded(dc, r, S(10), FieldColor(), focused ? colors.accent : colors.border, focused ? 2.0f : 1.0f);
        }
        const RECT thumb = ScrollThumb();
        Rounded(dc, thumb, S(3), colors.secondary, colors.secondary);
    } else {
        HPEN pen = CreatePen(PS_SOLID, 1, colors.border);
        HGDIOBJ old = SelectObject(dc, pen);
        MoveToEx(dc, 0, rect.bottom - S(FooterHeight), nullptr); LineTo(dc, rect.right, rect.bottom - S(FooterHeight));
        SelectObject(dc, old); DeleteObject(pen);
    }
}
void App::Paint(HWND target) {
    ++paintPasses;
    PAINTSTRUCT ps{}; HDC output = BeginPaint(target, &ps);
    RECT rect{}; GetClientRect(target, &rect);
    HDC dc = nullptr;
    HPAINTBUFFER buffer = BeginBufferedPaint(output, &rect, BPBF_COMPATIBLEBITMAP, nullptr, &dc);
    DrawSurface(target, buffer ? dc : output);
    if (buffer) EndBufferedPaint(buffer, TRUE);
    EndPaint(target, &ps);
}
void App::PaintControl(HWND control, HDC output) {
    const int state = SaveDC(output);
    RECT rect{}; GetClientRect(control, &rect);
    HDC dc = nullptr;
    HPAINTBUFFER buffer = BeginBufferedPaint(output, &rect, BPBF_COMPATIBLEBITMAP, nullptr, &dc);
    DrawControl(control, buffer ? dc : output);
    if (buffer) EndBufferedPaint(buffer, TRUE);
    RestoreDC(output, state);
}
void App::DrawControl(HWND control, HDC dc) {
    RECT rect{}; GetClientRect(control, &rect);
    const int id = GetDlgCtrlID(control);
    const bool enabled = IsWindowEnabled(control) != FALSE;
    const bool hover = GetPropW(control, L"setup.hover") != nullptr;
    const bool focus = GetFocus() == control;
    const bool focusVisible = focus && (SendMessageW(control, WM_QUERYUISTATE, 0, 0) & UISF_HIDEFOCUS) == 0;
    const bool pressed = (SendMessageW(control, BM_GETSTATE, 0, 0) & BST_PUSHED) != 0;
    const bool checked = Checked(id);
    FillRect(dc, &rect, surface);
    SetBkMode(dc, TRANSPARENT);
    COLORREF textColor = enabled ? colors.text : colors.secondary;
    RECT text = rect;
    UINT textFlags = DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX;
    if (id == Footer) {
        HGDIOBJ old = SelectObject(dc, fonts[4]);
        const std::wstring caption = Text(id);
        SetTextColor(dc, colors.secondary);
        RECT measure{0, 0, rect.right, 0};
        DrawTextW(dc, caption.c_str(), -1, &measure, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
        text.top += std::max(0L, (rect.bottom - measure.bottom) / 2);
        DrawTextW(dc, caption.c_str(), -1, &text, DT_WORDBREAK | DT_NOPREFIX);
        SelectObject(dc, old); return;
    } else if (IsSwitch(id)) {
        RECT track{ rect.right - S(46), (rect.bottom - S(26)) / 2, rect.right - S(2), (rect.bottom + S(26)) / 2 };
        const COLORREF trackColor = checked && enabled ? colors.accent : contrast ? colors.surface : colors.border;
        Rounded(dc, track, S(13), trackColor, focus ? colors.accent : contrast ? colors.text : trackColor, focus ? 2.0f : 1.0f);
        const int left = checked ? track.right - S(23) : track.left + S(3);
        RECT knob{ left, track.top + S(3), left + S(20), track.bottom - S(3) };
        Rounded(dc, knob, S(10), contrast ? (checked ? GetSysColor(COLOR_HIGHLIGHTTEXT) : colors.text) : RGB(255,255,255), trackColor, 0.5f);
        text.right = track.left - S(16); textFlags |= DT_LEFT;
    } else {
        const bool primary = id == ApplyButton;
        const bool quiet = id == Advanced;
        const bool segment = IsSegment(id);
        COLORREF fill = FieldColor();
        COLORREF edge = quiet ? fill : colors.border;
        if (enabled && ((primary) || (segment && checked))) {
            fill = primary || contrast ? colors.accent : dark ? RGB(38,65,98) : RGB(229,240,255);
            edge = primary ? colors.accent : colors.accent;
            textColor = contrast ? GetSysColor(COLOR_HIGHLIGHTTEXT) : primary ? RGB(255,255,255) : colors.accent;
        } else if (quiet) textColor = colors.secondary;
        if (enabled && (hover || pressed) && !primary && !contrast) fill = dark ? RGB(52,58,69) : RGB(236,240,246);
        if (enabled && pressed && primary && !contrast) fill = dark ? RGB(59,126,234) : RGB(0,82,190);
        if (quiet) edge = contrast ? colors.border : fill;
        const bool ring = IsActionButton(id) ? focusVisible : focus;
        Rounded(dc, rect, S(10), fill, ring ? colors.accent : edge, ring ? 2.0f : 1.0f);
        if (primary && focusVisible) {
            RECT inset = rect; InflateRect(&inset, -S(3), -S(3));
            Rounded(dc, inset, S(7), fill, textColor, 1.0f);
        }
        textFlags |= DT_CENTER;
        if (pressed) OffsetRect(&text, 0, S(1));
        if (quiet) {
            text.left += S(advanced ? 26 : 10); text.right -= S(advanced ? 10 : 26);
            const float x = static_cast<float>(advanced ? S(17) : rect.right - S(17));
            const float y = static_cast<float>(rect.bottom) / 2 + (pressed ? S(1) : 0);
            const float direction = advanced ? -1.0f : 1.0f;
            Gdiplus::Graphics graphics(dc); graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            Gdiplus::Pen pen(Gdiplus::Color(255, GetRValue(textColor), GetGValue(textColor), GetBValue(textColor)), static_cast<float>(S(1)) + 0.3f);
            pen.SetStartCap(Gdiplus::LineCapRound); pen.SetEndCap(Gdiplus::LineCapRound);
            graphics.DrawLine(&pen, x - direction * S(2), y - S(4), x + direction * S(2), y);
            graphics.DrawLine(&pen, x + direction * S(2), y, x - direction * S(2), y + S(4));
        }
    }
    SetTextColor(dc, textColor);
    HGDIOBJ old = SelectObject(dc, fonts[id == ApplyButton ? 1 : id == Advanced ? 4 : IsSegment(id) ? 3 : 0]);
    const std::wstring caption = Text(id);
    DrawTextW(dc, caption.c_str(), -1, &text, textFlags);
    SelectObject(dc, old);
    if (focusVisible && !IsActionButton(id)) {
        RECT frame = rect; InflateRect(&frame, -S(3), -S(3)); DrawFocusRect(dc, &frame);
    }
}
LRESULT ColorControl(App& app, HDC dc, HWND control) {
    const int id = GetDlgCtrlID(control);
    if (IsField(id)) {
        SetTextColor(dc, IsWindowEnabled(control) ? app.colors.text : app.colors.secondary);
        SetBkColor(dc, app.FieldColor()); return reinterpret_cast<LRESULT>(app.field);
    }
    const bool surface = (id >= StartTitle && id <= Port) || (id >= DirectoryLabel && id <= Extra)
        || id == Footer || id == CloseButton || id == ApplyButton || id == Advanced;
    const bool secondary = id == Subtitle || id == State || id == SilentHint || id == PreviewHint
        || id == DirectoryHint || id == ProtocolLabel || id == HostLabel || id == PortLabel || id == Footer;
    SetTextColor(dc, secondary ? app.colors.secondary : app.colors.text);
    SetBkColor(dc, surface ? app.colors.surface : app.colors.background);
    return reinterpret_cast<LRESULT>(surface ? app.surface : app.background);
}
LRESULT CALLBACK StyledControl(HWND window, UINT message, WPARAM wparam, LPARAM lparam, UINT_PTR, DWORD_PTR reference) {
    App& app = *reinterpret_cast<App*>(reference);
    const int id = GetDlgCtrlID(window);
    // Also allow retry by clicking an empty field that already has focus.
    if (id == Port && message == WM_LBUTTONDOWN && GetFocus() == window)
        PostMessageW(app.window, DetectPortMessage, 0, 0);
    if (IsActionButton(id) && message == WM_GETDLGCODE)
        return DLGC_BUTTON | (GetFocus() == window || GetPropW(window, L"setup.default") ? DLGC_DEFPUSHBUTTON : DLGC_UNDEFPUSHBUTTON);
    if (IsActionButton(id) && message == BM_SETSTYLE) {
        if ((wparam & BS_TYPEMASK) == BS_DEFPUSHBUTTON) SetPropW(window, L"setup.default", reinterpret_cast<HANDLE>(1));
        else RemovePropW(window, L"setup.default");
        // Dialog navigation may request a default push button; never restore native painting.
        wparam = (wparam & ~static_cast<WPARAM>(BS_TYPEMASK)) | BS_OWNERDRAW;
    }
    if ((IsButton(id) || id == Footer) && (message == WM_PAINT || message == WM_PRINTCLIENT)) {
        PAINTSTRUCT ps{};
        HDC output = message == WM_PAINT ? BeginPaint(window, &ps) : reinterpret_cast<HDC>(wparam);
        app.PaintControl(window, output);
        if (message == WM_PAINT) EndPaint(window, &ps);
        return 0;
    }
    if ((IsButton(id) || id == Footer) && message == WM_ERASEBKGND) return 1;
    if (message == WM_MOUSEMOVE && IsButton(id) && !GetPropW(window, L"setup.hover")) {
        SetPropW(window, L"setup.hover", reinterpret_cast<HANDLE>(1));
        TRACKMOUSEEVENT track{ sizeof(track), TME_LEAVE, window, 0 }; TrackMouseEvent(&track);
        InvalidateRect(window, nullptr, FALSE);
    }
    if (message == WM_MOUSELEAVE) { RemovePropW(window, L"setup.hover"); InvalidateRect(window, nullptr, FALSE); }
    if (message == WM_MOUSEWHEEL) {
        if (id != Preview) return SendMessageW(app.body, message, wparam, lparam);
        RECT bounds{}; GetWindowRect(window, &bounds);
        const POINT point{ GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
        if (!PtInRect(&bounds, point)) return SendMessageW(app.body, message, wparam, lparam);
        const LRESULT before = SendMessageW(window, EM_GETFIRSTVISIBLELINE, 0, 0);
        const LRESULT result = DefSubclassProc(window, message, wparam, lparam);
        return before == SendMessageW(window, EM_GETFIRSTVISIBLELINE, 0, 0)
            ? SendMessageW(app.body, message, wparam, lparam) : result;
    }
    if (message == WM_KEYDOWN && wparam == L'A' && IsField(id) && (GetKeyState(VK_CONTROL) & 0x8000)) {
        SendMessageW(window, EM_SETSEL, 0, -1); return 0;
    }
    if (message == WM_KEYDOWN && (wparam == VK_NEXT || wparam == VK_PRIOR) && id != Preview) {
        RECT rect{}; GetClientRect(app.body, &rect);
        app.ScrollTo(app.scroll + (wparam == VK_NEXT ? 1 : -1) * static_cast<int>(rect.bottom - app.S(36))); return 0;
    }
    const LRESULT result = DefSubclassProc(window, message, wparam, lparam);
    // Owner-drawn actions route synchronous drawing through WM_DRAWITEM as well.
    // Keep deferred invalidation for theme changes and the native checkbox/radio state model.
    if ((IsButton(id) || id == Footer) && (message == WM_SETTEXT || message == WM_UPDATEUISTATE || message == WM_THEMECHANGED
        || (message == BM_SETSTYLE && LOWORD(lparam)) || (message == WM_SETFONT && lparam))) {
        InvalidateRect(window, nullptr, FALSE);
    }
    if (message == WM_SETFOCUS || message == WM_KILLFOCUS || message == WM_ENABLE || message == BM_SETCHECK || message == BM_SETSTATE) {
        InvalidateRect(window, nullptr, FALSE);
        if (IsField(id)) {
            RECT frame{}; GetWindowRect(window, &frame); MapWindowPoints(nullptr, app.body, reinterpret_cast<POINT*>(&frame), 2);
            InflateRect(&frame, app.S(14), app.S(12)); InvalidateRect(app.body, &frame, FALSE);
        }
    }
    if (message == WM_NCDESTROY) { RemovePropW(window, L"setup.hover"); RemovePropW(window, L"setup.default"); RemoveWindowSubclass(window, StyledControl, 1); }
    return result;
}
LRESULT CALLBACK BodyProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    App* app = StateOf(window);
    if (message == WM_NCCREATE) {
        app = static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    }
    if (!app) return DefWindowProcW(window, message, wparam, lparam);
    switch (message) {
    case WM_COMMAND: app->Command(LOWORD(wparam), HIWORD(wparam)); return 0;
    case WM_DRAWITEM: {
        const auto item = reinterpret_cast<const DRAWITEMSTRUCT*>(lparam);
        if (item && IsActionButton(static_cast<int>(item->CtlID))) { app->PaintControl(item->hwndItem, item->hDC); return TRUE; }
        break;
    }
    case WM_PAINT: app->Paint(window); return 0;
    case WM_PRINTCLIENT: app->DrawSurface(window, reinterpret_cast<HDC>(wparam)); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_CTLCOLORSTATIC: case WM_CTLCOLOREDIT: case WM_CTLCOLORBTN: case WM_CTLCOLORLISTBOX:
        return ColorControl(*app, reinterpret_cast<HDC>(wparam), reinterpret_cast<HWND>(lparam));
    case WM_MOUSEWHEEL: {
        app->wheelRemainder += GET_WHEEL_DELTA_WPARAM(wparam);
        const int delta = MulDiv(app->wheelRemainder, app->S(54), WHEEL_DELTA);
        if (delta) { app->wheelRemainder = 0; app->ScrollTo(app->scroll - delta); }
        return 0;
    }
    case WM_LBUTTONDOWN: {
        const RECT thumb = app->ScrollThumb();
        const POINT point{ GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
        RECT hit = thumb; InflateRect(&hit, app->S(7), 0);
        if (thumb.bottom && PtInRect(&hit, point)) {
            app->draggingScroll = true; app->scrollGrab = point.y - thumb.top; SetCapture(window);
        }
        return 0;
    }
    case WM_MOUSEMOVE:
        if (app->draggingScroll) {
            RECT area{}; GetClientRect(window, &area); const RECT thumb = app->ScrollThumb();
            const int travel = area.bottom - app->S(16) - (thumb.bottom - thumb.top);
            if (travel > 0) app->ScrollTo(MulDiv(GET_Y_LPARAM(lparam) - app->scrollGrab - app->S(8), app->contentHeight - area.bottom, travel));
        }
        return 0;
    case WM_LBUTTONUP: if (app->draggingScroll) { app->draggingScroll = false; ReleaseCapture(); } return 0;
    case WM_CAPTURECHANGED: app->draggingScroll = false; return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}
LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    App* app = StateOf(window);
    if (message == WM_NCCREATE) {
        app = static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);
        app->window = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    }
    if (!app) return DefWindowProcW(window, message, wparam, lparam);
    switch (message) {
    case WM_CREATE: {
        app->dpi = GetDpiForWindow(window); app->Theme();
        app->body = CreateWindowExW(WS_EX_CONTROLPARENT | WS_EX_COMPOSITED, L"ChatGPTSetupBody", L"启动配置",
            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, 0, 0, 0, 0, window, nullptr, app->instance, app);
        app->CreateControls(); app->Theme();
        setup::Loaded loaded;
        setup::Result result;
        if (app->paths.defaultDirectory.empty()) result = setup::GetPaths(app->paths);
        if (result) result = setup::Load(app->paths, loaded);
        if (!result) {
            loaded.options.directory = app->paths.defaultDirectory;
            app->fatal = true;
            ShowSetupError(window, result, L"无法读取现有配置");
        }
        app->RestoreOptions(loaded);
        if (app->fatal) app->Set(Footer, L"现有配置读取失败，请处理错误后重新打开。");
        app->Layout(); return 0;
    }
    case WM_GETMINMAXINFO: {
        auto limits = reinterpret_cast<MINMAXINFO*>(lparam);
        limits->ptMinTrackSize = { app->S(620), app->S(500) }; return 0;
    }
    case WM_SIZE: app->Layout(); return 0;
    case WM_DPICHANGED: {
        app->dpi = HIWORD(wparam); app->Theme();
        const RECT* suggested = reinterpret_cast<RECT*>(lparam);
        SetWindowPos(window, nullptr, suggested->left, suggested->top, suggested->right - suggested->left,
            suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE);
        app->Layout(); return 0;
    }
    case WM_SETTINGCHANGE: case WM_THEMECHANGED: app->Theme(); app->Layout(); return 0;
    case WM_DRAWITEM: {
        const auto item = reinterpret_cast<const DRAWITEMSTRUCT*>(lparam);
        if (item && (IsActionButton(static_cast<int>(item->CtlID)) || item->CtlID == Footer)) {
            app->PaintControl(item->hwndItem, item->hDC); return TRUE;
        }
        break;
    }
    case WM_PAINT: app->Paint(window); return 0;
    case WM_PRINTCLIENT: app->DrawSurface(window, reinterpret_cast<HDC>(wparam)); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_CTLCOLORSTATIC: case WM_CTLCOLOREDIT: case WM_CTLCOLORBTN:
        return ColorControl(*app, reinterpret_cast<HDC>(wparam), reinterpret_cast<HWND>(lparam));
    case WM_COMMAND: app->Command(LOWORD(wparam), HIWORD(wparam)); return 0;
    case WM_MOUSEWHEEL: return SendMessageW(app->body, message, wparam, lparam);
    case DetectPortMessage: app->DetectPort(); return 0;
    case PortDetectedMessage: app->FinishPortDetection(); return 0;
    case SavedMessage: {
        std::unique_ptr<Completion> done(reinterpret_cast<Completion*>(lparam));
        if (app->worker.joinable()) app->worker.join();
        app->busy = false;
        for (const auto& control : app->controls) EnableWindow(control.window, TRUE);
        if (done->result) {
            setup::Loaded loaded;
            const setup::Result result = setup::Load(app->paths, loaded);
            if (result) app->RestoreOptions(loaded);
            else { app->dirty = false; app->savedTarget = done->target; }
            app->savedThisSession = true;
            app->Refresh(false);
            const bool moved = !done->previous.empty() && _wcsicmp(done->previous.c_str(), done->target.c_str()) != 0;
            app->Set(Footer, moved ? L"已保存。旧目录中的文件已保留。" : L"已保存。下次启动时使用这些选项。");
            app->Set(State, app->Checked(Startup) ? L"已部署  ·  登录自启已配置  ·  参数已保存" : L"已部署  ·  登录自启未配置  ·  参数已保存");
            if (done->checkStartup) {
                app->startupReminderShown = true;
                const std::wstring reminder = L"启动器、启动参数和登录自启快捷方式已保存，不需要再次部署。\n\n"
                    L"接下来请按这几步操作：\n"
                    L"1. 点击本提示的「确定」，打开 Windows「启动应用」设置。\n"
                    L"2. 找到「ChatGPT」，确认开关为「关」；如果已经关闭，不用再改。\n"
                    L"3. 保持「ChatGPT Proxy Launcher」（本工具的启动项）开启，不要一起关闭。\n"
                    L"4. 核对后回到部署器，点击右下方「完成」即可退出。\n\n"
                    L"这样可以避免 ChatGPT 自带自启先运行，导致本工具配置的启动参数无法生效。\n\n"
                    L"自动处理结果：\n" + done->startupReport.Message();
                app->Set(Footer, L"已保存，可关闭本工具。请在 Windows 设置中核对自带自启。");
                MessageBoxW(window, reminder.c_str(), L"部署已完成", MB_OK | MB_ICONINFORMATION);
                const setup::Result opened = setup::OpenStartupSettings(window);
                if (!opened) MessageBoxW(window, (L"配置已保存。\n\n" + opened.message).c_str(), L"请手动打开启动设置", MB_OK | MB_ICONINFORMATION);
            }
        } else {
            app->Refresh(false);
            ShowSetupError(window, done->result, L"保存未完成");
        }
        SetFocus(app->Get(ApplyButton)); return 0;
    }
    case WM_CLOSE:
        if (app->busy) return 0;
        if (app->dirty && MessageBoxW(window, L"还有尚未保存的更改。要放弃这些更改并关闭吗？",
            L"关闭配置工具", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES) return 0;
        DestroyWindow(window); return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}
} // namespace

int WINAPI wWinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE, _In_ PWSTR, _In_ int show) {
    HANDLE mutex = CreateMutexW(nullptr, FALSE, L"Local\\Community.ChatGPTLauncherSetup.v1");
    if (!mutex) return static_cast<int>(GetLastError());
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND existing = FindWindowW(L"ChatGPTLauncherSetup", nullptr);
        if (existing) { ShowWindow(existing, SW_RESTORE); SetForegroundWindow(existing); }
        CloseHandle(mutex); return 0;
    }
    const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(com)) { CloseHandle(mutex); return static_cast<int>(com); }
    int exitCode = 0;
    Gdiplus::GdiplusStartupInput graphicsInput;
    ULONG_PTR graphicsToken = 0;
    if (Gdiplus::GdiplusStartup(&graphicsToken, &graphicsInput, nullptr) != Gdiplus::Ok) {
        CoUninitialize(); CloseHandle(mutex); return ERROR_NOT_ENOUGH_MEMORY;
    }
    BufferedPaintInit();
    {
        App app; app.instance = instance;
        HRSRC resource = FindResourceW(instance, MAKEINTRESOURCEW(IDR_LAUNCHER), RT_RCDATA);
        HGLOBAL loaded = resource ? LoadResource(instance, resource) : nullptr;
        app.payload = { loaded ? static_cast<const BYTE*>(LockResource(loaded)) : nullptr,
            resource ? SizeofResource(instance, resource) : 0 };
        if (!app.payload.data || !app.payload.size) {
            MessageBoxW(nullptr, L"内置启动器资源缺失，请重新下载完整的配置工具。", L"无法打开", MB_OK | MB_ICONERROR);
            exitCode = ERROR_RESOURCE_DATA_NOT_FOUND;
        } else {
            INITCOMMONCONTROLSEX init{ sizeof(init), ICC_STANDARD_CLASSES }; InitCommonControlsEx(&init);
            WNDCLASSEXW cls{}; cls.cbSize = sizeof(cls); cls.hInstance = instance; cls.hCursor = LoadCursorW(nullptr, IDC_ARROW);
            cls.hIcon = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_CHATGPT), IMAGE_ICON,
                GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_SHARED));
            cls.hIconSm = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_CHATGPT), IMAGE_ICON,
                GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_SHARED));
            cls.lpfnWndProc = WindowProc; cls.lpszClassName = L"ChatGPTLauncherSetup"; RegisterClassExW(&cls);
            cls.lpfnWndProc = BodyProc; cls.lpszClassName = L"ChatGPTSetupBody"; RegisterClassExW(&cls);
            const UINT dpi = GetDpiForSystem();
            RECT work{}; SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
            const int width = MulDiv(700, static_cast<int>(dpi), 96);
            const int height = std::min(MulDiv(840, static_cast<int>(dpi), 96), static_cast<int>(work.bottom - work.top - 32));
            HWND window = CreateWindowExW(WS_EX_CONTROLPARENT, L"ChatGPTLauncherSetup", L"ChatGPT 启动器设置",
                WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, work.left + (work.right - work.left - width) / 2,
                work.top + (work.bottom - work.top - height) / 2, width, height, nullptr, nullptr, instance, &app);
            if (!window) exitCode = static_cast<int>(GetLastError());
            else {
                ShowWindow(window, show); UpdateWindow(window);
                MSG message{};
                BOOL status = 0;
                while ((status = GetMessageW(&message, nullptr, 0, 0)) > 0) {
                    const bool tabbed = message.message == WM_KEYDOWN && message.wParam == VK_TAB;
                    if (message.message == WM_KEYDOWN && message.wParam == VK_ESCAPE) {
                        if (app.advanced) app.Command(Advanced, BN_CLICKED);
                        else SendMessageW(window, WM_CLOSE, 0, 0);
                    }
                    else if (!IsDialogMessageW(window, &message)) { TranslateMessage(&message); DispatchMessageW(&message); }
                    HWND focus = GetFocus();
                    if (tabbed && focus && GetParent(focus) == app.body) {
                        RECT bounds{}; GetWindowRect(focus, &bounds); MapWindowPoints(nullptr, app.body, reinterpret_cast<POINT*>(&bounds), 2);
                        RECT viewport{}; GetClientRect(app.body, &viewport);
                        if (bounds.top < 0 || bounds.bottom > viewport.bottom) {
                            app.ScrollTo(app.scroll + (bounds.top < 0 ? bounds.top - app.S(12) : bounds.bottom - viewport.bottom + app.S(12)));
                        }
                    }
                }
                exitCode = status < 0 ? static_cast<int>(GetLastError()) : static_cast<int>(message.wParam);
            }
        }
    }
    BufferedPaintUnInit(); Gdiplus::GdiplusShutdown(graphicsToken);
    CoUninitialize(); CloseHandle(mutex); return exitCode;
}
