// SPDX-License-Identifier: GPL-3.0-only
#include "SetupCore.h"
#include "Localization.h"
#include "SetupCommandLine.h"
#include "StartupApps.h"
#include "SetupResources.h"
#include "StartupIntegration.h"
#include "ProxyDetection.h"
#include "AppVersion.h"
#include "CodexClose.h"
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
#include <atomic>
#include "NativeMemory.h"

namespace {
constexpr UINT SavedMessage = WM_APP + 1;
constexpr UINT DetectPortMessage = WM_APP + 2;
constexpr UINT PortDetectedMessage = WM_APP + 3;
constexpr UINT_PTR MotionTimer = 1;
constexpr UINT_PTR WorkerTimer = 2;
constexpr int HeaderButtonHeight = 36, HeaderButtonRadius = 10;
constexpr int LanguageMenuPadding = 6, LanguageRowHeight = 36, LanguageMenuRadius = 12;
enum ControlId {
    Title = 1000, SetupVersion, ThemeToggle, LanguageButton, State, StartTitle, Startup, Silent, SilentHint, Desktop,
    ProxyTitle, Proxy, CodexEnv, CodexEnvHelp, ProtocolLabel, ProtocolHelp, Protocol, ProtocolHttps, ProtocolSocks, HostLabel, HostHelp, Host, PortLabel, PortHelp, Port,
    PreviewTitle, Preview, Advanced, DirectoryLabel, Directory, Browse, Reset,
    DirectoryHint, BypassLabel, Bypass, ExtraLabel, Extra, CompanionLabel, AddProgram, OpenCompanions, Footer, ApplyButton, CloseButton
};
struct Palette {
    COLORREF background, surface, text, secondary, border, accent;
    COLORREF field, hover, selected, selectedText, divider, primary, primaryHover, primaryText;
};
struct Motion {
    float from = 0, to = 0;
    ULONGLONG started = 0;
    float Value(ULONGLONG now) const {
        const float t = std::min(1.0f, static_cast<float>(now - started) / 150.0f);
        const float eased = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
        return from + (to - from) * eased;
    }
    bool Active(ULONGLONG now) const { return from != to && now - started < 150; }
    void Set(float value, bool animate, ULONGLONG now) {
        if (!animate) { from = to = value; return; }
        if (to == value) return;
        from = Value(now); to = value; started = now;
    }
};
struct Control { int id; HWND window; int font; Motion hover{}, toggle{}; };
COLORREF Blend(COLORREF a, COLORREF b, float t) {
    auto channel = [t](int x, int y) { return static_cast<BYTE>(x + (y - x) * t + 0.5f); };
    return RGB(channel(GetRValue(a), GetRValue(b)), channel(GetGValue(a), GetGValue(b)), channel(GetBValue(a), GetBValue(b)));
}
struct Completion {
    setup::Result result;
    std::wstring target, previous;
    bool checkStartup = false;
    bool codexEnv = true, wroteCodexEnv = false;
    setup::ChatGPTCloseResult closeResult = setup::ChatGPTCloseResult::NeedsManualExit;
    setup::StartupReport startupReport;
};
bool IsSwitch(int id) { return id == Startup || id == Silent || id == Proxy || id == Desktop || id == CodexEnv; }
bool IsSegment(int id) { return id == Protocol || id == ProtocolHttps || id == ProtocolSocks; }
bool IsField(int id) { return id == Host || id == Port || id == Directory || id == Bypass || id == Extra || id == Preview; }
bool IsHelp(int id) { return id == CodexEnvHelp || id == ProtocolHelp || id == HostHelp || id == PortHelp; }
bool IsButton(int id) {
    return IsSwitch(id) || IsSegment(id) || IsHelp(id) || id == Advanced || id == Browse || id == Reset || id == CloseButton || id == ApplyButton || id == ThemeToggle || id == LanguageButton || id == AddProgram || id == OpenCompanions;
}
bool IsActionButton(int id) { return IsButton(id) && !IsSwitch(id) && !IsSegment(id); }
bool IsOwnerDrawButton(int id) { return IsActionButton(id) || IsSegment(id); }
constexpr wchar_t AppearanceKey[] = L"Software\\Community\\ChatGPTLauncherSetup";
PCWSTR HelpText(int id) { return l10n::Text(id == CodexEnvHelp ? "help.codex" : id == PortHelp ? "help.port" : "help.protocol"); }
PCWSTR ChatGPTCloseMessage(setup::ChatGPTCloseResult result) {
    if (result == setup::ChatGPTCloseResult::Closed) return l10n::Text("setup.quit.closed");
    if (result == setup::ChatGPTCloseResult::NotRunning) return l10n::Text("setup.quit.absent");
    return l10n::Text("setup.quit.manual");
}
bool ReadCodexEnvPreference() {
    DWORD enabled = 1, size = sizeof(enabled);
    return RegGetValueW(HKEY_CURRENT_USER, AppearanceKey, L"ConfigureCodexEnv", RRF_RT_REG_DWORD,
        nullptr, &enabled, &size) != ERROR_SUCCESS || enabled != 0;
}
bool SaveCodexEnvPreference(bool enabled) {
    const DWORD value = enabled ? 1 : 0;
    return RegSetKeyValueW(HKEY_CURRENT_USER, AppearanceKey, L"ConfigureCodexEnv", REG_DWORD,
        &value, sizeof(value)) == ERROR_SUCCESS;
}
bool ReadDarkPreference() {
    DWORD light = 0, size = sizeof(light);
    return RegGetValueW(HKEY_CURRENT_USER, AppearanceKey, L"LightTheme", RRF_RT_REG_DWORD,
        nullptr, &light, &size) != ERROR_SUCCESS || light != 1;
}
bool SaveDarkPreference(bool dark) {
    const DWORD light = dark ? 0 : 1;
    return RegSetKeyValueW(HKEY_CURRENT_USER, AppearanceKey, L"LightTheme", REG_DWORD,
        &light, sizeof(light)) == ERROR_SUCCESS;
}
bool SaveLanguagePreference() {
    PCWSTR value = l10n::FollowsSystem() ? L"auto" : l10n::Current().tag;
    return RegSetKeyValueW(HKEY_CURRENT_USER, AppearanceKey, L"Language", REG_SZ,
        value, static_cast<DWORD>((wcslen(value) + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
}
constexpr UINT LanguageMenuBase = 40000;
PCWSTR LanguageMenuText(size_t index) { return index == 0 ? l10n::Text("setup.language.system") : l10n::At(index - 1).name; }
bool LanguageMenuSelected(size_t index) {
    return index == 0 ? l10n::FollowsSystem() : !l10n::FollowsSystem() && !wcscmp(l10n::Current().tag, l10n::At(index - 1).tag);
}
void Rounded(HDC dc, const RECT& rect, int radius, COLORREF fill, COLORREF border, float thickness = 1.0f) {
    if (rect.right <= rect.left || rect.bottom <= rect.top || !RectVisible(dc, &rect)) return;
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
    HWND window = nullptr, body = nullptr, tooltip = nullptr;
    HWND languagePopup = nullptr;
    UINT dpi = 96;
    bool advanced = false, loading = true, dirty = false, busy = false, fatal = false;
    bool savedThisSession = false, startupReminderShown = false;
    bool dark = false, contrast = false, draggingScroll = false;
    bool preferredDark = true;
    int selectedProtocol = Protocol;
    bool motionEnabled = false;
    int scrollGrab = 0, wheelRemainder = 0;
    unsigned layoutPasses = 0;
    int scroll = 0, contentHeight = 0;
    int headerOffset = 0;
    int footerTextHeight = 32;
    std::string completionKey;
    bool envPreferenceWarning = false;
    std::wstring savedTarget;
    setup::Paths paths;
    setup::Payload payload{};
    Palette colors{};
    HBRUSH background = nullptr, surface = nullptr, field = nullptr;
    HFONT fonts[6]{};
    std::vector<Control> controls;
    std::thread worker;
    std::thread portWorker;
    std::unique_ptr<Completion> pendingSave;
    std::atomic<bool> saveFinished{false}, portFinished{false};
    bool closing = false;
    bool detectingPort = false;
    unsigned editRevision = 0, detectionRevision = 0;
    setup::ProxyDetection detectedPort;
    ~App() {
        StopWorkers();
        if (window) DestroyWindow(window);
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
        if (Text(id) == text) return;
        SetWindowTextW(Get(id), text.c_str());
        if (!loading) ResizeCaption(id);
    }
    bool Checked(int id) const { return SendMessageW(Get(id), BM_GETCHECK, 0, 0) == BST_CHECKED; }
    void Check(int id, bool value) { SendMessageW(Get(id), BM_SETCHECK, value ? BST_CHECKED : BST_UNCHECKED, 0); }
    void Theme(int mode = -1);
    int CaptionWidth(int id, int font) const;
    int CaptionHeight(int id, int font, int width) const;
    void ResizeCaption(int id);
    void AppendFooterWarning(PCWSTR warning);
    HFONT LanguageMenuFont(size_t index) const;
    void MeasureLanguageItem(MEASUREITEMSTRUCT& item) const;
    void LanguageMenu();
    void CloseLanguageMenu(bool restoreFocus);
    bool LanguageMenuMessage(MSG& message);
    void DrawLanguageMenu(HDC dc, const RECT& bounds);
    void ChangeLanguage(size_t menuIndex);
    void TranslateControls();
    void UpdateState(const setup::Loaded& loaded);
    void DrawLanguageItem(const DRAWITEMSTRUCT& item);
    void SelectProtocol(int id);
    void CreateControls();
    void UpdateTooltip();
    void Layout();
    void Refresh(bool edited);
    void DetectPort();
    void FinishPortDetection();
    void PollWorkers();
    void StopWorkers();
    void RestoreOptions(const setup::Loaded& loaded);
    void Command(int id, int notification);
    void BeginSave();
    void FinishSave(std::unique_ptr<Completion> done);
    void ShowStartupReminder(const Completion& done, bool preferenceSaved);
    void Paint(HWND target);
    void DrawSurface(HWND target, HDC dc);
    void DrawControl(HWND control, HDC dc);
    void PaintControl(HWND control, HDC output);
    void UpdateMotion(HWND control);
    bool TickMotion();
    void ScrollTo(int position);
    RECT ScrollThumb() const;
    COLORREF FieldColor() const { return colors.field; }
    setup::Options Options() const;
};
App* StateOf(HWND window) { return reinterpret_cast<App*>(GetWindowLongPtrW(window, GWLP_USERDATA)); }
void ShowSetupError(HWND window, const setup::Result& result, PCWSTR title) {
    std::wstring message = result.message;
    if (!result.conflictDirectory.empty())
        message += l10n::Format("setup.error.conflictlocation", {{"path", result.conflictDirectory}});
    MessageBoxW(window, message.c_str(), title, MB_OK | MB_ICONERROR);
    if (result.conflictDirectory.empty()) return;
    SHELLEXECUTEINFOW info{};
    info.cbSize = sizeof(info); info.fMask = SEE_MASK_FLAG_NO_UI;
    info.hwnd = window; info.lpVerb = L"open";
    info.lpFile = result.conflictDirectory.c_str(); info.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&info)) {
        const DWORD error = GetLastError();
        const std::wstring failure = l10n::Format("setup.error.openfolderdetails", {{"path", result.conflictDirectory}, {"error", setup::ErrorText(error)}});
        MessageBoxW(window, failure.c_str(), l10n::Text("setup.error.openfolder"), MB_OK | MB_ICONERROR);
    }
}

void App::Theme(int mode) {
    CloseLanguageMenu(false);
    HIGHCONTRASTW hc{ sizeof(hc), 0, nullptr };
    SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(hc), &hc, 0);
    contrast = (hc.dwFlags & HCF_HIGHCONTRASTON) != 0;
    dark = preferredDark && !contrast;
    if (mode >= 0) { dark = mode == 1; contrast = mode == 2; }
    // Radix Gray: 1/2 surfaces, 3 fields, 4 hover, 6/7 borders, 11/12 text.
    // Blue 3/11 identify selection. The light primary uses Blue 11 for readable white text.
    if (contrast) colors = { GetSysColor(COLOR_WINDOW), GetSysColor(COLOR_WINDOW), GetSysColor(COLOR_WINDOWTEXT),
        GetSysColor(COLOR_GRAYTEXT), GetSysColor(COLOR_WINDOWTEXT), GetSysColor(COLOR_HIGHLIGHT),
        GetSysColor(COLOR_WINDOW), GetSysColor(COLOR_WINDOW), GetSysColor(COLOR_HIGHLIGHT), GetSysColor(COLOR_HIGHLIGHTTEXT),
        GetSysColor(COLOR_WINDOWTEXT), GetSysColor(COLOR_HIGHLIGHT), GetSysColor(COLOR_HIGHLIGHT), GetSysColor(COLOR_HIGHLIGHTTEXT) };
    else if (dark) colors = { RGB(17,17,17), RGB(25,25,25), RGB(238,238,238), RGB(180,180,180),
        RGB(72,72,72), RGB(0,144,255), RGB(34,34,34), RGB(42,42,42),
        RGB(13,40,71), RGB(112,184,255), RGB(58,58,58), RGB(0,144,255), RGB(59,158,255), RGB(13,21,32) };
    else colors = { RGB(249,249,249), RGB(255,255,255), RGB(32,32,32), RGB(100,100,100),
        RGB(206,206,206), RGB(0,144,255), RGB(240,240,240), RGB(232,232,232),
        RGB(230,244,254), RGB(13,116,206), RGB(217,217,217), RGB(13,116,206), RGB(17,50,100), RGB(255,255,255) };
    BOOL clientAnimation = FALSE;
    SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &clientAnimation, 0);
    motionEnabled = clientAnimation && !contrast;
    if (background) DeleteObject(background);
    if (surface) DeleteObject(surface);
    if (field) DeleteObject(field);
    background = CreateSolidBrush(colors.background); surface = CreateSolidBrush(colors.surface);
    field = CreateSolidBrush(FieldColor());
    const int sizes[] = { 14, 14, 24, 12, 12, 14 };
    for (int i = 0; i < static_cast<int>(std::size(fonts)); ++i) {
        if (fonts[i]) DeleteObject(fonts[i]);
        fonts[i] = CreateFontW(-S(sizes[i]), 0, 0, 0, i == 1 || i == 2 ? FW_SEMIBOLD : FW_NORMAL,
            FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH, i == 5 ? L"Segoe UI" : l10n::Current().font);
    }
    for (const auto& item : controls) {
        SendMessageW(item.window, WM_SETFONT, reinterpret_cast<WPARAM>(fonts[item.font]), TRUE);
        // Checkbox custom-draw notifications require the v6 themed control path.
        SetWindowTheme(item.window, IsSwitch(item.id) ? nullptr : L"", IsSwitch(item.id) ? nullptr : L"");
    }
    for (auto& item : controls) {
        item.hover = {};
        item.toggle.from = item.toggle.to = Checked(item.id) ? 1.0f : 0.0f;
    }
    KillTimer(window, MotionTimer);
    Set(ThemeToggle, dark ? l10n::Text("setup.theme.light") : l10n::Text("setup.theme.dark"));
    EnableWindow(Get(ThemeToggle), !contrast && !busy);
    EnableWindow(Get(LanguageButton), !busy && !detectingPort);
    const BOOL useDark = dark;
    DwmSetWindowAttribute(window, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &useDark, sizeof(useDark));
    UpdateTooltip();
    if (body) InvalidateRect(body, nullptr, TRUE);
    InvalidateRect(window, nullptr, TRUE);
}

void App::CreateControls() {
    auto add = [&](int id, PCWSTR type, PCWSTR text, DWORD style, int font = 0, bool footer = false) {
        if (IsField(id)) font = 5; // Keep literal backslashes readable in paths, including Japanese Windows.
        if (IsOwnerDrawButton(id)) style = (style & ~BS_TYPEMASK) | BS_OWNERDRAW;
        HWND h = CreateWindowExW(0, type, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 0, 0,
            footer ? window : body, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance, nullptr);
        controls.push_back({ id, h, font });
        SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(fonts[font]), FALSE);
        if (IsButton(id) || IsField(id) || id == Footer) SetWindowSubclass(h, StyledControl, 1, reinterpret_cast<DWORD_PTR>(this));
        return h;
    };
    const DWORD label = SS_LEFT | SS_NOPREFIX;
    const DWORD edit = WS_TABSTOP | ES_AUTOHSCROLL;
    add(Title, L"STATIC", l10n::Text("common.launcher"), label, 2);
    add(SetupVersion, L"STATIC", l10n::Format("setup.version", {{"version", L"" APP_VERSION_TEXT}}).c_str(), label, 3);
    add(LanguageButton, L"BUTTON", l10n::Current().name, WS_TABSTOP | BS_OWNERDRAW);
    add(ThemeToggle, L"BUTTON", l10n::Text("setup.theme.light"), WS_TABSTOP | BS_OWNERDRAW);
    add(State, L"STATIC", l10n::Text("setup.state.initial"), label, 3);
    add(StartTitle, L"STATIC", l10n::Text("setup.section.startup"), label, 1);
    add(Startup, L"BUTTON", l10n::Text("setup.option.startup"), WS_TABSTOP | BS_AUTOCHECKBOX);
    add(Desktop, L"BUTTON", l10n::Text("setup.option.desktop"), WS_TABSTOP | BS_AUTOCHECKBOX);
    add(Silent, L"BUTTON", l10n::Text("setup.option.silent"), WS_DISABLED | WS_TABSTOP | BS_AUTOCHECKBOX);
    add(SilentHint, L"STATIC", l10n::Text("setup.hint.startup"), label, 3);
    add(ProxyTitle, L"STATIC", l10n::Text("setup.section.proxy"), label, 1);
    add(Proxy, L"BUTTON", l10n::Text("setup.option.proxy"), WS_TABSTOP | BS_AUTOCHECKBOX);
    add(CodexEnv, L"BUTTON", l10n::Text("setup.option.codex"), WS_TABSTOP | WS_CLIPSIBLINGS | BS_AUTOCHECKBOX);
    add(CodexEnvHelp, L"BUTTON", l10n::Text("setup.help.codex"), WS_TABSTOP | BS_OWNERDRAW);
    // The help button sits above the full-width switch, next to its label.
    SetWindowPos(Get(CodexEnvHelp), Get(Proxy), 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    Check(CodexEnv, ReadCodexEnvPreference());
    add(ProtocolLabel, L"STATIC", l10n::Text("setup.label.protocol"), label, 3);
    add(ProtocolHelp, L"BUTTON", l10n::Text("setup.help.protocol"), WS_TABSTOP | BS_OWNERDRAW);
    add(Protocol, L"BUTTON", L"HTTP", WS_TABSTOP | WS_GROUP | BS_AUTORADIOBUTTON, 3);
    add(ProtocolHttps, L"BUTTON", L"HTTPS", WS_TABSTOP | BS_AUTORADIOBUTTON, 3);
    add(ProtocolSocks, L"BUTTON", L"SOCKS5", WS_TABSTOP | BS_AUTORADIOBUTTON, 3);
    Check(Protocol, true);
    add(HostLabel, L"STATIC", l10n::Text("setup.label.host"), label, 3);
    add(HostHelp, L"BUTTON", l10n::Text("setup.help.host"), WS_TABSTOP | BS_OWNERDRAW);
    add(Host, L"EDIT", L"127.0.0.1", edit | WS_GROUP);
    add(PortLabel, L"STATIC", l10n::Text("setup.label.port"), label, 3);
    add(PortHelp, L"BUTTON", l10n::Text("setup.help.port"), WS_TABSTOP | BS_OWNERDRAW);
    add(Port, L"EDIT", L"", edit);
    SendMessageW(Get(Port), EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(l10n::Text("setup.placeholder.port")));
    SendMessageW(Get(Port), EM_SETLIMITTEXT, 5, 0);
    add(PreviewTitle, L"STATIC", l10n::Text("setup.section.preview"), label, 1);
    add(Preview, L"EDIT", L"", WS_TABSTOP | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL);
    SendMessageW(Get(Preview), EM_SETLIMITTEXT, 32767, 0);
    add(Advanced, L"BUTTON", l10n::Text("setup.action.advanced"), WS_TABSTOP | BS_OWNERDRAW, 4, true);
    add(DirectoryLabel, L"STATIC", l10n::Text("setup.label.directory"), label, 1);
    add(Directory, L"EDIT", L"", edit);
    add(Browse, L"BUTTON", l10n::Text("setup.action.browse"), WS_TABSTOP | BS_PUSHBUTTON);
    add(Reset, L"BUTTON", l10n::Text("setup.action.reset"), WS_TABSTOP | BS_PUSHBUTTON);
    add(DirectoryHint, L"STATIC", l10n::Text("setup.hint.directory"), label, 3);
    add(BypassLabel, L"STATIC", l10n::Text("setup.label.bypass"), label, 1);
    add(Bypass, L"EDIT", L"", edit);
    SendMessageW(Get(Bypass), EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(l10n::Text("setup.placeholder.bypass")));
    add(ExtraLabel, L"STATIC", l10n::Text("setup.label.extra"), label, 1);
    add(Extra, L"EDIT", L"", edit);
    add(CompanionLabel, L"STATIC", l10n::Text("setup.label.companions"), label, 1);
    add(AddProgram, L"BUTTON", l10n::Text("setup.action.add"), WS_TABSTOP | BS_PUSHBUTTON);
    add(OpenCompanions, L"BUTTON", l10n::Text("setup.action.openstartup"), WS_TABSTOP | BS_PUSHBUTTON);
    SendMessageW(Get(Extra), EM_SETLIMITTEXT, static_cast<WPARAM>(setup::MaxArguments), 0);
    add(Footer, L"STATIC", l10n::Text("setup.status.savefirst"), SS_OWNERDRAW, 4, true);
    add(CloseButton, L"BUTTON", l10n::Text("common.close"), WS_TABSTOP | BS_PUSHBUTTON, 0, true);
    add(ApplyButton, L"BUTTON", l10n::Text("setup.action.deploy"), WS_TABSTOP | BS_PUSHBUTTON, 1, true);
    INITCOMMONCONTROLSEX tipClasses{ sizeof(tipClasses), ICC_BAR_CLASSES };
    InitCommonControlsEx(&tipClasses);
    tooltip = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr, WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, window, nullptr, instance, nullptr);
    // No reserved v6 fields are used; this size also works without a visual-styles manifest.
    for (int id : {CodexEnvHelp, ProtocolHelp, HostHelp, PortHelp}) {
        TOOLINFOW tip{ TTTOOLINFOW_V2_SIZE };
        tip.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
        tip.hwnd = body; tip.uId = reinterpret_cast<UINT_PTR>(Get(id));
        tip.lpszText = const_cast<PWSTR>(HelpText(id));
        SendMessageW(tooltip, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&tip));
    }
    SendMessageW(tooltip, TTM_SETDELAYTIME, TTDT_AUTOPOP, 30000);
    UpdateTooltip();
}
void App::UpdateTooltip() {
    if (!tooltip) return;
    SetWindowTheme(tooltip, L"", L"");
    SendMessageW(tooltip, WM_SETFONT, reinterpret_cast<WPARAM>(fonts[0]), FALSE);
    SendMessageW(tooltip, TTM_SETMAXTIPWIDTH, 0, S(460));
    SendMessageW(tooltip, TTM_SETTIPBKCOLOR, colors.surface, 0);
    SendMessageW(tooltip, TTM_SETTIPTEXTCOLOR, colors.text, 0);
    for (int id : {CodexEnvHelp, ProtocolHelp, HostHelp, PortHelp}) {
        TOOLINFOW tip{TTTOOLINFOW_V2_SIZE}; tip.hwnd = body;
        tip.uId = reinterpret_cast<UINT_PTR>(Get(id)); tip.lpszText = const_cast<PWSTR>(HelpText(id));
        SendMessageW(tooltip, TTM_UPDATETIPTEXTW, 0, reinterpret_cast<LPARAM>(&tip));
    }
}

int App::CaptionWidth(int id, int font) const {
    HDC dc = GetDC(body); const auto old = SelectObject(dc, fonts[font]); SIZE size{};
    const auto caption = Text(id);
    GetTextExtentPoint32W(dc, caption.c_str(), static_cast<int>(caption.size()), &size);
    SelectObject(dc, old); ReleaseDC(body, dc);
    return MulDiv(size.cx, 96, static_cast<int>(dpi)) + 2;
}
int App::CaptionHeight(int id, int font, int width) const {
    HDC dc = GetDC(body); const auto old = SelectObject(dc, fonts[font]);
    RECT bounds{0, 0, S(width), 0}; const auto caption = Text(id);
    DrawTextW(dc, caption.c_str(), -1, &bounds, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
    SelectObject(dc, old); ReleaseDC(body, dc);
    return MulDiv(bounds.bottom, 96, static_cast<int>(dpi)) + 1;
}
void App::ResizeCaption(int id) {
    if (id != Footer && id != State && id != ApplyButton && id != CloseButton) return;
    RECT root{}, bounds{}; GetClientRect(window, &root); GetClientRect(Get(id), &bounds);
    const int width = MulDiv(root.right, 96, static_cast<int>(dpi));
    // Reflow only when text changes the required geometry. Typing and scrolling stay cheap.
    const bool changed = id == Footer ? footerTextHeight != std::max(32, CaptionHeight(id, 4, width - 64))
        : id == State ? bounds.bottom != S(std::max(20, CaptionHeight(id, 3, width - 64)))
        : id == ApplyButton ? bounds.right != S(std::max(116, CaptionWidth(id, 1) + 40))
        : bounds.right != S(std::max(80, CaptionWidth(id, 0) + 32));
    if (changed) Layout();
}
void App::AppendFooterWarning(PCWSTR warning) {
    const auto current = Text(Footer);
    if (current.find(warning) == std::wstring::npos) Set(Footer, current + L"\n" + warning);
}
HFONT App::LanguageMenuFont(size_t index) const {
    LOGFONTW info{}; GetObjectW(fonts[0], sizeof(info), &info);
    wcscpy_s(info.lfFaceName, index ? l10n::At(index - 1).font : l10n::Current().font);
    return CreateFontIndirectW(&info);
}
void App::MeasureLanguageItem(MEASUREITEMSTRUCT& item) const {
    // Measure with the same font used to paint each native language name.
    HFONT font = LanguageMenuFont(item.itemData);
    HDC dc = GetDC(window); const auto old = SelectObject(dc, font ? font : fonts[0]); SIZE size{};
    PCWSTR caption = LanguageMenuText(item.itemData);
    GetTextExtentPoint32W(dc, caption, static_cast<int>(wcslen(caption)), &size);
    SelectObject(dc, old); ReleaseDC(window, dc); if (font) DeleteObject(font);
    item.itemWidth = static_cast<UINT>(std::max<LONG>(S(156), size.cx + S(48 + 2 * LanguageMenuPadding)));
    item.itemHeight = S(LanguageRowHeight + (item.itemData ? 0 : 8));
}
LRESULT CALLBACK LanguageListProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam, UINT_PTR, DWORD_PTR) {
    if (message == WM_LBUTTONUP) {
        const auto result = DefSubclassProc(window, message, wparam, lparam);
        const LRESULT hit = SendMessageW(window, LB_ITEMFROMPOINT, 0, lparam);
        if (!HIWORD(hit)) SendMessageW(GetParent(window), WM_COMMAND, MAKEWPARAM(IDOK, 0), 0);
        return result;
    }
    if (message == WM_MOUSEMOVE) {
        const LRESULT hit = SendMessageW(window, LB_ITEMFROMPOINT, 0, lparam);
        if (!HIWORD(hit) && SendMessageW(window, LB_GETCURSEL, 0, 0) != LOWORD(hit)) SendMessageW(window, LB_SETCURSEL, LOWORD(hit), 0);
    }
    if (message == WM_NCDESTROY) RemoveWindowSubclass(window, LanguageListProc, 1);
    return DefSubclassProc(window, message, wparam, lparam);
}
LRESULT CALLBACK LanguagePopupProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    App* app = StateOf(window);
    if (message == WM_NCCREATE) {
        app = static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    }
    if (!app) return DefWindowProcW(window, message, wparam, lparam);
    switch (message) {
    case WM_DRAWITEM: {
        const auto item = reinterpret_cast<const DRAWITEMSTRUCT*>(lparam);
        if (item && item->CtlType == ODT_LISTBOX && item->itemID != static_cast<UINT>(-1)) { app->DrawLanguageItem(*item); return TRUE; }
        break;
    }
    case WM_MEASUREITEM: {
        const auto item = reinterpret_cast<MEASUREITEMSTRUCT*>(lparam);
        if (item && item->CtlType == ODT_LISTBOX) {
            item->itemData = item->itemID; app->MeasureLanguageItem(*item); return TRUE;
        }
        break;
    }
    case WM_CTLCOLORLISTBOX: return reinterpret_cast<LRESULT>(app->surface);
    case WM_PAINT: {
        PAINTSTRUCT paint{}; HDC dc = BeginPaint(window, &paint); RECT bounds{}; GetClientRect(window, &bounds);
        app->DrawLanguageMenu(dc, bounds); EndPaint(window, &paint); return 0;
    }
    case WM_PRINTCLIENT: {
        RECT bounds{}; GetClientRect(window, &bounds); app->DrawLanguageMenu(reinterpret_cast<HDC>(wparam), bounds); return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_COMMAND:
        if (LOWORD(wparam) == IDOK) {
            const LRESULT selected = SendDlgItemMessageW(window, LanguageMenuBase, LB_GETCURSEL, 0, 0);
            app->CloseLanguageMenu(true);
            if (selected >= 0) app->ChangeLanguage(static_cast<size_t>(selected));
            return 0;
        }
        break;
    case WM_ACTIVATE:
        if (LOWORD(wparam) == WA_INACTIVE) app->CloseLanguageMenu(false);
        return 0;
    case WM_CLOSE: app->CloseLanguageMenu(true); return 0;
    case WM_NCDESTROY:
        if (app->languagePopup == window) app->languagePopup = nullptr;
        break;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}
void App::DrawLanguageMenu(HDC dc, const RECT& bounds) {
    FillRect(dc, &bounds, surface);
    Rounded(dc, bounds, S(LanguageMenuRadius), colors.surface, colors.divider, static_cast<float>(dpi) / 96.0f);
}
void App::CloseLanguageMenu(bool restoreFocus) {
    if (!languagePopup) return;
    const HWND popup = languagePopup; languagePopup = nullptr;
    DestroyWindow(popup);
    if (restoreFocus && window && !closing) { SetForegroundWindow(window); SetFocus(Get(LanguageButton)); }
    if (Get(LanguageButton)) InvalidateRect(Get(LanguageButton), nullptr, FALSE);
}
bool App::LanguageMenuMessage(MSG& message) {
    if (!languagePopup || (message.hwnd != languagePopup && !IsChild(languagePopup, message.hwnd))) return false;
    if (message.message == WM_KEYDOWN) {
        if (message.wParam == VK_ESCAPE || message.wParam == VK_TAB) { CloseLanguageMenu(true); return true; }
        if (message.wParam == VK_RETURN || message.wParam == VK_SPACE) {
            SendMessageW(languagePopup, WM_COMMAND, IDOK, 0); return true;
        }
    }
    return IsDialogMessageW(languagePopup, &message) != FALSE;
}
void App::LanguageMenu() {
    if (languagePopup) { CloseLanguageMenu(true); return; }
    if (busy || detectingPort) return;
    WNDCLASSEXW cls{sizeof(cls)}; cls.hInstance = instance; cls.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    cls.lpfnWndProc = LanguagePopupProc; cls.lpszClassName = L"ChatGPTSetupLanguagePopup"; cls.style = CS_DROPSHADOW;
    if (!RegisterClassExW(&cls) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return;
    int width = 0, height = S(2 * LanguageMenuPadding);
    for (size_t i = 0; i <= l10n::Count(); ++i) {
        MEASUREITEMSTRUCT item{}; item.itemData = i; MeasureLanguageItem(item);
        width = std::max(width, static_cast<int>(item.itemWidth)); height += item.itemHeight;
    }
    RECT bounds{}; GetWindowRect(Get(LanguageButton), &bounds);
    MONITORINFO monitor{sizeof(monitor)};
    if (!GetMonitorInfoW(MonitorFromRect(&bounds, MONITOR_DEFAULTTONEAREST), &monitor)) return;
    const RECT work = monitor.rcWork;
    const int maxHeight = std::max(S(48), static_cast<int>(work.bottom - work.top) - S(12));
    if (height > maxHeight) { height = maxHeight; width += GetSystemMetricsForDpi(SM_CXVSCROLL, dpi); }
    width = std::min(width, static_cast<int>(work.right - work.left));
    int x = std::clamp(bounds.right - width, work.left, work.right - width);
    int y = bounds.bottom + S(6);
    if (y + height > work.bottom) y = bounds.top - S(6) - height;
    y = std::max(y, static_cast<int>(work.top));
    languagePopup = CreateWindowExW(WS_EX_TOOLWINDOW, cls.lpszClassName, Text(LanguageButton).c_str(),
        WS_POPUP | WS_CLIPCHILDREN, x, y, width, height, window, nullptr, instance, this);
    if (!languagePopup) return;
    // Own the complete frame. A region also keeps corners consistent on Windows 10.
    HRGN region = CreateRoundRectRgn(0, 0, width + 1, height + 1, S(LanguageMenuRadius * 2), S(LanguageMenuRadius * 2));
    if (region && !SetWindowRgn(languagePopup, region, FALSE)) DeleteObject(region);
    const int inset = S(LanguageMenuPadding);
    HWND list = CreateWindowExW(0, L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL |
        LBS_NOTIFY | LBS_HASSTRINGS | LBS_OWNERDRAWVARIABLE | LBS_NOINTEGRALHEIGHT,
        inset, inset, width - inset * 2, height - inset * 2, languagePopup,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(LanguageMenuBase)), instance, nullptr);
    if (!list) { CloseLanguageMenu(false); return; }
    SetWindowTheme(list, L"", L"");
    if (!SetWindowSubclass(list, LanguageListProc, 1, 0)) { CloseLanguageMenu(false); return; }
    SendMessageW(list, WM_SETFONT, reinterpret_cast<WPARAM>(fonts[0]), FALSE);
    for (size_t i = 0; i <= l10n::Count(); ++i) {
        const auto index = SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(LanguageMenuText(i)));
        if (index < 0) { CloseLanguageMenu(false); return; }
        SendMessageW(list, LB_SETITEMDATA, index, i);
        if (LanguageMenuSelected(i)) SendMessageW(list, LB_SETCURSEL, index, 0);
    }
    ShowWindow(languagePopup, SW_SHOW); SetForegroundWindow(languagePopup); SetFocus(list);
    InvalidateRect(Get(LanguageButton), nullptr, FALSE);
}
void App::DrawLanguageItem(const DRAWITEMSTRUCT& item) {
    const bool hover = (item.itemState & ODS_SELECTED) != 0;
    const bool selected = LanguageMenuSelected(item.itemData);
    HBRUSH brush = CreateSolidBrush(colors.surface); FillRect(item.hDC, &item.rcItem, brush); DeleteObject(brush);
    RECT row = item.rcItem; InflateRect(&row, 0, -S(2));
    if (!item.itemData) row.bottom -= S(8);
    const COLORREF fill = selected || (contrast && hover) ? colors.selected : colors.hover;
    if (selected || hover) Rounded(item.hDC, row, S(LanguageMenuRadius - LanguageMenuPadding), fill, fill);
    const COLORREF textColor = selected || (contrast && hover) ? colors.selectedText : colors.text;
    SetBkMode(item.hDC, TRANSPARENT); SetTextColor(item.hDC, textColor);
    // Use the entry's font so language names remain readable from any active language.
    HFONT font = LanguageMenuFont(item.itemData); const auto old = SelectObject(item.hDC, font ? font : fonts[0]);
    RECT text = row; text.left = item.rcItem.left + S(30); text.right = item.rcItem.right - S(12);
    DrawTextW(item.hDC, LanguageMenuText(item.itemData), -1, &text, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    if (selected) {
        HPEN pen = CreatePen(PS_SOLID, S(2), textColor);
        const auto oldPen = SelectObject(item.hDC, pen);
        const int x = item.rcItem.left + S(10), y = (row.top + row.bottom) / 2;
        MoveToEx(item.hDC, x, y, nullptr); LineTo(item.hDC, x + S(3), y + S(3)); LineTo(item.hDC, x + S(10), y - S(4));
        SelectObject(item.hDC, oldPen); DeleteObject(pen);
    }
    SelectObject(item.hDC, old); if (font) DeleteObject(font);
    if (!item.itemData) {
        HPEN pen = CreatePen(PS_SOLID, 1, colors.divider); const auto oldPen = SelectObject(item.hDC, pen);
        const int y = item.rcItem.bottom - S(4);
        MoveToEx(item.hDC, item.rcItem.left + S(8), y, nullptr); LineTo(item.hDC, item.rcItem.right - S(8), y);
        SelectObject(item.hDC, oldPen); DeleteObject(pen);
    }
}
void App::ChangeLanguage(size_t menuIndex) {
    if (busy || detectingPort || menuIndex > l10n::Count()) return;
    if (!menuIndex) l10n::UseSystem(); else l10n::Select(l10n::At(menuIndex - 1).tag);
    TranslateControls();
    // Keep deployment/quit instructions visible if only the appearance preference fails.
    if (!SaveLanguagePreference()) AppendFooterWarning(l10n::Text("setup.language.unsaved"));
}
void App::TranslateControls() {
    // Change captions only. Do not restore options, overwrite edits or reset dirty state.
    const bool wasLoading = loading; loading = true;
    const struct { int id; const char* key; } labels[] = {
        {StartTitle,"setup.section.startup"}, {Startup,"setup.option.startup"}, {Desktop,"setup.option.desktop"},
        {Silent,"setup.option.silent"}, {SilentHint,"setup.hint.startup"}, {ProxyTitle,"setup.section.proxy"},
        {Proxy,"setup.option.proxy"}, {CodexEnv,"setup.option.codex"}, {CodexEnvHelp,"setup.help.codex"},
        {ProtocolLabel,"setup.label.protocol"}, {ProtocolHelp,"setup.help.protocol"}, {HostLabel,"setup.label.host"},
        {HostHelp,"setup.help.host"}, {PortLabel,"setup.label.port"}, {PortHelp,"setup.help.port"},
        {PreviewTitle,"setup.section.preview"}, {DirectoryLabel,"setup.label.directory"}, {Browse,"setup.action.browse"},
        {Reset,"setup.action.reset"}, {DirectoryHint,"setup.hint.directory"}, {BypassLabel,"setup.label.bypass"},
        {ExtraLabel,"setup.label.extra"}, {CompanionLabel,"setup.label.companions"}, {AddProgram,"setup.action.add"},
        {OpenCompanions,"setup.action.openstartup"}
    };
    for (const auto& label : labels) Set(label.id, l10n::Text(label.key));
    Set(Title, l10n::Text(advanced ? "setup.section.advanced" : "common.launcher"));
    Set(Advanced, l10n::Text(advanced ? "common.back" : "setup.action.advanced"));
    Set(SetupVersion, l10n::Format("setup.version", {{"version", L"" APP_VERSION_TEXT}}));
    Set(LanguageButton, l10n::Current().name);
    SetWindowTextW(window, l10n::Text("setup.window")); SetWindowTextW(body, l10n::Text("setup.body"));
    SendMessageW(Get(Port), EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(l10n::Text("setup.placeholder.port")));
    SendMessageW(Get(Bypass), EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(l10n::Text("setup.placeholder.bypass")));
    // A single read on explicit switching refreshes localized deployment warnings, never the form.
    setup::Loaded loaded;
    if (setup::Load(paths, loaded)) UpdateState(loaded);
    else Set(State, l10n::Text(savedThisSession ? "setup.state.readfailed" : "setup.status.loadfailed"));
    loading = wasLoading;
    Theme(); Refresh(false);
    if (fatal) Set(Footer, l10n::Text("setup.status.loadfailed"));
    Layout();
}

void App::Layout() {
    CloseLanguageMenu(false);
    if (!body) return;
    if (tooltip) SendMessageW(tooltip, TTM_POP, 0, 0);
    ++layoutPasses;
    RECT root{}; GetClientRect(window, &root);
    const int rootWidth = MulDiv(root.right, 96, static_cast<int>(dpi));
    footerTextHeight = std::max(32, CaptionHeight(Footer, 4, rootWidth - 64));
    const int footerHeight = S(footerTextHeight + 64);
    SetWindowPos(body, nullptr, 0, 0, root.right, std::max(1L, root.bottom - footerHeight), SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW);
    RECT area{}; GetClientRect(body, &area);
    const int envOffset = Checked(Proxy) ? 48 : 0;
    const int width = MulDiv(area.right, 96, static_cast<int>(dpi));
    const int inner = width - 96;
    const int languageWidth = std::clamp(CaptionWidth(LanguageButton, 0) + 40, 112, 180);
    const int languageLeft = width - 80 - languageWidth;
    const int titleWidth = CaptionWidth(Title, 2), versionWidth = CaptionWidth(SetupVersion, 3);
    const int versionOffset = 44 + titleWidth + versionWidth > languageLeft - 16 ? 24 : 0;
    const int stateHeight = std::max(20, CaptionHeight(State, 3, width - 64));
    headerOffset = versionOffset + stateHeight - 20;
    contentHeight = S((advanced ? 710 : 666 + envOffset) + headerOffset);
    scroll = std::clamp(scroll, 0, std::max(0, contentHeight - static_cast<int>(area.bottom)));
    HDWP batch = BeginDeferWindowPos(static_cast<int>(controls.size()));
    auto place = [&](int id, int x, int y, int w, int h, bool visible = true) {
        if (y >= 108) y += headerOffset;
        HWND control = Get(id);
        const UINT flags = SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW | (visible ? SWP_SHOWWINDOW : SWP_HIDEWINDOW);
        if (batch) batch = DeferWindowPos(batch, control, nullptr, S(x), S(y) - scroll, S(w), S(h), flags);
        else SetWindowPos(control, nullptr, S(x), S(y) - scroll, S(w), S(h), flags);
    };
    place(Title, 32, 24, titleWidth + 2, 34);
    place(SetupVersion, versionOffset ? 32 : 44 + titleWidth, versionOffset ? 64 : 39, versionWidth, 20);
    place(LanguageButton, languageLeft, 24, languageWidth, HeaderButtonHeight);
    place(ThemeToggle, width - 68, 24, HeaderButtonHeight, HeaderButtonHeight);
    place(State, 32, 66 + versionOffset, width - 64, stateHeight);
    place(StartTitle, 32, 108, width - 64, 20, !advanced);
    place(Startup, 48, 152, inner, 36, !advanced);
    place(SilentHint, 48, 190, inner, 18, !advanced);
    place(Desktop, 48, 220, inner, 36, !advanced);
    place(Silent, 48, 268, inner, 32, !advanced);
    place(ProxyTitle, 32, 340, width - 64, 20, !advanced);
    place(Proxy, 48, 384, inner, 36, !advanced);
    place(CodexEnv, 48, 432, inner, 36, !advanced && envOffset);
    HDC measure = GetDC(body); HGDIOBJ oldFont = SelectObject(measure, fonts[0]); SIZE labelSize{};
    const auto envLabel = Text(CodexEnv);
    GetTextExtentPoint32W(measure, envLabel.c_str(), static_cast<int>(envLabel.size()), &labelSize);
    SelectObject(measure, oldFont); ReleaseDC(body, measure);
    place(CodexEnvHelp, std::min(48 + MulDiv(labelSize.cx, 96, static_cast<int>(dpi)) + 6, width - 128), 438, 24, 24, !advanced && envOffset);
    const auto proxyLabel = [&](int label, int help, int x) {
        const int labelWidth = CaptionWidth(label, 3);
        place(label, x, 432 + envOffset, labelWidth, 20, !advanced);
        place(help, x + labelWidth + 4, 429 + envOffset, 24, 24, !advanced);
    };
    proxyLabel(ProtocolLabel, ProtocolHelp, 48);
    place(Protocol, 52, 462 + envOffset, 64, 28, !advanced);
    place(ProtocolHttps, 120, 462 + envOffset, 64, 28, !advanced);
    place(ProtocolSocks, 188, 462 + envOffset, 64, 28, !advanced);
    proxyLabel(HostLabel, HostHelp, 272);
    place(Host, 284, 466 + envOffset, inner - 368, 20, !advanced);
    proxyLabel(PortLabel, PortHelp, width - 152);
    place(Port, width - 140, 466 + envOffset, 80, 20, !advanced);
    place(PreviewTitle, 32, advanced ? 588 : 542 + envOffset, width - 64, 20);
    place(Preview, 44, advanced ? 632 : 586 + envOffset, width - 88, 44);
    place(DirectoryLabel, 32, 108, width - 64, 20, advanced);
    const int browseWidth = std::max(72, CaptionWidth(Browse, 0) + 24);
    const int resetWidth = std::max(88, CaptionWidth(Reset, 0) + 24);
    const int resetLeft = width - 48 - resetWidth, browseLeft = resetLeft - 8 - browseWidth;
    place(Directory, 60, 166, browseLeft - 84, 20, advanced);
    place(Browse, browseLeft, 156, browseWidth, 40, advanced);
    place(Reset, resetLeft, 156, resetWidth, 40, advanced);
    place(DirectoryHint, 48, 216, inner, 20, advanced);
    place(BypassLabel, 32, 280, width - 64, 20, advanced);
    place(Bypass, 44, 322, width - 88, 20, advanced);
    place(ExtraLabel, 32, 380, width - 64, 20, advanced);
    place(Extra, 44, 422, width - 88, 20, advanced);
    place(CompanionLabel, 32, 480, width - 64, 20, advanced);
    const int addWidth = std::max(112, CaptionWidth(AddProgram, 0) + 32);
    place(AddProgram, 32, 512, addWidth, 40, advanced);
    place(OpenCompanions, 44 + addWidth, 512, std::max(128, CaptionWidth(OpenCompanions, 0) + 32), 40, advanced);
    if (batch) EndDeferWindowPos(batch);
    const int footerTop = root.bottom - footerHeight;
    HDWP footerBatch = BeginDeferWindowPos(4);
    auto footer = [&](int id, int x, int y, int w, int h) {
        const UINT flags = SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW;
        if (footerBatch) footerBatch = DeferWindowPos(footerBatch, Get(id), nullptr, x, y, w, h, flags);
        else SetWindowPos(Get(id), nullptr, x, y, w, h, flags);
    };
    footer(Footer, S(32), footerTop + S(4), root.right - S(64), S(footerTextHeight));
    const int applyWidth = std::max(116, CaptionWidth(ApplyButton, 1) + 40);
    const int closeWidth = std::max(80, CaptionWidth(CloseButton, 0) + 32);
    const int buttonTop = root.bottom - S(56);
    footer(Advanced, S(32), buttonTop, S(std::max(92, CaptionWidth(Advanced, 4) + 40)), S(40));
    footer(CloseButton, root.right - S(44 + applyWidth + closeWidth), buttonTop, S(closeWidth), S(40));
    footer(ApplyButton, root.right - S(32 + applyWidth), buttonTop, S(applyWidth), S(40));
    if (footerBatch) EndDeferWindowPos(footerBatch);
    RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
}
void App::ScrollTo(int position) {
    RECT viewport{}; GetClientRect(body, &viewport);
    position = std::clamp(position, 0, std::max(0, contentHeight - static_cast<int>(viewport.bottom)));
    const int delta = scroll - position;
    if (!delta) return;
    CloseLanguageMenu(false);
    if (tooltip) SendMessageW(tooltip, TTM_POP, 0, 0);
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
    options.desktop = Checked(Desktop);
    options.codexEnv = Checked(CodexEnv);
    options.protocol = Checked(ProtocolSocks) ? L"socks5" : Checked(ProtocolHttps) ? L"https" : L"http";
    options.host = Text(Host); options.port = Text(Port); options.directory = Text(Directory);
    options.bypass = Text(Bypass); options.extra = Text(Extra);
    return options;
}
void App::Refresh(bool edited) {
    if (loading) return;
    if (edited) { dirty = true; ++editRevision; }
    const setup::Options options = Options();
    // Temporarily unavailable; preserve the saved value but keep the switch locked after saving.
    EnableWindow(Get(Silent), FALSE);
    EnableWindow(Get(ThemeToggle), !contrast && !busy);
    EnableWindow(Get(LanguageButton), !busy && !detectingPort);
    EnableWindow(Get(Protocol), options.proxy && !busy);
    EnableWindow(Get(ProtocolHttps), options.proxy && !busy);
    EnableWindow(Get(ProtocolSocks), options.proxy && !busy);
    EnableWindow(Get(Host), options.proxy && !busy);
    EnableWindow(Get(Port), options.proxy && !busy);
    EnableWindow(Get(CodexEnv), options.proxy && !busy);
    EnableWindow(Get(CodexEnvHelp), options.proxy && !busy);
    EnableWindow(Get(ProtocolHelp), !busy);
    EnableWindow(Get(HostHelp), !busy);
    EnableWindow(Get(PortHelp), !busy);
    if (((GetWindowLongPtrW(Get(CodexEnv), GWL_STYLE) & WS_VISIBLE) != 0) != (!advanced && options.proxy)) Layout();
    std::wstring arguments, directory;
    const setup::Result args = setup::BuildArguments(options, arguments);
    const setup::Result path = setup::NormalizeDirectory(options.directory, directory);
    // Never present a partial command as the command that will actually be saved.
    Set(Preview, args ? arguments : l10n::Text("setup.preview.invalid"));
    const bool valid = args && path && !fatal;
    EnableWindow(Get(ApplyButton), valid && !busy && (!savedThisSession || dirty));
    if (!fatal) {
        if (!args) Set(Footer, args.message.substr(0, args.message.find(L'\n')));
        else if (!path) Set(Footer, path.message.substr(0, path.message.find(L'\n')));
        else Set(Footer, busy ? l10n::Text("setup.status.saving") : savedThisSession && !dirty ?
            std::wstring(l10n::Text(completionKey.empty() ? "setup.status.saved" : completionKey)) + (envPreferenceWarning ? l10n::Text("setup.status.envunsaved") : L"")
            : options.proxy && options.codexEnv ? l10n::Text("setup.status.quitwarning") : dirty ? l10n::Text("setup.status.unsaved") : l10n::Text("setup.status.savefirst"));
    }
    Set(ApplyButton, busy ? l10n::Text("setup.status.saving") : savedThisSession && !dirty ? l10n::Text("setup.action.saved") : savedTarget.empty() ? l10n::Text("setup.action.deploy") : l10n::Text("setup.action.save"));
    Set(CloseButton, savedThisSession && !dirty ? l10n::Text("common.done") : l10n::Text("common.close"));
}
void App::DetectPort() {
    if (closing || loading || busy || fatal || detectingPort || !Checked(Proxy) || !Text(Port).empty()) return;
    const auto options = Options();
    detectingPort = true; detectionRevision = editRevision;
    EnableWindow(Get(LanguageButton), FALSE);
    Set(Footer, l10n::Text("setup.status.detecting"));
    try {
        detectedPort = {}; // Allocate the fallback before starting the worker.
        portFinished.store(false, std::memory_order_relaxed);
        if (!SetTimer(window, WorkerTimer, 100, nullptr)) throw std::bad_alloc();
        portWorker = std::thread([this, protocol = options.protocol, host = options.host] {
            try { detectedPort = setup::DetectProxyPort(protocol, host); }
            catch (...) {} // Keep the preallocated fallback; do not allocate in this handler.
            portFinished.store(true, std::memory_order_release);
            PostMessageW(window, PortDetectedMessage, 0, 0);
        });
    } catch (...) {
        detectingPort = false;
        EnableWindow(Get(LanguageButton), !busy);
        if (!busy) KillTimer(window, WorkerTimer);
        Set(Footer, setup::ProxyDetection{}.message);
    }
}
void App::FinishPortDetection() {
    if (!detectingPort) return;
    if (portWorker.joinable()) portWorker.join();
    detectingPort = false;
    EnableWindow(Get(LanguageButton), !busy);
    if (!busy) KillTimer(window, WorkerTimer);
    // Editing, loading or saving while discovery runs invalidates that result.
    if (busy || loading || editRevision != detectionRevision || !Checked(Proxy) || !Text(Port).empty()) return;
    if (!detectedPort.port.empty()) {
        Set(Port, detectedPort.port);
        SendMessageW(Get(Port), EM_SETSEL, 0, -1);
    }
    Set(Footer, detectedPort.message);
}
void App::PollWorkers() {
    if (closing) return;
    if (detectingPort && portFinished.load(std::memory_order_acquire)) FinishPortDetection();
    if (busy && saveFinished.load(std::memory_order_acquire)) {
        if (worker.joinable()) worker.join();
        if (pendingSave) FinishSave(std::move(pendingSave));
    }
    if (!busy && !detectingPort) KillTimer(window, WorkerTimer);
}
void App::StopWorkers() {
    if (closing) return;
    closing = true;
    CloseLanguageMenu(false);
    if (window) KillTimer(window, WorkerTimer);
    // Workers only post messages. They never wait for this window to process them.
    if (worker.joinable()) worker.join();
    if (portWorker.joinable()) portWorker.join();
    pendingSave.reset();
    busy = detectingPort = false;
}
void App::RestoreOptions(const setup::Loaded& loaded) {
    loading = true;
    ++editRevision;
    savedTarget = loaded.managedTarget;
    Check(Startup, loaded.options.startup); Check(Silent, loaded.options.silent); Check(Proxy, loaded.options.proxy);
    Check(Desktop, loaded.options.desktop);
    Check(CodexEnv, ReadCodexEnvPreference());
    Check(Protocol, loaded.options.protocol == L"http");
    Check(ProtocolHttps, loaded.options.protocol == L"https");
    Check(ProtocolSocks, loaded.options.protocol == L"socks5");
    Set(Host, loaded.options.host); Set(Port, loaded.options.port);
    Set(Directory, loaded.options.directory); Set(Bypass, loaded.options.bypass); Set(Extra, loaded.options.extra);
    UpdateState(loaded);
    loading = false; dirty = false;
    Refresh(false);
    Layout();
}
void App::UpdateState(const setup::Loaded& loaded) {
    const std::wstring deployment = loaded.deployed
        ? loaded.deployedVersion.empty() ? l10n::Text("setup.state.unknownversion") : l10n::Format("setup.state.version", {{"version", loaded.deployedVersion}})
        : loaded.hasSettings ? l10n::Text("setup.state.missing") : l10n::Text("setup.state.absent");
    Set(State, l10n::Format(!loaded.hasSettings ? "setup.state.pending" : loaded.options.startup ? "setup.state.startupon" : "setup.state.startupoff",
        {{"deployment", deployment}}));
    if (!loaded.notice.empty()) {
        Set(State, loaded.notice);
    }
}
void App::Command(int id, int notification) {
    if (loading || busy) return;
    if (id == LanguageButton && notification == BN_CLICKED) { LanguageMenu(); return; }
    if (IsHelp(id) && notification == BN_CLICKED) {
        MessageBoxW(window, HelpText(id), id == CodexEnvHelp ? l10n::Text("setup.dialog.codex") : l10n::Text("setup.dialog.proxy"), MB_OK | MB_ICONINFORMATION); return;
    }
    if (id == ThemeToggle && notification == BN_CLICKED) {
        if (contrast) return;
        preferredDark = !dark;
        Theme(); Layout();
        if (!SaveDarkPreference(preferredDark)) AppendFooterWarning(l10n::Text("setup.status.themeunsaved"));
        return;
    }
    if (IsSegment(id) && notification == BN_CLICKED) {
        if (!IsWindowEnabled(Get(id))) return;
        SelectProtocol(id); Refresh(true); return;
    }
    if (id == Port && notification == EN_SETFOCUS) { DetectPort(); return; }
    if (id == Advanced && notification == BN_CLICKED) {
        advanced = !advanced; scroll = 0;
        Set(Advanced, advanced ? l10n::Text("common.back") : l10n::Text("setup.action.advanced"));
        Set(Title, advanced ? l10n::Text("setup.section.advanced") : l10n::Text("common.launcher"));
        Layout(); SetFocus(Get(Advanced)); return;
    }
    if (id == Reset && notification == BN_CLICKED) { Set(Directory, paths.defaultDirectory); return; }
    if (id == OpenCompanions && notification == BN_CLICKED) {
        std::wstring folder;
        auto result = setup::EnsureCompanionFolder(paths, folder);
        if (result) {
            SHELLEXECUTEINFOW info{sizeof(info)};
            info.fMask = SEE_MASK_FLAG_NO_UI; info.hwnd = window; info.lpVerb = L"open";
            info.lpFile = folder.c_str(); info.nShow = SW_SHOWNORMAL;
            if (!ShellExecuteExW(&info)) { const DWORD error = GetLastError(); result = {error, setup::ErrorText(error)}; }
        }
        if (!result) ShowSetupError(window, result, l10n::Text("setup.error.openstartup"));
        return;
    }
    if ((id == Browse || id == AddProgram) && notification == BN_CLICKED) {
        const bool selectProgram = id == AddProgram;
        IFileOpenDialog* dialog = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
        native::ComPtr<IFileOpenDialog> ownedDialog(dialog);
        if (SUCCEEDED(hr)) {
            DWORD flags = 0;
            hr = dialog->GetOptions(&flags);
            if (SUCCEEDED(hr)) hr = dialog->SetOptions(flags | (selectProgram ? FOS_FILEMUSTEXIST | FOS_NODEREFERENCELINKS : FOS_PICKFOLDERS) | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
            std::wstring supported;
            if (selectProgram) for (const auto& format : companions::Formats) {
                if (!supported.empty()) supported += L';';
                supported += L'*'; supported += format.ext;
            }
            const COMDLG_FILTERSPEC filters[] = {
                {l10n::Text("setup.filter.supported"), supported.c_str()},
                {l10n::Text("setup.filter.shortcuts"), L"*.lnk"}, {l10n::Text("setup.filter.batch"), L"*.bat;*.cmd"},
                {l10n::Text("setup.filter.powershell"), L"*.ps1"}, {l10n::Text("setup.filter.windows"), L"*.vbs;*.js;*.wsf"},
                {l10n::Text("setup.filter.python"), L"*.py;*.pyw"}, {l10n::Text("setup.filter.node"), L"*.cjs;*.mjs"},
                {l10n::Text("setup.filter.java"), L"*.jar"}, {l10n::Text("setup.filter.apps"), L"*.exe"}
            };
            if (SUCCEEDED(hr) && selectProgram) hr = dialog->SetFileTypes(ARRAYSIZE(filters), filters);
            dialog->SetTitle(selectProgram ? l10n::Text("setup.dialog.program") : l10n::Text("setup.dialog.directory"));
            IShellItem* initial = nullptr;
            if (!selectProgram && SUCCEEDED(SHCreateItemFromParsingName(Text(Directory).c_str(), nullptr, IID_PPV_ARGS(&initial)))) {
                dialog->SetFolder(initial); initial->Release();
            }
            if (SUCCEEDED(hr)) hr = dialog->Show(window);
            if (SUCCEEDED(hr)) {
                IShellItem* item = nullptr;
                hr = dialog->GetResult(&item);
                if (SUCCEEDED(hr)) {
                    native::ComPtr<IShellItem> ownedItem(item);
                    PWSTR path = nullptr; hr = item->GetDisplayName(SIGDN_FILESYSPATH, &path);
                    native::TaskPtr<wchar_t> ownedPath(path);
                    if (SUCCEEDED(hr)) {
                        if (selectProgram) {
                            const auto added = setup::AddCompanionProgram(paths, path);
                            if (!added) ShowSetupError(window, added, l10n::Text("setup.error.add"));
                            else Set(Footer, savedThisSession ? l10n::Text("setup.status.added")
                                : l10n::Text("setup.status.addeddeploy"));
                        } else Set(Directory, path);
                    }
                }
            }
        }
        if (FAILED(hr) && hr != HRESULT_FROM_WIN32(ERROR_CANCELLED))
            MessageBoxW(window, setup::ErrorText(static_cast<DWORD>(hr)).c_str(), selectProgram ? l10n::Text("setup.error.program") : l10n::Text("setup.error.directory"), MB_OK | MB_ICONERROR);
        return;
    }
    if (id == CloseButton && notification == BN_CLICKED) { SendMessageW(window, WM_CLOSE, 0, 0); return; }
    if (id == ApplyButton && notification == BN_CLICKED) { BeginSave(); return; }
    if (notification == EN_CHANGE || notification == BN_CLICKED) {
        if (id != Preview) Refresh(true);
    }
}

void App::BeginSave() {
    if (closing || busy || fatal) return;
    if (savedThisSession && !dirty) {
        Set(Footer, l10n::Text("setup.status.completed")); return;
    }
    const setup::Options options = Options();
    std::wstring unused;
    if (!setup::BuildArguments(options, unused) || !setup::NormalizeDirectory(options.directory, unused)) { Refresh(false); return; }
    try {
        auto completion = std::make_unique<Completion>();
        completion->previous = savedTarget;
        completion->checkStartup = options.startup && !startupReminderShown;
        completion->codexEnv = options.codexEnv;
        completion->wroteCodexEnv = options.proxy && options.codexEnv;
        std::wstring failureMessage = l10n::Text("setup.error.saveinterrupted");
        pendingSave = std::move(completion);
        saveFinished.store(false, std::memory_order_relaxed);
        if (!SetTimer(window, WorkerTimer, 100, nullptr)) throw std::bad_alloc();
        // System operations run here; only FinishSave updates controls after completion.
        worker = std::thread([this, options, completion = pendingSave.get(), failureMessage = std::move(failureMessage)]() mutable {
            const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
            if (FAILED(com)) {
                completion->result.code = static_cast<DWORD>(com);
                completion->result.message.swap(failureMessage);
            }
            else {
                automation::UserLock operation;
                try {
                    const DWORD locked = operation.Acquire(paths.settingsLink);
                    completion->result = locked ? setup::Result{locked, l10n::Text("error.busy")}
                        : setup::Apply(paths, options, payload, completion->target);
                }
                catch (...) {
                    completion->result.code = ERROR_NOT_ENOUGH_MEMORY;
                    completion->result.message.swap(failureMessage);
                }
                if (completion->result && completion->checkStartup) {
                    try { completion->startupReport = setup::TryDisableOfficialStartup(); }
                    catch (...) { completion->startupReport.error = ERROR_NOT_ENOUGH_MEMORY; }
                }
                if (completion->result && completion->wroteCodexEnv) {
                    try { completion->closeResult = setup::TryCloseChatGPT(); }
                    catch (...) { completion->closeResult = setup::ChatGPTCloseResult::NeedsManualExit; }
                }
                CoUninitialize();
            }
            saveFinished.store(true, std::memory_order_release);
            PostMessageW(window, SavedMessage, 0, 0);
        });
    } catch (...) {
        pendingSave.reset();
        if (!detectingPort) KillTimer(window, WorkerTimer);
        Set(Footer, l10n::Text("setup.error.savestart"));
        return;
    }
    busy = true;
    for (const auto& control : controls) EnableWindow(control.window, FALSE);
    Refresh(false); UpdateWindow(window);
}

void App::FinishSave(std::unique_ptr<Completion> done) {
    if (worker.joinable()) worker.join();
    busy = false;
    if (!detectingPort) KillTimer(window, WorkerTimer);
    for (const auto& control : controls) EnableWindow(control.window, TRUE);
    if (done->result) {
        const bool preferenceSaved = SaveCodexEnvPreference(done->codexEnv);
        setup::Loaded loaded;
        const setup::Result result = setup::Load(paths, loaded);
        if (result) RestoreOptions(loaded);
        else {
            dirty = false; savedTarget = done->target;
            Set(State, l10n::Text("setup.state.readfailed"));
        }
        Check(CodexEnv, done->codexEnv);
        savedThisSession = true;
        const bool moved = !done->previous.empty() && _wcsicmp(done->previous.c_str(), done->target.c_str()) != 0;
        completionKey = done->wroteCodexEnv ? done->closeResult == setup::ChatGPTCloseResult::Closed ? "setup.quit.closed"
            : done->closeResult == setup::ChatGPTCloseResult::NotRunning ? "setup.quit.absent" : "setup.quit.manual"
            : !preferenceSaved ? "setup.status.configsaved" : moved ? "setup.status.keptold" : "setup.status.nextlaunch";
        envPreferenceWarning = !preferenceSaved;
        Refresh(false);
        if (done->checkStartup) ShowStartupReminder(*done, preferenceSaved);
    } else {
        Refresh(false);
        ShowSetupError(window, done->result, l10n::Text("setup.error.saving"));
    }
    SetFocus(Get(done->result ? CloseButton : ApplyButton));
}

void App::ShowStartupReminder(const Completion& done, bool preferenceSaved) {
    startupReminderShown = true;
    std::wstring reminder = l10n::Format("setup.dialog.startupsteps", {{"result", done.startupReport.Message()}});
    if (done.wroteCodexEnv) reminder += L"\n\n" + std::wstring(ChatGPTCloseMessage(done.closeResult));
    if (!done.wroteCodexEnv && preferenceSaved) {
        completionKey = "setup.status.checkstartup";
        Set(Footer, l10n::Text("setup.status.checkstartup"));
    }
    MessageBoxW(window, reminder.c_str(), l10n::Text("setup.dialog.deployed"), MB_OK | MB_ICONINFORMATION);
    const setup::Result opened = setup::OpenStartupSettings(window);
    if (!opened) MessageBoxW(window, l10n::Format("setup.dialog.savedprefix", {{"error", opened.message}}).c_str(), l10n::Text("setup.dialog.manualstartup"), MB_OK | MB_ICONINFORMATION);
}

void App::DrawSurface(HWND target, HDC dc) {
    RECT rect{}; GetClientRect(target, &rect);
    FillRect(dc, &rect, target == body ? background : surface);
    if (target == body) {
        auto card = [&](int top, int bottom) {
            const RECT r{ S(32), S(top + headerOffset) - scroll, rect.right - S(32), S(bottom + headerOffset) - scroll };
            Rounded(dc, r, S(16), colors.surface, colors.divider);
        };
        if (advanced) card(140, 256);
        else {
            const int offset = Checked(Proxy) ? 48 : 0;
            card(140, 316); card(372, 518 + offset);
            RECT segments{S(48), S(458 + offset + headerOffset) - scroll, S(256), S(494 + offset + headerOffset) - scroll};
            Rounded(dc, segments, S(10), colors.field, colors.border);
        }
        for (const auto& item : controls) {
            if (!IsField(item.id) || !(GetWindowLongPtrW(item.window, GWL_STYLE) & WS_VISIBLE)) continue;
            RECT r{}; GetWindowRect(item.window, &r); MapWindowPoints(nullptr, body, reinterpret_cast<POINT*>(&r), 2);
            InflateRect(&r, S(12), S(item.id == Host || item.id == Port ? 8 : 10));
            const bool focused = GetFocus() == item.window;
            Rounded(dc, r, S(10), FieldColor(), focused ? colors.accent : colors.border, focused ? 2.0f : 1.0f);
        }
        const RECT thumb = ScrollThumb();
        Rounded(dc, thumb, S(3), colors.secondary, colors.secondary);
    } else {
        HPEN pen = CreatePen(PS_SOLID, 1, colors.border);
        HGDIOBJ old = SelectObject(dc, pen);
        MoveToEx(dc, 0, rect.bottom - S(footerTextHeight + 64), nullptr); LineTo(dc, rect.right, rect.bottom - S(footerTextHeight + 64));
        SelectObject(dc, old); DeleteObject(pen);
    }
}
void App::Paint(HWND target) {
    PAINTSTRUCT ps{}; HDC output = BeginPaint(target, &ps);
    const RECT& rect = ps.rcPaint;
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
void App::SelectProtocol(int id) {
    selectedProtocol = id;
    for (int segment : {Protocol, ProtocolHttps, ProtocolSocks}) {
        HWND control = Get(segment);
        const LONG_PTR style = GetWindowLongPtrW(control, GWL_STYLE);
        SetWindowLongPtrW(control, GWL_STYLE, segment == id ? style | WS_TABSTOP : style & ~WS_TABSTOP);
        InvalidateRect(control, nullptr, FALSE);
    }
}
void App::UpdateMotion(HWND control) {
    if (!IsButton(GetDlgCtrlID(control))) return;
    const ULONGLONG now = GetTickCount64();
    for (auto& item : controls) if (item.window == control) {
        const bool enabled = IsWindowEnabled(control) != FALSE;
        const bool animate = motionEnabled && !loading && enabled && IsWindowVisible(control);
        item.hover.Set(enabled && GetPropW(control, L"setup.hover") ? 1.0f : 0.0f, animate, now);
        item.toggle.Set(Checked(item.id) ? 1.0f : 0.0f, animate, now);
        if (item.hover.Active(now) || item.toggle.Active(now)) SetTimer(window, MotionTimer, 16, nullptr);
        break;
    }
}
bool App::TickMotion() {
    bool active = false;
    const ULONGLONG now = GetTickCount64();
    for (auto& item : controls) {
        if (item.hover.from == item.hover.to && item.toggle.from == item.toggle.to) continue;
        InvalidateRect(item.window, nullptr, FALSE);
        if (item.hover.Active(now) || item.toggle.Active(now)) active = true;
        else { item.hover.from = item.hover.to; item.toggle.from = item.toggle.to; }
    }
    if (!active) KillTimer(window, MotionTimer);
    return active;
}
void App::DrawControl(HWND control, HDC dc) {
    RECT rect{}; GetClientRect(control, &rect);
    const int id = GetDlgCtrlID(control);
    const bool enabled = IsWindowEnabled(control) != FALSE;
    const bool focus = GetFocus() == control;
    const bool focusVisible = enabled && focus && (SendMessageW(control, WM_QUERYUISTATE, 0, 0) & UISF_HIDEFOCUS) == 0;
    const bool pressed = (SendMessageW(control, BM_GETSTATE, 0, 0) & BST_PUSHED) != 0;
    const bool checked = Checked(id);
    float hover = 0, toggle = checked ? 1.0f : 0.0f;
    const ULONGLONG now = GetTickCount64();
    for (const auto& item : controls) if (item.window == control) {
        hover = item.hover.Value(now); toggle = item.toggle.Value(now); break;
    }
    FillRect(dc, &rect, id == ThemeToggle || id == LanguageButton || id == AddProgram || id == OpenCompanions ? background : IsSegment(id) ? field : surface);
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
    } else if (IsHelp(id)) {
        RECT circle{S(3), S(3), rect.right - S(3), rect.bottom - S(3)};
        Rounded(dc, circle, S(9), Blend(colors.surface, colors.hover, hover), focusVisible ? colors.accent : colors.secondary);
        SetTextColor(dc, colors.secondary);
        HGDIOBJ old = SelectObject(dc, fonts[3]);
        DrawTextW(dc, L"?", 1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(dc, old); return;
    } else if (id == ThemeToggle) {
        const COLORREF fill = enabled ? Blend(colors.surface, colors.hover, pressed ? 1.0f : hover) : colors.surface;
        Rounded(dc, rect, S(HeaderButtonRadius), fill, focusVisible ? colors.accent : colors.divider,
            (focusVisible ? 2.0f : 1.0f) * static_cast<float>(dpi) / 96.0f);
        Gdiplus::Graphics graphics(dc);
        graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        const float scale = static_cast<float>(dpi) / 96.0f;
        const float x = rect.right / 2.0f, y = rect.bottom / 2.0f + (pressed ? scale : 0.0f);
        Gdiplus::Pen pen(Gdiplus::Color(255, GetRValue(textColor), GetGValue(textColor), GetBValue(textColor)), 1.6f * scale);
        pen.SetStartCap(Gdiplus::LineCapRound); pen.SetEndCap(Gdiplus::LineCapRound);
        if (dark) {
            // Sun: switching from dark to light. Vector strokes stay crisp at every DPI.
            graphics.DrawEllipse(&pen, x - 3.5f * scale, y - 3.5f * scale, 7.0f * scale, 7.0f * scale);
            const float directions[][2] = {{1,0},{0,1},{-1,0},{0,-1},
                {0.7071f,0.7071f},{-0.7071f,0.7071f},{-0.7071f,-0.7071f},{0.7071f,-0.7071f}};
            for (const auto& direction : directions)
                graphics.DrawLine(&pen, x + direction[0] * 6.5f * scale, y + direction[1] * 6.5f * scale,
                    x + direction[0] * 9.0f * scale, y + direction[1] * 9.0f * scale);
        } else {
            // Crescent: switching from light to dark. No font or bitmap dependency.
            Gdiplus::GraphicsPath moon;
            moon.AddBezier(x + 4 * scale, y - 8 * scale, x - 4 * scale, y - 10 * scale,
                x - 10 * scale, y - 3 * scale, x - 7 * scale, y + 4 * scale);
            moon.AddBezier(x - 7 * scale, y + 4 * scale, x - 4 * scale, y + 10 * scale,
                x + 5 * scale, y + 10 * scale, x + 8 * scale, y + 3 * scale);
            moon.AddBezier(x + 8 * scale, y + 3 * scale, x + 1 * scale, y + 6 * scale,
                x - 4 * scale, y - 2 * scale, x + 4 * scale, y - 8 * scale);
            moon.CloseFigure(); graphics.DrawPath(&pen, &moon);
        }
        return; // The window caption remains available to accessibility tools, but is not painted.
    } else if (id == LanguageButton) {
        const float scale = static_cast<float>(dpi) / 96.0f;
        Rounded(dc, rect, S(HeaderButtonRadius), Blend(colors.surface, colors.hover, pressed || languagePopup ? 1.0f : hover),
            focusVisible ? colors.accent : colors.divider, (focusVisible ? 2.0f : 1.0f) * scale);
        text.left += S(12); text.right -= S(28);
        Gdiplus::Graphics graphics(dc); graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        Gdiplus::Pen pen(Gdiplus::Color(255, GetRValue(textColor), GetGValue(textColor), GetBValue(textColor)), 1.5f * scale);
        pen.SetStartCap(Gdiplus::LineCapRound); pen.SetEndCap(Gdiplus::LineCapRound); pen.SetLineJoin(Gdiplus::LineJoinRound);
        const float x = static_cast<float>(rect.right - S(16)), y = rect.bottom / 2.0f + (pressed ? scale : 0);
        const float direction = languagePopup ? -1.0f : 1.0f;
        const Gdiplus::PointF points[]{{x - 3.5f * scale, y - 1.75f * scale * direction},
            {x, y + 1.75f * scale * direction}, {x + 3.5f * scale, y - 1.75f * scale * direction}};
        graphics.DrawLines(&pen, points, 3);
    } else if (IsSwitch(id)) {
        RECT track{ rect.right - S(49), (rect.bottom - S(26)) / 2, rect.right - S(5), (rect.bottom + S(26)) / 2 };
        if (focusVisible) {
            RECT ring = track; InflateRect(&ring, S(3), S(3));
            Rounded(dc, ring, S(16), colors.surface, colors.accent, static_cast<float>(S(2)));
        }
        const COLORREF trackColor = contrast ? (checked && enabled ? colors.accent : colors.surface)
            : enabled ? Blend(colors.border, colors.accent, toggle) : colors.divider;
        Rounded(dc, track, S(13), trackColor, contrast ? (enabled ? colors.text : colors.secondary) : trackColor);
        const int left = track.left + S(3) + static_cast<int>(S(18) * toggle + 0.5f);
        RECT knob{ left, track.top + S(3), left + S(20), track.bottom - S(3) };
        Rounded(dc, knob, S(10), contrast ? (!enabled ? colors.secondary : checked ? GetSysColor(COLOR_HIGHLIGHTTEXT) : colors.text)
            : RGB(255,255,255), trackColor, 0.5f);
        text.right = track.left - S(16); textFlags |= DT_LEFT;
    } else {
        const bool primary = id == ApplyButton;
        const bool quiet = id == Advanced;
        const bool segment = IsSegment(id);
        COLORREF fill = quiet ? colors.surface : FieldColor();
        COLORREF edge = quiet ? fill : colors.border;
        if (enabled && ((primary) || (segment && checked))) {
            fill = primary ? Blend(colors.primary, colors.primaryHover, hover)
                : contrast ? colors.selected : dark ? colors.hover : colors.surface;
            edge = primary ? fill : contrast ? colors.text : colors.divider;
            textColor = primary ? colors.primaryText : colors.selectedText;
        } else if (quiet) textColor = colors.secondary;
        if (enabled && !primary && !(segment && checked) && !contrast)
            fill = Blend(fill, colors.hover, pressed ? 1.0f : hover);
        if (enabled && pressed && primary) fill = colors.primaryHover;
        if (primary && enabled) edge = fill;
        if (quiet) edge = contrast ? colors.border : fill;
        const bool ring = focusVisible;
        if (segment && !checked && !ring) edge = fill;
        Rounded(dc, rect, S(segment ? 6 : 10), fill, ring ? colors.accent : edge, ring ? 2.0f : 1.0f);
        if (primary && focusVisible) {
            RECT inset = rect; InflateRect(&inset, -S(3), -S(3));
            Rounded(dc, inset, S(7), fill, textColor, 1.0f);
        }
        textFlags |= DT_CENTER;
        if (pressed && !segment) OffsetRect(&text, 0, S(1));
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
}
LRESULT ColorControl(App& app, HDC dc, HWND control) {
    const int id = GetDlgCtrlID(control);
    if (IsField(id)) {
        SetTextColor(dc, IsWindowEnabled(control) ? app.colors.text : app.colors.secondary);
        SetBkColor(dc, app.FieldColor()); return reinterpret_cast<LRESULT>(app.field);
    }
    const bool surface = id == SilentHint || id == ProtocolLabel || id == HostLabel || id == PortLabel || id == DirectoryHint
        || id == Footer || id == CloseButton || id == ApplyButton || id == Advanced;
    const bool secondary = id == State || id == SetupVersion || id == SilentHint
        || id == DirectoryHint || id == ProtocolLabel || id == HostLabel || id == PortLabel || id == Footer;
    SetTextColor(dc, secondary ? app.colors.secondary : app.colors.text);
    SetBkColor(dc, surface ? app.colors.surface : app.colors.background);
    return reinterpret_cast<LRESULT>(surface ? app.surface : app.background);
}
LRESULT CALLBACK StyledControl(HWND window, UINT message, WPARAM wparam, LPARAM lparam, UINT_PTR, DWORD_PTR reference) {
    App& app = *reinterpret_cast<App*>(reference);
    const int id = GetDlgCtrlID(window);
    if (IsButton(id) && (message == WM_LBUTTONDOWN || message == WM_KEYDOWN)) {
        // Keep one input-mode state for the whole window; Tab also updates this through IsDialogMessage.
        SendMessageW(app.window, WM_CHANGEUISTATE,
            MAKEWPARAM(message == WM_LBUTTONDOWN ? UIS_SET : UIS_CLEAR, UISF_HIDEFOCUS), 0);
    }
    if (IsSegment(id)) {
        if (message == BM_GETCHECK) return app.selectedProtocol == id ? BST_CHECKED : BST_UNCHECKED;
        if (message == BM_SETCHECK) {
            if (wparam == BST_CHECKED) app.SelectProtocol(id);
            else if (app.selectedProtocol == id) app.SelectProtocol(0);
            return 0;
        }
        if (message == WM_GETDLGCODE) return DLGC_BUTTON | DLGC_RADIOBUTTON | DLGC_WANTARROWS;
        if (message == WM_KEYDOWN && (wparam == VK_LEFT || wparam == VK_RIGHT || wparam == VK_UP || wparam == VK_DOWN)) {
            if (!IsWindowEnabled(window)) return 0;
            const int direction = wparam == VK_LEFT || wparam == VK_UP ? -1 : 1;
            const int next = Protocol + (id - Protocol + direction + 3) % 3;
            SetFocus(app.Get(next)); app.Command(next, BN_CLICKED); return 0;
        }
    }
    // Also allow retry by clicking an empty field that already has focus.
    if (id == Port && message == WM_LBUTTONDOWN && GetFocus() == window)
        PostMessageW(app.window, DetectPortMessage, 0, 0);
    if (IsActionButton(id) && message == WM_GETDLGCODE)
        return DLGC_BUTTON | (GetFocus() == window || GetPropW(window, L"setup.default") ? DLGC_DEFPUSHBUTTON : DLGC_UNDEFPUSHBUTTON);
    if (IsOwnerDrawButton(id) && message == BM_SETSTYLE) {
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
        app.UpdateMotion(window);
        TRACKMOUSEEVENT track{ sizeof(track), TME_LEAVE, window, 0 }; TrackMouseEvent(&track);
        InvalidateRect(window, nullptr, FALSE);
    }
    if (message == WM_MOUSELEAVE) {
        RemovePropW(window, L"setup.hover"); app.UpdateMotion(window); InvalidateRect(window, nullptr, FALSE);
    }
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
    if (message == BM_SETCHECK || message == BM_CLICK || message == WM_LBUTTONUP || message == WM_KEYUP || message == WM_ENABLE)
        app.UpdateMotion(window);
    // Synchronous painting uses WM_DRAWITEM for actions/segments and NM_CUSTOMDRAW for native checkboxes.
    // Keep invalidation for theme, focus and check-state changes; never rely on it to cover native painting.
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
    case WM_NOTIFY: {
        const auto header = reinterpret_cast<const NMHDR*>(lparam);
        if (header && header->code == NM_CUSTOMDRAW && IsSwitch(static_cast<int>(header->idFrom))
            && app->Get(static_cast<int>(header->idFrom)) == header->hwndFrom) {
            const auto draw = reinterpret_cast<const NMCUSTOMDRAW*>(lparam);
            // Preserve native checkbox state, keyboard and accessibility behavior, but own every paint.
            if (draw->dwDrawStage == CDDS_PREERASE || draw->dwDrawStage == CDDS_PREPAINT) {
                app->PaintControl(header->hwndFrom, draw->hdc); return CDRF_SKIPDEFAULT;
            }
        }
        break;
    }
    case WM_DRAWITEM: {
        const auto item = reinterpret_cast<const DRAWITEMSTRUCT*>(lparam);
        if (item && IsOwnerDrawButton(static_cast<int>(item->CtlID))) { app->PaintControl(item->hwndItem, item->hDC); return TRUE; }
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
        app->preferredDark = ReadDarkPreference();
        app->dpi = GetDpiForWindow(window); app->Theme();
        app->body = CreateWindowExW(WS_EX_CONTROLPARENT | WS_EX_COMPOSITED, L"ChatGPTSetupBody", l10n::Text("setup.body"),
            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, 0, 0, 0, 0, window, nullptr, app->instance, app);
        app->CreateControls(); app->Theme();
        setup::Loaded loaded;
        setup::Result result;
        if (app->paths.defaultDirectory.empty()) result = setup::GetPaths(app->paths);
        if (result) result = setup::Load(app->paths, loaded);
        if (!result) {
            loaded.options.directory = app->paths.defaultDirectory;
            app->fatal = true;
            ShowSetupError(window, result, l10n::Text("setup.error.load"));
        }
        app->RestoreOptions(loaded);
        if (app->fatal) app->Set(Footer, l10n::Text("setup.status.loadfailed"));
        app->Layout(); return 0;
    }
    case WM_GETMINMAXINFO: {
        auto limits = reinterpret_cast<MINMAXINFO*>(lparam);
        limits->ptMinTrackSize = { app->S(620), app->S(500) }; return 0;
    }
    case WM_MOUSEACTIVATE:
        if (app->languagePopup) { app->CloseLanguageMenu(false); return MA_ACTIVATEANDEAT; }
        break;
    case WM_SIZE: app->Layout(); return 0;
    case WM_DPICHANGED: {
        app->dpi = HIWORD(wparam); app->Theme();
        const RECT* suggested = reinterpret_cast<RECT*>(lparam);
        SetWindowPos(window, nullptr, suggested->left, suggested->top, suggested->right - suggested->left,
            suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE);
        app->Layout(); return 0;
    }
    case WM_SETTINGCHANGE: case WM_THEMECHANGED: app->Theme(); app->Layout(); return 0;
    case WM_TIMER:
        if (wparam == MotionTimer) { app->TickMotion(); return 0; }
        if (wparam == WorkerTimer) { app->PollWorkers(); return 0; }
        break;
    case WM_DRAWITEM: {
        const auto item = reinterpret_cast<const DRAWITEMSTRUCT*>(lparam);
        if (item && (IsOwnerDrawButton(static_cast<int>(item->CtlID)) || item->CtlID == Footer)) {
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
    case PortDetectedMessage: case SavedMessage: app->PollWorkers(); return 0;
    case WM_CLOSE:
        if (app->busy) return 0;
        if (app->dirty && MessageBoxW(window, l10n::Text("setup.dialog.discard"),
            l10n::Text("setup.dialog.close"), MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES) return 0;
        DestroyWindow(window); return 0;
    case WM_DESTROY: app->StopWorkers(); app->window = nullptr; PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}
} // namespace

int WINAPI wWinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE, _In_ PWSTR arguments, _In_ int show) {
    l10n::Initialize();
    // Dispatch before GUI mutex, foreground activation, GDI+ or any window creation.
    if (arguments && *arguments) return setup::RunSetupCommandLine(instance, arguments);
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
            MessageBoxW(nullptr, l10n::Text("setup.error.payload"), l10n::Text("setup.error.open"), MB_OK | MB_ICONERROR);
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
            const int height = std::min(MulDiv(812, static_cast<int>(dpi), 96), static_cast<int>(work.bottom - work.top - 32));
            HWND window = CreateWindowExW(WS_EX_CONTROLPARENT, L"ChatGPTLauncherSetup", l10n::Text("setup.window"),
                WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, work.left + (work.right - work.left - width) / 2,
                work.top + (work.bottom - work.top - height) / 2, width, height, nullptr, nullptr, instance, &app);
            if (!window) exitCode = static_cast<int>(GetLastError());
            else {
                ShowWindow(window, show); UpdateWindow(window);
                MSG message{};
                BOOL status = 0;
                while ((status = GetMessageW(&message, nullptr, 0, 0)) > 0) {
                    if (app.LanguageMenuMessage(message)) continue;
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
