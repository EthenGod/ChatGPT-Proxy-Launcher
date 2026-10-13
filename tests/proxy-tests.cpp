// SPDX-License-Identifier: GPL-3.0-only
#include "../src/ProxyDetection.cpp"
#include <stdio.h>
#include <map>
struct FakeDiscovery : setup::Discovery {
    std::map<std::wstring, std::vector<DWORD>> processes;
    std::map<DWORD, std::wstring> images;
    std::map<std::wstring, std::string> files;
    std::vector<DWORD> Processes(PCWSTR name) const override {
        const auto item = processes.find(name); return item == processes.end() ? std::vector<DWORD>{} : item->second;
    }
    std::wstring Image(DWORD id) const override {
        const auto item = images.find(id); return item == images.end() ? L"" : item->second;
    }
    std::wstring Folder(REFKNOWNFOLDERID id) const override {
        return IsEqualGUID(id, FOLDERID_Profile) ? L"C:\\Example" : L"C:\\Example\\Local";
    }
    bool File(const std::wstring& path) const override { return files.count(path) != 0; }
    std::string Read(const std::wstring& path) const override {
        const auto item = files.find(path); return item == files.end() ? "" : item->second;
    }
};
#define REQUIRE(condition) do { if (!(condition)) { printf("FAIL line %d: %s\n", __LINE__, #condition); return 20; } } while (0)
int main() {
    using namespace setup;
    REQUIRE(PortNumber("65535") == 65535 && !PortNumber("65536") && !PortNumber("-1"));
    REQUIRE(!PortNumber("7890abc") && !PortNumber("0") && !PortNumber("99999999999"));
    REQUIRE(ConfigPort("mixed-port: 12345\nexternal-controller: 127.0.0.1:9090\n", L"http") == 12345);
    REQUIRE(ConfigPort("\xEF\xBB\xBFmixed-port: '23456' # comment\r\n", L"socks5") == 23456);
    REQUIRE(ConfigPort("mixed-port: 0\nport: 12345\nsocks-port: 23456\n", L"http") == 12345);
    REQUIRE(ConfigPort("mixed-port: 0\nport: 12345\nsocks-port: 23456\n", L"socks5") == 23456);
    REQUIRE(!ConfigPort("port: 12345\n", L"socks5"));
    REQUIRE(!ConfigPort("socks-port: 23456\n", L"http"));
    REQUIRE(!ConfigPort("external-controller: 127.0.0.1:9090\nproxies:\n  - name: test\n    port: 443\n", L"http"));
    REQUIRE(!ConfigPort("mixed-port: 12345\nmixed-port: 23456\n", L"http"));
    for (const auto* invalid : {"65536", "-1", "true", "12abc", "[7890]", "&port 7890", "7890#bad"})
        REQUIRE(!ConfigPort(std::string("mixed-port: ") + invalid, L"http"));
    REQUIRE(!ConfigPort("mixed-port: 12345\n---\nmixed-port: 23456\n", L"http"));
    const std::string legacy = R"({"inbounds":[
        {"protocol":"socks","port":12345,"listen":"127.0.0.1","settings":{"auth":"noauth"}},
        {"protocol":"http","port":23456,"listen":"0.0.0.0"},
        {"protocol":"dokodemo-door","port":34567}],
        "outbounds":[{"protocol":"http","port":443}]})";
    REQUIRE(JsonPorts(legacy, L"http", false) == std::vector<unsigned>{23456});
    REQUIRE(JsonPorts(legacy, L"http", true) == std::vector<unsigned>{23456});
    REQUIRE(JsonPorts(legacy, L"socks5", false) == std::vector<unsigned>{12345});
    const std::string modernXray = R"({"inbounds":[{"protocol":"socks","port":12345,"settings":{"auth":"noauth"}}]})";
    REQUIRE(JsonPorts(modernXray, L"http", true) == std::vector<unsigned>{12345});
    REQUIRE(JsonPorts(modernXray, L"http", false).empty()); // V2Ray SOCKS is not assumed to accept HTTP.
    const std::string sing = R"({"inbounds":[{"type":"tun","listen_port":34567},
        {"type":"mixed","listen_port":23456,"listen":"127.0.0.1","users":[]}],
        "outbounds":[{"type":"http","server_port":443}]})";
    REQUIRE(JsonPorts(sing, L"http", false) == std::vector<unsigned>{23456});
    REQUIRE(JsonPorts(sing, L"socks5", false) == std::vector<unsigned>{23456});
    REQUIRE(JsonPorts("\xEF\xBB\xBF" + sing, L"socks5", false) == std::vector<unsigned>{23456});
    for (const auto* invalid : {"0", "65536", "-1", "12.5", "true", "null", "\"1000-2000\""})
        REQUIRE(JsonPorts(std::string(R"({"inbounds":[{"protocol":"http","port":)") + invalid + "}]}", L"http", false).empty());
    for (const auto* unsafe : {
        R"({"protocol":"socks","port":12345,"settings":{"auth":"password"}})",
        R"({"protocol":"http","port":12345,"settings":{"accounts":[{"user":"example"}]}})",
        R"({"type":"mixed","listen_port":12345,"users":[{"username":"example"}]})",
        R"({"type":"http","listen_port":12345,"tls":{"enabled":true}})",
        R"({"protocol":"http","port":12345,"streamSettings":{"security":"tls"}})",
        R"({"protocol":"http","port":12345,"listen":"192.0.2.1"})",
        R"({"protocol":"http","port":12345,"listen":"::1"})",
        R"({"protocol":"http","port":12345,"listen":false})"})
        REQUIRE(JsonPorts(std::string("{\"inbounds\":[") + unsafe + "]}", L"http", true).empty());
    REQUIRE(JsonPorts(R"({"outbounds":[{"protocol":"http","port":443}]})", L"http", false).empty());
    REQUIRE(JsonPorts(R"({"inbounds":[{"protocol":"http","port":12345}]})" "trailing", L"http", false).empty());
    REQUIRE(JsonPorts("{broken", L"http", false).empty());
    REQUIRE(JsonPorts(std::string(4 * 1024 * 1024 + 1, ' '), L"http", false).empty());
    std::string boundary = "{\"inbounds\":[";
    for (int i = 0; i < 255; ++i) boundary += "{\"type\":\"tun\"},";
    boundary += "{\"protocol\":\"http\",\"port\":12345}]}";
    REQUIRE(JsonPorts(boundary, L"http", false) == std::vector<unsigned>{12345});
    boundary.insert(boundary.size() - 2, ",{\"type\":\"tun\"}");
    REQUIRE(JsonPorts(boundary, L"http", false).empty());
    REQUIRE(V2rayDirectory(L"C:\\Example\\v2rayN\\bin\\xray\\xray.exe") == L"C:\\Example\\v2rayN");
    REQUIRE(V2rayDirectory(L"C:\\示例 路径\\v2rayN\\v2ray.exe") == L"C:\\示例 路径\\v2rayN");
    REQUIRE(V2rayDirectory(L"C:\\Example\\v2rayN\\bin\\sing-box.exe") == L"C:\\Example\\v2rayN");
    REQUIRE(UnderDirectory(L"C:\\CFW\\resources\\clash.exe", L"c:\\cfw"));
    REQUIRE(!UnderDirectory(L"C:\\CFW-other\\clash.exe", L"C:\\CFW"));
    REQUIRE(!UnderDirectory(L"C:\\CFW\\clash.exe", L""));
    REQUIRE(SystemHttpPort(L"127.0.0.1:12345") == 12345);
    REQUIRE(SystemHttpPort(L"http=localhost:12345;https=127.0.0.1:23456") == 12345);
    REQUIRE(!SystemHttpPort(L"https=127.0.0.1:23456;socks=127.0.0.1:34567"));
    REQUIRE(!SystemHttpPort(L"http=127.0.0.1:12345;http=127.0.0.1:23456"));
    REQUIRE(!SystemHttpPort(L"proxy.example.com:12345") && !SystemHttpPort(L"127.0.0.1:65536"));
    REQUIRE(!SystemHttpPort(L"http://127.0.0.1:12345") && !SystemHttpPort(L"127.0.0.1:12345;localhost:23456"));
    const std::vector<Listener> listeners = {{12345, 42}, {23456, 43}};
    const std::vector<DWORD> owners = {42}, none;
    REQUIRE(Listening(12345, listeners, &owners) && !Listening(23456, listeners, &owners));
    REQUIRE(!Listening(12345, listeners, &none) && !Listening(34567, listeners));
    FakeDiscovery cfw;
    cfw.processes[L"clash-win64.exe"] = {42};
    cfw.files[L"C:\\Example\\.config\\clash\\config.yaml"] = "port: 12345\nsocks-port: 23456\n";
    std::vector<Candidate> found;
    DiscoverCfw(found, L"http", listeners, cfw);
    REQUIRE(Select(found).port == L"12345");
    found.clear(); DiscoverCfw(found, L"socks5", listeners, cfw);
    REQUIRE(found.empty()); // The configured SOCKS port belongs to another process.
    cfw.files.clear();
    cfw.processes[L"Clash for Windows.exe"] = {1};
    cfw.images[1] = L"C:\\便携 CFW\\Clash for Windows.exe";
    cfw.images[42] = L"C:\\便携 CFW\\resources\\static\\files\\clash-win64.exe";
    cfw.files[L"C:\\便携 CFW\\data\\config.yaml"] = "mixed-port: 12345\n";
    DiscoverCfw(found, L"socks5", listeners, cfw);
    REQUIRE(Select(found).port == L"12345");
    cfw.images[42] = L"C:\\便携 CFW-other\\clash-win64.exe";
    found.clear(); DiscoverCfw(found, L"socks5", listeners, cfw);
    REQUIRE(found.empty());
    FakeDiscovery v2;
    v2.processes[L"xray.exe"] = {42};
    v2.images[42] = L"C:\\示例 v2rayN\\bin\\xray\\xray.exe";
    v2.files[L"C:\\示例 v2rayN\\v2rayN.exe"] = "";
    v2.files[L"C:\\示例 v2rayN\\binConfigs\\config.json"] = modernXray;
    DiscoverV2rayN(found, L"http", listeners, v2);
    REQUIRE(Select(found).port == L"12345");
    v2.files[L"C:\\示例 v2rayN\\binConfigs\\config.json"] = "broken";
    v2.files[L"C:\\示例 v2rayN\\config.json"] = modernXray;
    found.clear(); DiscoverV2rayN(found, L"http", listeners, v2);
    REQUIRE(found.empty()); // Never fall back to old data when the current file is unreadable.
    v2.files.erase(L"C:\\示例 v2rayN\\binConfigs\\config.json");
    v2.images[42] = L"C:\\示例 v2rayN\\xray.exe";
    DiscoverV2rayN(found, L"socks5", listeners, v2);
    REQUIRE(Select(found).port == L"12345");
    v2.files.erase(L"C:\\示例 v2rayN\\v2rayN.exe");
    found.clear(); DiscoverV2rayN(found, L"socks5", listeners, v2);
    REQUIRE(found.empty()); // Standalone Xray is not assumed to be a v2rayN install.
    v2.images[42] = L"C:\\Example\\Local\\v2rayN\\bin\\xray\\xray.exe";
    v2.processes[L"v2rayN.exe"] = {1};
    v2.files[L"C:\\Example\\Local\\v2rayN\\binConfigs\\config.json"] = modernXray;
    DiscoverV2rayN(found, L"http", listeners, v2);
    REQUIRE(Select(found).port == L"12345");
    REQUIRE(Select({}).port.empty());
    REQUIRE(Select({{12345, L"Clash Verge Rev"}, {12345, L"Windows"}}).port == L"12345");
    REQUIRE(Select({{12345, L"Clash Verge Rev"}, {23456, L"FlClash"}}).port.empty());
    REQUIRE(Select({{12345, L"A"}, {23456, L"B"}}).message.find(L"多个") != std::wstring::npos);
    REQUIRE(DetectProxyPort(L"https", L"127.0.0.1").port.empty());
    REQUIRE(DetectProxyPort(L"http", L"proxy.example.com").port.empty());
    REQUIRE(DetectProxyPort(L"socks5", L"[::1]").port.empty());
    puts("Proxy detection tests passed (synthetic inputs, no user configuration accessed).");
    return 0;
}
