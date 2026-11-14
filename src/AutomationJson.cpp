// SPDX-License-Identifier: GPL-3.0-only
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "AutomationJson.h"
#include <algorithm>
#include <limits>

namespace automation {
Json Json::Number(long long value) { Json j; j.type = Type::Number; j.text = std::to_wstring(value); return j; }
Json Json::Object() { Json j; j.type = Type::Object; return j; }
Json Json::Array() { Json j; j.type = Type::Array; return j; }
bool Json::Has(const wchar_t* key) const { return type == Type::Object && object.count(key) != 0; }
const Json& Json::At(const wchar_t* key) const {
    if (!Has(key)) throw Invalid("missing-field");
    return object.at(key);
}
Json& Json::operator[](const wchar_t* key) {
    if (type != Type::Object) throw Invalid("expected-object");
    return object[key];
}
const std::wstring& Json::String(size_t maximum) const {
    if (type != Type::String || text.size() > maximum || text.find(L'\0') != text.npos) throw Invalid("invalid-string");
    return text;
}
bool Json::Bool() const { if (type != Type::Boolean) throw Invalid("expected-boolean"); return boolean; }
long long Json::Integer(long long minimum, long long maximum) const {
    if (type != Type::Number) throw Invalid("expected-integer");
    // Work in decimal, not floating point: 1.00000000000000001 must not become version 1.
    const bool negative = !text.empty() && text[0] == L'-';
    const auto exponentAt = text.find_first_of(L"eE");
    auto digits = text.substr(negative ? 1 : 0, exponentAt == text.npos ? text.npos : exponentAt - (negative ? 1 : 0));
    const auto dot = digits.find(L'.');
    int scale = dot == digits.npos ? 0 : -static_cast<int>(digits.size() - dot - 1);
    if (dot != digits.npos) digits.erase(dot, 1);
    const auto nonzero = digits.find_first_not_of(L'0');
    if (nonzero == digits.npos) { if (minimum > 0 || maximum < 0) throw Invalid("integer-out-of-range"); return 0; }
    digits.erase(0, nonzero);
    if (exponentAt != text.npos) {
        const auto exponent = text.substr(exponentAt + 1);
        if (exponent.size() > 6) throw Invalid("integer-out-of-range");
        scale += _wtoi(exponent.c_str());
    }
    while (scale < 0 && digits.back() == L'0') { digits.pop_back(); ++scale; }
    if (scale < 0 || scale > 19 || digits.size() + scale > 19 || (negative && minimum >= 0) || (!negative && maximum < 0)) throw Invalid("integer-out-of-range");
    digits.append(static_cast<size_t>(scale), L'0');
    const unsigned long long limit = negative ? static_cast<unsigned long long>(-(minimum + 1)) + 1 : static_cast<unsigned long long>(maximum);
    unsigned long long value = 0;
    for (const auto c : digits) {
        const unsigned digit = c - L'0';
        if (value > limit / 10 || (value == limit / 10 && digit > limit % 10)) throw Invalid("integer-out-of-range");
        value = value * 10 + digit;
    }
    const long long result = negative ? value == (1ull << 63) ? std::numeric_limits<long long>::min() : -static_cast<long long>(value) : static_cast<long long>(value);
    if (result < minimum || result > maximum) throw Invalid("integer-out-of-range");
    return result;
}
void Json::Fields(std::initializer_list<const wchar_t*> allowed) const {
    if (type != Type::Object) throw Invalid("expected-object");
    for (const auto& item : object)
        if (std::none_of(allowed.begin(), allowed.end(), [&](const wchar_t* name) { return item.first == name; }))
            throw Invalid("unknown-field");
}
std::wstring FromUtf8(const std::string& text) {
    if (text.empty()) return {};
    if (text.size() > INT_MAX) throw Invalid("text-too-large");
    const int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (!n) throw Invalid("invalid-utf8");
    std::wstring out(n, L'\0');
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), out.data(), n)) throw Invalid("invalid-utf8");
    return out;
}
std::string Utf8(const std::wstring& text) {
    if (text.empty()) return {};
    if (text.size() > INT_MAX) throw Invalid("text-too-large");
    const int n = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (!n) throw Invalid("invalid-unicode");
    std::string out(n, '\0');
    if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), out.data(), n, nullptr, nullptr)) throw Invalid("invalid-unicode");
    return out;
}
namespace {
class Reader {
    const std::wstring& s;
    size_t p = 0;
    wchar_t Peek() const { return p < s.size() ? s[p] : 0; }
    wchar_t Take() { if (p == s.size()) throw Invalid("incomplete-json"); return s[p++]; }
    void Space() { while (Peek() && wcschr(L" \t\r\n", Peek())) ++p; }
    void Expect(wchar_t c) { if (Take() != c) throw Invalid("invalid-json"); }
    wchar_t Hex() {
        unsigned value = 0;
        for (int i = 0; i < 4; ++i) {
            const wchar_t c = Take();
            const unsigned digit = c >= L'0' && c <= L'9' ? c - L'0' : c >= L'a' && c <= L'f' ? c - L'a' + 10 : c >= L'A' && c <= L'F' ? c - L'A' + 10 : 16;
            if (digit == 16) throw Invalid("invalid-unicode-escape");
            value = value * 16 + digit;
        }
        return static_cast<wchar_t>(value);
    }
    std::wstring String() {
        Expect(L'"'); std::wstring out;
        while (Peek() != L'"') {
            wchar_t c = Take();
            if (c < 32) throw Invalid("invalid-json-string");
            if (c == L'\\') {
                c = Take();
                switch (c) {
                case L'"': case L'\\': case L'/': break;
                case L'b': c = L'\b'; break; case L'f': c = L'\f'; break;
                case L'n': c = L'\n'; break; case L'r': c = L'\r'; break; case L't': c = L'\t'; break;
                case L'u': c = Hex(); break;
                default: throw Invalid("invalid-json-escape");
                }
            }
            if (!c) throw Invalid("nul-string");
            out += c;
        }
        Expect(L'"');
        // Reject unpaired surrogates, including escaped ones, before using filesystem APIs.
        for (size_t i = 0; i < out.size(); ++i) {
            const unsigned c = out[i];
            if (c >= 0xd800 && c <= 0xdbff) {
                if (++i == out.size() || out[i] < 0xdc00 || out[i] > 0xdfff) throw Invalid("invalid-surrogate");
            } else if (c >= 0xdc00 && c <= 0xdfff) throw Invalid("invalid-surrogate");
        }
        return out;
    }
    bool Digit() const { return Peek() >= L'0' && Peek() <= L'9'; }
    Json Value(unsigned depth) {
        Space(); if (depth > contract::JsonDepth) throw Invalid("json-too-deep");
        const wchar_t c = Peek();
        if (c == L'"') return Json(String());
        if (c == L'{' || c == L'[') {
            ++p; Space(); const bool object = c == L'{'; const wchar_t end = object ? L'}' : L']';
            Json j = object ? Json::Object() : Json::Array();
            if (Peek() == end) { ++p; return j; }
            for (;;) {
                Space();
                if (object) {
                    auto key = String(); Space(); Expect(L':');
                    auto value = Value(depth + 1);
                    if (!j.object.emplace(std::move(key), std::move(value)).second) throw Invalid("duplicate-key");
                } else j.array.push_back(Value(depth + 1));
                Space(); const wchar_t sep = Take(); if (sep == end) break;
                if (sep != L',') throw Invalid("invalid-json-separator");
            }
            return j;
        }
        for (const auto literal : {L"null", L"true", L"false"}) {
            const size_t n = wcslen(literal);
            if (s.compare(p, n, literal) == 0) { p += n; return literal[0] == L'n' ? Json{} : Json(literal[0] == L't'); }
        }
        const size_t start = p;
        if (Peek() == L'-') ++p;
        if (Peek() == L'0') ++p;
        else { if (!Digit()) throw Invalid("invalid-number"); while (Digit()) ++p; }
        if (Peek() == L'.') { ++p; if (!Digit()) throw Invalid("invalid-number"); while (Digit()) ++p; }
        if (Peek() == L'e' || Peek() == L'E') {
            ++p; if (Peek() == L'+' || Peek() == L'-') ++p;
            if (!Digit()) throw Invalid("invalid-number"); while (Digit()) ++p;
        }
        Json j; j.type = Json::Type::Number; j.text = s.substr(start, p - start); return j;
    }
public:
    explicit Reader(const std::wstring& text) : s(text) {}
    Json Read() { if (Peek() == 0xfeff) ++p; auto value = Value(0); Space(); if (p != s.size()) throw Invalid("json-trailing-data"); return value; }
};
void AppendString(std::wstring& out, const std::wstring& value) {
    constexpr wchar_t hex[] = L"0123456789abcdef"; out += L'"';
    for (const auto c : value) {
        if (c == L'"' || c == L'\\') { out += L'\\'; out += c; }
        else if (c < 32) { out += L"\\u00"; out += hex[(c >> 4) & 15]; out += hex[c & 15]; }
        else out += c;
    }
    out += L'"';
}
void Append(std::wstring& out, const Json& j, unsigned depth) {
    if (depth > contract::OutputDepth) throw Invalid("json-too-deep");
    switch (j.type) {
    case Json::Type::Null: out += L"null"; break;
    case Json::Type::Boolean: out += j.boolean ? L"true" : L"false"; break;
    case Json::Type::Number: out += j.text; break;
    case Json::Type::String: AppendString(out, j.text); break;
    case Json::Type::Array:
        out += L'['; for (size_t i = 0; i < j.array.size(); ++i) { if (i) out += L','; Append(out, j.array[i], depth + 1); } out += L']'; break;
    case Json::Type::Object:
        out += L'{'; bool first = true;
        for (const auto& kv : j.object) { if (!first) out += L','; first = false; AppendString(out, kv.first); out += L':'; Append(out, kv.second, depth + 1); }
        out += L'}'; break;
    }
}
}
Json ParseJson(const std::string& bytes, size_t maximum) {
    if (bytes.empty() || bytes.size() > maximum) throw Invalid("invalid-json-size");
    const auto text = FromUtf8(bytes); return Reader(text).Read();
}
std::string WriteJson(const Json& value) { std::wstring out; Append(out, value, 0); return Utf8(out) + "\n"; }
}
