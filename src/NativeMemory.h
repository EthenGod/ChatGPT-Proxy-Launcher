// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <windows.h>
#include <objbase.h>
#include <memory>

namespace native {
struct LocalDeleter {
    void operator()(void* value) const noexcept { LocalFree(value); }
};
struct TaskDeleter {
    void operator()(void* value) const noexcept { CoTaskMemFree(value); }
};
// Adopt immediately after the API returns, before copying to an allocating C++ object.
template<class T> using LocalPtr = std::unique_ptr<T, LocalDeleter>;
template<class T> using TaskPtr = std::unique_ptr<T, TaskDeleter>;
template<class T> struct ComDeleter {
    void operator()(T* value) const noexcept { value->Release(); }
};
template<class T> using ComPtr = std::unique_ptr<T, ComDeleter<T>>;
}
