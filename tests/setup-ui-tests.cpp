// SPDX-License-Identifier: GPL-3.0-only
#include "../src/StartupIntegration.h"
#include "../src/ProxyDetection.h"
#include <stdio.h>
#include <chrono>
#include <shellapi.h>
static unsigned disableCalls = 0, settingsCalls = 0, reminderCalls = 0;
static unsigned folderCalls = 0;
static bool openFolderSucceeds = true, folderRequestValid = false;
static std::wstring lastDialog, lastFolder;
static bool settingsAfterReminder = false;
static DWORD settingsError = 0;
static setup::ProxyDetection proxyResult;
static unsigned proxyCalls = 0;
namespace setup {
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
#define wWinMain SetupEntryForTest
#include "../src/ChatGPTLauncherSetup.cpp"
#undef wWinMain
#undef MessageBoxW
#undef ShellExecuteExW

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
    const COLORREF expected = !IsWindowEnabled(button) ? app.FieldColor() : pressed && !app.contrast
        ? app.dark ? RGB(59,126,234) : RGB(0,82,190) : app.colors.accent;
    SelectObject(dc, old); DeleteObject(bitmap); DeleteDC(dc); ReleaseDC(button, screen);
    return actual == expected;
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
    App app;
    app.instance = GetModuleHandleW(nullptr);
    const std::wstring root = setup::Join(folder, L"ui-isolated-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64()));
    app.paths = {root, setup::Join(root, L"startup.lnk"), setup::Join(root, L"settings.lnk")};
    WNDCLASSEXW cls{}; cls.cbSize = sizeof(cls); cls.hInstance = app.instance;
    cls.lpfnWndProc = WindowProc; cls.lpszClassName = L"SetupTestWindow"; RegisterClassExW(&cls);
    cls.lpfnWndProc = BodyProc; cls.lpszClassName = L"ChatGPTSetupBody"; RegisterClassExW(&cls);
    REQUIRE(CreateWindowExW(WS_EX_CONTROLPARENT, L"SetupTestWindow", L"Isolated UI test", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        -20000, -20000, 720, 840, nullptr, nullptr, app.instance, &app));
    ShowWindow(app.window, SW_SHOWNOACTIVATE);
    UpdateWindow(app.window);
    REQUIRE(app.Checked(Startup) && app.Checked(Silent) && app.savedTarget.empty());
    REQUIRE(app.Text(Preview) == L"--no-startup-window");
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
        SendMessageW(app.Get(Silent), BM_CLICK, 0, 0);
        SendMessageW(app.Get(Startup), BM_CLICK, 0, 0);
        SendMessageW(app.Get(Proxy), BM_CLICK, 0, 0); // Empty port makes Save unavailable.
        REQUIRE(!IsWindowEnabled(apply) && ButtonFillMatches(app));
        SendMessageW(app.Get(Proxy), BM_CLICK, 0, 0);
        REQUIRE(IsWindowEnabled(apply) && ButtonFillMatches(app));
        UpdateWindow(apply);
    }
    REQUIRE(probe.captions == captions);
    for (int mode : {0, 1, 2}) {
        app.Theme(mode);
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
    app.Theme();
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
        for (const auto& control : app.controls) if (IsActionButton(control.id))
            REQUIRE((GetWindowLongPtrW(control.window, GWL_STYLE) & BS_TYPEMASK) == BS_OWNERDRAW);
    }
    SetFocus(app.body);
    REQUIRE(Capture(app, setup::Join(folder, L"setup-main.png").c_str()));
    SendMessageW(app.Get(Proxy), BM_CLICK, 0, 0);
    app.Set(Port, L"7897");
    SendMessageW(app.Get(ProtocolSocks), BM_CLICK, 0, 0);
    REQUIRE(app.Text(Preview).find(L"socks5://127.0.0.1:7897") != std::wstring::npos);
    app.Command(Advanced, BN_CLICKED);
    REQUIRE(app.advanced && app.Text(Advanced) == L"返回");
    REQUIRE((GetWindowLongPtrW(app.Get(Directory), GWL_STYLE) & WS_VISIBLE) != 0);
    REQUIRE(!(GetWindowLongPtrW(app.Get(Startup), GWL_STYLE) & WS_VISIBLE));
    app.Set(Directory, L"D:\\Applications\\ChatGPT Launcher");
    app.Set(Extra, L"--label=\"中文 空格\"");
    REQUIRE(app.Options().directory == L"D:\\Applications\\ChatGPT Launcher");
    REQUIRE(app.Text(Preview).find(L"--label=\"中文 空格\"") != std::wstring::npos);
    REQUIRE(Capture(app, setup::Join(folder, L"setup-advanced.png").c_str()));
    const UINT originalDpi = app.dpi;
    app.dpi = 144; app.Theme(1);
    SetWindowPos(app.window, nullptr, 0, 0, 1080, 1260, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    app.Layout();
    REQUIRE(FooterAligned(app));
    REQUIRE(Capture(app, setup::Join(folder, L"setup-dark-150.png").c_str()));
    app.dpi = originalDpi; app.Theme();
    SetWindowPos(app.window, nullptr, 0, 0, 720, 840, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    app.Command(Advanced, BN_CLICKED);
    REQUIRE(!app.advanced && app.Options().directory == L"D:\\Applications\\ChatGPT Launcher");
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
    REQUIRE(!disableCalls && !settingsCalls && !reminderCalls); // UI edits alone do not change the OS.
    // Exercise the actual worker and file commit; only registry/Settings/reminder endpoints are replaced.
    HANDLE payloadFile = CreateFileW(setup::Join(folder, L"..\\outputs\\ChatGPTProxyLauncher.exe").c_str(), GENERIC_READ,
        FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    REQUIRE(payloadFile != INVALID_HANDLE_VALUE);
    std::vector<BYTE> payload(GetFileSize(payloadFile, nullptr)); DWORD read = 0;
    REQUIRE(ReadFile(payloadFile, payload.data(), static_cast<DWORD>(payload.size()), &read, nullptr));
    CloseHandle(payloadFile); REQUIRE(read == payload.size());
    app.payload = {payload.data(), payload.size()};
    app.Check(Startup, true);
    app.Command(ApplyButton, BN_CLICKED); REQUIRE(WaitForSave(app));
    REQUIRE(disableCalls == 1 && settingsCalls == 1 && reminderCalls == 1);
    REQUIRE(settingsAfterReminder && !app.busy && !app.dirty && app.savedThisSession);
    REQUIRE(!IsWindowEnabled(app.Get(ApplyButton)) && app.Text(ApplyButton) == L"已保存");
    REQUIRE(IsWindowEnabled(app.Get(CloseButton)) && app.Text(CloseButton) == L"完成");
    app.Command(ApplyButton, BN_CLICKED);
    REQUIRE(!app.busy && disableCalls == 1 && settingsCalls == 1 && reminderCalls == 1);
    // Manual startup review is advisory, and edits after it do not repeat the reminder.
    app.Check(Silent, true); app.Refresh(true);
    REQUIRE(IsWindowEnabled(app.Get(ApplyButton)));
    app.Command(ApplyButton, BN_CLICKED); REQUIRE(WaitForSave(app));
    REQUIRE(disableCalls == 1 && settingsCalls == 1 && reminderCalls == 1 && !app.dirty);
    setup::Loaded deployed; REQUIRE(setup::Load(app.paths, deployed) && deployed.deployed && deployed.options.startup);
    app.Check(Startup, false); app.Refresh(true);
    app.Command(ApplyButton, BN_CLICKED); REQUIRE(WaitForSave(app));
    REQUIRE(disableCalls == 1 && settingsCalls == 1 && reminderCalls == 1);
    app.Check(Startup, true); app.Refresh(true); app.payload = {};
    app.Command(ApplyButton, BN_CLICKED); REQUIRE(WaitForSave(app));
    REQUIRE(disableCalls == 1 && settingsCalls == 1 && reminderCalls == 2);
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
    printf("PASS UI defaults, button repaint after captions/styles/state and 160 toggles, live preview, secondary page, retained fields, 301 scrolls without relayout (%lld ms), saved-state restore and Settings handoff (mocked)\n", milliseconds);
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
