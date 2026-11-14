// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <new>
#include <cstdlib>
#include <cstddef>

// Test-only allocation failure/counting. Arming is local to the calling thread.
namespace memory_fault {
inline thread_local bool armed = false;
inline thread_local std::ptrdiff_t remaining = -1;
inline thread_local size_t calls = 0;
struct Scope {
    explicit Scope(std::ptrdiff_t after = -1) { calls = 0; remaining = after; armed = true; }
    ~Scope() { armed = false; }
};
}
void* operator new(size_t size) {
    if (memory_fault::armed) {
        ++memory_fault::calls;
        if (memory_fault::remaining == 0) throw std::bad_alloc();
        if (memory_fault::remaining > 0) --memory_fault::remaining;
    }
    if (void* value = std::malloc(size ? size : 1)) return value;
    throw std::bad_alloc();
}
void* operator new[](size_t size) { return ::operator new(size); }
void operator delete(void* value) noexcept { std::free(value); }
void operator delete[](void* value) noexcept { std::free(value); }
void operator delete(void* value, size_t) noexcept { std::free(value); }
void operator delete[](void* value, size_t) noexcept { std::free(value); }
