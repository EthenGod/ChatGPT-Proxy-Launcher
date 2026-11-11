// SPDX-License-Identifier: GPL-3.0-only
#include "../src/StartupIntegration.h"
#include "../src/ProxyDetection.h"
#include "../src/CodexClose.h"
#include <stdio.h>
#include <chrono>
#include <shellapi.h>
#include <uxtheme.h>
static bool trackPaintBuffer = false;
static RECT paintBufferBounds{};
static HPAINTBUFFER WINAPI TestPaintBuffer(HDC target, const RECT* bounds, BP_BUFFERFORMAT format,
    BP_PAINTPARAMS* parameters, HDC* output) {
    if (trackPaintBuffer) paintBufferBounds = *bounds;
    return BeginBufferedPaint(target, bounds, format, parameters, output);
}
static unsigned disableCalls = 0, settingsCalls = 0, reminderCalls = 0;
static unsigned folderCalls = 0;
static bool openFolderSucceeds = true, folderRequestValid = false;
static std::wstring lastDialog, lastFolder;
static bool settingsAfterReminder = false;
static DWORD settingsError = 0;
static setup::ProxyDetection proxyResult;
static unsigned proxyCalls = 0;
static unsigned codexCloseCalls = 0;
static setup::ChatGPTCloseResult closeResult = setup::ChatGPTCloseResult::Closed;
static std::wstring envFileForClose;
static bool envSavedBeforeClose = true;
static DWORD storedLightTheme = 0;
static LSTATUS themeReadResult = ERROR_FILE_NOT_FOUND, themeWriteResult = ERROR_SUCCESS;
static unsigned themeWrites = 0;
static bool themePathValid = true;
static DWORD storedCodexEnv = 1;
static LSTATUS envReadResult = ERROR_FILE_NOT_FOUND, envWriteResult = ERROR_SUCCESS;
static LSTATUS WINAPI TestThemeRead(HKEY key, LPCWSTR path, LPCWSTR name, DWORD, LPDWORD, PVOID data, LPDWORD size) {
    if (name && !wcscmp(name, L"ConfigureCodexEnv")) {
        themePathValid = themePathValid && key == HKEY_CURRENT_USER && path && !wcscmp(path, L"Software\\Community\\ChatGPTLauncherSetup");
        if (envReadResult != ERROR_SUCCESS) return envReadResult;
        if (*size < sizeof(DWORD)) return ERROR_MORE_DATA;
        *static_cast<DWORD*>(data) = storedCodexEnv; *size = sizeof(DWORD); return ERROR_SUCCESS;
    }
    themePathValid = themePathValid && key == HKEY_CURRENT_USER && path && name
        && !wcscmp(path, L"Software\\Community\\ChatGPTLauncherSetup") && !wcscmp(name, L"LightTheme");
    if (themeReadResult != ERROR_SUCCESS) return themeReadResult;
    if (*size < sizeof(DWORD)) return ERROR_MORE_DATA;
    *static_cast<DWORD*>(data) = storedLightTheme; *size = sizeof(DWORD); return ERROR_SUCCESS;
}
static LSTATUS WINAPI TestThemeWrite(HKEY key, LPCWSTR path, LPCWSTR name, DWORD type, LPCVOID data, DWORD size) {
    if (name && !wcscmp(name, L"ConfigureCodexEnv")) {
        themePathValid = themePathValid && key == HKEY_CURRENT_USER && path && !wcscmp(path, L"Software\\Community\\ChatGPTLauncherSetup");
        if (type != REG_DWORD || size != sizeof(DWORD)) return ERROR_INVALID_DATA;
        if (envWriteResult != ERROR_SUCCESS) return envWriteResult;
        storedCodexEnv = *static_cast<const DWORD*>(data); envReadResult = ERROR_SUCCESS; return ERROR_SUCCESS;
    }
    ++themeWrites;
    themePathValid = themePathValid && key == HKEY_CURRENT_USER && path && name && type == REG_DWORD && size == sizeof(DWORD)
        && !wcscmp(path, L"Software\\Community\\ChatGPTLauncherSetup") && !wcscmp(name, L"LightTheme");
    if (themeWriteResult != ERROR_SUCCESS) return themeWriteResult;
    storedLightTheme = *static_cast<const DWORD*>(data); themeReadResult = ERROR_SUCCESS; return ERROR_SUCCESS;
}
namespace setup {
ChatGPTCloseResult TryCloseChatGPT() {
    ++codexCloseCalls;
    envSavedBeforeClose = envSavedBeforeClose && GetFileAttributesW(envFileForClose.c_str()) != INVALID_FILE_ATTRIBUTES;
    return closeResult;
}
ProxyDetection DetectProxyPort(const std::wstring&, const std::wstring&) {
    ++proxyCalls; Sleep(20); return proxyResult;
}
StartupReport TryDisableOfficialStartup() { ++disableCalls; return {0, 0, 0, true}; }
Result OpenStartupSettings(HWND) {
    ++settingsCalls; settingsAfterReminder = lastDialog.find(L"不需要再次部署") != std::wstring::npos;
    return {settingsError, settingsError ? L"Settings unavailable" : L""};
}
std::wstring StartupReport::Message() const { return L"Test reminder"; }
}
static int WINAPI TestMessageBox(HWND, LPCWSTR message, LPCWSTR, UINT) {
    ++reminderCalls; lastDialog = message; return IDOK;
}
static BOOL WINAPI TestShellExecute(SHELLEXECUTEINFOW* info) {
    ++folderCalls; lastFolder = info->lpFile;
    folderRequestValid = info->lpVerb && !wcscmp(info->lpVerb, L"open") && !info->lpParameters
        && (info->fMask & SEE_MASK_FLAG_NO_UI) && lastDialog.find(lastFolder) != std::wstring::npos;
    if (!openFolderSucceeds) SetLastError(ERROR_ACCESS_DENIED);
    return openFolderSucceeds;
}
#define MessageBoxW TestMessageBox
#define ShellExecuteExW TestShellExecute
#define RegGetValueW TestThemeRead
#define RegSetKeyValueW TestThemeWrite
#define BeginBufferedPaint TestPaintBuffer
#define wWinMain SetupEntryForTest
#include "../src/ChatGPTLauncherSetup.cpp"
#undef wWinMain
#undef MessageBoxW
#undef ShellExecuteExW
#undef RegGetValueW
#undef RegSetKeyValueW
#undef BeginBufferedPaint

#define REQUIRE(condition) do { if (!(condition)) { printf("FAIL line %d: %s\n", __LINE__, #condition); return 20; } } while (0)
struct ButtonPaintProbe { unsigned paints = 0, captions = 0; };
static LRESULT CALLBACK ProbeButton(HWND window, UINT message, WPARAM wparam, LPARAM lparam, UINT_PTR, DWORD_PTR reference) {
    auto& probe = *reinterpret_cast<ButtonPaintProbe*>(reference);
    if (message == WM_PAINT) ++probe.paints;
    if (message == WM_SETTEXT) ++probe.captions;
    return DefSubclassProc(window, message, wparam, lparam);
}
static bool HasScheduledPaint(HWND button, const ButtonPaintProbe& probe, unsigned previous) {
    return probe.paints > previous || GetUpdateRect(button, nullptr, FALSE);
}
static bool ButtonFillMatches(App& app) {
    const HWND button = app.Get(ApplyButton);
    RECT rect{}; GetClientRect(button, &rect);
    HDC screen = GetDC(button), dc = CreateCompatibleDC(screen);
    HBITMAP bitmap = CreateCompatibleBitmap(screen, rect.right, rect.bottom);
    HGDIOBJ old = SelectObject(dc, bitmap);
    PatBlt(dc, 0, 0, rect.right, rect.bottom, WHITENESS);
    SendMessageW(button, WM_PRINT, reinterpret_cast<WPARAM>(dc), PRF_CLIENT | PRF_ERASEBKGND);
    const COLORREF actual = GetPixel(dc, app.S(10), rect.bottom / 2);
    const bool pressed = (SendMessageW(button, BM_GETSTATE, 0, 0) & BST_PUSHED) != 0;
    const COLORREF expected = !IsWindowEnabled(button) ? app.FieldColor()
        : pressed ? app.colors.primaryHover : app.colors.primary;
    SelectObject(dc, old); DeleteObject(bitmap); DeleteDC(dc); ReleaseDC(button, screen);
    return actual == expected;
}
static bool SegmentFillMatches(App& app, int id) {
    HWND control = app.Get(id);
    RECT rect{}; GetClientRect(control, &rect);
    HDC screen = GetDC(control), dc = CreateCompatibleDC(screen);
    HBITMAP bitmap = CreateCompatibleBitmap(screen, rect.right, rect.bottom);
    HGDIOBJ old = SelectObject(dc, bitmap);
    SendMessageW(control, WM_PRINT, reinterpret_cast<WPARAM>(dc), PRF_CLIENT | PRF_ERASEBKGND);
    const COLORREF fill = GetPixel(dc, app.S(8), rect.bottom / 2);
    const COLORREF corner = GetPixel(dc, 0, 0);
    const COLORREF expected = app.Checked(id) && IsWindowEnabled(control)
        ? app.contrast ? app.colors.selected : app.dark ? app.colors.hover : app.colors.surface : app.FieldColor();
    SelectObject(dc, old); DeleteObject(bitmap); DeleteDC(dc); ReleaseDC(control, screen);
    return fill == expected && corner == app.FieldColor();
}
static LRESULT CALLBACK ProbeOwnerDraw(HWND window, UINT message, WPARAM wparam, LPARAM lparam, UINT_PTR, DWORD_PTR reference) {
    if (message == WM_DRAWITEM) ++*reinterpret_cast<unsigned*>(reference);
    return DefSubclassProc(window, message, wparam, lparam);
}
static bool FooterAligned(App& app) {
    RECT root{}, body{}, status{}, advanced{}, apply{}, close{};
    GetClientRect(app.window, &root); GetClientRect(app.body, &body);
    auto bounds = [&](int id, RECT& rect) { GetWindowRect(app.Get(id), &rect); MapWindowPoints(nullptr, app.window, reinterpret_cast<POINT*>(&rect), 2); };
    bounds(Footer, status); bounds(Advanced, advanced); bounds(ApplyButton, apply); bounds(CloseButton, close);
    return abs((status.top + status.bottom) - (body.bottom + advanced.top)) <= 1 && root.bottom - apply.bottom == app.S(16)
        && advanced.left == status.left && advanced.left == app.S(32) && root.right - apply.right == app.S(32)
        && advanced.top == close.top && close.top == apply.top && apply.bottom - apply.top == app.S(40);
}
static bool Capture(App& app, PCWSTR filename) {
    RECT r{}; GetClientRect(app.window, &r);
    HDC screen = GetDC(app.window), dc = CreateCompatibleDC(screen);
    HBITMAP bitmap = CreateCompatibleBitmap(screen, r.right, r.bottom);
    HGDIOBJ old = SelectObject(dc, bitmap);
    app.DrawSurface(app.window, dc);
    RECT bodyRect{}; GetClientRect(app.body, &bodyRect);
    int state = SaveDC(dc); IntersectClipRect(dc, 0, 0, bodyRect.right, bodyRect.bottom);
    app.DrawSurface(app.body, dc); RestoreDC(dc, state);
    for (const auto& item : app.controls) {
        if (!(GetWindowLongPtrW(item.window, GWL_STYLE) & WS_VISIBLE)) continue;
        RECT itemRect{}; GetWindowRect(item.window, &itemRect);
        MapWindowPoints(nullptr, app.window, reinterpret_cast<POINT*>(&itemRect), 2);
        state = SaveDC(dc);
        if (GetParent(item.window) == app.body) IntersectClipRect(dc, 0, 0, bodyRect.right, bodyRect.bottom);
        IntersectClipRect(dc, itemRect.left, itemRect.top, itemRect.right, itemRect.bottom);
        SetViewportOrgEx(dc, itemRect.left, itemRect.top, nullptr);
        SendMessageW(item.window, WM_PRINT, reinterpret_cast<WPARAM>(dc), PRF_CLIENT | PRF_ERASEBKGND);
        RestoreDC(dc, state);
    }
    SelectObject(dc, old);
    CLSID png{};
    UINT count = 0, bytes = 0;
    Gdiplus::GetImageEncodersSize(&count, &bytes);
    std::vector<BYTE> buffer(bytes);
    auto codecs = reinterpret_cast<Gdiplus::ImageCodecInfo*>(buffer.data());
    Gdiplus::GetImageEncoders(count, bytes, codecs);
    for (UINT i = 0; i < count; ++i) if (!wcscmp(codecs[i].MimeType, L"image/png")) png = codecs[i].Clsid;
    bool saved = false;
    { Gdiplus::Bitmap image(bitmap, nullptr); saved = image.Save(filename, &png) == Gdiplus::Ok; }
    DeleteObject(bitmap); DeleteDC(dc); ReleaseDC(app.window, screen);
    return saved;
}
static bool PartialSurfaceMatches(App& app, HWND target) {
    RECT rect{}; GetClientRect(target, &rect);
    BITMAPINFO info{}; info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = rect.right; info.bmiHeader.biHeight = -rect.bottom;
    info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32; info.bmiHeader.biCompression = BI_RGB;
    HDC screen = GetDC(target), dc[2]{};
    HBITMAP bitmap[2]{}; HGDIOBJ previous[2]{}; DWORD* pixels[2]{};
    bool matches = screen != nullptr && rect.right > 0 && rect.bottom > 0;
    for (int i = 0; matches && i < 2; ++i) {
        dc[i] = CreateCompatibleDC(screen);
        bitmap[i] = CreateDIBSection(screen, &info, DIB_RGB_COLORS, reinterpret_cast<void**>(&pixels[i]), nullptr, 0);
        if (!dc[i] || !bitmap[i]) { matches = false; break; }
        previous[i] = SelectObject(dc[i], bitmap[i]);
        PatBlt(dc[i], 0, 0, rect.right, rect.bottom, WHITENESS);
    }
    if (matches) {
        app.DrawSurface(target, dc[0]);
        // Tile boundaries deliberately cross card corners, input outlines and the footer.
        for (LONG y = 0; matches && y < rect.bottom; y += 137) {
            for (LONG x = 0; matches && x < rect.right; x += 173) {
                const RECT part{x, y, (std::min)(x + 173, rect.right), (std::min)(y + 137, rect.bottom)};
                HDC buffered = nullptr;
                HPAINTBUFFER paint = BeginBufferedPaint(dc[1], &part, BPBF_COMPATIBLEBITMAP, nullptr, &buffered);
                if (!paint) { matches = false; break; }
                app.DrawSurface(target, buffered);
                matches = SUCCEEDED(EndBufferedPaint(paint, TRUE));
            }
        }
        GdiFlush();
        const size_t count = static_cast<size_t>(rect.right) * rect.bottom;
        for (size_t i = 0; matches && i < count; ++i)
            matches = (pixels[0][i] & 0xffffff) == (pixels[1][i] & 0xffffff);
    }
    for (int i = 0; i < 2; ++i) {
        if (previous[i]) SelectObject(dc[i], previous[i]);
        if (bitmap[i]) DeleteObject(bitmap[i]);
        if (dc[i]) DeleteDC(dc[i]);
    }
    if (screen) ReleaseDC(target, screen);
    return matches;
}
static bool LabelsFit(App& app) {
    for (const auto& item : app.controls) {
        if (!IsWindowVisible(item.window) || IsField(item.id) || item.id == Footer || item.id == ThemeToggle || IsHelp(item.id)) continue;
        RECT bounds{}; GetClientRect(item.window, &bounds);
        HDC dc = GetDC(item.window);
        HGDIOBJ old = SelectObject(dc, app.fonts[item.font]);
        const auto caption = app.Text(item.id);
        SIZE measured{};
        GetTextExtentPoint32W(dc, caption.c_str(), static_cast<int>(caption.size()), &measured);
        SelectObject(dc, old); ReleaseDC(item.window, dc);
        const int allowance = IsSwitch(item.id) ? app.S(64) : item.id == Advanced ? app.S(36) : 0;
        if (measured.cx > bounds.right - allowance || measured.cy > bounds.bottom) return false;
    }
    return true;
}
static bool WaitForSave(App& app) {
    const ULONGLONG until = GetTickCount64() + 10000;
    while (app.busy && GetTickCount64() < until) {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&message); DispatchMessageW(&message); }
        if (app.busy) MsgWaitForMultipleObjectsEx(0, nullptr, 10, QS_ALLINPUT, 0);
    }
    return !app.busy;
}
static int RunTests(PCWSTR folder) {
    REQUIRE(ReadCodexEnvPreference());
    envReadResult = ERROR_SUCCESS; storedCodexEnv = 9; REQUIRE(ReadCodexEnvPreference());
    storedCodexEnv = 0; REQUIRE(!ReadCodexEnvPreference());
    envReadResult = ERROR_FILE_NOT_FOUND; storedCodexEnv = 1;
    REQUIRE(ReadDarkPreference()); // Missing or malformed preference defaults to dark.
    themeReadResult = ERROR_SUCCESS; storedLightTheme = 9;
    REQUIRE(ReadDarkPreference());
    storedLightTheme = 1; REQUIRE(!ReadDarkPreference());
    themeReadResult = ERROR_FILE_NOT_FOUND;
    // Animation can reverse from the current presentation value and has a bounded lifetime.
    Motion motion;
    motion.Set(1.0f, true, 1000);
    REQUIRE(motion.Value(1000) == 0 && motion.Active(1075));
    const float halfway = motion.Value(1075);
    REQUIRE(halfway > 0 && halfway < 1);
    motion.Set(0.0f, true, 1075);
    REQUIRE(motion.Value(1075) == halfway && motion.Value(1225) == 0 && !motion.Active(1225));
    motion.Set(1.0f, false, 1225);
    REQUIRE(motion.Value(1225) == 1 && !motion.Active(1225));
    App app;
    app.instance = GetModuleHandleW(nullptr);
    const std::wstring root = setup::Join(folder, L"ui-isolated-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64()));
    app.paths = {root, setup::Join(root, L"startup.lnk"), setup::Join(root, L"settings.lnk"), setup::Join(root, L"desktop.lnk"), setup::Join(root, L"profile\\.codex\\.env")};
    envFileForClose = app.paths.codexEnvFile;
    WNDCLASSEXW cls{}; cls.cbSize = sizeof(cls); cls.hInstance = app.instance;
    cls.lpfnWndProc = WindowProc; cls.lpszClassName = L"SetupTestWindow"; RegisterClassExW(&cls);
    cls.lpfnWndProc = BodyProc; cls.lpszClassName = L"ChatGPTSetupBody"; RegisterClassExW(&cls);
    REQUIRE(CreateWindowExW(WS_EX_CONTROLPARENT, L"SetupTestWindow", L"Isolated UI test", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        -20000, -20000, 720, 812, nullptr, nullptr, app.instance, &app));
    ShowWindow(app.window, SW_SHOWNOACTIVATE);
    UpdateWindow(app.window);
    // Partial surface invalidation must not allocate a whole-window backing buffer.
    ValidateRect(app.body, nullptr);
    RECT repaint{2, 2, 18, 18};
    InvalidateRect(app.body, &repaint, FALSE);
    trackPaintBuffer = true;
    app.Paint(app.body);
    trackPaintBuffer = false;
    REQUIRE(EqualRect(&repaint, &paintBufferBounds));
    REQUIRE(PartialSurfaceMatches(app, app.window) && PartialSurfaceMatches(app, app.body));
    REQUIRE(app.preferredDark && (app.contrast || app.dark));
    REQUIRE(app.Text(SetupVersion) == L"配置器 v" APP_VERSION_TEXT);
    const auto initialArguments = app.Text(Preview);
    const bool initialDirty = app.dirty;
    app.Theme(1);
    SendMessageW(app.Get(ThemeToggle), BM_CLICK, 0, 0);
    REQUIRE(!app.preferredDark && !ReadDarkPreference() && storedLightTheme == 1);
    REQUIRE(app.Text(Preview) == initialArguments && app.dirty == initialDirty);
    app.Theme(); REQUIRE(app.contrast || !app.dark); // Refresh retains explicit selection.
    app.Theme(0);
    SendMessageW(app.Get(ThemeToggle), BM_CLICK, 0, 0);
    REQUIRE(app.preferredDark && ReadDarkPreference() && storedLightTheme == 0 && themeWrites == 2);
    themeWriteResult = ERROR_ACCESS_DENIED;
    app.Theme(1); SendMessageW(app.Get(ThemeToggle), BM_CLICK, 0, 0);
    REQUIRE(!app.preferredDark && app.Text(Footer).find(L"未能记住") != std::wstring::npos);
    themeWriteResult = ERROR_SUCCESS;
    app.Theme(0); SendMessageW(app.Get(ThemeToggle), BM_CLICK, 0, 0);
    REQUIRE(themePathValid);
    REQUIRE(app.Text(ThemeToggle) == L"切换为浅色");
    RECT themeBounds{}; GetClientRect(app.Get(ThemeToggle), &themeBounds);
    REQUIRE(themeBounds.right == themeBounds.bottom && themeBounds.right == app.S(36));
    SetFocus(app.Get(ThemeToggle));
    SendMessageW(app.Get(ThemeToggle), WM_KEYDOWN, VK_SPACE, 0);
    SendMessageW(app.Get(ThemeToggle), WM_KEYUP, VK_SPACE, 0);
    REQUIRE(!app.preferredDark && app.Text(ThemeToggle) == L"切换为深色");
    MSG themeKey{}; themeKey.hwnd = app.Get(ThemeToggle); themeKey.message = WM_KEYDOWN; themeKey.wParam = VK_RETURN;
    REQUIRE(IsDialogMessageW(app.window, &themeKey));
    REQUIRE(app.preferredDark && app.Text(ThemeToggle) == L"切换为浅色" && app.dirty == initialDirty);
    SetFocus(app.body);
    // Reduced-motion mode also makes subsequent rendering comparisons deterministic.
    app.motionEnabled = false;
    std::vector<int> fontSizes;
    for (const auto font : app.fonts) {
        LOGFONTW info{}; REQUIRE(GetObjectW(font, sizeof(info), &info));
        REQUIRE(abs(info.lfHeight) >= app.S(12));
        if (std::find(fontSizes.begin(), fontSizes.end(), info.lfHeight) == fontSizes.end()) fontSizes.push_back(info.lfHeight);
    }
    REQUIRE(fontSizes.size() == 3);
    REQUIRE(app.Checked(Startup) && app.Checked(Silent) && app.savedTarget.empty());
    REQUIRE(app.Text(Preview) == L"--no-startup-window");
    REQUIRE(app.Checked(CodexEnv) && !IsWindowVisible(app.Get(CodexEnv)) && !IsWindowVisible(app.Get(CodexEnvHelp)));
    SendMessageW(app.Get(Proxy), BM_CLICK, 0, 0);
    REQUIRE(IsWindowVisible(app.Get(CodexEnv)) && IsWindowVisible(app.Get(CodexEnvHelp)));
    REQUIRE(app.Options().codexEnv && app.tooltip);
    REQUIRE(SendMessageW(app.tooltip, TTM_GETTOOLCOUNT, 0, 0) == 4);
    wchar_t helpText[2048]{};
    TOOLINFOW help{TTTOOLINFOW_V2_SIZE}; help.hwnd = app.body;
    help.uId = reinterpret_cast<UINT_PTR>(app.Get(CodexEnvHelp)); help.lpszText = helpText;
    SendMessageW(app.tooltip, TTM_GETTEXTW, _countof(helpText), reinterpret_cast<LPARAM>(&help));
    REQUIRE(!wcscmp(helpText, CodexEnvHelpText));
    for (int id : {ProtocolHelp, HostHelp, PortHelp}) {
        help.uId = reinterpret_cast<UINT_PTR>(app.Get(id));
        SendMessageW(app.tooltip, TTM_GETTEXTW, _countof(helpText), reinterpret_cast<LPARAM>(&help));
        REQUIRE(!wcscmp(helpText, id == PortHelp ? PortHelpText : L"不要更改，除非你知道自己在做什么"));
    }
    RECT helpBounds{}; GetWindowRect(app.Get(CodexEnvHelp), &helpBounds);
    MapWindowPoints(nullptr, app.body, reinterpret_cast<POINT*>(&helpBounds), 2);
    REQUIRE(ChildWindowFromPointEx(app.body, {(helpBounds.left + helpBounds.right) / 2, (helpBounds.top + helpBounds.bottom) / 2},
        CWP_SKIPINVISIBLE | CWP_SKIPDISABLED) == app.Get(CodexEnvHelp));
    app.Set(Port, L"12345");
    REQUIRE(app.Text(Footer).find(L"请先保存工作") != std::wstring::npos);
    const auto proxyArguments = app.Text(Preview);
    SendMessageW(app.Get(CodexEnv), BM_CLICK, 0, 0);
    REQUIRE(!app.Options().codexEnv && app.Text(Preview) == proxyArguments);
    SendMessageW(app.Get(CodexEnv), BM_CLICK, 0, 0);
    SendMessageW(app.Get(Proxy), BM_CLICK, 0, 0);
    REQUIRE(!IsWindowVisible(app.Get(CodexEnv)) && app.Checked(CodexEnv));
    app.Set(Port, L"");
    REQUIRE(!app.Checked(Desktop) && IsWindowEnabled(app.Get(Desktop)));
    SendMessageW(app.Get(Desktop), BM_CLICK, 0, 0);
    REQUIRE(app.Options().desktop && app.Text(Preview) == L"--no-startup-window");
    SendMessageW(app.Get(Desktop), BM_CLICK, 0, 0);
    REQUIRE(!IsWindowEnabled(app.Get(Silent)));
    REQUIRE(app.Text(Silent).find(L"暂不可用") != std::wstring::npos);
    REQUIRE(!(GetWindowLongPtrW(app.Get(Directory), GWL_STYLE) & WS_VISIBLE));
    REQUIRE(GetParent(app.Get(Advanced)) == app.window);
    REQUIRE(FooterAligned(app));
    setup::Loaded initial; initial.options = app.Options();
    SendMessageW(app.Get(Proxy), BM_CLICK, 0, 0);
    proxyResult = {L"12345", L"已填入测试代理端口"};
    app.Command(Port, EN_SETFOCUS);
    REQUIRE(app.detectingPort);
    app.FinishPortDetection();
    REQUIRE(app.Text(Port) == L"12345" && proxyCalls == 1);
    REQUIRE(app.Text(Preview).find(L"http://127.0.0.1:12345") != std::wstring::npos);
    app.Command(Port, EN_SETFOCUS); // Existing values never get overwritten.
    REQUIRE(!app.detectingPort && proxyCalls == 1);
    app.Set(Port, L"");
    app.Command(Port, EN_SETFOCUS);
    app.Set(Port, L"23456"); // Typing while discovery runs wins.
    app.FinishPortDetection();
    REQUIRE(app.Text(Port) == L"23456");
    app.Set(Port, L"");
    app.Command(Port, EN_SETFOCUS);
    SendMessageW(app.Get(ProtocolSocks), BM_CLICK, 0, 0);
    app.FinishPortDetection();
    REQUIRE(app.Text(Port).empty()); // A result for the old protocol is stale.
    app.Command(Port, EN_SETFOCUS);
    app.Set(Host, L"proxy.example.com");
    app.FinishPortDetection();
    REQUIRE(app.Text(Port).empty());
    app.Set(Host, L"127.0.0.1");
    app.Command(Port, EN_SETFOCUS);
    app.busy = true;
    app.FinishPortDetection();
    REQUIRE(app.Text(Port).empty());
    app.busy = false;
    proxyResult = {};
    app.Command(Port, EN_SETFOCUS); app.FinishPortDetection();
    REQUIRE(app.Text(Port).empty() && app.Text(Footer).find(L"端口设置") != std::wstring::npos);
    REQUIRE(reminderCalls == 0); // No modal prompt that interrupts typing.
    proxyResult = {L"12345", L"已填入测试代理端口"};
    SetFocus(app.Get(Port)); // Keyboard focus and a repeated click both support discovery.
    if (app.detectingPort) app.FinishPortDetection();
    app.Set(Port, L"");
    SendMessageW(app.Get(Port), WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(5, 5));
    SendMessageW(app.Get(Port), WM_LBUTTONUP, 0, MAKELPARAM(5, 5));
    MSG detectMessage{};
    while (PeekMessageW(&detectMessage, app.window, DetectPortMessage, DetectPortMessage, PM_REMOVE)) DispatchMessageW(&detectMessage);
    REQUIRE(app.detectingPort);
    app.FinishPortDetection();
    REQUIRE(app.Text(Port) == L"12345");
    // Drain completion messages before running unrelated UI regression checks.
    while (PeekMessageW(&detectMessage, app.window, PortDetectedMessage, PortDetectedMessage, PM_REMOVE)) DispatchMessageW(&detectMessage);
    SetFocus(app.body);
    app.RestoreOptions(initial);
    puts("PASS async proxy fill, preview, manual input preservation, protocol/host/save invalidation, fallback and click retry (mock discovery)");
    unsigned ownerDraws = 0;
    REQUIRE(SetWindowSubclass(app.window, ProbeOwnerDraw, 3, reinterpret_cast<DWORD_PTR>(&ownerDraws)));
    REQUIRE(SetWindowSubclass(app.body, ProbeOwnerDraw, 3, reinterpret_cast<DWORD_PTR>(&ownerDraws)));
    for (const auto& control : app.controls) if (IsActionButton(control.id))
        REQUIRE((GetWindowLongPtrW(control.window, GWL_STYLE) & BS_TYPEMASK) == BS_OWNERDRAW);
    // Native BUTTON can draw directly during WM_SETTEXT, bypassing our WM_PAINT.
    // A changed caption must still schedule (or finish) the styled repaint.
    ButtonPaintProbe probe;
    const HWND apply = app.Get(ApplyButton);
    REQUIRE(SetWindowSubclass(apply, ProbeButton, 2, reinterpret_cast<DWORD_PTR>(&probe)));
    ValidateRect(apply, nullptr);
    unsigned previous = probe.paints;
    SetWindowTextW(apply, L"正在保存…");
    REQUIRE(HasScheduledPaint(apply, probe, previous));
    UpdateWindow(apply);
    REQUIRE(probe.paints > previous);
    app.Refresh(false);
    REQUIRE(app.Text(ApplyButton) == L"部署并保存" && ButtonFillMatches(app));
    const unsigned captions = probe.captions;
    // Rapid edits used to send WM_SETTEXT even when the button caption was unchanged.
    for (int i = 0; i < 40; ++i) {
        SendMessageW(app.Get(Startup), BM_CLICK, 0, 0);
        SendMessageW(app.Get(Proxy), BM_CLICK, 0, 0); // Empty port makes Save unavailable.
        REQUIRE(!IsWindowEnabled(apply) && ButtonFillMatches(app));
        SendMessageW(app.Get(Proxy), BM_CLICK, 0, 0);
        REQUIRE(IsWindowEnabled(apply) && ButtonFillMatches(app));
        REQUIRE(!IsWindowEnabled(app.Get(Silent)) && app.Checked(Silent));
        UpdateWindow(apply);
    }
    REQUIRE(probe.captions == captions);
    for (int mode : {0, 1, 2}) {
        app.Theme(mode);
        app.motionEnabled = false;
        const struct { UINT message; WPARAM wparam; LPARAM lparam; } changes[] = {
            {WM_SETTEXT, 0, reinterpret_cast<LPARAM>(L"部署并保存")},
            {BM_SETSTYLE, BS_DEFPUSHBUTTON, TRUE}, {BM_SETSTYLE, BS_PUSHBUTTON, TRUE},
            {WM_SETFONT, reinterpret_cast<WPARAM>(app.fonts[1]), TRUE},
            {WM_UPDATEUISTATE, MAKEWPARAM(UIS_CLEAR, UISF_HIDEFOCUS), 0},
            {BM_SETSTATE, TRUE, 0}, {BM_SETSTATE, FALSE, 0}
        };
        for (const auto& change : changes) {
            ValidateRect(apply, nullptr); previous = probe.paints;
            SendMessageW(apply, change.message, change.wparam, change.lparam);
            REQUIRE((GetWindowLongPtrW(apply, GWL_STYLE) & BS_TYPEMASK) == BS_OWNERDRAW);
            REQUIRE(HasScheduledPaint(apply, probe, previous));
            UpdateWindow(apply);
            REQUIRE(probe.paints > previous && ButtonFillMatches(app));
        }
        for (BOOL enabled : {FALSE, TRUE}) {
            ValidateRect(apply, nullptr); previous = probe.paints;
            EnableWindow(apply, enabled);
            REQUIRE(HasScheduledPaint(apply, probe, previous));
            UpdateWindow(apply);
            REQUIRE(probe.paints > previous && ButtonFillMatches(app));
        }
    }
    REQUIRE(ownerDraws > 0); // Synchronous native button updates now ask our renderer to draw.
    app.Check(Proxy, true); app.Set(Port, L"12345"); app.Refresh(false);
    for (int mode : {0, 1, 2}) {
        app.Theme(mode); app.motionEnabled = false;
        if (mode == 2) REQUIRE(!IsWindowEnabled(app.Get(ThemeToggle)));
        for (int i = 0; i < 12; ++i) {
            const int selected = Protocol + i % 3;
            SendMessageW(app.Get(selected), BM_CLICK, 0, 0);
            for (int id : {Protocol, ProtocolHttps, ProtocolSocks}) {
                REQUIRE(app.Checked(id) == (id == selected));
                REQUIRE((GetWindowLongPtrW(app.Get(id), GWL_STYLE) & BS_TYPEMASK) == BS_OWNERDRAW);
                REQUIRE(SegmentFillMatches(app, id));
            }
        }
        SetFocus(app.Get(ProtocolSocks));
        SendMessageW(app.Get(ProtocolSocks), WM_KEYDOWN, VK_RIGHT, 0);
        REQUIRE(app.Checked(Protocol) && GetFocus() == app.Get(Protocol));
        SendMessageW(app.Get(Protocol), WM_KEYDOWN, VK_LEFT, 0);
        REQUIRE(app.Checked(ProtocolSocks));
        SetFocus(app.Get(ProtocolHttps));
        SendMessageW(app.Get(ProtocolHttps), WM_KEYDOWN, VK_SPACE, 0);
        SendMessageW(app.Get(ProtocolHttps), WM_KEYUP, VK_SPACE, 0);
        REQUIRE(app.Checked(ProtocolHttps));
        for (int id : {Protocol, ProtocolHttps, ProtocolSocks}) {
            REQUIRE(((GetWindowLongPtrW(app.Get(id), GWL_STYLE) & WS_TABSTOP) != 0) == (id == ProtocolHttps));
            SendMessageW(app.Get(id), BM_SETSTYLE, BS_AUTORADIOBUTTON, TRUE);
            REQUIRE((GetWindowLongPtrW(app.Get(id), GWL_STYLE) & BS_TYPEMASK) == BS_OWNERDRAW);
        }
        SetFocus(app.body);
        REQUIRE(PartialSurfaceMatches(app, app.body));
        REQUIRE(Capture(app, setup::Join(folder, mode == 0 ? L"setup-protocol-light.png"
            : mode == 1 ? L"setup-protocol-dark.png" : L"setup-protocol-contrast.png").c_str()));
    }
    app.RestoreOptions(initial);
    app.Theme(0); app.motionEnabled = false;
    // Exercise dialog navigation, not just BM_CLICK: Enter and Space retain button semantics.
    SetFocus(app.Get(Advanced));
    MSG key{}; key.hwnd = app.Get(Advanced); key.message = WM_KEYDOWN; key.wParam = VK_RETURN;
    const BOOL entered = IsDialogMessageW(app.window, &key);
    REQUIRE(entered && app.advanced);
    SendMessageW(app.Get(Advanced), WM_KEYDOWN, VK_SPACE, 0);
    SendMessageW(app.Get(Advanced), WM_KEYUP, VK_SPACE, 0);
    REQUIRE(!app.advanced);
    for (int i = 0; i < 18; ++i) {
        key.hwnd = GetFocus(); key.wParam = VK_TAB;
        REQUIRE(IsDialogMessageW(app.window, &key));
        REQUIRE(GetFocus() != app.Get(Silent));
        for (const auto& control : app.controls) if (IsActionButton(control.id))
            REQUIRE((GetWindowLongPtrW(control.window, GWL_STYLE) & BS_TYPEMASK) == BS_OWNERDRAW);
    }
    SetFocus(app.body);
    REQUIRE(Capture(app, setup::Join(folder, L"setup-main.png").c_str()));
    app.Theme(1); app.motionEnabled = false;
    REQUIRE(Capture(app, setup::Join(folder, L"setup-dark.png").c_str()));
    app.Theme(2);
    REQUIRE(!app.motionEnabled && Capture(app, setup::Join(folder, L"setup-contrast.png").c_str()));
    app.Theme(0); app.motionEnabled = false;
    // Native hover/toggle events start only local, short-lived timers; idle work stops.
    app.motionEnabled = true;
    SendMessageW(app.Get(Advanced), WM_MOUSEMOVE, 0, MAKELPARAM(5, 5));
    REQUIRE(app.TickMotion());
    SendMessageW(app.Get(Advanced), WM_MOUSELEAVE, 0, 0);
    SendMessageW(app.Get(Desktop), BM_CLICK, 0, 0);
    REQUIRE(app.TickMotion());
    for (auto& control : app.controls) { control.hover.started -= 150; control.toggle.started -= 150; }
    REQUIRE(!app.TickMotion());
    app.motionEnabled = false;
    SendMessageW(app.Get(Desktop), BM_CLICK, 0, 0);
    REQUIRE(!app.TickMotion());
    SendMessageW(app.Get(Proxy), BM_CLICK, 0, 0);
    app.Set(Port, L"7897");
    SendMessageW(app.Get(ProtocolSocks), BM_CLICK, 0, 0);
    REQUIRE(app.Text(Preview).find(L"socks5://127.0.0.1:7897") != std::wstring::npos);
    app.Command(Advanced, BN_CLICKED);
    REQUIRE(app.advanced && app.Text(Advanced) == L"返回");
    REQUIRE(IsWindowVisible(app.Get(AddProgram)) && IsWindowVisible(app.Get(OpenCompanions)));
    const auto previewBeforeFolder = app.Text(Preview);
    const auto revisionBeforeFolder = app.editRevision;
    SendMessageW(app.Get(OpenCompanions), BM_CLICK, 0, 0);
    REQUIRE(folderCalls == 1 && lastFolder == setup::Join(root, L"StartupApps"));
    REQUIRE(GetFileAttributesW(lastFolder.c_str()) != INVALID_FILE_ATTRIBUTES);
    REQUIRE(app.Text(Preview) == previewBeforeFolder && app.editRevision == revisionBeforeFolder);
    folderCalls = 0; // Subsequent counts cover ownership-error handoffs only.
    REQUIRE((GetWindowLongPtrW(app.Get(Directory), GWL_STYLE) & WS_VISIBLE) != 0);
    REQUIRE(!(GetWindowLongPtrW(app.Get(Startup), GWL_STYLE) & WS_VISIBLE));
    REQUIRE(!(GetWindowLongPtrW(app.Get(Desktop), GWL_STYLE) & WS_VISIBLE));
    app.Set(Directory, L"D:\\Applications\\ChatGPT Launcher");
    app.Set(Extra, L"--label=\"中文 空格\"");
    REQUIRE(app.Options().directory == L"D:\\Applications\\ChatGPT Launcher");
    REQUIRE(app.Text(Preview).find(L"--label=\"中文 空格\"") != std::wstring::npos);
    REQUIRE(Capture(app, setup::Join(folder, L"setup-advanced.png").c_str()));
    const UINT originalDpi = app.dpi;
    app.dpi = 144; app.Theme(1);
    SetWindowPos(app.window, nullptr, 0, 0, 1080, 1218, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    app.Layout();
    REQUIRE(FooterAligned(app));
    REQUIRE(PartialSurfaceMatches(app, app.body));
    REQUIRE(Capture(app, setup::Join(folder, L"setup-dark-150.png").c_str()));
    for (UINT scale : {96u, 120u, 144u, 192u}) {
        app.dpi = scale; app.Theme(0); app.motionEnabled = false;
        SetWindowPos(app.window, nullptr, 0, 0, app.S(620), app.S(812), SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        app.Layout();
        REQUIRE(LabelsFit(app) && FooterAligned(app));
        app.Command(Advanced, BN_CLICKED);
        REQUIRE(!app.advanced && LabelsFit(app));
        if (scale == 192) REQUIRE(Capture(app, setup::Join(folder, L"setup-narrow-200.png").c_str()));
        const bool proxyEnabled = app.Checked(Proxy);
        app.Check(Proxy, true); app.Refresh(false);
        REQUIRE(LabelsFit(app));
        REQUIRE(PartialSurfaceMatches(app, app.body));
        RECT row{}, question{};
        GetWindowRect(app.Get(CodexEnv), &row); GetWindowRect(app.Get(CodexEnvHelp), &question);
        HDC measure = GetDC(app.Get(CodexEnv)); HGDIOBJ previousFont = SelectObject(measure, app.fonts[0]); SIZE label{};
        const auto caption = app.Text(CodexEnv);
        GetTextExtentPoint32W(measure, caption.c_str(), static_cast<int>(caption.size()), &label);
        SelectObject(measure, previousFont); ReleaseDC(app.Get(CodexEnv), measure);
        REQUIRE(question.left >= row.left + label.cx && question.right <= row.right - app.S(62));
        REQUIRE(abs((row.top + row.bottom) - (question.top + question.bottom)) <= 1);
        if (scale == 192) REQUIRE(Capture(app, setup::Join(folder, L"setup-env-narrow-200.png").c_str()));
        app.Check(Proxy, proxyEnabled); app.Refresh(false);
        app.Command(Advanced, BN_CLICKED);
    }
    app.dpi = originalDpi; app.Theme(0); app.motionEnabled = false;
    SetWindowPos(app.window, nullptr, 0, 0, 720, 812, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    app.Command(Advanced, BN_CLICKED);
    REQUIRE(!app.advanced && app.Options().directory == L"D:\\Applications\\ChatGPT Launcher");
    REQUIRE(!IsWindowVisible(app.Get(AddProgram)) && !IsWindowVisible(app.Get(OpenCompanions)));
    REQUIRE(app.Text(Preview).find(L"--label=\"中文 空格\"") != std::wstring::npos);
    // Reproduce short-window scrolling and verify movement without rebuilding the layout.
    SetWindowPos(app.window, nullptr, 0, 0, 720, 510, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    app.Layout();
    REQUIRE(FooterAligned(app));
    const unsigned layouts = app.layoutPasses;
    RECT before{}, after{}; GetWindowRect(app.Get(Preview), &before);
    const auto started = std::chrono::steady_clock::now();
    for (int i = 0; i < 300; ++i) app.ScrollTo(i % 2 ? 0 : 160);
    app.ScrollTo(160); GetWindowRect(app.Get(Preview), &after);
    REQUIRE(app.layoutPasses == layouts && before.top - after.top == 160);
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count();
    REQUIRE(PartialSurfaceMatches(app, app.body));
    REQUIRE(Capture(app, setup::Join(folder, L"setup-scroll.png").c_str()));
    const std::wstring savedFooter = app.Text(Footer);
    app.Set(Footer, L"当前部署目录无法写入。请在高级设置中选择另一个目录，原文件不会被覆盖。");
    REQUIRE(Capture(app, setup::Join(folder, L"setup-footer-message.png").c_str()));
    app.Set(Footer, savedFooter);
    // Existing saved preferences override first-run defaults.
    setup::Loaded saved; saved.hasSettings = true; saved.options.directory = root;
    saved.options.startup = false; saved.options.silent = false;
    app.RestoreOptions(saved);
    REQUIRE(!app.Checked(Startup) && !app.Checked(Silent) && app.Text(Preview).empty());
    REQUIRE(app.Text(State).find(L"启动器缺失") != std::wstring::npos);
    saved.deployed = true; saved.deployedVersion = L"2.3.4.5";
    app.RestoreOptions(saved);
    REQUIRE(app.Text(State).find(L"已部署 v2.3.4.5") == 0);
    app.Set(Directory, setup::Join(root, L"not-saved")); app.Refresh(true);
    REQUIRE(app.Text(State).find(L"已部署 v2.3.4.5") == 0); // Editing does not pretend the new directory is deployed.
    saved.deployedVersion.clear(); app.RestoreOptions(saved);
    REQUIRE(app.Text(State).find(L"版本无法识别") != std::wstring::npos);
    saved.deployed = false; app.RestoreOptions(saved);
    REQUIRE(!IsWindowEnabled(app.Get(Silent)));
    REQUIRE(!disableCalls && !settingsCalls && !reminderCalls); // UI edits alone do not change the OS.
    REQUIRE(!codexCloseCalls);
    // Exercise the actual worker and file commit; only registry/Settings/reminder endpoints are replaced.
    HANDLE payloadFile = CreateFileW(setup::Join(folder, L"..\\outputs\\ChatGPTProxyLauncher.exe").c_str(), GENERIC_READ,
        FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    REQUIRE(payloadFile != INVALID_HANDLE_VALUE);
    std::vector<BYTE> payload(GetFileSize(payloadFile, nullptr)); DWORD read = 0;
    REQUIRE(ReadFile(payloadFile, payload.data(), static_cast<DWORD>(payload.size()), &read, nullptr));
    CloseHandle(payloadFile); REQUIRE(read == payload.size());
    app.payload = {payload.data(), payload.size()};
    app.Check(Startup, true);
    app.Check(Proxy, true); app.Set(Port, L"12345"); app.Refresh(true);
    SendMessageW(app.Get(Desktop), BM_CLICK, 0, 0);
    app.Command(ApplyButton, BN_CLICKED); REQUIRE(WaitForSave(app));
    REQUIRE(app.Checked(Desktop) && IsWindowEnabled(app.Get(Desktop)));
    REQUIRE(GetFileAttributesW(app.paths.codexEnvFile.c_str()) != INVALID_FILE_ATTRIBUTES);
    REQUIRE(storedCodexEnv == 1 && app.Checked(CodexEnv));
    REQUIRE(codexCloseCalls == 1 && envSavedBeforeClose && app.Text(Footer).find(L"已退出 ChatGPT") != std::wstring::npos);
    setup::Loaded versionState; REQUIRE(setup::Load(app.paths, versionState) && !versionState.deployedVersion.empty());
    REQUIRE(app.Text(State).find(L"已部署 v" + versionState.deployedVersion) == 0);
    app.ScrollTo(0);
    REQUIRE(Capture(app, setup::Join(folder, L"setup-version.png").c_str()));
    REQUIRE(lastDialog.find(L"重新打开 ChatGPT") != std::wstring::npos && app.Text(Footer).find(L"重新打开 ChatGPT") != std::wstring::npos);
    setup::Link desktopLink;
    REQUIRE(setup::ReadLink(app.paths.desktopLink, desktopLink) && desktopLink.target == app.savedTarget);
    REQUIRE(disableCalls == 1 && settingsCalls == 1 && reminderCalls == 1);
    REQUIRE(settingsAfterReminder && !app.busy && !app.dirty && app.savedThisSession);
    REQUIRE(!IsWindowEnabled(app.Get(Silent)) && !app.Checked(Silent));
    REQUIRE(!IsWindowEnabled(app.Get(ApplyButton)) && app.Text(ApplyButton) == L"已保存");
    REQUIRE(IsWindowEnabled(app.Get(CloseButton)) && app.Text(CloseButton) == L"完成");
    app.Command(ApplyButton, BN_CLICKED);
    REQUIRE(!app.busy && disableCalls == 1 && settingsCalls == 1 && reminderCalls == 1);
    // Manual startup review is advisory, and edits after it do not repeat the reminder.
    app.Set(Extra, L"--test-option"); app.Refresh(true);
    SendMessageW(app.Get(CodexEnv), BM_CLICK, 0, 0);
    REQUIRE(IsWindowEnabled(app.Get(ApplyButton)));
    app.Command(ApplyButton, BN_CLICKED); REQUIRE(WaitForSave(app));
    REQUIRE(disableCalls == 1 && settingsCalls == 1 && reminderCalls == 1 && !app.dirty);
    REQUIRE(!app.Checked(CodexEnv) && storedCodexEnv == 0);
    REQUIRE(codexCloseCalls == 1);
    // A preference failure must not undo the completed deployment or flip the visible choice.
    envWriteResult = ERROR_ACCESS_DENIED;
    SendMessageW(app.Get(CodexEnv), BM_CLICK, 0, 0);
    app.Command(ApplyButton, BN_CLICKED); REQUIRE(WaitForSave(app));
    REQUIRE(app.Checked(CodexEnv) && storedCodexEnv == 0 && !app.dirty);
    REQUIRE(app.Text(Footer).find(L"未能记住") != std::wstring::npos);
    envWriteResult = ERROR_SUCCESS;
    REQUIRE(setup::ReadLink(app.paths.desktopLink, desktopLink) && desktopLink.arguments == app.Text(Preview));
    SendMessageW(app.Get(Desktop), BM_CLICK, 0, 0);
    setup::Loaded deployed; REQUIRE(setup::Load(app.paths, deployed) && deployed.deployed && deployed.options.startup);
    app.Check(Startup, false); app.Refresh(true);
    closeResult = setup::ChatGPTCloseResult::NeedsManualExit;
    app.Command(ApplyButton, BN_CLICKED); REQUIRE(WaitForSave(app));
    REQUIRE(app.Text(Footer).find(L"请手动退出") != std::wstring::npos && !app.dirty);
    REQUIRE(disableCalls == 1 && settingsCalls == 1 && reminderCalls == 1);
    REQUIRE(!app.Checked(Desktop) && GetFileAttributesW(app.paths.desktopLink.c_str()) == INVALID_FILE_ATTRIBUTES);
    app.Check(Startup, true); app.Refresh(true); app.payload = {};
    const unsigned closeBeforeFailure = codexCloseCalls;
    app.Command(ApplyButton, BN_CLICKED); REQUIRE(WaitForSave(app));
    REQUIRE(codexCloseCalls == closeBeforeFailure);
    REQUIRE(disableCalls == 1 && settingsCalls == 1 && reminderCalls == 2);
    REQUIRE(!IsWindowEnabled(app.Get(Silent)) && !app.Checked(Silent));
    REQUIRE(folderCalls == 0); // Neither success nor unrelated errors open Explorer.
    // Actual EXE ownership failure propagates its directory through the worker to the UI.
    const std::wstring conflictDirectory = setup::Join(root, L"冲突 目录");
    REQUIRE(CreateDirectoryW(conflictDirectory.c_str(), nullptr));
    HANDLE foreign = CreateFileW(setup::Join(conflictDirectory, setup::LauncherName).c_str(), GENERIC_WRITE,
        0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    REQUIRE(foreign != INVALID_HANDLE_VALUE);
    DWORD written = 0;
    const BOOL wrote = WriteFile(foreign, "test", 4, &written, nullptr); CloseHandle(foreign);
    REQUIRE(wrote && written == 4);
    app.payload = {payload.data(), payload.size()}; app.Set(Directory, conflictDirectory);
    app.Command(ApplyButton, BN_CLICKED); REQUIRE(WaitForSave(app));
    REQUIRE(folderCalls == 1 && lastFolder == conflictDirectory && folderRequestValid);
    REQUIRE(disableCalls == 1 && settingsCalls == 1);
    // Shortcut location and Explorer failure are passed through without parsing error text.
    auto failure = std::make_unique<Completion>();
    const std::wstring shortcutDirectory = setup::Join(root, L"启动 文件夹");
    failure->result = {ERROR_ALREADY_EXISTS, L"Shortcut conflict", shortcutDirectory};
    openFolderSucceeds = false;
    const unsigned dialogsBefore = reminderCalls;
    SendMessageW(app.window, SavedMessage, 0, reinterpret_cast<LPARAM>(failure.release()));
    REQUIRE(folderCalls == 2 && lastFolder == shortcutDirectory && folderRequestValid);
    REQUIRE(reminderCalls == dialogsBefore + 2 && lastDialog.find(shortcutDirectory) != std::wstring::npos);
    REQUIRE(lastDialog.find(L"无法打开文件夹") != std::wstring::npos);
    // Even an ERROR_ALREADY_EXISTS without an ownership location must not open a folder.
    ShowSetupError(app.window, {ERROR_ALREADY_EXISTS, L"Other error"}, L"Test");
    REQUIRE(folderCalls == 2);
    // A Settings launch failure must never trap an already successful deployment.
    auto savedWithSettingsFailure = std::make_unique<Completion>();
    savedWithSettingsFailure->checkStartup = true;
    settingsError = ERROR_ACCESS_DENIED;
    SendMessageW(app.window, SavedMessage, 0, reinterpret_cast<LPARAM>(savedWithSettingsFailure.release()));
    REQUIRE(!app.busy && !app.dirty && IsWindowEnabled(app.Get(CloseButton)));
    REQUIRE(app.Text(CloseButton) == L"完成" && app.Text(ApplyButton) == L"已保存");
    REQUIRE(lastDialog.find(L"配置已保存") != std::wstring::npos);
    const unsigned finalReminders = reminderCalls, finalSettings = settingsCalls;
    app.Command(ApplyButton, BN_CLICKED);
    REQUIRE(!app.busy && reminderCalls == finalReminders && settingsCalls == finalSettings);
    app.Command(CloseButton, BN_CLICKED);
    REQUIRE(!IsWindow(app.window) && reminderCalls == finalReminders);
    printf("PASS UI defaults, button repaint after captions/styles/state and 120 toggles, live preview, secondary page, retained fields, 301 scrolls without relayout (%lld ms), saved-state restore and Settings handoff (mocked)\n", milliseconds);
    puts("PASS dirty-region buffers and pixel-identical partial/full surface painting across themes, scaling and scrolling");
    puts("PASS unavailable silent switch stays disabled across edits, restore and save success/failure; Tab skips it and saved values are retained");
    puts("PASS three font sizes, interruptible 150 ms motion, stopped idle timer, reduced motion and high contrast");
    puts("PASS dark default, independent theme preference and write failure (mock registry); owner-drawn protocol selection, arrows, Space, Tab stops and rendering");
    puts("PASS Codex env defaults, expansion, help hit target/text, 100-200 percent layout, isolated save, preference persistence/failure and restart reminder");
    puts("PASS deployed version display, unknown/missing state, unsaved path changes and refresh after actual isolated deployment");
    puts("PASS configurator version, protocol/address tooltips and close request only after successful env save, opt-out and failed-save exclusion (mock close)");
    printf("PASS conflict folder handoff, Unicode/spaces, unrelated errors and Explorer failure fallback (mocked)\n");
    printf("PASS saved state, one startup reminder per session, repeat-save no-op, later edits, Settings-after-reminder and failure recovery\n");
    printf("PASS owner-draw synchronous dispatch, preserved style through Tab/default-button changes, Enter/Space, balanced footer at 100/150 percent and short windows\n");
    return 0;
}
int wmain(int argc, wchar_t** argv) {
    if (argc != 2) return 2;
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    Gdiplus::GdiplusStartupInput input; ULONG_PTR token = 0;
    if (Gdiplus::GdiplusStartup(&token, &input, nullptr) != Gdiplus::Ok) return 3;
    BufferedPaintInit();
    const int result = RunTests(argv[1]);
    BufferedPaintUnInit(); Gdiplus::GdiplusShutdown(token); CoUninitialize();
    return result;
}
