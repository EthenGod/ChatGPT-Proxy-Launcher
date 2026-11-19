// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
#define SETUP_NOEXCEPT noexcept
extern "C" {
#else
#define SETUP_NOEXCEPT
#endif

// Internal, statically linked ABI. Buffers are owned and freed by C++ only.
// A nonzero status can have no JSON, including during allocation failure.
// The desktop caller serializes read_state and action, including preference changes.
typedef struct SetupBridgeBuffer {
    uint8_t* data;
    size_t length;
} SetupBridgeBuffer;

void setup_bridge_initialize(void) SETUP_NOEXCEPT;
void setup_bridge_free(SetupBridgeBuffer* buffer) SETUP_NOEXCEPT;
// Null paths select the current user's paths. Tests must provide all seven
// explicit paths as UTF-8 JSON. Paths are never accepted from the webview.
uint32_t setup_bridge_read_state(const uint8_t* paths, size_t length,
    SetupBridgeBuffer* output) SETUP_NOEXCEPT;
enum SetupBridgeAction {
    SETUP_VALIDATE = 1, SETUP_DETECT = 2, SETUP_SAVE = 3, SETUP_DIRECTORY = 4,
    SETUP_ADD_PROGRAM = 5, SETUP_OPEN_LOCATION = 6, SETUP_PREFERENCES = 7, SETUP_LIST_PROGRAMS = 8
};
// commit: 0 not started, 1 saved, 2 rolled back, 3 rollback incomplete.
// Written before serializing the response, even when allocation fails afterwards.
// owner is the trusted main HWND; request and paths are bounded UTF-8 JSON.
uint32_t setup_bridge_action(uint32_t action, const uint8_t* request, size_t requestLength,
    const uint8_t* paths, size_t pathsLength, uintptr_t owner,
    SetupBridgeBuffer* output, uint32_t* commit) SETUP_NOEXCEPT;
int32_t setup_bridge_headless(const uint16_t* tail, size_t tailLength,
    const uint8_t* paths, size_t pathsLength) SETUP_NOEXCEPT;
int32_t setup_bridge_compatibility(const uint8_t* paths, size_t length) SETUP_NOEXCEPT;
// Bootstrap errors must also work without WebView2 or an initialized Tauri app.
void setup_bridge_show_bootstrap_error(uint32_t compatibilityFailed) SETUP_NOEXCEPT;

#ifdef __cplusplus
}
#endif
#undef SETUP_NOEXCEPT
