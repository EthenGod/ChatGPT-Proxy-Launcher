// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "AutomationContract.h"
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <stdexcept>
#include <initializer_list>

// Small bounded JSON value layer for local automation contracts. No I/O or COM.
namespace automation {
struct Invalid : std::runtime_error { explicit Invalid(const char* message) : runtime_error(message) {} };
struct Json {
    enum class Type { Null, Boolean, Number, String, Array, Object } type = Type::Null;
    bool boolean = false;
    std::wstring text;
    std::vector<Json> array;
    // Transparent lookup accepts field literals without allocating a temporary string.
    std::map<std::wstring, Json, std::less<>> object;
    Json() = default;
    explicit Json(bool value) : type(Type::Boolean), boolean(value) {}
    explicit Json(std::wstring value) : type(Type::String), text(std::move(value)) {}
    explicit Json(const wchar_t* value) : type(Type::String), text(value) {}
    static Json Number(long long value);
    static Json Object();
    static Json Array();
    bool Has(const wchar_t* key) const;
    const Json& At(const wchar_t* key) const;
    Json& operator[](const wchar_t* key);
    const std::wstring& String(size_t maximum = 32767) const;
    bool Bool() const;
    long long Integer(long long minimum, long long maximum) const;
    void Fields(std::initializer_list<const wchar_t*> allowed) const;
};
std::wstring FromUtf8(std::string_view text);
std::string Utf8(const std::wstring& text);
Json ParseJson(std::string_view bytes, size_t maximum = contract::JsonBytes);
std::string WriteJson(const Json& value);
}
