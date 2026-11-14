// SPDX-License-Identifier: GPL-3.0-only
#include "CodexEnv.h"
#include "Localization.h"
#include <algorithm>

namespace setup {
namespace {
Result Fail(DWORD code, const std::wstring& what) {
    return { code ? code : ERROR_GEN_FAILURE, what + L"\n" + ErrorText(code) };
}
} // namespace

Result MergeCodexEnv(const std::vector<BYTE>& source, const Options& options, std::vector<BYTE>& output) {
    // Preserve unrelated bytes, comments, BOM and line endings. Never parse or expose secrets.
    if (source.size() > 1024 * 1024 || std::find(source.begin(), source.end(), 0) != source.end()
        || (!source.empty() && !MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
            reinterpret_cast<const char*>(source.data()), static_cast<int>(source.size()), nullptr, 0)))
        return Fail(ERROR_INVALID_DATA, l10n::Text("error.env.encoding"));
    for (size_t i = 0; i < source.size(); ++i) {
        if (source[i] == '\r' && (i + 1 == source.size() || source[i + 1] != '\n'))
            return Fail(ERROR_INVALID_DATA, l10n::Text("error.env.newlines"));
    }
    const std::wstring address = options.protocol + L"://" + options.host + L":" + options.port;
    if (address.find_first_of(L"'\"$`\r\n") != std::wstring::npos
        || std::any_of(address.begin(), address.end(), [](wchar_t c) { return c < L' '; }))
        return Fail(ERROR_INVALID_PARAMETER, l10n::Text("error.env.address"));
    const int count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, address.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (!count) return Fail(ERROR_INVALID_PARAMETER, l10n::Text("error.env.encode"));
    std::string url(static_cast<size_t>(count), '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, address.c_str(), -1, &url[0], count, nullptr, nullptr);
    url.pop_back();
    const char* keys[] = {"all_proxy", "ALL_PROXY", "HTTP_PROXY", "HTTPS_PROXY"};
    bool found[4]{};
    const std::string input(source.begin(), source.end());
    const size_t bom = input.compare(0, 3, "\xef\xbb\xbf") == 0 ? 3 : 0;
    std::string result = input.substr(0, bom);
    const std::string newline = input.find('\n') != std::string::npos && input.find("\r\n") == std::string::npos ? "\n" : "\r\n";
    char quote = 0;
    for (size_t start = bom; start < input.size();) {
        const size_t eol = input.find('\n', start);
        const size_t next = eol == std::string::npos ? input.size() : eol + 1;
        size_t end = eol == std::string::npos ? input.size() : eol;
        if (end > start && input[end - 1] == '\r') --end;
        const std::string line = input.substr(start, end - start);
        const std::string ending = input.substr(end, next - end);
        size_t keyStart = line.find_first_not_of(" \t");
        if (keyStart != std::string::npos && line.compare(keyStart, 6, "export") == 0
            && keyStart + 6 < line.size() && (line[keyStart + 6] == ' ' || line[keyStart + 6] == '\t'))
            keyStart = line.find_first_not_of(" \t", keyStart + 7);
        const size_t equals = line.find('=', keyStart);
        int match = -1, exact = -1;
        if (!quote && keyStart != std::string::npos && line[keyStart] != '#' && equals != std::string::npos) {
            const size_t last = line.find_last_not_of(" \t", equals - 1);
            const std::string key = last == std::string::npos ? "" : line.substr(keyStart, last + 1 - keyStart);
            for (int i = 0; i < 4; ++i) {
                // Windows environment names ignore case. Update existing aliases too,
                // while still ensuring all four requested spellings are present.
                if (_stricmp(key.c_str(), keys[i]) == 0) match = i;
                if (key == keys[i]) exact = i;
            }
        }
        // Track quoted multiline values so proxy-like text inside another value stays untouched.
        size_t valueStart = line.size();
        if (quote) valueStart = 0;
        else if (keyStart != std::string::npos && line[keyStart] != '#' && equals != std::string::npos)
            valueStart = equals + 1;
        bool escaped = false;
        size_t commentStart = std::string::npos;
        for (size_t i = valueStart; i < line.size(); ++i) {
            const char c = line[i];
            if (escaped) { escaped = false; continue; }
            if (c == '\\' && quote != '\'') { escaped = true; continue; }
            if (quote) { if (c == quote) quote = 0; }
            else if (c == '#' && (i == valueStart || line[i - 1] == ' ' || line[i - 1] == '\t')) {
                commentStart = i;
                while (commentStart > valueStart && (line[commentStart - 1] == ' ' || line[commentStart - 1] == '\t')) --commentStart;
                break;
            }
            else if (c == '\'' || c == '"') quote = c;
        }
        if (match >= 0) {
            if (quote) return Fail(ERROR_INVALID_DATA, l10n::Text("error.env.multiline"));
            result += line.substr(0, equals + 1) + url;
            if (commentStart != std::string::npos) {
                if (line[commentStart] == '#') result += ' ';
                result += line.substr(commentStart);
            }
            result += ending;
            if (exact >= 0) found[exact] = true;
        } else result += line + ending;
        start = next;
    }
    if (quote) return Fail(ERROR_INVALID_DATA, l10n::Text("error.env.quote"));
    for (int i = 0; i < 4; ++i) if (!found[i]) {
        if (result.size() > bom && result.back() != '\n') result += newline;
        result += std::string(keys[i]) + "=" + url + newline;
    }
    output.assign(result.begin(), result.end());
    return {};
}
} // namespace setup
