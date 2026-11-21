[简体中文](release-upload.md) | [English](release-upload.en.md) | [日本語](release-upload.ja.md)

> This document may include machine-translated content. If anything differs, refer to the [Chinese original](release-upload.md).

# v1.2.3 Loading indicators in the configurator

[![Quick start: step-by-step guide](https://img.shields.io/badge/First_time-Quick_start-11775B?style=for-the-badge)](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.3/README.en.md#quick-start) [![Download the Windows x64 configurator](https://img.shields.io/badge/Windows%20x64-Download%20ChatGPTLauncherSetup.exe-2563EB?style=for-the-badge)](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases/download/v1.2.3/ChatGPTLauncherSetup.exe)

**New to the launcher? Start with the [Quick start guide](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.3/README.en.md#quick-start).** The one-line PowerShell command downloads and opens the latest stable configurator by default. The guide also covers manual downloads and first-time setup.

**Upgrading from an earlier version?** Open the new configurator and choose **Save changes** to update your launcher. [Download the v1.2.3 configurator](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases/download/v1.2.3/ChatGPTLauncherSetup.exe)

If **Apply to Codex environment variables** is on, save your work in ChatGPT and Codex first. The configurator will try to quit ChatGPT when you save the settings.

## ✨ What's new

- The configurator shows placeholders when it takes a moment to open, read settings, validate launch arguments, or load companion programs for the first time. Refreshing the companion list keeps the current entries visible.

## 🛠️ Fixes

- When settings take a while to load and argument validation finishes quickly, the preview goes straight to the result without flashing a placeholder.
- The confirmation dialog now leaves more space between the next steps and the action buttons after saving.

## ⚠️ Known issues

- Silent startup is still unavailable. The main ChatGPT window may appear at launch.
- If the configurator reports that ChatGPT is still running, quit ChatGPT completely and reopen it from the **ChatGPT Launcher** shortcut.
