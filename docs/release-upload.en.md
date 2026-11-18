[简体中文](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.0-beta.1/docs/release-upload.md) | [English](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.0-beta.1/docs/release-upload.en.md) | [日本語](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.0-beta.1/docs/release-upload.ja.md)

> This document may include machine-translated content. If anything differs, refer to the [Chinese original](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.0-beta.1/docs/release-upload.md).

# v1.2.0 Beta 1

[![Download the Windows x64 configurator](https://img.shields.io/badge/Windows%20x64-Download%20ChatGPTLauncherSetup.exe-2563EB?style=for-the-badge)](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases/download/v1.2.0-beta.1/ChatGPTLauncherSetup.exe)

Double-click the download to open it. To update an existing installation, choose **Save changes**. [Direct download](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/releases/download/v1.2.0-beta.1/ChatGPTLauncherSetup.exe) | [User guide](https://github.com/EthenGod/ChatGPT-Proxy-Launcher/blob/v1.2.0-beta.1/README.en.md)

## What's new

- The redesigned configurator supports Chinese, English, Japanese, and light and dark themes. Turning on the proxy detects its port once. Click the notification to see where the port came from.
- Advanced settings lists companion programs and identifies those that support the launcher’s integration protocol.
- If WebView2 is missing, PowerShell offers to install it or open the native compatibility interface. Double-clicking the configurator opens compatibility mode automatically.
- The PowerShell command defaults to stable releases. Use `-Channel Preview` to include prereleases, `-Branch` to select a branch, and `-Tag` to pin a release.

## Before saving

Silent startup remains unavailable. When applying proxy settings to Codex, save your work in ChatGPT first. The configurator will try to quit ChatGPT and will prompt you if you need to quit and reopen it manually.
