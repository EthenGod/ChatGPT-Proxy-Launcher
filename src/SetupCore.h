// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <string>
#include <vector>

namespace setup {
inline constexpr wchar_t LauncherName[] = L"ChatGPTProxyLauncher.exe";
inline constexpr wchar_t LinkName[] = L"ChatGPT Proxy Launcher.lnk";
inline constexpr wchar_t Marker[] = L"ChatGPT Launcher Setup / v1";
inline constexpr size_t MaxArguments = 8191; // Conservative, verified by Shell round trip.
struct Options {
    bool startup = false, silent = false, proxy = false, desktop = false;
    bool codexEnv = true;
    std::wstring protocol = L"http", host = L"127.0.0.1", port;
    std::wstring bypass, extra, directory;
};
struct Paths { std::wstring defaultDirectory, startupLink, settingsLink, desktopLink, codexEnvFile; };
struct Link { std::wstring target, arguments, directory, description; };
struct Result {
    DWORD code = 0;
    std::wstring message;
    std::wstring conflictDirectory{}; // Only ownership conflicts offer an Explorer location.
    explicit operator bool() const { return code == 0; }
};
struct Loaded {
    Options options;
    std::wstring managedTarget, notice, deployedVersion;
    bool deployed = false, hasSettings = false;
};
struct Payload { const BYTE* data; size_t size; };
std::wstring ErrorText(DWORD error);
std::wstring Join(const std::wstring& directory, const std::wstring& file);

// Paths and companion shortcuts (SetupCore.cpp).
Result GetPaths(Paths& paths);
Result EnsureCompanionFolder(const Paths& paths, std::wstring& folder);
Result AddCompanionProgram(const Paths& paths, const std::wstring& program);
Result NormalizeDirectory(const std::wstring& input, std::wstring& output);

// Text conversion only; no file writes or process launches (SetupArguments.cpp).
std::wstring QuoteArgument(const std::wstring& value);
Result BuildArguments(const Options& options, std::wstring& arguments);
Result ParseArguments(const std::wstring& arguments, Options& options);

// Deployment owns file validation, commit and rollback (SetupCore.cpp).
Result ReadLink(const std::wstring& path, Link& link, bool expandedTarget = false);
Result Load(const Paths& paths, Loaded& loaded);
Result Apply(const Paths& paths, const Options& options, Payload payload,
    std::wstring& deployedTarget);
} // namespace setup
