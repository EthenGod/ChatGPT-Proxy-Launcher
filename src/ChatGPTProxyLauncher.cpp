// SPDX-License-Identifier: GPL-3.0-only
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <appmodel.h>
#include <wchar.h>

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

static DWORD StartApplication(PCWSTR executable, PCWSTR arguments) {
    const size_t required = CommandCapacity(executable, arguments);
    if (!required) return ERROR_FILENAME_EXCED_RANGE;
    const size_t length = wcslen(executable);
    Buffer command(required * sizeof(wchar_t)), directory((length + 1) * sizeof(wchar_t));
    if (!command.get() || !directory.get()) return ERROR_NOT_ENOUGH_MEMORY;
    DWORD result = BuildCommand(executable, arguments, static_cast<PWSTR>(command.get()), required);
    if (result) return result;
    CopyMemory(directory.get(), executable, (length + 1) * sizeof(wchar_t));
    PWSTR separator = wcsrchr(static_cast<PWSTR>(directory.get()), L'\\');
    if (!separator) return ERROR_BAD_PATHNAME;
    // Preserve the slash of a drive root: C:\app.exe must use C:\, not C:.
    if (separator == static_cast<PWSTR>(directory.get()) + 2
        && static_cast<PCWSTR>(directory.get())[1] == L':') ++separator;
    *separator = L'\0';
    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process = {};
    if (!CreateProcessW(executable, static_cast<PWSTR>(command.get()), nullptr, nullptr, FALSE,
        0, nullptr, static_cast<PCWSTR>(directory.get()), &startup, &process)) return GetLastError();
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

int WINAPI wWinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ PWSTR arguments, _In_ int) {
    // The only reserved option is an opt-in, read-only diagnostic, without any window.
    const wchar_t checkOption[] = L"--launcher-check";
    const size_t checkLength = sizeof(checkOption) / sizeof(wchar_t) - 1;
    bool check = wcsncmp(arguments, checkOption, checkLength) == 0
        && (!arguments[checkLength] || arguments[checkLength] == L' ' || arguments[checkLength] == L'\t');
    if (check) {
        arguments += checkLength;
        while (*arguments == L' ' || *arguments == L'\t') ++arguments;
    }
    Buffer executable(PathCapacity * sizeof(wchar_t));
    if (!executable.get()) return ReportError(ERROR_NOT_ENOUGH_MEMORY);
    DWORD result = FindExecutable(static_cast<PWSTR>(executable.get()), PathCapacity);
    if (result) return ReportError(result);
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
    return ReportError(StartApplication(static_cast<PCWSTR>(executable.get()), arguments));
}
