[简体中文](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.2/docs/release-upload.md) | [English](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.2/docs/release-upload.en.md) | [日本語](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.2/docs/release-upload.ja.md)

> This document may include machine-translated content. If anything differs, refer to the [Chinese original](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.2/docs/release-upload.md).

# 🚀 v1.2.2 Proxy support for cloud Codex conversations

[![Quick start: step-by-step guide](https://img.shields.io/badge/First_time-Quick_start-11775B?style=for-the-badge)](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.2/README.en.md#quick-start) [![Download the Windows x64 configurator](https://img.shields.io/badge/Windows%20x64-Download%20ChatGPTLauncherSetup.exe-2563EB?style=for-the-badge)](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases/download/v1.2.2/ChatGPTLauncherSetup.exe)

**New to the launcher? Start with the [Quick start guide](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.2/README.en.md#quick-start).** The one-line PowerShell command downloads and opens the latest stable configurator by default. The guide also covers manual downloads and first-time setup.

**Upgrading from an earlier version?** Open the new configurator and choose **Save changes** to update your launcher. [Download the v1.2.2 configurator](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases/download/v1.2.2/ChatGPTLauncherSetup.exe)

If **Apply to Codex environment variables** is on, save your work in ChatGPT and Codex first. The configurator will try to quit ChatGPT when you save the settings.

## ✨ What's new

- **Fix connection issues in cloud Codex conversations** is on by default. After configuring a single HTTP or HTTPS proxy that requires no credentials, quit ChatGPT completely and reopen it from your configured **ChatGPT Launcher** shortcut. The new process can use the selected proxy for cloud conversations. A proxy bypass rule preserves the existing proxy environment.
- Changes to the **Launch argument preview** fade between values in the main configurator. Footer messages stay readable when they wrap in a narrow window. Selecting text or enabling reduced motion immediately shows the latest arguments.
- The launcher releases configuration data before waiting for companion programs to become ready, reducing memory use during the wait.

## 🛠️ Fixes

- Updating an existing Codex `.env` file preserves its permissions. If saving fails, the configurator protects the file copies it needs for recovery.
- The configurator explains when a companion program is missing a dependency. The PowerShell script now reports failures returned by the graphical configurator.

## ⚠️ Known issues

- Silent startup is still unavailable. The main ChatGPT window may appear at launch.
- If the configurator reports that ChatGPT is still running, quit ChatGPT completely and reopen it from the **ChatGPT Launcher** shortcut.
