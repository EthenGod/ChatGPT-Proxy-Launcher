// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <windows.h>
#include <objbase.h>
#include <oleauto.h>
#include <UIAutomation.h>
#include <wrl/client.h>
#include <algorithm>
#include <atomic>
#include <memory>
#include <thread>
#include <system_error>
#include <vector>
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

namespace setup::tray_quit {
using Microsoft::WRL::ComPtr;
enum class Stage { Searching, Invoking, Sent, Unavailable, Cancelled };
struct Attempt {
    DWORD pid;
    HANDLE process = nullptr;
    ULONGLONG deadline;
    std::atomic<Stage> stage{Stage::Searching};
    ~Attempt() { if (process) CloseHandle(process); }
    bool searching() const {
        return stage == Stage::Searching && GetTickCount64() < deadline && WaitForSingleObject(process, 0) == WAIT_TIMEOUT;
    }
};
inline std::atomic_bool busy{false};

inline HWND ShellTray() {
    HWND tray = FindWindowW(L"Shell_TrayWnd", nullptr);
    DWORD trayPid = 0, shellPid = 0;
    GetWindowThreadProcessId(tray, &trayPid);
    GetWindowThreadProcessId(GetShellWindow(), &shellPid);
    return shellPid && trayPid == shellPid ? tray : nullptr;
}
inline ComPtr<IUIAutomationElement> Unique(IUIAutomation* ui, IUIAutomationElement* parent, TreeScope scope,
    PROPERTYID property, PCWSTR value) {
    VARIANT v{}; v.vt = VT_BSTR; v.bstrVal = SysAllocString(value);
    if (!v.bstrVal) return {};
    ComPtr<IUIAutomationCondition> condition;
    HRESULT hr = ui->CreatePropertyCondition(property, v, &condition);
    VariantClear(&v);
    ComPtr<IUIAutomationElementArray> found;
    int count = 0;
    if (FAILED(hr) || FAILED(parent->FindAll(scope, condition.Get(), &found)) || !found
        || FAILED(found->get_Length(&count)) || count != 1) return {};
    ComPtr<IUIAutomationElement> result;
    found->GetElement(0, &result);
    return result;
}
inline bool TextEquals(IUIAutomationElement* element, PROPERTYID property, PCWSTR value) {
    VARIANT v{};
    const bool match = SUCCEEDED(element->GetCurrentPropertyValue(property, &v)) && v.vt == VT_BSTR
        && v.bstrVal && wcscmp(v.bstrVal, value) == 0;
    VariantClear(&v);
    return match;
}
struct Windows {
    DWORD pid;
    std::vector<HWND> visible;
};
inline BOOL CALLBACK Collect(HWND w, LPARAM param) {
    auto& state = *reinterpret_cast<Windows*>(param);
    DWORD pid = 0; GetWindowThreadProcessId(w, &pid);
    wchar_t name[64]{};
    if (pid == state.pid && IsWindowVisible(w) && GetClassNameW(w, name, 64) && !wcscmp(name, L"Chrome_WidgetWin_1")) {
        if (state.visible.size() >= 64) return FALSE;
        state.visible.push_back(w);
    }
    return TRUE;
}
inline void Search(Attempt& attempt) {
    ComPtr<IUIAutomation2> ui;
    if (FAILED(CoCreateInstance(CLSID_CUIAutomation8, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&ui)))
        || FAILED(ui->put_ConnectionTimeout(250)) || FAILED(ui->put_TransactionTimeout(250))) return;
    HWND tray = ShellTray();
    ComPtr<IUIAutomationElement> bar;
    if (!tray || FAILED(ui->ElementFromHandle(tray, &bar)) || !bar) return;
    // This name/ID/class tuple is the Windows 11 notification icon, not the task-list button.
    auto icon = Unique(ui.Get(), bar.Get(), TreeScope_Descendants, UIA_NamePropertyId, L" ChatGPT");
    ComPtr<IUIAutomationElement3> context;
    if (!icon || !TextEquals(icon.Get(), UIA_AutomationIdPropertyId, L"NotifyItemIcon")
        || !TextEquals(icon.Get(), UIA_ClassNamePropertyId, L"SystemTray.NormalButton") || FAILED(icon.As(&context))) return;
    Windows before{attempt.pid, {}};
    before.visible.reserve(64);
    if (!EnumWindows(Collect, reinterpret_cast<LPARAM>(&before)) || !attempt.searching()
        || FAILED(context->ShowContextMenu())) return;
    while (attempt.searching()) {
        Windows now{attempt.pid, {}};
        now.visible.reserve(64);
        if (!EnumWindows(Collect, reinterpret_cast<LPARAM>(&now))) return;
        for (HWND w : now.visible) {
            if (std::find(before.visible.begin(), before.visible.end(), w) != before.visible.end()) continue;
            ComPtr<IUIAutomationElement> popup;
            if (FAILED(ui->ElementFromHandle(w, &popup)) || !popup) continue;
            auto host = Unique(ui.Get(), popup.Get(), TreeScope_Children, UIA_ClassNamePropertyId, L"MenuHostRootView");
            if (!host) continue;
            auto menu = Unique(ui.Get(), host.Get(), TreeScope_Descendants, UIA_ClassNamePropertyId, L"SubmenuView");
            if (!menu) return;
            auto exit = Unique(ui.Get(), menu.Get(), TreeScope_Children, UIA_NamePropertyId, L"Exit");
            int pid = 0; CONTROLTYPEID type = 0; BOOL enabled = FALSE, offscreen = TRUE;
            ComPtr<IUIAutomationInvokePattern> invoke;
            DWORD owner = 0; GetWindowThreadProcessId(w, &owner);
            if (!exit || owner != attempt.pid || !IsWindowVisible(w)
                || FAILED(exit->get_CurrentProcessId(&pid)) || static_cast<DWORD>(pid) != attempt.pid
                || FAILED(exit->get_CurrentControlType(&type)) || type != UIA_MenuItemControlTypeId
                || !TextEquals(exit.Get(), UIA_ClassNamePropertyId, L"MenuItemView")
                || FAILED(exit->get_CurrentIsEnabled(&enabled)) || !enabled
                || FAILED(exit->get_CurrentIsOffscreen(&offscreen)) || offscreen
                || FAILED(exit->GetCurrentPatternAs(UIA_InvokePatternId, IID_PPV_ARGS(&invoke))) || !invoke
                || !attempt.searching()) return;
            Stage expected = Stage::Searching;
            if (!attempt.stage.compare_exchange_strong(expected, Stage::Invoking)) return;
            const HRESULT result = invoke->Invoke();
            if (SUCCEEDED(result)) attempt.stage = Stage::Sent;
            else if (result != UIA_E_TIMEOUT && result != RPC_E_TIMEOUT) attempt.stage = Stage::Unavailable;
            // A timed-out Invoke may have reached the app. Leave it in Invoking.
            return;
        }
        Sleep(25);
    }
}
inline bool Request(DWORD pid, HANDLE process, ULONGLONG exitDeadline, void (*search)(Attempt&) = Search) {
    auto attempt = std::make_shared<Attempt>();
    attempt->pid = pid;
    const auto now = GetTickCount64();
    attempt->deadline = (std::min)(exitDeadline, now + 1200);
    if (attempt->deadline <= now || !DuplicateHandle(GetCurrentProcess(), process, GetCurrentProcess(),
        &attempt->process, SYNCHRONIZE, FALSE, 0) || busy.exchange(true)) return false;
    try {
        std::thread worker([attempt, search] {
            const HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            if (SUCCEEDED(hr)) {
                try { search(*attempt); } catch (...) { /* Provider unavailable. */ }
                CoUninitialize();
            }
            busy = false;
        });
        const auto started = GetTickCount64();
        const DWORD wait = started < attempt->deadline ? static_cast<DWORD>(attempt->deadline - started) : 0;
        if (WaitForSingleObject(worker.native_handle(), wait) == WAIT_OBJECT_0) worker.join();
        else {
            Stage expected = Stage::Searching;
            attempt->stage.compare_exchange_strong(expected, Stage::Cancelled);
            worker.detach();
        }
    } catch (const std::system_error&) { busy = false; return false; }
    // Invoke may already be in progress when a provider stops responding. Observe process exit
    // instead of delivering a second quit command. A timed-out search cannot invoke later.
    const Stage stage = attempt->stage.load();
    return stage == Stage::Sent || stage == Stage::Invoking;
}
}
