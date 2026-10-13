// SPDX-License-Identifier: GPL-3.0-only
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <windows.h>
#include <iphlpapi.h>
#include <tlhelp32.h>
#include <shlobj.h>
#include <roapi.h>
#include <windows.data.json.h>
#include <wrl.h>
#include <wrl/wrappers/corewrappers.h>
#include "ProxyDetection.h"
#include <algorithm>
#include <sstream>
#include <vector>

namespace setup {
namespace {
struct Listener { unsigned port; DWORD process; };
struct Candidate { unsigned port; std::wstring source; };
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Wrappers::HString;
using Microsoft::WRL::Wrappers::HStringReference;
using namespace ABI::Windows::Data::Json;
struct RuntimeScope {
    HRESULT result = RoInitialize(RO_INIT_MULTITHREADED);
    ~RuntimeScope() { if (SUCCEEDED(result)) RoUninitialize(); }
};
std::wstring JsonString(IJsonObject* object, PCWSTR key) {
    HString value;
    if (FAILED(object->GetNamedString(HStringReference(key).Get(), value.GetAddressOf()))) return {};
    UINT32 length = 0;
    const wchar_t* text = value.GetRawBuffer(&length);
    return std::wstring(text, length);
}
bool HasJsonMember(IJsonObject* object, PCWSTR key) {
    ComPtr<IJsonValue> value;
    return SUCCEEDED(object->GetNamedValue(HStringReference(key).Get(), &value));
}
// Windows' native JSON parser keeps outbound/server ports and nested fields separate.
std::vector<unsigned> JsonPorts(const std::string& text, const std::wstring& protocol, bool xray) {
    if (text.empty() || text.size() > 4 * 1024 * 1024) return {};
    RuntimeScope runtime;
    if (FAILED(runtime.result) && runtime.result != RPC_E_CHANGED_MODE) return {};
    const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (!length) return {};
    std::wstring wide(static_cast<size_t>(length), L'\0');
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), wide.data(), length)) return {};
    if (wide.front() == 0xFEFF) wide.erase(0, 1);
    HString json;
    if (FAILED(json.Set(wide.data(), static_cast<UINT32>(wide.size())))) return {};
    ComPtr<IJsonObjectStatics> factory;
    if (FAILED(RoGetActivationFactory(HStringReference(RuntimeClass_Windows_Data_Json_JsonObject).Get(), IID_PPV_ARGS(&factory)))) return {};
    ComPtr<IJsonObject> root;
    if (FAILED(factory->Parse(json.Get(), &root))) return {};
    ComPtr<IJsonArray> inbounds;
    if (FAILED(root->GetNamedArray(HStringReference(L"inbounds").Get(), &inbounds))) return {};
    std::vector<unsigned> ports, xrayMixed;
    for (UINT32 index = 0; index <= 256; ++index) {
        ComPtr<IJsonObject> inbound;
        const HRESULT result = inbounds->GetObjectAt(index, &inbound);
        if (result == E_BOUNDS) return ports.empty() ? xrayMixed : ports;
        if (FAILED(result) || index == 256) return {};
        const bool singbox = HasJsonMember(inbound.Get(), L"type");
        const std::wstring kind = JsonString(inbound.Get(), singbox ? L"type" : L"protocol");
        const bool matches = singbox
            ? kind == L"mixed" || kind == (protocol == L"http" ? L"http" : L"socks")
            : kind == (protocol == L"http" ? L"http" : L"socks") || (xray && protocol == L"http" && kind == L"socks");
        if (!matches) continue;
        const std::wstring listen = JsonString(inbound.Get(), L"listen");
        if (HasJsonMember(inbound.Get(), L"listen") && listen != L"127.0.0.1" && listen != L"0.0.0.0" && listen != L"localhost") continue;
        // Authenticated or TLS/custom-transport inbounds cannot be represented by this port field.
        if (HasJsonMember(inbound.Get(), L"tls") || HasJsonMember(inbound.Get(), L"streamSettings")) continue;
        ComPtr<IJsonArray> users;
        if (HasJsonMember(inbound.Get(), L"users")) {
            if (FAILED(inbound->GetNamedArray(HStringReference(L"users").Get(), &users))) continue;
            ComPtr<IJsonObject> user;
            if (users->GetObjectAt(0, &user) != E_BOUNDS) continue;
        }
        ComPtr<IJsonObject> settings;
        if (HasJsonMember(inbound.Get(), L"settings")) {
            if (FAILED(inbound->GetNamedObject(HStringReference(L"settings").Get(), &settings))) continue;
            if (HasJsonMember(settings.Get(), L"auth") && JsonString(settings.Get(), L"auth") != L"noauth") continue;
            if (HasJsonMember(settings.Get(), L"accounts")) {
                if (FAILED(settings->GetNamedArray(HStringReference(L"accounts").Get(), &users))) continue;
                ComPtr<IJsonObject> user;
                if (users->GetObjectAt(0, &user) != E_BOUNDS) continue;
            }
        }
        double port = 0;
        if (FAILED(inbound->GetNamedNumber(HStringReference(singbox ? L"listen_port" : L"port").Get(), &port))) continue;
        if (port >= 1 && port <= 65535 && port == static_cast<unsigned>(port)) {
            // Older configs may provide separate HTTP and SOCKS ports. Prefer explicit HTTP.
            auto& target = !singbox && xray && protocol == L"http" && kind == L"socks" ? xrayMixed : ports;
            target.push_back(static_cast<unsigned>(port));
        }
    }
    return {}; // Unreasonably large inbound arrays need manual confirmation.
}
std::string Trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    return value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
}
unsigned PortNumber(const std::string& text) {
    if (text.empty() || text.size() > 5) return 0;
    unsigned port = 0;
    for (char c : text) {
        if (c < '0' || c > '9') return 0;
        port = port * 10 + (c - '0');
    }
    return port <= 65535 ? port : 0;
}
// Deliberately accepts only simple top-level port scalars, never node/controller ports.
unsigned ConfigPort(std::string text, const std::wstring& protocol) {
    if (text.compare(0, 3, "\xEF\xBB\xBF") == 0) text.erase(0, 3);
    unsigned ports[3]{};
    bool seen[3]{};
    std::istringstream lines(text);
    std::string line;
    while (std::getline(lines, line)) {
        if (Trim(line) == "---" || Trim(line) == "...") return 0; // No multi-document YAML.
        const auto colon = line.find(':');
        if (colon == std::string::npos || line.empty() || line[0] == ' ' || line[0] == '\t') continue;
        const std::string key = Trim(line.substr(0, colon));
        const int index = key == "mixed-port" ? 0 : key == "port" ? 1 : key == "socks-port" ? 2 : -1;
        if (index < 0) continue;
        if (seen[index]) return 0;
        seen[index] = true;
        std::string value = Trim(line.substr(colon + 1));
        const auto comment = value.find('#');
        if (comment != std::string::npos) {
            if (!comment || (value[comment - 1] != ' ' && value[comment - 1] != '\t')) return 0;
            value = Trim(value.substr(0, comment));
        }
        if (value.size() >= 2 && ((value.front() == '\'' && value.back() == '\'') || (value.front() == '"' && value.back() == '"')))
            value = value.substr(1, value.size() - 2);
        ports[index] = PortNumber(value);
        if (!ports[index] && value != "0") return 0;
    }
    return ports[0] ? ports[0] : ports[protocol == L"http" ? 1 : 2];
}
std::string ReadConfig(const std::wstring& path) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (file == INVALID_HANDLE_VALUE) return {};
    LARGE_INTEGER size{};
    std::string text;
    if (GetFileSizeEx(file, &size) && size.QuadPart > 0 && size.QuadPart <= 4 * 1024 * 1024) {
        text.resize(static_cast<size_t>(size.QuadPart));
        DWORD count = 0;
        if (!ReadFile(file, text.data(), static_cast<DWORD>(text.size()), &count, nullptr) || count != text.size()) text.clear();
    }
    CloseHandle(file);
    return text;
}
std::vector<Listener> Listeners() {
    DWORD size = sizeof(MIB_TCPTABLE_OWNER_PID);
    std::vector<BYTE> buffer(size);
    for (int attempt = 0; attempt < 3; ++attempt) {
        const DWORD result = GetExtendedTcpTable(buffer.data(), &size, FALSE,
            AF_INET, TCP_TABLE_OWNER_PID_LISTENER, 0);
        if (result == NO_ERROR && !buffer.empty()) {
            const auto table = reinterpret_cast<const MIB_TCPTABLE_OWNER_PID*>(buffer.data());
            std::vector<Listener> listeners;
            for (DWORD i = 0; i < table->dwNumEntries; ++i) {
                const auto& row = table->table[i];
                if (row.dwLocalAddr == 0 || row.dwLocalAddr == htonl(INADDR_LOOPBACK))
                    listeners.push_back({ntohs(static_cast<u_short>(row.dwLocalPort)), row.dwOwningPid});
            }
            return listeners;
        }
        if (result != ERROR_INSUFFICIENT_BUFFER || size < sizeof(MIB_TCPTABLE_OWNER_PID) || size > 16 * 1024 * 1024) return {};
        buffer.resize(size);
    }
    return {};
}
std::vector<DWORD> CoreProcesses(PCWSTR name) {
    std::vector<DWORD> ids;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return ids;
    PROCESSENTRY32W entry{}; entry.dwSize = sizeof(entry);
    if (Process32FirstW(snapshot, &entry)) do {
        if (!_wcsicmp(entry.szExeFile, name)) ids.push_back(entry.th32ProcessID);
    } while (Process32NextW(snapshot, &entry));
    CloseHandle(snapshot);
    return ids;
}
bool Listening(unsigned port, const std::vector<Listener>& listeners, const std::vector<DWORD>* ids = nullptr) {
    return port && std::any_of(listeners.begin(), listeners.end(), [&](const Listener& item) {
        return item.port == port && (!ids || std::find(ids->begin(), ids->end(), item.process) != ids->end());
    });
}
std::wstring KnownFolder(REFKNOWNFOLDERID id) {
    PWSTR path = nullptr;
    if (FAILED(SHGetKnownFolderPath(id, KF_FLAG_DONT_VERIFY, nullptr, &path))) return {};
    std::wstring result(path); CoTaskMemFree(path); return result;
}
std::wstring ProcessImage(DWORD id) {
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, id);
    if (!process) return {};
    std::wstring image(32768, L'\0'); DWORD size = static_cast<DWORD>(image.size());
    const BOOL found = QueryFullProcessImageNameW(process, 0, image.data(), &size);
    CloseHandle(process);
    if (!found) return {};
    image.resize(size); return image;
}
std::wstring ParentDirectory(const std::wstring& path) {
    const auto separator = path.find_last_of(L"\\/");
    return separator == std::wstring::npos ? L"" : path.substr(0, separator);
}
bool UnderDirectory(const std::wstring& image, const std::wstring& directory) {
    return !directory.empty() && image.size() > directory.size() && image[directory.size()] == L'\\'
        && !_wcsnicmp(image.c_str(), directory.c_str(), directory.size());
}
std::wstring V2rayDirectory(const std::wstring& image) {
    const auto directory = ParentDirectory(image);
    const auto bin = ParentDirectory(directory);
    const auto separator = bin.find_last_of(L"\\/");
    if (separator != std::wstring::npos && !_wcsicmp(bin.substr(separator + 1).c_str(), L"bin"))
        return ParentDirectory(bin); // Current layout: <app>/bin/<core>/<core>.exe.
    if (directory.size() >= 4 && !_wcsicmp(directory.substr(directory.size() - 4).c_str(), L"\\bin"))
        return ParentDirectory(directory);
    return directory; // Older portable versions keep their core beside v2rayN.exe.
}
bool RegularFile(const std::wstring& path) {
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}
// Discovery I/O is replaceable so tests never consult the user's tools or configuration.
struct Discovery {
    virtual ~Discovery() = default;
    virtual std::vector<DWORD> Processes(PCWSTR name) const { return CoreProcesses(name); }
    virtual std::wstring Image(DWORD id) const { return ProcessImage(id); }
    virtual std::wstring Folder(REFKNOWNFOLDERID id) const { return KnownFolder(id); }
    virtual bool File(const std::wstring& path) const { return RegularFile(path); }
    virtual std::string Read(const std::wstring& path) const { return ReadConfig(path); }
};
void AddYaml(std::vector<Candidate>& candidates, const std::wstring& path, PCWSTR name,
    const std::wstring& protocol, const std::vector<Listener>& listeners, const std::vector<DWORD>& ids, const Discovery& io) {
    const unsigned port = ConfigPort(io.Read(path), protocol);
    if (Listening(port, listeners, &ids)) candidates.push_back({port, name});
}
void DiscoverCfw(std::vector<Candidate>& candidates, const std::wstring& protocol, const std::vector<Listener>& listeners, const Discovery& io) {
    std::vector<DWORD> ids;
    for (const auto* name : {L"clash-win64.exe", L"clash-win64-n.exe", L"clash-win32.exe"}) {
        const auto found = io.Processes(name); ids.insert(ids.end(), found.begin(), found.end());
    }
    if (ids.empty()) return;
    const auto profile = io.Folder(FOLDERID_Profile);
    if (!profile.empty()) AddYaml(candidates, profile + L"\\.config\\clash\\config.yaml", L"CFW", protocol, listeners, ids, io);
    // Portable CFW keeps its config in data beside the running GUI executable.
    for (DWORD gui : io.Processes(L"Clash for Windows.exe")) {
        const auto directory = ParentDirectory(io.Image(gui));
        std::vector<DWORD> owned;
        for (DWORD id : ids) if (UnderDirectory(io.Image(id), directory)) owned.push_back(id);
        if (!owned.empty()) AddYaml(candidates, directory + L"\\data\\config.yaml", L"CFW", protocol, listeners, owned, io);
    }
}
void DiscoverV2rayN(std::vector<Candidate>& candidates, const std::wstring& protocol, const std::vector<Listener>& listeners, const Discovery& io) {
    const auto local = io.Folder(FOLDERID_LocalAppData);
    const bool runningGui = !io.Processes(L"v2rayN.exe").empty();
    for (const auto* name : {L"xray.exe", L"v2ray.exe", L"sing-box.exe"}) {
        for (DWORD id : io.Processes(name)) {
            const auto root = V2rayDirectory(io.Image(id));
            // Do not label unrelated standalone cores as v2rayN or scan arbitrary directories.
            const bool localCore = !local.empty() && !_wcsicmp(root.c_str(), (local + L"\\v2rayN").c_str()) && runningGui;
            if (root.empty() || (!io.File(root + L"\\v2rayN.exe") && !localCore)) continue;
            std::vector<std::wstring> roots = {root};
            if (!local.empty()) roots.push_back(local + L"\\v2rayN");
            const std::vector<DWORD> owner = {id};
            for (const auto& directory : roots) {
                const auto modern = directory + L"\\binConfigs\\config.json";
                const auto path = io.File(modern) ? modern : directory + L"\\config.json";
                for (unsigned port : JsonPorts(io.Read(path), protocol, !_wcsicmp(name, L"xray.exe")))
                    if (Listening(port, listeners, &owner)) candidates.push_back({port, L"v2rayN"});
            }
        }
    }
}
unsigned SystemHttpPort(const std::wstring& server) {
    // A Windows "https=" entry selects target URLs; it does not imply TLS to the proxy.
    if (server.size() > 4096) return 0;
    std::string ascii;
    for (wchar_t c : server) { if (c > 127) return 0; ascii += static_cast<char>(c); }
    std::string endpoint;
    if (ascii.find('=') == std::string::npos) endpoint = Trim(ascii);
    else {
        std::istringstream entries(ascii);
        std::string entry;
        bool found = false;
        while (std::getline(entries, entry, ';')) {
            entry = Trim(entry);
            if (entry.compare(0, 5, "http=") == 0) {
                if (found) return 0;
                found = true; endpoint = Trim(entry.substr(5));
            }
        }
    }
    const auto colon = endpoint.find(':');
    if (colon == std::string::npos) return 0;
    const std::string host = endpoint.substr(0, colon);
    if (host != "127.0.0.1" && _stricmp(host.c_str(), "localhost")) return 0;
    return PortNumber(endpoint.substr(colon + 1));
}
ProxyDetection Select(const std::vector<Candidate>& candidates) {
    if (candidates.empty()) return {};
    const auto& first = candidates.front();
    for (const auto& item : candidates) if (item.port != first.port)
        return {L"", L"检测到多个代理端口，未自动填写。请在要使用的代理工具「端口设置」中核对后填写。"};
    return {std::to_wstring(first.port), L"已填入 " + first.source + L" 的端口。请核对后保存；检测不代表联网已成功。"};
}
}
ProxyDetection DetectProxyPort(const std::wstring& protocol, const std::wstring& host) {
    if (_wcsicmp(host.c_str(), L"localhost") && host != L"127.0.0.1")
        return {L"", L"自动检测仅支持本机地址 127.0.0.1 / localhost。请到对应代理工具的「端口设置」查看并手动填写。"};
    if (protocol != L"http" && protocol != L"socks5")
        return {L"", L"HTTPS 代理端口需要手动确认。请查看代理工具「端口设置」；混合端口通常请选择 HTTP 或 SOCKS5。"};
    const auto listeners = Listeners();
    std::vector<Candidate> candidates;
    const Discovery io;
    DiscoverCfw(candidates, protocol, listeners, io);
    DiscoverV2rayN(candidates, protocol, listeners, io);
    PWSTR roaming = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_DONT_VERIFY, nullptr, &roaming))) {
        const std::wstring root(roaming); CoTaskMemFree(roaming);
        const struct { PCWSTR path; PCWSTR process; PCWSTR name; } sources[] = {
            {L"\\io.github.clash-verge-rev.clash-verge-rev\\clash-verge.yaml", L"verge-mihomo.exe", L"Clash Verge Rev"},
            {L"\\com.follow\\clash\\config.yaml", L"FlClashCore.exe", L"FlClash"},
            {L"\\com.follow\\flclash\\config.yaml", L"FlClashCore.exe", L"FlClash"}
        };
        for (const auto& source : sources) {
            const auto ids = CoreProcesses(source.process);
            if (ids.empty()) continue;
            const unsigned port = ConfigPort(ReadConfig(root + source.path), protocol);
            if (Listening(port, listeners, &ids)) candidates.push_back({port, source.name});
        }
    }
    if (protocol == L"http") {
        constexpr PCWSTR key = L"Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings";
        DWORD enabled = 0, size = sizeof(enabled);
        if (RegGetValueW(HKEY_CURRENT_USER, key, L"ProxyEnable", RRF_RT_REG_DWORD, nullptr, &enabled, &size) == ERROR_SUCCESS && enabled == 1) {
            wchar_t server[4097]{}; size = sizeof(server);
            if (RegGetValueW(HKEY_CURRENT_USER, key, L"ProxyServer", RRF_RT_REG_SZ, nullptr, server, &size) == ERROR_SUCCESS) {
                const unsigned port = SystemHttpPort(server);
                if (Listening(port, listeners)) candidates.push_back({port, L"Windows 当前系统代理"});
            }
        }
    }
    return Select(candidates);
}
}
