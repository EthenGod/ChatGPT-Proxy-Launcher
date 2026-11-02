// SPDX-License-Identifier: GPL-3.0-only
#include "../src/StartupApps.cpp"
#include "../src/ChatGPTProxyLauncher.cpp"
#include "../src/SetupCore.h"
#include <chrono>
#include <cstdio>

// Exercise the production dispatch and CreateProcess functions in isolated folders.
void RepairTaskbarShortcut(PCWSTR) noexcept {}
#define REQUIRE(c) do { if (!(c)) { printf("FAIL companion line %d: %s\n", __LINE__, #c); return 1; } } while (0)
static unsigned attempts = 0, started = 0;
static DWORD CountStart(PCWSTR exe, PCWSTR args, PCWSTR cwd, int show) {
    ++attempts;
    DWORD result = StartApplication(exe, args, cwd, show);
    if (!result) ++started;
    return result;
}
static bool Exists(const std::wstring& path) { return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES; }
static bool Touch(PCWSTR path) {
    HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    CloseHandle(h); return true;
}
static bool WriteScript(const std::wstring& path, const char* contents) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0, size = static_cast<DWORD>(strlen(contents));
    const bool ok = WriteFile(h, contents, size, &written, nullptr) && size == written;
    CloseHandle(h); return ok;
}
static bool MakeShortcut(const std::wstring& path, const std::wstring& target, PCWSTR args, const std::wstring& cwd) {
    companions::ComPtr<IShellLinkW> shell; companions::ComPtr<IPersistFile> persist;
    PIDLIST_ABSOLUTE item = ILCreateFromPathW(target.c_str());
    if (!item) return false;
    const bool bound = SUCCEEDED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&shell.p)))
        && SUCCEEDED(shell.p->SetIDList(item));
    CoTaskMemFree(item);
    return bound && SUCCEEDED(shell.p->SetArguments(args))
        && SUCCEEDED(shell.p->SetWorkingDirectory(cwd.c_str()))
        && SUCCEEDED(shell.p->QueryInterface(IID_PPV_ARGS(&persist.p))) && SUCCEEDED(persist.p->Save(path.c_str(), TRUE));
}
static int Scripts(const std::wstring& root, PCWSTR self) {
    setup::Paths paths{setup::Join(root, L"scripts"), {}, {}, {}};
    std::wstring folder;
    REQUIRE(setup::EnsureCompanionFolder(paths, folder));
    const auto bat = setup::Join(folder, L"直接 & %PATH% !.BAT"), cmd = setup::Join(root, L"工具 & %TEMP% !.CmD");
    const auto ps = setup::Join(root, L"插件 ' & 空格.PS1"), lnk = setup::Join(root, L"自定义入口.LNK");
    REQUIRE(WriteScript(bat, "@echo off\r\n> bat.ok echo ok\r\n"));
    REQUIRE(WriteScript(cmd, "@echo off\r\n> cmd.ok echo ok\r\n"));
    REQUIRE(WriteScript(ps, "param([string]$value)\r\nif ($value -ceq 'two words') { Set-Content -LiteralPath ps.ok -Value ok }\r\n"));
    REQUIRE(setup::AddCompanionProgram(paths, bat));
    const auto addCmd = setup::AddCompanionProgram(paths, cmd);
    if (!addCmd) printf("Add CMD failed: %lu\n", addCmd.code);
    REQUIRE(addCmd);
    const auto plainPS = setup::Join(folder, L"direct.ps1");
    REQUIRE(WriteScript(plainPS, "Set-Content -LiteralPath direct-ps.ok -Value ok\r\n"));
    REQUIRE(MakeShortcut(lnk, ps, L"\"two words\"", folder));
    REQUIRE(setup::AddCompanionProgram(paths, lnk) && setup::AddCompanionProgram(paths, lnk));
    const auto imported = setup::Join(folder, L"自定义入口.lnk");
    setup::Link info;
    REQUIRE(setup::ReadLink(imported, info) && info.target == ps && info.arguments == L"\"two words\"" && info.directory == folder);
    REQUIRE(MakeShortcut(lnk, ps, L"\"different settings\"", folder));
    REQUIRE(!setup::AddCompanionProgram(paths, lnk));
    REQUIRE(MakeShortcut(lnk, cmd, L"& echo unexpected", folder));
    REQUIRE(!setup::AddCompanionProgram(paths, lnk));
    REQUIRE(MakeShortcut(setup::Join(folder, L"unsupported-args.lnk"), cmd, L"& echo unexpected", folder));
    // Local unsigned fixtures only. This sets policy for this isolated test process and its children.
    // The production launcher never sets or overrides execution policy.
    REQUIRE(SetEnvironmentVariableW(L"PSExecutionPolicyPreference", L"RemoteSigned"));
    attempts = started = 0;
    companions::Dispatch(folder, self, CountStart);
    REQUIRE(attempts == 4 && started == 4);
    const auto end = GetTickCount64() + 15000;
    auto done = [&] { return Exists(setup::Join(folder, L"bat.ok")) && Exists(setup::Join(root, L"cmd.ok"))
        && Exists(setup::Join(folder, L"ps.ok")) && Exists(setup::Join(folder, L"direct-ps.ok")); };
    while (!done() && GetTickCount64() < end) Sleep(20);
    REQUIRE(done());
    puts("PASS BAT/CMD/PS1 dispatch, literal paths with special characters, LNK import, preserved arguments and working directory, collision protection.");
    return 0;
}
static int Suite(PCWSTR work, PCWSTR self) {
    const auto root = setup::Join(work, L"companion-test-" + std::to_wstring(GetTickCount64()) + L"-" + std::to_wstring(GetCurrentProcessId()));
    REQUIRE(CreateDirectoryW(root.c_str(), nullptr));
    setup::Paths paths{root, {}, {}, {}};
    std::wstring folder = setup::Join(root, companions::Folder);
    auto measure = [&] {
        const auto begin = std::chrono::steady_clock::now();
        for (int i = 0; i < 500; ++i) companions::Dispatch(folder, self, CountStart);
        return std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - begin).count() / 500;
    };
    const auto missingUs = measure();
    REQUIRE(!Exists(folder) && !attempts);
    REQUIRE(setup::EnsureCompanionFolder(paths, folder));
    const auto emptyUs = measure();
    REQUIRE(!attempts);
    const auto direct = setup::Join(folder, L"直接启动.exe");
    const auto external = setup::Join(root, L"外部 工具.exe");
    const auto shortcut = setup::Join(folder, L"外部 工具.lnk");
    const auto working = setup::Join(root, L"工作 目录");
    REQUIRE(CreateDirectoryW(working.c_str(), nullptr));
    REQUIRE(CopyFileW(self, direct.c_str(), TRUE) && CopyFileW(self, external.c_str(), TRUE));
    REQUIRE(setup::AddCompanionProgram(paths, external) && setup::AddCompanionProgram(paths, external));
    setup::Link link;
    REQUIRE(setup::ReadLink(shortcut, link) && link.target == external && link.directory == root);
    REQUIRE(setup::AddCompanionProgram(paths, direct) && !Exists(setup::Join(folder, L"直接启动.lnk")));
    const auto other = setup::Join(working, L"外部 工具.exe");
    REQUIRE(CopyFileW(self, other.c_str(), TRUE));
    REQUIRE(!setup::AddCompanionProgram(paths, other));
    REQUIRE(setup::ReadLink(shortcut, link) && link.target == external);
    {
        companions::ComPtr<IShellLinkW> shell; companions::ComPtr<IPersistFile> persist;
        REQUIRE(SUCCEEDED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&shell.p))));
        REQUIRE(SUCCEEDED(shell.p->QueryInterface(IID_PPV_ARGS(&persist.p))));
        REQUIRE(SUCCEEDED(persist.p->Load(shortcut.c_str(), STGM_READ)));
        REQUIRE(SUCCEEDED(shell.p->SetArguments(L"--probe \"中文 参数\" \"\"")));
        REQUIRE(SUCCEEDED(shell.p->SetWorkingDirectory(working.c_str())));
        REQUIRE(SUCCEEDED(persist.p->Save(shortcut.c_str(), TRUE)));
    }
    const auto nested = setup::Join(folder, L"nested");
    REQUIRE(CreateDirectoryW(nested.c_str(), nullptr));
    REQUIRE(Touch(setup::Join(nested, L"nested.exe").c_str()));
    REQUIRE(Touch(setup::Join(folder, L"broken.lnk").c_str()));
    REQUIRE(Touch(setup::Join(folder, L"readme.txt").c_str()));
    REQUIRE(Touch(setup::Join(folder, L"bad.exe").c_str()));
    REQUIRE(Touch(setup::Join(folder, L"ChatGPTProxyLauncher.exe").c_str()));
    REQUIRE(!setup::AddCompanionProgram(paths, setup::Join(folder, L"readme.txt")));
    REQUIRE(SetEnvironmentVariableW(companions::Guard, L"1"));
    companions::Dispatch(folder, self, CountStart);
    REQUIRE(!attempts);
    REQUIRE(SetEnvironmentVariableW(companions::Guard, nullptr));
    companions::Dispatch(folder, self, CountStart);
    REQUIRE(attempts == 3 && started == 2);
    const auto end = GetTickCount64() + 10000;
    while ((!Exists(setup::Join(folder, L"direct.ok")) || !Exists(setup::Join(working, L"link.ok"))) && GetTickCount64() < end) Sleep(10);
    REQUIRE(Exists(setup::Join(folder, L"direct.ok")) && Exists(setup::Join(working, L"link.ok")));
    printf("PASS companion EXE and shortcut launched isolated children; args, cwd, collision, filtering and recursion guard. Missing/empty folder checks: %.1f/%.1f us mean (500 calls; excludes known-folder lookup and process startup).\n", missingUs, emptyUs);
    return Scripts(root, self);
}
int wmain(int argc, wchar_t** argv) {
    if (argc == 2) {
        if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) return 2;
        wchar_t self[32768]; DWORD n = GetModuleFileNameW(nullptr, self, 32768);
        const int result = n && n < 32768 ? Suite(argv[1], self) : 2;
        CoUninitialize(); return result;
    }
    wchar_t guard[2]{};
    if (GetEnvironmentVariableW(companions::Guard, guard, 2) != 1) return 2;
    if (argc == 1) return Touch(L"direct.ok") ? 0 : 2;
    return argc == 4 && !wcscmp(argv[1], L"--probe") && !wcscmp(argv[2], L"中文 参数") && !*argv[3] && Touch(L"link.ok") ? 0 : 2;
}
