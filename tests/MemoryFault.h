// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <new>
#include <cstdlib>
#include <cstddef>
#include <cstdint>
#include <malloc.h>

// Test-only allocation failure/counting. Arming is local to the calling thread.
namespace memory_fault {
inline thread_local bool armed = false;
inline thread_local std::ptrdiff_t remaining = -1;
inline thread_local size_t calls = 0;
inline thread_local size_t bytes = 0;
// Observe one data pointer without keeping a heap-backed allocation registry.
inline thread_local const void* watchedAllocation = nullptr;
inline thread_local bool watchedReleased = false;
struct Scope {
    explicit Scope(std::ptrdiff_t after = -1) { calls = 0; bytes = 0; remaining = after; armed = true; }
    ~Scope() { armed = false; }
};
}
void* operator new(size_t size) {
    if (memory_fault::armed) {
        ++memory_fault::calls;
        memory_fault::bytes += size;
        if (memory_fault::remaining == 0) throw std::bad_alloc();
        if (memory_fault::remaining > 0) --memory_fault::remaining;
    }
    if (void* value = std::malloc(size ? size : 1)) return value;
    throw std::bad_alloc();
}
void* operator new[](size_t size) { return ::operator new(size); }
void operator delete(void* value) noexcept {
    if (value && memory_fault::watchedAllocation) {
        // Large MSVC vectors can align data inside the malloc block returned by operator new.
        const auto begin = reinterpret_cast<std::uintptr_t>(value);
        const auto watched = reinterpret_cast<std::uintptr_t>(memory_fault::watchedAllocation);
        if (watched >= begin && watched - begin < _msize(value)) {
            memory_fault::watchedReleased = true;
            memory_fault::watchedAllocation = nullptr;
        }
    }
    std::free(value);
}
void operator delete[](void* value) noexcept { ::operator delete(value); }
void operator delete(void* value, size_t) noexcept { ::operator delete(value); }
void operator delete[](void* value, size_t) noexcept { ::operator delete(value); }
