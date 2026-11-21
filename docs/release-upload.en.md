[简体中文](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.1/docs/release-upload.md) | [English](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.1/docs/release-upload.en.md) | [日本語](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.1/docs/release-upload.ja.md)

> This document may include machine-translated content. If anything differs, refer to the [Chinese original](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.1/docs/release-upload.md).

# 🚀 v1.2.1 Redesigned settings

[![Quick start: step-by-step guide](https://img.shields.io/badge/First_time-Quick_start-11775B?style=for-the-badge)](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.1/README.en.md#quick-start) [![Download the Windows x64 configurator](https://img.shields.io/badge/Windows%20x64-Download%20ChatGPTLauncherSetup.exe-2563EB?style=for-the-badge)](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases/download/v1.2.1/ChatGPTLauncherSetup.exe)

**New to the launcher? Start with the [Quick start guide](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.1/README.en.md#quick-start).** The one-line PowerShell command downloads and opens the latest stable configurator by default. The guide also covers manual downloads and first-time setup.

**Upgrading from an earlier version?** Open the new configurator and choose **Save changes** to update your launcher. [Download the v1.2.1 configurator](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases/download/v1.2.1/ChatGPTLauncherSetup.exe)

If **Apply to Codex** is on, save your work in ChatGPT and Codex first. The configurator will try to quit ChatGPT when you save the settings.

## ✨ What's new

- Set up launch at sign-in, your desktop shortcut, and your proxy on the main screen. The launch argument preview updates as you make changes.
- Turn on the proxy and the configurator looks up the port and fills it in if it finds one. Click the notification in the lower right to see where it came from.
- See and add programs that open alongside ChatGPT under **Advanced settings**. The list marks programs with launcher integration and shows which ones are disabled.
- If your PC is missing WebView2, double-clicking the configurator opens compatibility mode so you can still change your settings.
- PowerShell downloads stable releases by default. You can also choose to try a beta.

## 🛠️ Fixes

- Fixed a black flash across the window when switching languages. The proxy address and port you entered also stay in place.
- If you minimize the configurator, double-clicking it again brings back the same window so you can pick up where you left off.
- Fixed an error message that could appear even after a successful save.

## ⚠️ Known issues

- Silent startup is still unavailable. The main ChatGPT window may appear at launch.
- If the configurator reports that ChatGPT is still running, quit ChatGPT completely and reopen it to use the new Codex proxy settings.
