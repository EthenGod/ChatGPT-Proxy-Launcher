// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#ifdef SETUP_UI_EMBEDDED
// Tauri's Windows resource compiler uses IDI_APPLICATION for the app icon.
#define IDI_CHATGPT 32512
#else
#define IDI_CHATGPT 1
#endif
