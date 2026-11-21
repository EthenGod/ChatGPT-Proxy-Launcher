[简体中文](release-upload.md) | [English](release-upload.en.md) | [日本語](release-upload.ja.md)

> This document may include machine-translated content. If anything differs, refer to the [Chinese original](release-upload.md).

# v1.2.4 Smoother text wrapping when resizing the configurator

[![Quick start: step-by-step guide](https://img.shields.io/badge/First_time-Quick_start-11775B?style=for-the-badge)](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.4/README.en.md#quick-start) [![Download the Windows x64 configurator](https://img.shields.io/badge/Windows%20x64-Download%20ChatGPTLauncherSetup.exe-2563EB?style=for-the-badge)](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases/download/v1.2.4/ChatGPTLauncherSetup.exe)

**New to the launcher? Start with the [Quick start guide](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.4/README.en.md#quick-start).** The one-line PowerShell command downloads and opens the latest stable configurator by default. The guide also covers manual downloads and first-time setup.

**Upgrading from an earlier version?** Open the new configurator and choose **Save changes** to update your launcher. [Download the v1.2.4 configurator](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases/download/v1.2.4/ChatGPTLauncherSetup.exe)

If **Apply to Codex environment variables** is on, save your work in ChatGPT and Codex first. The configurator will try to quit ChatGPT when you save the settings.

## ✨ What's new

- As you resize the configurator, labels, launch arguments and help text move smoothly between lines. The proxy fields rearrange as the window narrows, and the scrollbar fades in or out as needed.
- Text wraps immediately when Windows animation effects are turned off.

## 🛠️ Fixes

- Close and Save changes stay centered as footer messages grow or shrink. The input warning also stays in place when it replaces the save reminder.

## ⚠️ Known issues

- Silent startup is still unavailable. The main ChatGPT window may appear at launch.
- If the configurator reports that ChatGPT is still running, quit ChatGPT completely and reopen it from the **ChatGPT Launcher** shortcut.
