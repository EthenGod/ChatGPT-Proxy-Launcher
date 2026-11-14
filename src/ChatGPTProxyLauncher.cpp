// SPDX-License-Identifier: GPL-3.0-only
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <appmodel.h>
#include <wchar.h>
#include "TaskbarShortcut.h"
#include "StartupApps.h"
#include "LauncherAutomation.h"

// Local tool, not an OpenAI product. No proxy settings are compiled in.
static const wchar_t Family[] = L"OpenAI.Codex_2p2nqsd0c76g0";
static const size_t PathCapacity = 32768;

class Buffer {
    void* value;
public:
    explicit Buffer(size_t bytes) : value(HeapAlloc(GetProcessHeap(), 0, bytes)) {}
    ~Buffer() { if (value) HeapFree(GetProcessHeap(), 0, value); }
    void* get() const { return value; }
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
};

static bool PathChanged(DWORD error) {
    return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND
        || error == ERROR_NOT_FOUND || error == APPMODEL_ERROR_NO_PACKAGE;
}

static DWORD FindExecutable(wchar_t* executable, size_t capacity) {
    DWORD lastError = ERROR_RETRY;
    // A store update can replace the package between enumeration and path lookup.
    for (int attempt = 0; attempt < 3; ++attempt) {
        UINT32 count = 0, characters = 0;
        LONG result = GetPackagesByPackageFamily(Family, &count, nullptr, &characters, nullptr);
        if (result == ERROR_SUCCESS && count == 0) return ERROR_NOT_FOUND;
        if (result != ERROR_INSUFFICIENT_BUFFER) return result == 0 ? ERROR_INVALID_DATA : result;
        if (!count || count > 1024 || !characters || characters > 2097152)
            return ERROR_INVALID_DATA;
        Buffer names(count * sizeof(PWSTR)), strings(characters * sizeof(wchar_t));
        if (!names.get() || !strings.get()) return ERROR_NOT_ENOUGH_MEMORY;
        result = GetPackagesByPackageFamily(Family, &count, static_cast<PWSTR*>(names.get()),
            &characters, static_cast<PWSTR>(strings.get()));
        if (result == ERROR_INSUFFICIENT_BUFFER) { lastError = ERROR_RETRY; continue; }
        if (result != ERROR_SUCCESS) return result;

        PCWSTR newest = nullptr;
        UINT64 newestVersion = 0;
        for (UINT32 index = 0; index < count; ++index) {
            PCWSTR fullName = static_cast<PWSTR*>(names.get())[index];
            UINT32 bytes = 0;
            result = PackageIdFromFullName(fullName, PACKAGE_INFORMATION_BASIC, &bytes, nullptr);
            if (result != ERROR_INSUFFICIENT_BUFFER) return result == 0 ? ERROR_INVALID_DATA : result;
            if (bytes < sizeof(PACKAGE_ID) || bytes > 131072) return ERROR_INVALID_DATA;
            Buffer identity(bytes);
            if (!identity.get()) return ERROR_NOT_ENOUGH_MEMORY;
            result = PackageIdFromFullName(fullName, PACKAGE_INFORMATION_BASIC, &bytes,
                static_cast<BYTE*>(identity.get()));
            if (result != ERROR_SUCCESS) return result;
            const PACKAGE_ID* id = static_cast<PACKAGE_ID*>(identity.get());
            if (!id->name || !id->publisherId || !id->resourceId) return ERROR_INVALID_DATA;
            if (wcscmp(id->name, L"OpenAI.Codex") || wcscmp(id->publisherId, L"2p2nqsd0c76g0")
                || *id->resourceId) continue;
            if (!newest || id->version.Version > newestVersion) {
                newest = fullName;
                newestVersion = id->version.Version;
            }
        }
        if (!newest) return ERROR_NOT_FOUND;
        UINT32 pathLength = static_cast<UINT32>(capacity);
        result = GetPackagePathByFullName(newest, &pathLength, executable);
        if (PathChanged(result)) { lastError = result; continue; }
        if (result != ERROR_SUCCESS) return result;
        const wchar_t suffix[] = L"\\app\\ChatGPT.exe";
        const size_t length = wcslen(executable);
        if (length + sizeof(suffix) / sizeof(wchar_t) > capacity) return ERROR_FILENAME_EXCED_RANGE;
        CopyMemory(executable + length, suffix, sizeof(suffix));
        DWORD attributes = GetFileAttributesW(executable);
        if (attributes == INVALID_FILE_ATTRIBUTES) {
            lastError = GetLastError();
            if (PathChanged(lastError)) continue;
            return lastError;
        }
        if (attributes & FILE_ATTRIBUTE_DIRECTORY) return ERROR_FILE_NOT_FOUND;
        return ERROR_SUCCESS;
    }
    return lastError;
}

static size_t CommandCapacity(PCWSTR executable, PCWSTR arguments) {
    const size_t pathLength = wcslen(executable), argumentLength = wcslen(arguments);
    if (!pathLength || pathLength > 32767 || argumentLength > 32767) return 0;
    const size_t required = pathLength + argumentLength + (argumentLength ? 4 : 3);
    return required <= 32767 ? required : 0;
}

static DWORD BuildCommand(PCWSTR executable, PCWSTR arguments, wchar_t* command, size_t capacity) {
    const size_t required = CommandCapacity(executable, arguments);
    if (!required || required > capacity) return ERROR_FILENAME_EXCED_RANGE;
    const size_t pathLength = wcslen(executable), argumentLength = wcslen(arguments);
    // Explicit application path plus a quoted argv[0]. Preserve the argument tail exactly.
    command[0] = L'"';
    CopyMemory(command + 1, executable, pathLength * sizeof(wchar_t));
    command[pathLength + 1] = L'"';
    size_t offset = pathLength + 2;
    if (argumentLength) command[offset++] = L' ';
    CopyMemory(command + offset, arguments, (argumentLength + 1) * sizeof(wchar_t));
    return ERROR_SUCCESS;
}

static DWORD StartApplication(PCWSTR executable, PCWSTR arguments, PCWSTR cwd = nullptr, int show = -1) {
    const size_t required = CommandCapacity(executable, arguments);
    if (!required) return ERROR_FILENAME_EXCED_RANGE;
    const size_t length = wcslen(executable);
    PCWSTR separator = wcsrchr(executable, L'\\');
    if (!separator) return ERROR_BAD_PATHNAME;
    // Command and optional working directory share one short-lived allocation.
    Buffer storage((required + (cwd ? 0 : length + 1)) * sizeof(wchar_t));
    if (!storage.get()) return ERROR_NOT_ENOUGH_MEMORY;
    PWSTR command = static_cast<PWSTR>(storage.get());
    DWORD result = BuildCommand(executable, arguments, command, required);
    if (result) return result;
    if (!cwd) {
        PWSTR directory = command + required;
        size_t directoryLength = static_cast<size_t>(separator - executable);
        // Preserve the slash of a drive root: C:\app.exe must use C:\, not C:.
        if (directoryLength == 2 && executable[1] == L':') ++directoryLength;
        CopyMemory(directory, executable, directoryLength * sizeof(wchar_t));
        directory[directoryLength] = L'\0';
        cwd = directory;
    }
    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);
    if (show >= 0) { startup.dwFlags = STARTF_USESHOWWINDOW; startup.wShowWindow = static_cast<WORD>(show); }
    PROCESS_INFORMATION process = {};
    if (!CreateProcessW(executable, command, nullptr, nullptr, FALSE,
        0, nullptr, cwd, &startup, &process)) return GetLastError();
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return ERROR_SUCCESS;
}

static void WriteText(DWORD stream, PCWSTR text) {
    HANDLE output = GetStdHandle(stream);
    if (!output || output == INVALID_HANDLE_VALUE) return;
    const int length = static_cast<int>(wcslen(text));
    DWORD mode = 0, written = 0;
    if (GetConsoleMode(output, &mode)) {
        WriteConsoleW(output, text, length, &written, nullptr);
        return;
    }
    const int bytes = WideCharToMultiByte(CP_UTF8, 0, text, length, nullptr, 0, nullptr, nullptr);
    if (!bytes) return;
    Buffer encoded(bytes);
    if (!encoded.get()) return;
    WideCharToMultiByte(CP_UTF8, 0, text, length, static_cast<char*>(encoded.get()), bytes, nullptr, nullptr);
    WriteFile(output, encoded.get(), bytes, &written, nullptr);
}

static int ReportError(DWORD error) {
    if (!error) return 0;
    const HANDLE output = GetStdHandle(STD_ERROR_HANDLE);
    if (!output || output == INVALID_HANDLE_VALUE) return static_cast<int>(error);
    PWSTR message = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, error, 0, reinterpret_cast<PWSTR>(&message), 0, nullptr);
    if (message) {
        WriteText(STD_ERROR_HANDLE, message);
        LocalFree(message);
    }
    return static_cast<int>(error);
}

int WINAPI wWinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ PWSTR arguments, _In_ int) try {
    chain::Invocation invocation;
    const int parsed = invocation.Parse(arguments);
    if (parsed) return parsed;
    if (invocation.mode == chain::Invocation::Mode::Info) return invocation.Info();
    arguments = const_cast<PWSTR>(invocation.forwarded);
    const bool explicitChain = invocation.mode == chain::Invocation::Mode::Chain;
    auto error = [&](DWORD code) { return explicitChain ? invocation.Finish(code, automation::Json{}) : ReportError(code); };
    // The original diagnostic remains read-only and bypasses all extension work.
    const wchar_t checkOption[] = L"--launcher-check";
    const size_t checkLength = sizeof(checkOption) / sizeof(wchar_t) - 1;
    bool check = !explicitChain && wcsncmp(arguments, checkOption, checkLength) == 0
        && (!arguments[checkLength] || arguments[checkLength] == L' ' || arguments[checkLength] == L'\t');
    if (check) {
        arguments += checkLength;
        while (*arguments == L' ' || *arguments == L'\t') ++arguments;
    }
    Buffer executable(PathCapacity * sizeof(wchar_t));
    if (!executable.get()) return error(ERROR_NOT_ENOUGH_MEMORY);
    DWORD result = FindExecutable(static_cast<PWSTR>(executable.get()), PathCapacity);
    if (result) return error(result);
    if (check) {
        if (!CommandCapacity(static_cast<PCWSTR>(executable.get()), arguments))
            return ReportError(ERROR_FILENAME_EXCED_RANGE);
        WriteText(STD_OUTPUT_HANDLE, L"Executable=");
        WriteText(STD_OUTPUT_HANDLE, static_cast<PCWSTR>(executable.get()));
        WriteText(STD_OUTPUT_HANDLE, L"\r\nArguments=");
        WriteText(STD_OUTPUT_HANDLE, arguments);
        WriteText(STD_OUTPUT_HANDLE, L"\r\n");
        return 0;
    }
    if (!CommandCapacity(static_cast<PCWSTR>(executable.get()), arguments))
        return error(ERROR_FILENAME_EXCED_RANGE);
    RepairTaskbarShortcut(static_cast<PCWSTR>(executable.get()));
    const PCWSTR chat = static_cast<PCWSTR>(executable.get());
    result = StartApplication(chat, arguments);
    if (result) return error(result);
    try {
        if (invocation.options.root.empty()) invocation.options.root = chain::RootDirectory();
        const auto report = chain::Run(invocation.options, chat, StartApplication);
        return explicitChain ? invocation.Finish(0, report) : 0;
    } catch (...) {
        if (explicitChain) {
            auto report = automation::Json::Object(); report[L"entries"] = automation::Json::Array();
            report[L"exitCode"] = automation::Json::Number(10); report[L"chainStatus"] = automation::Json(L"partial");
            return invocation.Finish(0, report);
        }
        return 0; // An optional extension failure must not change the ChatGPT launch result.
    }
} catch (...) {
    // Even an empty JSON value can allocate. Do not allocate another error report here.
    return 70;
}
